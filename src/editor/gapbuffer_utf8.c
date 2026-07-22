#include "gapbuffer_utf8.h"

/* A UTF-8 continuation byte matches the bit pattern 10xxxxxx. */
static int is_continuation_byte(int b) {
    return b != -1 && (b & 0xC0) == 0x80;
}

/* How many bytes a codepoint occupies, given its LEAD byte (the first byte
 * of the sequence). Malformed lead bytes are treated as length 1 so a
 * corrupt/non-UTF8 file can't send us into an infinite loop. */
static int utf8_seq_len_from_lead(int lead) {
    if (lead == -1)               return 1;
    if ((lead & 0x80) == 0x00)    return 1; /* 0xxxxxxx : ASCII        */
    if ((lead & 0xE0) == 0xC0)    return 2; /* 110xxxxx               */
    if ((lead & 0xF0) == 0xE0)    return 3; /* 1110xxxx               */
    if ((lead & 0xF8) == 0xF0)    return 4; /* 11110xxx               */
    return 1; /* continuation byte or invalid lead seen out of place */
}

/* Number of bytes in the codepoint ending immediately before `pos`. */
static size_t codepoint_len_before(const GapBuffer *gb, size_t pos) {
    if (pos == 0) return 0;

    size_t len = 1;
    size_t scan = pos - 1;
    /* Walk backward over continuation bytes to find the lead byte. Capped
     * at 4 since no valid UTF-8 sequence is longer than that. */
    while (scan > 0 && len < 4 && is_continuation_byte(gb_byte_at(gb, scan))) {
        scan--;
        len++;
    }
    return len;
}

/* Number of bytes in the codepoint starting at `pos`. */
static size_t codepoint_len_at(const GapBuffer *gb, size_t pos) {
    int lead = gb_byte_at(gb, pos);
    if (lead == -1) return 0;

    int wanted = utf8_seq_len_from_lead(lead);
    size_t len = 1;
    /* Only count as many continuation bytes as are actually present --
     * protects against a truncated/corrupt sequence at end-of-buffer. */
    while (len < (size_t)wanted && is_continuation_byte(gb_byte_at(gb, pos + len))) {
        len++;
    }
    return len;
}

void gb_move_left_utf8(GapBuffer *gb) {
    if (!gb) return;
    size_t n = codepoint_len_before(gb, gb_cursor(gb));
    gb_move_left(gb, n > 0 ? n : 1);
}

void gb_move_right_utf8(GapBuffer *gb) {
    if (!gb) return;
    size_t n = codepoint_len_at(gb, gb_cursor(gb));
    gb_move_right(gb, n > 0 ? n : 1);
}

void gb_backspace_utf8(GapBuffer *gb) {
    if (!gb) return;
    size_t n = codepoint_len_before(gb, gb_cursor(gb));
    gb_delete_backward(gb, n > 0 ? n : 1);
}

void gb_delete_utf8(GapBuffer *gb) {
    if (!gb) return;
    size_t n = codepoint_len_at(gb, gb_cursor(gb));
    gb_delete_forward(gb, n > 0 ? n : 1);
}
