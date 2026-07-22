#define STB_TRUETYPE_IMPLEMENTATION
#include "font.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- font loading ---------- */

int font_load(Font *font, const Config *cfg) {
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

void font_destroy(Font *font) {
    if (font && font->texture) {
        glDeleteTextures(1, &font->texture);
        font->texture = 0;
    }
}

/* ---------- text renderer ---------- */

typedef struct {
    float x, y;
    float u, v;
} Vertex;

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

static void ortho_matrix(float *m, float w, float h) {
    memset(m, 0, sizeof(float) * 16);
    m[0]  =  2.0f / w;
    m[5]  = -2.0f / h;
    m[10] = -1.0f;
    m[12] = -1.0f;
    m[13] =  1.0f;
    m[15] =  1.0f;
}

void text_renderer_init(TextRenderer *tr) {
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

void text_renderer_destroy(TextRenderer *tr) {
    if (!tr) return;
    glDeleteProgram(tr->program);
    glDeleteBuffers(1, &tr->vbo);
    glDeleteVertexArrays(1, &tr->vao);
}

void text_renderer_draw(TextRenderer *tr, Font *font, const char *text,
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
