#include "port.h"

#include <stdlib.h>
#include <string.h>

/* The roll lives in RAM (PSRAM on the device). Light sleep keeps it.
   A power cycle does not. Eight frames is the lot. */

struct pb_slot {
    bool used;
    uint16_t *frame;
    uint16_t *thumb;
    uint8_t *jpeg;
    size_t jpeg_len;
};

static pb_slot slots[PB_GALLERY_MAX];
static int slot_count;

void gallery_scale(const uint16_t *src, int sw, int sh, uint16_t *dst, int dw, int dh) {
    if (!src || !dst || sw < 1 || sh < 1 || dw < 1 || dh < 1) return;
    for (int y = 0; y < dh; y++) {
        int sy = y * sh / dh;
        if (sy >= sh) sy = sh - 1;
        const uint16_t *row = src + (size_t)sy * (size_t)sw;
        uint16_t *out = dst + (size_t)y * (size_t)dw;
        for (int x = 0; x < dw; x++) {
            int sx = x * sw / dw;
            if (sx >= sw) sx = sw - 1;
            out[x] = row[sx];
        }
    }
}

static void free_slot(pb_slot *s) {
    if (!s) return;
    free(s->frame);
    free(s->thumb);
    free(s->jpeg);
    memset(s, 0, sizeof(*s));
}

int gallery_push(const uint16_t *frame, const uint8_t *jpeg, size_t jpeg_len) {
    if (!frame) return -1;
    if (slot_count == PB_GALLERY_MAX) {
        free_slot(&slots[slot_count - 1]);
        slot_count--;
    }
    for (int i = slot_count; i > 0; i--) slots[i] = slots[i - 1];
    pb_slot *s = &slots[0];
    memset(s, 0, sizeof(*s));
    size_t frame_bytes = (size_t)PB_FRAME_W * (size_t)PB_FRAME_H * 2;
    s->frame = (uint16_t *)port_alloc(frame_bytes);
    if (!s->frame) return -1;
    memcpy(s->frame, frame, frame_bytes);
    size_t thumb_bytes = (size_t)PB_THUMB_W * (size_t)PB_THUMB_H * 2;
    s->thumb = (uint16_t *)port_alloc(thumb_bytes);
    if (s->thumb) gallery_scale(s->frame, PB_FRAME_W, PB_FRAME_H, s->thumb, PB_THUMB_W, PB_THUMB_H);
    if (jpeg && jpeg_len > 0 && jpeg_len <= PB_JPEG_MAX) {
        s->jpeg = (uint8_t *)port_alloc(jpeg_len);
        if (s->jpeg) {
            memcpy(s->jpeg, jpeg, jpeg_len);
            s->jpeg_len = jpeg_len;
        }
    }
    s->used = true;
    slot_count++;
    return slot_count;
}

int port_gallery_count(void) { return slot_count; }

bool port_gallery_frame(int index, const uint16_t **px, int *w, int *h) {
    if (index < 0 || index >= slot_count || !slots[index].frame) return false;
    *px = slots[index].frame;
    *w = PB_FRAME_W;
    *h = PB_FRAME_H;
    return true;
}

bool port_gallery_thumb(int index, const uint16_t **px, int *w, int *h) {
    if (index < 0 || index >= slot_count || !slots[index].thumb) return false;
    *px = slots[index].thumb;
    *w = PB_THUMB_W;
    *h = PB_THUMB_H;
    return true;
}

size_t port_gallery_jpeg_size(int index) {
    if (index < 0 || index >= slot_count) return 0;
    return slots[index].jpeg_len;
}

void port_gallery_drop(int index) {
    if (index < 0 || index >= slot_count) return;
    free_slot(&slots[index]);
    for (int i = index; i < slot_count - 1; i++) slots[i] = slots[i + 1];
    memset(&slots[slot_count - 1], 0, sizeof(slots[0]));
    slot_count--;
}
