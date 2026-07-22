#ifndef RENDER_WINDOW_H
#define RENDER_WINDOW_H

#include <SDL3/SDL.h>
#include "../editor/gapbuffer.h"

typedef struct {
    SDL_Window   *handle;
    SDL_GLContext gl_ctx;
    int width;
    int height;
} Window;

/* Initializes SDL video, requests an OpenGL 3.3 core profile context,
   creates a resizable window titled `title` at `width`x`height`, creates
   the GL context, loads GL functions via glad, and calls
   SDL_StartTextInput so SDL_EVENT_TEXT_INPUT/TEXT_EDITING start firing.
   Returns 1 on success, 0 on failure (details go to stderr). On failure,
   any partially-created SDL resources are cleaned up before returning. */
int window_create(Window *win, const char *title, int width, int height);

/* Destroys the GL context and window (if created) and shuts down SDL. */
void window_destroy(Window *win);

/* Pumps the SDL event queue. Updates win->width/height on resize, and
   routes text/key events into `buf` (insert on TEXT_INPUT, backspace/
   delete/arrow-keys on KEY_DOWN). Returns 0 if a quit was requested,
   1 otherwise (i.e. "keep running"). */
int window_poll_events(Window *win, GapBuffer *buf);

/* Presents the back buffer (swaps front/back buffers). */
void window_swap(Window *win);

#endif /* RENDER_WINDOW_H */