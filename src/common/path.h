#ifndef COMMON_PATH_H
#define COMMON_PATH_H

#include <stddef.h>

/* Joins `dir` and `name` into `dst`, inserting a '/' separator only if
   `dir` doesn't already end with '/' or '\'. If `dir` is NULL or empty,
   `dst` just becomes a copy of `name`. Always null-terminates, truncating
   if `dst_size` is too small. */
void path_join(char *dst, size_t dst_size, const char *dir, const char *name);

/* Returns 1 if `path` is absolute: starts with '/' or '\', or is a
   Windows drive-letter path like "C:\...". Returns 0 otherwise
   (including for NULL/empty paths). */
int path_is_absolute(const char *path);

#endif /* COMMON_PATH_H */
