#ifndef FILE_H
#define FILE_H

#include <sys/types.h>

typedef struct {
    void *buf;
    off_t len;
    int fd;
} mmapped_file_t;

mmapped_file_t *mmap_file(const char *path, off_t size, int prot, int flags);
void munmap_file(mmapped_file_t *mf);
int msync_file(mmapped_file_t *mf, off_t len);

int is_binary(const void* buf, const int buf_len);

#endif
