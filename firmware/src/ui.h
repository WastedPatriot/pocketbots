#pragma once

#include <stdbool.h>

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
void ui_set_brightness_cb(void (*fn)(int percent));

#ifdef __cplusplus
}
#endif
