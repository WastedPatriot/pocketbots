#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ui_init(void);
void ui_rebuild_carousel(void);
void ui_sheet(void);
void ui_tick(bool dimmed);
void ui_next(void);
void ui_prev(void);
void ui_on_shake(void);
void ui_wake(void);
bool ui_shade_open(void);
bool ui_blocks_sleep(void);
void ui_close_shade(void);
void ui_open_lens(void);
void ui_close_lens(void);
bool ui_lens_open(void);
void ui_lens_tick(uint32_t now);
void ui_set_brightness_cb(void (*fn)(int percent));

#ifdef __cplusplus
}
#endif
