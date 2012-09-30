#include <stdlib.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#include "diff.h"
#include "log.h"
#include "net.h"
#include "util.h"


void event_cb(ConstFSEventStreamRef streamRef, void *cb_data, size_t count, void *paths,
              const FSEventStreamEventFlags flags[], const FSEventStreamEventId ids[]) {
    size_t i;
    const char *path;

    for (i = 0; i < count; i++) {
        path = ((char**)paths)[i];
        /* flags are unsigned long, IDs are uint64_t */
        log_debug("Change %llu in %s, flags %lu", ids[i], path, (long)flags[i]);
        if (ignored(path)) {
            /* we triggered this event */
            unignore_path(path);
        } else {
            push_changes(path);
        }
    }

    /* TODO: EXTREMELY BAD CODE FOLLOWS */
    /* seriously. I winced as I wrote this */
    /* this should be in its own thread. we shouldn't have to wait for local changes before checking for remote changes */
    ssize_t max_len = 8192;
    ssize_t msg_len;
    char *buf = malloc(max_len);
    ssize_t rv;
    strcpy(buf, "updates?");
    msg_len = strlen(buf);
/*    rv = send_bytes(buf, msg_len);
    if (rv != msg_len)
        die("not all of message sent");
        */
    rv = recv_bytes(buf, max_len);
    if (strncmp(buf, "no", 2) == 0) {
        log_debug("no updates");
    }
    else {
        log_debug("updates");
        apply_diff(buf, rv);
    }
    free(buf);

    if (count > 0) {
/*        exit(1);*/
    }
}


void init() {
    ignored_paths = NULL;
    ignored_paths_len = 0;
    set_log_level(LOG_LEVEL_DEBUG);
}


int main(int argc, char **argv) {
    int rv;
    char *path;

    init();

    if (argc < 2)
        die("No path to watch specified");
    path = realpath(argv[1], NULL);

    rv = run_cmd("mkdir -p %s%s", TMP_BASE, path);
    if (rv != 0)
        die("error creating temp directior %s", TMP_BASE);

    rv = run_cmd("cp -fr %s/ %s%s", path, TMP_BASE, path);
    if (rv != 0)
        die("error creating copying files to tmp dir %s", TMP_BASE);

    log_msg("Watching %s", path);
    free(path);

    CFStringRef cfs_path = CFStringCreateWithCString(NULL, argv[1], kCFStringEncodingUTF8); /* pretty sure I'm leaking this */
    CFArrayRef paths = CFArrayCreate(NULL, (const void **)&cfs_path, 1, NULL); /* ditto */
    void *cb_data = NULL;
    FSEventStreamRef stream;
    CFAbsoluteTime latency = 0.25;

    stream = FSEventStreamCreate(NULL, &event_cb, cb_data, paths, kFSEventStreamEventIdSinceNow, latency, kFSEventStreamCreateFlagNone);
    FSEventStreamScheduleWithRunLoop(stream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    FSEventStreamStart(stream);

    rv = server_connect("127.0.0.1", "3148");
    if (rv != 0)
        die("Couldn't connect to server");

    CFRunLoopRun();
    /* We never get here */

    return(0);
}
