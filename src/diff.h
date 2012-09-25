#ifndef DIFF_H
#define DIFF_H

#include "dmp.h"

#include "file.h"

typedef struct {
    const char *f1;
    const char *f2;
    mmapped_file_t *mf1;
    mmapped_file_t *mf2;
    dmp_diff *diff;
    dmp_options opts;
} ftc_diff_t;

void push_changes(const char *path);

void diff_files(ftc_diff_t *f, const char *f1, const char *f2);
void free_ftc_diff(ftc_diff_t *ftc_diff);


#endif
