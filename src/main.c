/*
 * text_render_stb.c
 *
 * SDL3 + OpenGL (core profile) + stb_truetype text rendering, configured
 * via config.json instead of compile-time constants.
 *
 * WHAT YOU NEED BEFORE THIS COMPILES:
 *   1. stb_truetype.h  -> https://github.com/nothings/stb/blob/master/stb_truetype.h
 *   2. cJSON.h / cJSON.c -> https://github.com/DaveGamble/cJSON
 *   3. A GL loader (glad -> https://glad.dav1d.de), generated for GL 3.3 Core.
 *   4. A .ttf font and a config.json (see config.json alongside this file).
 *   5. SDL3 dev libraries.
 *
 * BUILD (Linux example):
 *   gcc text_render_stb.c config.c cJSON.c glad.c -o text_render_stb \
 *       $(pkg-config --cflags --libs sdl3) -lGL -ldl -lm
 */

#include <glad/glad.h>   /* must be included BEFORE SDL/GL headers */
#include <SDL3/SDL.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include "/lib/stb/stb_truetype.h"

#include "/editor/config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- font state ---------- */
typedef struct {
    GLuint texture;
    stbtt_bakedchar cdata[MAX_CHARS];
    int first_char;
    int num_chars;
    int atlas_w;
    int atlas_h;
} Font;

static int font_load(Font *font, const Config *cfg) {
    FILE *f = NULL;
#ifdef _MSC_VER
    if (fopen_s(&f, cfg->font_path, "rb") != 0) {
        fprintf(stderr, "Could not open font file: %s\n", cfg->font_path);
        return 0;
    }
#else
    f = fopen(cfg->font_path, "rb");
    if (!f) {
        fprintf(stderr, "Could not open font file: %s\n", cfg->font_path);
        return 0;
    }
#endif
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    unsigned char *ttf_buffer = malloc((size_t)size);
    fread(ttf_buffer, 1, (size_t)size, f);
    fclose(f);

    unsigned char *bitmap = malloc((size_t)(cfg->atlas_width * cfg->atlas_height));

    int result = stbtt_BakeFontBitmap(ttf_buffer, 0, cfg->font_pixel_height,
                                       bitmap, cfg->atlas_width, cfg->atlas_height,
                                       cfg->first_char, cfg->num_chars, font->cdata);
    free(ttf_buffer);

    if (result <= 0) {
        fprintf(stderr, "stbtt_BakeFontBitmap failed (atlas too small for this font/size?)\n");
        free(bitmap);
        return 0;
    }

    glGenTextures(1, &font->texture);
    glBindTexture(GL_TEXTURE_2D, font->texture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, cfg->atlas_width, cfg->atlas_height, 0,
                 GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    free(bitmap);
    font->first_char = cfg->first_char;
    font->num_chars  = cfg->num_chars;
    font->atlas_w    = cfg->atlas_width;
    font->atlas_h    = cfg->atlas_height;
    return 1;
}

/* ---------- vertex/shader setup ---------- */
typedef struct {
    float x, y;
    float u, v;
} Vertex;

typedef struct {
    GLuint program;
    GLuint vao, vbo;
    GLint  uProjLoc;
    GLint  uColorLoc;
    GLint  uTexLoc;
} TextRenderer;

static const char *VERT_SRC =
    "#version 330 core\n"
    "layout(location=0) in vec2 aPos;\n"
    "layout(location=1) in vec2 aUV;\n"
    "uniform mat4 uProj;\n"
    "out vec2 vUV;\n"
    "void main() {\n"
    "    vUV = aUV;\n"
    "    gl_Position = uProj * vec4(aPos, 0.0, 1.0);\n"
    "}\n";

static const char *FRAG_SRC =
    "#version 330 core\n"
    "in vec2 vUV;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D uTex;\n"
    "uniform vec4 uColor;\n"
    "void main() {\n"
    "    float alpha = texture(uTex, vUV).r;\n"
    "    FragColor = vec4(uColor.rgb, uColor.a * alpha);\n"
    "}\n";

static GLuint compile_shader(GLenum type, const char *src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    GLint ok;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[1024];
        glGetShaderInfoLog(s, sizeof(log), NULL, log);
        fprintf(stderr, "Shader compile error: %s\n", log);
    }
    return s;
}

static void text_renderer_init(TextRenderer *tr) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, VERT_SRC);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, FRAG_SRC);

    tr->program = glCreateProgram();
    glAttachShader(tr->program, vs);
    glAttachShader(tr->program, fs);
    glLinkProgram(tr->program);
    glDeleteShader(vs);
    glDeleteShader(fs);

    tr->uProjLoc  = glGetUniformLocation(tr->program, "uProj");
    tr->uColorLoc = glGetUniformLocation(tr->program, "uColor");
    tr->uTexLoc   = glGetUniformLocation(tr->program, "uTex");

    glGenVertexArrays(1, &tr->vao);
    glGenBuffers(1, &tr->vbo);

    glBindVertexArray(tr->vao);
    glBindBuffer(GL_ARRAY_BUFFER, tr->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * 6 * 256, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)(2 * sizeof(float)));

    glBindVertexArray(0);
}

