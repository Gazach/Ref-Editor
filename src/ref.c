#include <glad/glad.h>   /* must be included BEFORE SDL/GL headers */
#include <SDL3/SDL.h>

#include "editor/config.h"
#include "render/window.h"
#include "render/font.h"
#include "common/path.h"

#include <stdio.h>

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

    int running = 1;
    while (running) {
        running = window_poll_events(&win);

        glViewport(0, 0, win.width, win.height);
        glClearColor(cfg.bg_color[0], cfg.bg_color[1], cfg.bg_color[2], cfg.bg_color[3]);
        glClear(GL_COLOR_BUFFER_BIT);

        text_renderer_draw(&tr, &font, "Hello, world! This is stb_truetype.",
                            40.0f, 80.0f, (float)win.width, (float)win.height, cfg.text_color);

        text_renderer_draw(&tr, &font, "int main(void) { return 0; }",
                            40.0f, 130.0f, (float)win.width, (float)win.height, cfg.text_color);

        window_swap(&win);
    }

    text_renderer_destroy(&tr);
    font_destroy(&font);
    window_destroy(&win);
    return 0;
}
