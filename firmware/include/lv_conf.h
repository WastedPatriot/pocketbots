/**
 * LVGL configuration for pocketbots.
 * Anything not set here falls through to LVGL's defaults
 * (lv_conf_internal.h). Kept short on purpose.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1

#define LV_FONT_DEFAULT &lv_font_montserrat_14

/* Rounded eyes and the squircle bezel of a card need the complex drawer. */
#define LV_DRAW_SW_COMPLEX 1

#define LV_DEF_REFR_PERIOD 16

#define LV_USE_LOG 0

#endif
