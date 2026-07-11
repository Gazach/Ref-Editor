#include "config.h"
#include "../lib/cJSON/cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void copy_string(char *dst, size_t dst_size, const char *src) {
    if (dst_size == 0) return;
    if (!src) {
        dst[0] = '\0';
        return;
    }

#ifdef _MSC_VER
    strncpy_s(dst, dst_size, src, _TRUNCATE);
#else
    const size_t len = strlen(src);
    if (len >= dst_size) {
        memcpy(dst, src, dst_size - 1);
        dst[dst_size - 1] = '\0';
    } else {
        memcpy(dst, src, len + 1);
    }
#endif
}

static int open_file_read(const char *path, FILE **out) {
#ifdef _MSC_VER
    if (fopen_s(out, path, "rb") != 0) {
        *out = NULL;
        return 0;
    }
    return *out != NULL;
#else
    *out = fopen(path, "rb");
    return *out != NULL;
#endif
}

static void set_defaults(Config *c) {
    memset(c, 0, sizeof(*c));

    copy_string(c->font_path, sizeof(c->font_path), "font.ttf");
    c->font_pixel_height = 32.0f;

    c->atlas_width  = 512;
    c->atlas_height = 512;
    c->first_char   = 32;
    c->num_chars    = 96;

    copy_string(c->window_title, sizeof(c->window_title), "SDL3 + OpenGL + stb_truetype");
    c->window_width  = 900;
    c->window_height = 600;

    /* dark background, light text, fully opaque */
    c->bg_color[0] = 0.11f; c->bg_color[1] = 0.11f; c->bg_color[2] = 0.13f; c->bg_color[3] = 1.0f;
    c->text_color[0] = 0.92f; c->text_color[1] = 0.92f; c->text_color[2] = 0.90f; c->text_color[3] = 1.0f;
}

static float json_num(const cJSON *obj, const char *key, float fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(item) ? (float)item->valuedouble : fallback;
}

static int json_int(const cJSON *obj, const char *key, int fallback) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsNumber(item) ? item->valueint : fallback;
}

static void json_str(const cJSON *obj, const char *key, char *dst, size_t dst_size) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        copy_string(dst, dst_size, item->valuestring);
    }
}

/* Reads a 4-element JSON array like [r, g, b, a] into `out[4]`.
   Leaves `out` untouched (keeps whatever default was already there)
   if the key is missing or malformed. */
static void json_color4(const cJSON *obj, const char *key, float out[4]) {
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsArray(arr) || cJSON_GetArraySize(arr) < 4) return;

    for (int i = 0; i < 4; i++) {
        const cJSON *el = cJSON_GetArrayItem(arr, i);
        if (cJSON_IsNumber(el)) out[i] = (float)el->valuedouble;
    }
}

int config_load(const char *path, Config *out) {
    set_defaults(out);

    FILE *f = NULL;
    if (!open_file_read(path, &f)) {
        fprintf(stderr, "config_load: could not open '%s', using defaults\n", path);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = malloc((size_t)size + 1);
    fread(buf, 1, (size_t)size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root) {
        const char *err = cJSON_GetErrorPtr();
        fprintf(stderr, "config_load: JSON parse error near: %s\n", err ? err : "(unknown)");
        return 0;
    }

    json_str(root, "font_path", out->font_path, sizeof(out->font_path));
    out->font_pixel_height = json_num(root, "font_pixel_height", out->font_pixel_height);

    out->atlas_width  = json_int(root, "atlas_width", out->atlas_width);
    out->atlas_height = json_int(root, "atlas_height", out->atlas_height);
    out->first_char    = json_int(root, "first_char", out->first_char);
    out->num_chars      = json_int(root, "num_chars", out->num_chars);

    /* Clamp so a bad config value can't overflow the fixed cdata[] array. */
    if (out->num_chars > MAX_CHARS) {
        fprintf(stderr, "config_load: num_chars %d exceeds MAX_CHARS %d, clamping\n",
                out->num_chars, MAX_CHARS);
        out->num_chars = MAX_CHARS;
    }
    if (out->num_chars < 1) out->num_chars = 1;

    json_str(root, "window_title", out->window_title, sizeof(out->window_title));
    out->window_width  = json_int(root, "window_width", out->window_width);
    out->window_height = json_int(root, "window_height", out->window_height);

    json_color4(root, "background_color", out->bg_color);
    json_color4(root, "text_color", out->text_color);

    cJSON_Delete(root);
    return 1;
}
