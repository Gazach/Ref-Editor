#ifndef RENDER_FONT_H
#define RENDER_FONT_H

#include <glad/glad.h>
#include "../lib/stb/stb_truetype.h"
#include "../editor/config.h"

typedef struct {
    GLuint texture;
    stbtt_bakedchar cdata[MAX_CHARS];
    int first_char;
    int num_chars;
    int atlas_w;
    int atlas_h;
} Font;

typedef struct {
    GLuint program;
    GLuint vao, vbo;
    GLint  uProjLoc;
    GLint  uColorLoc;
    GLint  uTexLoc;
} TextRenderer;

/* Loads the .ttf at cfg->font_path, bakes glyphs into a bitmap atlas sized
   cfg->atlas_width x atlas_height (via stb_truetype), and uploads it as a
   single-channel GL texture. Returns 1 on success, 0 on failure. */
int font_load(Font *font, const Config *cfg);

/* Deletes the font's GL texture. */
void font_destroy(Font *font);

/* Compiles/links the text shader program and sets up the VAO/VBO used to
   draw glyph quads (room for up to 256 quads per draw call). */
void text_renderer_init(TextRenderer *tr);

/* Deletes the shader program, VAO, and VBO. */
void text_renderer_destroy(TextRenderer *tr);

/* Draws `text` starting at pixel position (x, y) — top-left origin,
   y-down — against a win_w x win_h viewport, tinted by `color` (RGBA). */
void text_renderer_draw(TextRenderer *tr, Font *font, const char *text,
                         float x, float y, float win_w, float win_h,
                         const float color[4]);

#endif /* RENDER_FONT_H */
