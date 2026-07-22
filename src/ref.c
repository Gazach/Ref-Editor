#include <glad/glad.h>   /* must be included BEFORE SDL/GL headers */
#include <SDL3/SDL.h>

#include "editor/config.h"
#include "editor/gapbuffer.h"
#include "render/window.h"
#include "render/font.h"
#include "common/path.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char **argv) {
    char config_path[1024] = "config.json";
    const char *base_path = SDL_GetBasePath();

    if (argc > 1 && argv[1] && argv[1][0] != '\0') {
        snprintf(config_path, sizeof(config_path), "%s", argv[1]);
    } else if (base_path && *base_path) {
        path_join(config_path, sizeof(config_path), base_path, "config.json");
    }

    Config cfg;
    config_load(config_path, &cfg); /* falls back to defaults on failure */
    config_resolve_font_path(&cfg, base_path);

    if (base_path) {
        SDL_free((void *)base_path);
        base_path = NULL;
    }

    Window win;
    if (!window_create(&win, cfg.window_title, cfg.window_width, cfg.window_height)) {
        return 1;
    }

    Font font;
    if (!font_load(&font, &cfg)) {
        fprintf(stderr, "Check 'font_path' in %s points to a valid .ttf\n", config_path);
        window_destroy(&win);
        return 1;
    }

    TextRenderer tr;
    text_renderer_init(&tr);

    GapBuffer *buf = gb_create("Hello, start typing...", 22);

    int running = 1;
    while (running) {
        running = window_poll_events(&win, buf);

        glViewport(0, 0, win.width, win.height);
        glClearColor(cfg.bg_color[0], cfg.bg_color[1], cfg.bg_color[2], cfg.bg_color[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        /* Pull the buffer's current content out fresh, every frame -- the
           buffer is the single source of truth, we never track a separate
           copy of "what's on screen" ourselves. */
        char text[4096];
        size_t n = gb_copy_text(buf, text, sizeof(text) - 1);
        text[n] = '\0'; /* gb_copy_text does NOT null-terminate for you */

        /* Placeholder cursor visualization: splice a '|' in at the cursor's
           byte offset, purely for this draw call -- doesn't touch the real
           buffer. Swap this for an actual drawn rectangle once you get to
           the "draw a real cursor" step. */
        char display[4096];
        size_t cur = gb_cursor(buf);
        if (cur > n) cur = n;
        memcpy(display, text, cur);
        display[cur] = '|';
        memcpy(display + cur + 1, text + cur, n - cur);
        display[n + 1] = '\0';

        text_renderer_draw(&tr, &font, display,
                            40.0f, 80.0f, (float)win.width, (float)win.height, cfg.text_color);

        window_swap(&win);
    }

    gb_free(buf);
    text_renderer_destroy(&tr);
    font_destroy(&font);
    window_destroy(&win);
    return 0;
}