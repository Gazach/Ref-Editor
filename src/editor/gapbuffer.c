#include "gapbuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Extra room left in the gap whenever we resize, so a burst of typing
 * doesn't reallocate on every single keystroke. */
static const size_t GAP_SLACK = 64;

/* Once the gap grows past this (e.g. after deleting a big selection), we
 * reclaim the memory rather than let it sit around unused forever. */
static const size_t GAP_SHRINK_THRESHOLD = GAP_SLACK * 4;

struct GapBuffer {
    char  *data;
    size_t capacity;    /* total allocated bytes                         */
    size_t length;      /* logical text length (excludes the gap)       */
    size_t gap_start;   /* first byte of the gap                        */
    size_t gap_end;     /* one past the last byte of the gap            */
    size_t cursor;      /* logical cursor position, always == gap_start */
};

#ifdef NDEBUG
#define ASSERT_VALID(gb) ((void)0)
#else
static void assert_valid(const GapBuffer *gb) {
    size_t gap_size = gb->gap_end - gb->gap_start;

    if (gb->gap_start > gb->gap_end || gb->gap_end > gb->capacity) {
        fprintf(stderr, "gapbuffer: gap bounds broken (start=%zu end=%zu cap=%zu)\n",
                gb->gap_start, gb->gap_end, gb->capacity);
        abort();
    }
    if (gb->length != gb->capacity - gap_size) {
        fprintf(stderr, "gapbuffer: length/capacity mismatch (length=%zu cap=%zu gap=%zu)\n",
                gb->length, gb->capacity, gap_size);
        abort();
    }
    if (gb->cursor > gb->length) {
        fprintf(stderr, "gapbuffer: cursor out of range (cursor=%zu length=%zu)\n",
                gb->cursor, gb->length);
        abort();
    }
    if (gb->cursor != gb->gap_start) {
        fprintf(stderr, "gapbuffer: cursor not synced to gap (cursor=%zu gap_start=%zu)\n",
                gb->cursor, gb->gap_start);
        abort();
    }
}
#define ASSERT_VALID(gb) assert_valid(gb)
#endif

static void *xmalloc(size_t size) {
    void *p = malloc(size);
    if (!p) {
        fprintf(stderr, "gapbuffer: out of memory\n");
        abort();
    }
    return p;
}

/* Physically relocates the gap so gap_start lands exactly at `target`
 * (a logical position). Every public mutation calls this first so the
 * rest of the function can assume "cursor == gap_start" is already true. */
static void slide_gap_to(GapBuffer *gb, size_t target) {
    size_t gap_size = gb->gap_end - gb->gap_start;

    if (target < gb->gap_start) {
        /* Cursor moved left of the gap: pull the [target, gap_start) run
         * of text across to the far side of the gap. */
        size_t run = gb->gap_start - target;
        memmove(gb->data + target + gap_size, gb->data + target, run);
        gb->gap_start = target;
        gb->gap_end   = target + gap_size;
    } else if (target > gb->gap_start) {
        /* Cursor moved right of the gap: the run to pull across lives
         * just after gap_end, of length (target - gap_start). */
        size_t run = target - gb->gap_start;
        memmove(gb->data + gb->gap_start, gb->data + gb->gap_end, run);
        gb->gap_start += run;
        gb->gap_end   += run;
    }
    /* target == gb->gap_start already: nothing to do. */

    gb->cursor = gb->gap_start;
}

static void ensure_gap_capacity(GapBuffer *gb, size_t needed) {
    size_t gap_size = gb->gap_end - gb->gap_start;
    if (needed <= gap_size) return;

    size_t new_gap = needed + GAP_SLACK;
    size_t new_capacity = gb->capacity + (new_gap - gap_size);
    char *new_data = xmalloc(new_capacity);

    size_t after_gap_len = gb->capacity - gb->gap_end;
    memcpy(new_data, gb->data, gb->gap_start);
    memcpy(new_data + gb->gap_start + new_gap, gb->data + gb->gap_end, after_gap_len);

    free(gb->data);
    gb->data     = new_data;
    gb->gap_end  = gb->gap_start + new_gap;
    gb->capacity = new_capacity;
}

