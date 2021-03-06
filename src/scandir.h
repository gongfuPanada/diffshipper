#ifndef SCANDIR_H
#define SCANDIR_H

#include "ignore.h"

typedef struct {
    const ignores_t *ig;
    const char *base_path;
    int level;
} scandir_baton_t;

typedef int (*filter_fp)(const char *path, const struct dirent *, void *);

int ds_scandir(const char *dirname,
                struct dirent ***namelist,
                filter_fp filter,
                void *baton
               );

#endif
