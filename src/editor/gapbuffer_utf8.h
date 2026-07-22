#ifndef GAPBUFFER_UTF8_H
#define GAPBUFFER_UTF8_H

#include "gapbuffer.h"

/*
 * The core GapBuffer only understands bytes. That's correct for the data
 * structure -- but if you wire raw gb_move_left/gb_delete_backward etc.
 * straight to arrow keys and Backspace, pressing Backspace after typing a
 * character like a Chinese hanzi or an emoji will chop off only the last
 * BYTE of it, leaving an invalid, unrenderable half-codepoint in the buffer.
 *
 * These wrappers figure out how many bytes make up one codepoint in the
 * relevant direction, then call the byte-level primitive with that count.
 * Use these from your input handler; use the raw gb_* functions only when
 * you already know exactly how many bytes you're working with (e.g. an
 * undo record that stores an exact byte range).
 */

/* Moves the cursor back/forward by exactly one whole UTF-8 codepoint. */
void gb_move_left_utf8(GapBuffer *gb);
void gb_move_right_utf8(GapBuffer *gb);

/* Deletes exactly one whole UTF-8 codepoint before/after the cursor. */
void gb_backspace_utf8(GapBuffer *gb);
void gb_delete_utf8(GapBuffer *gb);

#endif