static void shrink_gap_if_bloated(GapBuffer *gb) {
    size_t gap_size = gb->gap_end - gb->gap_start;
    if (gap_size <= GAP_SHRINK_THRESHOLD) return;

    size_t new_capacity = gb->length + GAP_SLACK;
    char *new_data = xmalloc(new_capacity);

    size_t after_gap_len = gb->length - gb->gap_start;
    memcpy(new_data, gb->data, gb->gap_start);
    memcpy(new_data + gb->gap_start + GAP_SLACK, gb->data + gb->gap_end, after_gap_len);

    free(gb->data);
    gb->data     = new_data;
    gb->gap_end  = gb->gap_start + GAP_SLACK;
    gb->capacity = new_capacity;
}

GapBuffer *gb_create(const void *data, size_t len) {
    GapBuffer *gb = xmalloc(sizeof(GapBuffer));

    gb->capacity  = len + GAP_SLACK;
    gb->data      = xmalloc(gb->capacity);
    gb->length    = len;
    gb->gap_start = len;
    gb->gap_end   = len + GAP_SLACK;
    gb->cursor    = len;

    if (data && len > 0) {
        memcpy(gb->data, data, len);
    }

    ASSERT_VALID(gb);
    return gb;
}

void gb_free(GapBuffer *gb) {
    if (!gb) return;
    free(gb->data);
    free(gb);
}

size_t gb_length(const GapBuffer *gb) { return gb ? gb->length : 0; }
size_t gb_cursor(const GapBuffer *gb) { return gb ? gb->cursor : 0; }

int gb_byte_at(const GapBuffer *gb, size_t logical_pos) {
    if (!gb || logical_pos >= gb->length) return -1;

    size_t physical = (logical_pos < gb->gap_start)
                     ? logical_pos
                     : logical_pos + (gb->gap_end - gb->gap_start);
    return (unsigned char)gb->data[physical];
}

void gb_insert(GapBuffer *gb, const void *data, size_t len) {
    if (!gb || !data || len == 0) return;

    slide_gap_to(gb, gb->cursor);
    ensure_gap_capacity(gb, len);

    memcpy(gb->data + gb->gap_start, data, len);
    gb->gap_start += len;
    gb->cursor    += len;
    gb->length    += len;

    ASSERT_VALID(gb);
}

void gb_delete_forward(GapBuffer *gb, size_t n) {
    if (!gb) return;
    slide_gap_to(gb, gb->cursor);

    size_t available = gb->length - gb->cursor;
    if (n > available) n = available;

    gb->gap_end += n;
    gb->length  -= n;

    ASSERT_VALID(gb);
    shrink_gap_if_bloated(gb);
}

void gb_delete_backward(GapBuffer *gb, size_t n) {
    if (!gb) return;
    slide_gap_to(gb, gb->cursor);

    size_t available = gb->cursor;
    if (n > available) n = available;

    gb->gap_start -= n;
    gb->cursor    -= n;
    gb->length    -= n;

    ASSERT_VALID(gb);
    shrink_gap_if_bloated(gb);
}

void gb_move_left(GapBuffer *gb, size_t n) {
    if (!gb) return;
    size_t target = (n >= gb->cursor) ? 0 : gb->cursor - n;
    slide_gap_to(gb, target);
    ASSERT_VALID(gb);
}

void gb_move_right(GapBuffer *gb, size_t n) {
    if (!gb) return;
    size_t remaining = gb->length - gb->cursor;
    size_t target = (n >= remaining) ? gb->length : gb->cursor + n;
    slide_gap_to(gb, target);
    ASSERT_VALID(gb);
}

void gb_move_to(GapBuffer *gb, size_t absolute_pos) {
    if (!gb) return;
    if (absolute_pos > gb->length) absolute_pos = gb->length;
    slide_gap_to(gb, absolute_pos);
    ASSERT_VALID(gb);
}

size_t gb_copy_text(const GapBuffer *gb, char *out, size_t out_size) {
    if (!gb || !out || out_size == 0) return 0;

    size_t want = gb->length < out_size ? gb->length : out_size;

    size_t before = gb->gap_start < want ? gb->gap_start : want;
    size_t after  = want - before;

    memcpy(out, gb->data, before);
    memcpy(out + before, gb->data + gb->gap_end, after);

    return before + after;
}
