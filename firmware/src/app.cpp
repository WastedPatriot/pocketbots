#include "app.h"

#include "port.h"
#include "ui.h"
#include "generated/bots.gen.h"

static uint32_t last_activity;
static bool dimmed;
static int brightness;
static bool was_down;
static uint32_t press_at;
static bool long_fired;

void app_note_activity(void) {
    last_activity = port_millis();
    if (dimmed) {
        dimmed = false;
        port_set_brightness(brightness);
    }
}

bool app_is_dimmed(void) { return dimmed; }

void app_set_brightness(int percent) {
    if (percent < 4) percent = 4;
    if (percent > 100) percent = 100;
    brightness = percent;
    port_set_brightness(percent);
    app_note_activity();
}

void app_init(void) {
    brightness = port_saved_brightness();
    if (brightness < 4 || brightness > 100) brightness = PB_CFG_BRIGHTNESS;
    port_set_brightness(brightness);
    ui_set_brightness_cb(app_set_brightness);
    last_activity = port_millis();
    ui_init();
}

void app_loop(void) {
    port_poll();
    bool down = port_button_down();
    uint32_t now = port_millis();
    if (down && !was_down) {
        press_at = now;
        long_fired = false;
        app_note_activity();
    }
    if (down && !long_fired && (now - press_at) > 700) {
        long_fired = true;
        ui_prev();
        app_note_activity();
    }
    if (!down && was_down && !long_fired) {
        ui_next();
        app_note_activity();
    }
    was_down = down;

    if (port_take_shake()) {
        app_note_activity();
        ui_on_shake();
    }

    lv_timer_handler();

    uint32_t idle = port_millis() - last_activity;
    uint32_t ambient_ms = (uint32_t)PB_CFG_AMBIENT_AFTER_S * 1000u;
    uint32_t sleep_ms = (uint32_t)PB_CFG_SLEEP_AFTER_S * 1000u;
    if (idle > sleep_ms && !down && !ui_blocks_sleep()) {
        port_sleep();
        app_note_activity();
        ui_wake();
    } else if (!dimmed && idle > ambient_ms) {
        dimmed = true;
        port_set_brightness(PB_CFG_AMBIENT_BRIGHTNESS);
    }
}
