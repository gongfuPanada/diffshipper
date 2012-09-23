#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>

#include "dmp.h"
#include "dmp_pool.h"

#include "diff.h"
#include "log.h"


void get_changes(const char *path) {
    log_debug("Getting changes for %s", path);
    
}

mmapped_file_t *mmap_file(const char *path) {
    int fd = -1;
    off_t f_len = 0;
    char *buf = NULL;
    struct stat statbuf;
    int rv = 0;
    mmapped_file_t *mf;

    fd = open(path, O_RDONLY);
    if (fd < 0) {
        log_err("Error opening file %s: %s Skipping...", path, strerror(errno));
        goto cleanup;
    }

    rv = fstat(fd, &statbuf);
    if (rv != 0) {
        log_err("Error fstat()ing file %s. Skipping...", path);
        goto cleanup;
    }
    if ((statbuf.st_mode & S_IFMT) == 0) {
        log_err("%s is not a file. Mode %u. Skipping...", path, statbuf.st_mode);
        goto cleanup;
    }
    if (statbuf.st_mode & S_IFIFO) {
        log_err("%s is a named pipe. Skipping...", path);
        goto cleanup;
    }

    f_len = statbuf.st_size;
    buf = mmap(0, f_len, PROT_READ, MAP_SHARED, fd, 0);
    if (buf == MAP_FAILED) {
        log_err("File %s failed to load: %s.", path, strerror(errno));
        goto cleanup;
    }

    mf = malloc(sizeof(mmapped_file_t));
    mf->buf = buf;
    mf->len = f_len;
    mf->fd = fd;
    return mf;

    cleanup:;
    if (fd != -1) {
        munmap(buf, f_len);
        close(fd);
    }
    return NULL;
}

void munmap_file(mmapped_file_t *mf) {
    munmap(mf->buf, mf->len);
    close(mf->fd);
}

void diff_files(const char *f1, const char *f2) {
    mmapped_file_t *mf1;
    mmapped_file_t *mf2;
    mf1 = mmap_file(f1);
    mf2 = mmap_file(f2);
    munmap_file(mf1);
    munmap_file(mf2);
    free(mf1);
    free(mf2);
}
