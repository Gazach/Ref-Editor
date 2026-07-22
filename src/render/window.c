#include <glad/glad.h>   /* must be included BEFORE SDL/GL headers */
#include "window.h"

#include <stdio.h>

int window_create(Window *win, const char *title, int width, int height) {
    win->handle = NULL;
    win->gl_ctx = NULL;
    win->width  = width;
    win->height = height;

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 0;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

    win->handle = SDL_CreateWindow(title, width, height,
                                    SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!win->handle) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 0;
    }

    win->gl_ctx = SDL_GL_CreateContext(win->handle);
    if (!win->gl_ctx) {
        fprintf(stderr, "SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win->handle);
        win->handle = NULL;
        SDL_Quit();
        return 0;
    }

    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) {
        fprintf(stderr, "Failed to load GL functions via glad\n");
        SDL_GL_DestroyContext(win->gl_ctx);
        SDL_DestroyWindow(win->handle);
        win->gl_ctx = NULL;
        win->handle = NULL;
        SDL_Quit();
        return 0;
    }

    return 1;
}

void window_destroy(Window *win) {
    if (!win) return;

    if (win->gl_ctx) {
        SDL_GL_DestroyContext(win->gl_ctx);
        win->gl_ctx = NULL;
    }
    if (win->handle) {
        SDL_DestroyWindow(win->handle);
        win->handle = NULL;
    }
    SDL_Quit();
}

int window_poll_events(Window *win) {
    SDL_Event e;
    int running = 1;

    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) running = 0;
        if (e.type == SDL_EVENT_WINDOW_RESIZED) {
            win->width  = e.window.data1;
            win->height = e.window.data2;
        }
    }

    return running;
}

void window_swap(Window *win) {
    SDL_GL_SwapWindow(win->handle);
}
