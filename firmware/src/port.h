#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* v2 glass is 240×280. The 1.64 build sets both from the environment. */
#ifndef PB_DISP_W
#define PB_DISP_W 240
#endif
#ifndef PB_DISP_H
#define PB_DISP_H 280
#endif

/* Viewfinder and the on-device roll. JPEG stays under this so eight of
   them still fit beside the frames in PSRAM. */
#define PB_FRAME_W 200
#define PB_FRAME_H 148
#define PB_THUMB_W 72
#define PB_THUMB_H 54
#define PB_GALLERY_MAX 8
#define PB_JPEG_MAX (60 * 1024)

#ifdef __cplusplus
extern "C" {
#endif

void port_log(const char *msg);
uint32_t port_millis(void);
int port_rand(void);

/* Heap that prefers PSRAM on the device. */
void *port_alloc(size_t bytes);

/* -1 when no cell is readable (USB, or the divider is floating). */
int port_battery_percent(void);

int port_saved_brightness(void);
void port_set_brightness(int percent);
void port_save_brightness(int percent);

bool port_egg_unlocked(void);
void port_set_egg_unlocked(bool on);

/* Debounced, active while held. */
bool port_button_down(void);

/* Latched. Reading it clears it. */
bool port_take_shake(void);

/* Sample the button, the IMU and the battery divider. */
void port_poll(void);

/* Block in light sleep until the button (or touch, on V1.1) wakes us.
   The simulator returns immediately. */
void port_sleep(void);

/* True while a finger is down. Coordinates are in screen pixels. */
bool port_read_touch(int *x, int *y);

/* Local camera. v1 returns false and the lens button stays off the glass.
   The roll is newest-first. Snap copies a PB_FRAME_W×PB_FRAME_H preview
   and, when it can, a JPEG next to it. Returns the new count, or -1. */
bool port_camera_available(void);
bool port_camera_preview(uint16_t *rgb565, int w, int h);
int port_camera_snap(const uint16_t *frame, int w, int h);

int port_gallery_count(void);
bool port_gallery_frame(int index, const uint16_t **px, int *w, int *h);
bool port_gallery_thumb(int index, const uint16_t **px, int *w, int *h);
size_t port_gallery_jpeg_size(int index);
void port_gallery_drop(int index);

/* Shared by the ports. Not for the UI. */
void gallery_scale(const uint16_t *src, int sw, int sh, uint16_t *dst, int dw, int dh);
int gallery_push(const uint16_t *frame, const uint8_t *jpeg, size_t jpeg_len);

#ifdef __cplusplus
}
#endif
