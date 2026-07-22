#include "path.h"

#include <stdio.h>
#include <string.h>

void path_join(char *dst, size_t dst_size, const char *dir, const char *name) {
    if (!dir || !*dir) {
        snprintf(dst, dst_size, "%s", name);
        return;
    }

    const size_t dir_len = strlen(dir);
    const int needs_sep = dir_len > 0 && dir[dir_len - 1] != '/' && dir[dir_len - 1] != '\\';
    if (needs_sep) {
        snprintf(dst, dst_size, "%s/%s", dir, name);
    } else {
        snprintf(dst, dst_size, "%s%s", dir, name);
    }
}

int path_is_absolute(const char *path) {
    if (!path || !*path) return 0;
    if (path[0] == '/' || path[0] == '\\') return 1;
    return (strlen(path) >= 2 && path[1] == ':' &&
            ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')));
}
