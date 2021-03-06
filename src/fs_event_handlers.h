#ifndef DIFF_H
#define DIFF_H

#include <dirent.h>

#include "buf.h"
#include "mmap.h"

typedef struct {
  buf_t *buf;
  mmapped_file_t *mf;
  char *patch;
  size_t patch_size;
} diff_info_t;

void push_changes(lua_State *l, const char *base_path, const char *full_path);

#endif
