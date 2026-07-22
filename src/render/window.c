#include <glad/glad.h>   /* must be included BEFORE SDL/GL headers */
#include "window.h"
#include "../editor/gapbuffer_utf8.h"

#include <stdio.h>
#include <string.h>

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

    /* Without this, SDL_EVENT_TEXT_INPUT / SDL_EVENT_TEXT_EDITING never fire.
       Key-down/up events (arrows, backspace, etc.) work either way, but the
       actual typed characters only arrive once text input is "started". */
    SDL_StartTextInput(win->handle);

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

int window_poll_events(Window *win, GapBuffer *buf) {
    SDL_Event e;
    int running = 1;

    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                running = 0;
                break;

            case SDL_EVENT_WINDOW_RESIZED:
                win->width  = e.window.data1;
                win->height = e.window.data2;
                break;

            /* e.text.text is a committed, already-decoded UTF-8 C string --
               NOT one keypress necessarily; could be a multi-byte char, or
               even multiple characters at once from an IME. Just insert
               the whole thing as-is; gb_insert doesn't care how long it is. */
            case SDL_EVENT_TEXT_INPUT:
                gb_insert(buf, e.text.text, strlen(e.text.text));
                break;

            /* Control keys that AREN'T "insert this character" -- those
               come through SDL_EVENT_TEXT_INPUT above instead. This is
               only for navigation/editing keys. */
            case SDL_EVENT_KEY_DOWN:
                switch (e.key.key) {
                    case SDLK_BACKSPACE:
                        gb_backspace_utf8(buf);
                        break;
                    case SDLK_DELETE:
                        gb_delete_utf8(buf);
                        break;
                    case SDLK_LEFT:
                        gb_move_left_utf8(buf);
                        break;
                    case SDLK_RIGHT:
                        gb_move_right_utf8(buf);
                        break;
                    case SDLK_RETURN:
                        /* The buffer happily stores '\n' -- but text_renderer_draw
                           doesn't understand line breaks yet (single line, and '\n'
                           falls outside first_char..first_char+num_chars so it's
                           silently skipped). It'll just vanish from view for now;
                           that's expected until multi-line rendering is built. */
                        gb_insert(buf, "\n", 1);
                        break;
                    default:
                        break;
                }
                break;

            default:
                break;
        }
    }

    return running;
}

void window_swap(Window *win) {
    SDL_GL_SwapWindow(win->handle);
}