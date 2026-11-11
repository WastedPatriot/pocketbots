#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PB_DISP_W 280
#define PB_DISP_H 456

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

#ifdef __cplusplus
}
#endif
