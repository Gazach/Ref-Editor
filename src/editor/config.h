#ifndef CONFIG_H
#define CONFIG_H

/* Hard ceiling for num_chars, since it's used to size a fixed array
   (stbtt_bakedchar cdata[MAX_CHARS]) in the font code. Bump if you need
   more than ASCII + a bit of headroom (e.g. Latin-1 supplement). */
#define MAX_CHARS 256

typedef struct {
    char  font_path[256];
    float font_pixel_height;

    int atlas_width;
    int atlas_height;
    int first_char;
    int num_chars;

    char window_title[128];
    int  window_width;
    int  window_height;

    float bg_color[4];
    float text_color[4];
} Config;

/* Loads and parses `path` into `out`, applying sane defaults for any
   field that's missing from the JSON. Returns 1 on success, 0 on failure
   (file not found / invalid JSON) — in which case `out` still contains
   defaults, so callers can choose to continue anyway. */
int config_load(const char *path, Config *out);

#endif /* CONFIG_H */