static void ortho_matrix(float *m, float w, float h) {
    memset(m, 0, sizeof(float) * 16);
    m[0]  =  2.0f / w;
    m[5]  = -2.0f / h;
    m[10] = -1.0f;
    m[12] = -1.0f;
    m[13] =  1.0f;
    m[15] =  1.0f;
}

static void text_renderer_draw(TextRenderer *tr, Font *font, const char *text,
                                float x, float y, float win_w, float win_h,
                                const float color[4]) {
    Vertex verts[6 * 256];
    int vcount = 0;

    for (const char *p = text; *p && vcount < 6 * 256; p++) {
        if (*p < font->first_char || *p >= font->first_char + font->num_chars) continue;

        stbtt_aligned_quad q;
        stbtt_GetBakedQuad(font->cdata, font->atlas_w, font->atlas_h,
                            *p - font->first_char, &x, &y, &q, 1);

        Vertex quad[6] = {
            { q.x0, q.y0, q.s0, q.t0 },
            { q.x1, q.y0, q.s1, q.t0 },
            { q.x1, q.y1, q.s1, q.t1 },
            { q.x0, q.y0, q.s0, q.t0 },
            { q.x1, q.y1, q.s1, q.t1 },
            { q.x0, q.y1, q.s0, q.t1 },
        };
        memcpy(&verts[vcount], quad, sizeof(quad));
        vcount += 6;
    }

    if (vcount == 0) return;

    float proj[16];
    ortho_matrix(proj, win_w, win_h);

    glUseProgram(tr->program);
    glUniformMatrix4fv(tr->uProjLoc, 1, GL_FALSE, proj);
    glUniform4f(tr->uColorLoc, color[0], color[1], color[2], color[3]);
    glUniform1i(tr->uTexLoc, 0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, font->texture);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glBindVertexArray(tr->vao);
    glBindBuffer(GL_ARRAY_BUFFER, tr->vbo);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * vcount, verts);
    glDrawArrays(GL_TRIANGLES, 0, vcount);
    glBindVertexArray(0);
}

static void join_path(char *dst, size_t dst_size, const char *dir, const char *name) {
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

static int is_absolute_path(const char *path) {
    if (!path || !*path) return 0;
    if (path[0] == '/' || path[0] == '\\') return 1;
    return (strlen(path) >= 2 && path[1] == ':' && ((path[0] >= 'A' && path[0] <= 'Z') || (path[0] >= 'a' && path[0] <= 'z')));
}

/* ---------- main ---------- */
int main(int argc, char **argv) {
    char config_path[1024] = "config.json";
    const char *base_path = SDL_GetBasePath();

    if (argc > 1 && argv[1] && argv[1][0] != '\0') {
        snprintf(config_path, sizeof(config_path), "%s", argv[1]);
    } else if (base_path && *base_path) {
        join_path(config_path, sizeof(config_path), base_path, "config.json");
    }

    Config cfg;
    config_load(config_path, &cfg); /* falls back to defaults on failure */

    if (cfg.font_path[0] != '\0' && !is_absolute_path(cfg.font_path)) {
        char resolved_font_path[1024] = {0};
        if (base_path && *base_path) {
            join_path(resolved_font_path, sizeof(resolved_font_path), base_path, cfg.font_path);
            memcpy(cfg.font_path, resolved_font_path, sizeof(cfg.font_path));
        }
    }

    if (base_path) {
        SDL_free((void *)base_path);
        base_path = NULL;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    int win_w = cfg.window_width, win_h = cfg.window_height;
    SDL_Window *window = SDL_CreateWindow(cfg.window_title, win_w, win_h,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext gl_ctx = SDL_GL_CreateContext(window);
    if (!gl_ctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        fprintf(stderr, "Failed to load GL functions via glad\n");
        return 1;
    }

    Font font;
    if (!font_load(&font, &cfg)) {
        fprintf(stderr, "Check 'font_path' in %s points to a valid .ttf\n", config_path);
        return 1;
    }

    TextRenderer tr;
    text_renderer_init(&tr);

    int running = 1;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_EVENT_QUIT) running = 0;
            if (e.type == SDL_EVENT_WINDOW_RESIZED) {
                win_w = e.window.data1;
                win_h = e.window.data2;
            }
        }

        glViewport(0, 0, win_w, win_h);
        glClearColor(cfg.bg_color[0], cfg.bg_color[1], cfg.bg_color[2], cfg.bg_color[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        text_renderer_draw(&tr, &font, "Hello, world! This is stb_truetype.",
                            40.0f, 80.0f, (float)win_w, (float)win_h, cfg.text_color);

        text_renderer_draw(&tr, &font, "int main(void) { return 0; }",
                            40.0f, 130.0f, (float)win_w, (float)win_h, cfg.text_color);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DestroyContext(gl_ctx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}