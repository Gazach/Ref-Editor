#ifndef GAPBUFFER_H
#define GAPBUFFER_H

#include <stddef.h>

/*
 * A gap buffer: the classic text-editor data structure.
 *
 * Conceptually the buffer holds one contiguous string, but physically it's
 * split into [text before cursor][unused gap][text after cursor]. Typing
 * just writes into the gap (O(1)); moving the cursor is the only operation
 * that costs anything (O(distance moved), via memmove).
 *
 * Everything here operates on raw BYTES, not Unicode codepoints. That's
 * intentional -- the buffer shouldn't need to know about UTF-8 at all.
 * Codepoint-aware cursor movement/deletion lives in gb_utf8.h, which is a
 * thin layer on top of this using gb_byte_at() to find codepoint boundaries.
 */

typedef struct GapBuffer GapBuffer;

/* Creates a buffer seeded with `data` (may be NULL if len == 0).
 * Cursor starts at the end of the seeded text. */
GapBuffer *gb_create(const void *data, size_t len);
void       gb_free(GapBuffer *gb);

/* Insert `len` bytes at the current cursor position; cursor ends up after
 * the inserted text. */
void gb_insert(GapBuffer *gb, const void *data, size_t len);

/* Delete up to `n` bytes after the cursor (Delete key). Cursor doesn't move.
 * Silently clamps if `n` overshoots the end of the text. */
void gb_delete_forward(GapBuffer *gb, size_t n);

/* Delete up to `n` bytes before the cursor (Backspace key). Cursor moves
 * back by however many bytes were actually deleted. */
void gb_delete_backward(GapBuffer *gb, size_t n);

/* Move the cursor by `n` bytes; clamps at the start/end of the text. */
void gb_move_left(GapBuffer *gb, size_t n);
void gb_move_right(GapBuffer *gb, size_t n);

/* Jump the cursor to an absolute byte offset; clamps to [0, gb_length()]. */
void gb_move_to(GapBuffer *gb, size_t absolute_pos);

size_t gb_length(const GapBuffer *gb); /* total bytes of text (excludes the gap) */
size_t gb_cursor(const GapBuffer *gb); /* cursor as a logical byte offset      */

/* Random single-byte read at a logical offset, independent of the cursor.
 * Returns -1 if `logical_pos` is out of range. Used by gb_utf8.h to walk
 * codepoint boundaries without disturbing the gap. */
int gb_byte_at(const GapBuffer *gb, size_t logical_pos);

/* Linearizes the buffer into `out` (NOT null-terminated). Returns the
 * number of bytes actually written, which is min(gb_length(), out_size). */
size_t gb_copy_text(const GapBuffer *gb, char *out, size_t out_size);

#endif
