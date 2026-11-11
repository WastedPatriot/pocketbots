#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void app_init(void);
void app_loop(void);
void app_note_activity(void);
bool app_is_dimmed(void);
void app_set_brightness(int percent);

#ifdef __cplusplus
}
#endif
