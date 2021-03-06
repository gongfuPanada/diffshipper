#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <xlocale.h>

#include <jansson.h>

#include "buf.h"
#include "dmp_lua.h"
#include "ignore.h"
#include "init_room.h"
#include "log.h"
#include "net.h"
#include "options.h"
#include "proto_handlers.h"
#include "util.h"


static void on_delete_buf(json_t *json_obj) {
    buf_t *buf;
    int buf_id;
    int user_id;
    char *username;
    char *path;

    parse_json(json_obj, "{s:i s:i s:s s:s}",
        "id", &buf_id,
        "user_id", &user_id,
        "username", &username,
        "path", &path
    );

    log_debug("User %s (id %i) deleted buf %i (path %s)", username, user_id, buf_id, path);
    buf = get_buf_by_id(buf_id);
    if (buf == NULL)
        die("tried to delete a buf that doesn't exist. this should never happen!");

    delete_buf(buf);
}

static void on_get_buf(json_t *json_obj) {
    buf_t *buf;
    buf_t *new_buf = calloc(1, sizeof(buf_t));
    buf_t tmp;

    parse_json(json_obj, "{s:i s:s s:s s:s}",
        "id", &(tmp.id),
        "buf", &(tmp.buf),
        "md5", &(tmp.md5),
        "path", &(tmp.path)
    );

    new_buf->id = tmp.id;
    /* Strings from parse_json disappear once json_obj has 0 refcount.
     * This happens at the end of the loop in remote_change_worker.
     */
    new_buf->buf = strdup(tmp.buf);
    new_buf->md5 = strdup(tmp.md5);
    new_buf->path = strdup(tmp.path);

    buf = get_buf_by_id(tmp.id);
    if (buf) {
        log_debug("old buf: %s", buf->buf);
        log_debug("new buf: %s", new_buf->buf);
        free(buf->buf);
        buf->buf = new_buf->buf;
        free(buf->md5);
        buf->md5 = new_buf->md5;
        free(buf->path);
        buf->path = new_buf->path;
        free(new_buf);
    } else {
        buf = new_buf;
        add_buf_to_bufs(buf);
    }
    save_buf(buf);
}


static void on_join(json_t *json_obj) {
    char *username;
    parse_json(json_obj, "{s:s}", "username", &username);
    log_msg("User %s joined the room", username);
}


static void on_msg(json_t *json_obj) {
    char *username;
    char *msg;

    parse_json(json_obj, "{s:s s:s}", "username", &username, "data", &msg);
    log_msg("Message from user %s: %s", username, msg);
}


static void on_part(json_t *json_obj) {
    char *username;

    parse_json(json_obj, "{s:s}", "username", &username);
    log_msg("User %s left the room", username);
}


static void on_patch(lua_State *l, json_t *json_obj) {
    int buf_id;
    int user_id;
    char *username;
    char *md5_before;
    char *md5_after;
    char *patch_str;
    char *path;

    parse_json(
        json_obj, "{s:i s:i s:s s:s s:s s:s s:s}",
        "id", &buf_id,
        "user_id", &user_id,
        "username", &username,
        "patch", &patch_str,
        "path", &path,
        "md5_before", &md5_before,
        "md5_after", &md5_after
    );

    log_debug("patch for buf %i (%s) from %s (id %i)", buf_id, path, username, user_id);

    buf_t *buf = get_buf_by_id(buf_id);
    if (buf == NULL) {
        die("we got a patch for a nonexistent buf id: %i", buf_id);
        return;
    }
    if (apply_patch(l, buf, patch_str) != 1) {
        log_err("Couldn't apply patch. Re-fetching buffer %i (%s)", buf_id, buf->path);
        send_json("{s:s s:i}", "name", "get_buf", "id", buf_id);
        return;
    }
    if (strcmp(buf->md5, md5_after) != 0) {
        log_err("Expected md5 %s but got %s after patching", md5_after, buf->md5);
        send_json("{s:s s:i}", "name", "get_buf", "id", buf_id);
        return;
    }
    save_buf(buf);
}


static void on_rename_buf(json_t *json_obj) {
    int buf_id;
    int user_id;
    char *username;
    char *path;
    char *old_path;
    buf_t *buf;
    char *full_path;

    parse_json(json_obj, "{s:i s:s s:s s:i s:s}",
        "id", &buf_id,
        "old_path", &old_path,
        "path", &path,
        "user_id", &user_id,
        "username", &username
    );

    log_debug("user %i %s renamed buf %i from %s to %s", user_id, username, buf_id, old_path, path);

    buf = get_buf_by_id(buf_id);
    if (buf == NULL) {
        die("trying to rename a nonexistent buf id: %i", buf_id);
        return;
    }

    if (strcmp(old_path, buf->path) != 0) {
        die("old rename path and buf %i path don't match! old_path: %s, buf path: %s", buf_id, old_path, buf->path);
    }
    full_path = get_full_path(buf->path);

    ignore_change(full_path);
    ignore_change(path);
    free(full_path);
}


static void on_room_info(json_t *json_obj) {
    const char *buf_id_str;
    json_t *bufs_obj;
    json_t *buf_obj;
    parse_json(json_obj, "{s:o}", "bufs", &bufs_obj);
    json_object_foreach(bufs_obj, buf_id_str, buf_obj) {
        send_json("{s:s s:i}", "name", "get_buf", "id", atoi(buf_id_str));
    }
    if (opts.create_room) {
        create_room(opts.path);
    }
}


void *remote_change_worker() {
    char *name;

    pthread_cond_wait(&server_conn_ready, &server_conn_mtx);
    pthread_mutex_unlock(&server_conn_mtx);

    json_t *json_obj;
    lua_State *l_ap = init_lua_state();

    while (1) {
        json_obj = recv_json();
        parse_json(json_obj, "{s:s}", "name", &name);

        if (strcmp(name, "room_info") == 0) {
            on_room_info(json_obj);
        } else if (strcmp(name, "create_buf") == 0) {
            on_get_buf(json_obj);
        } else if (strcmp(name, "delete_buf") == 0) {
            on_delete_buf(json_obj);
        } else if (strcmp(name, "get_buf") == 0) {
            on_get_buf(json_obj);
        } else if (strcmp(name, "highlight") == 0) {
            /* Don't print anything. Highlights are super-spammy. */
        } else if (strcmp(name, "join") == 0) {
            on_join(json_obj);
        } else if (strcmp(name, "msg") == 0) {
            on_msg(json_obj);
        } else if (strcmp(name, "part") == 0) {
            on_part(json_obj);
        } else if (strcmp(name, "patch") == 0) {
            on_patch(l_ap, json_obj);
        } else if (strcmp(name, "rename_buf") == 0) {
            on_rename_buf(json_obj);
        } else {
            log_err("Unknown event name: %s", name);
        }

        json_decref(json_obj);
    }

    lua_close(l_ap);
    pthread_exit(NULL);
    return NULL;
}
