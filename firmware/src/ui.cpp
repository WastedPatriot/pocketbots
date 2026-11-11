#include "ui.h"

#include "app.h"
#include "mark.h"
#include "port.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BG 0x141416
#define INK 0xF2F2F3
#define MUTED 0x8E8E96
#define HAIR 0x3A3A42

enum { k_max_pages = 12 };

static int mark_px(void) { return PB_DISP_H >= 400 ? 176 : 128; }
static int mark_oy(void) { return PB_DISP_H >= 400 ? -28 : -16; }
static int name_oy(void) { return PB_DISP_H >= 400 ? 108 : 70; }
static int sub_oy(void) { return PB_DISP_H >= 400 ? 142 : 96; }

static lv_obj_t *scroller;
static lv_obj_t *dots[k_max_pages];
static lv_obj_t *pages[k_max_pages];
static pb_mark marks[k_max_pages];
static const pb_bot_t *page_bot[k_max_pages];
static int page_count;
static int last_scroll;
static bool scroll_seen;

static lv_obj_t *splash;
static pb_mark splash_mark;
static lv_timer_t *splash_timer;

static lv_obj_t *shade;
static lv_obj_t *shade_slider;
static lv_obj_t *toast;
static lv_timer_t *toast_timer;

static lv_obj_t *battery_lbl;
static lv_obj_t *lens_btn;
static uint32_t next_battery_at;

static void (*brightness_cb)(int);

static const char *k_quips[] = {
    "not a phone. put it back.",
    "cyber is watching. politely.",
    "vik will get to it.",
    "mind the keys.",
    "this pocket is taken.",
};

static void style_flat(lv_obj_t *o) {
    lv_obj_set_style_bg_color(o, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static int active_index(void) {
    if (!scroller || page_count <= 0) return 0;
    int sx = lv_obj_get_scroll_x(scroller);
    int i = (sx + PB_DISP_W / 2) / PB_DISP_W;
    if (i < 0) i = 0;
    if (i >= page_count) i = page_count - 1;
    return i;
}

static void paint_dots(void) {
    int active = active_index();
    for (int i = 0; i < page_count; i++) {
        if (!dots[i]) continue;
        uint32_t c = (i == active && page_bot[i]) ? page_bot[i]->color : 0x3A3A42;
        lv_obj_set_style_bg_color(dots[i], lv_color_hex(c), 0);
    }
}

static void show_toast(const char *text) {
    if (!toast) {
        toast = lv_label_create(lv_screen_active());
        lv_obj_set_style_text_font(toast, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(toast, lv_color_hex(INK), 0);
        lv_obj_set_style_bg_color(toast, lv_color_hex(0x1C1C20), 0);
        lv_obj_set_style_bg_opa(toast, LV_OPA_COVER, 0);
        lv_obj_set_style_pad_hor(toast, 12, 0);
        lv_obj_set_style_pad_ver(toast, 6, 0);
        lv_obj_set_style_radius(toast, 12, 0);
        lv_obj_align(toast, LV_ALIGN_BOTTOM_MID, 0, -36);
    }
    lv_label_set_text(toast, text);
    lv_obj_remove_flag(toast, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(toast, LV_OPA_COVER, 0);
    lv_obj_move_foreground(toast);
    if (toast_timer) lv_timer_delete(toast_timer);
    toast_timer = lv_timer_create([](lv_timer_t *t) {
        if (toast) lv_obj_add_flag(toast, LV_OBJ_FLAG_HIDDEN);
        lv_timer_delete(t);
        toast_timer = NULL;
    }, 1600, NULL);
    lv_timer_set_repeat_count(toast_timer, 1);
}

static void on_mark_clicked(lv_event_t *e) {
    static uint8_t taps;
    static uint32_t last;
    uint32_t now = port_millis();
    taps = (now - last < 420) ? taps + 1 : 1;
    last = now;
    pb_mark *m = (pb_mark *)lv_event_get_user_data(e);
    if (taps >= 3) {
        taps = 0;
        if (m) mark_bounce(m);
        show_toast(k_quips[port_rand() % (int)(sizeof k_quips / sizeof k_quips[0])]);
    }
    app_note_activity();
}

static void on_subtitle_long(lv_event_t *e) {
    (void)e;
    bool any = false;
    for (unsigned i = 0; i < pb_bot_count; i++) {
        if (pb_bots[i].hidden) any = true;
    }
    if (!any) return;
    if (!port_egg_unlocked()) {
        port_set_egg_unlocked(true);
        ui_rebuild_carousel();
        show_toast("a moth. it lives here now.");
    } else if (page_count > 0) {
        lv_obj_scroll_to_view(pages[page_count - 1], LV_ANIM_ON);
    }
    app_note_activity();
}

static void add_page(const pb_bot_t *bot) {
    if (page_count >= k_max_pages) return;
    int i = page_count++;
    page_bot[i] = bot;
    lv_obj_t *page = lv_obj_create(scroller);
    pages[i] = page;
    lv_obj_set_size(page, PB_DISP_W, PB_DISP_H);
    style_flat(page);

    mark_create(&marks[i], page, bot, mark_px());
    lv_obj_align(marks[i].root, LV_ALIGN_CENTER, 0, mark_oy());
    lv_obj_add_event_cb(marks[i].root, on_mark_clicked, LV_EVENT_CLICKED, &marks[i]);

    lv_obj_t *name = lv_label_create(page);
    lv_label_set_text(name, bot->name);
    lv_obj_set_style_text_font(name, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(name, lv_color_hex(INK), 0);
    lv_obj_align(name, LV_ALIGN_CENTER, 0, name_oy());

    lv_obj_t *sub = lv_label_create(page);
    lv_label_set_text(sub, bot->subtitle);
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(MUTED), 0);
    lv_obj_align(sub, LV_ALIGN_CENTER, 0, sub_oy());
    lv_obj_add_flag(sub, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(sub, on_subtitle_long, LV_EVENT_LONG_PRESSED, NULL);
}

static void place_dots(void) {
    for (int i = 0; i < page_count; i++) {
        if (!dots[i]) {
            dots[i] = lv_obj_create(lv_screen_active());
            lv_obj_set_size(dots[i], 7, 7);
            lv_obj_set_style_radius(dots[i], LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(dots[i], 0, 0);
            lv_obj_set_style_bg_color(dots[i], lv_color_hex(HAIR), 0);
            lv_obj_remove_flag(dots[i], LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_add_flag(dots[i], LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(dots[i], [](lv_event_t *e) {
                int idx = (int)(intptr_t)lv_event_get_user_data(e);
                if (idx >= 0 && idx < page_count) lv_obj_scroll_to_view(pages[idx], LV_ANIM_ON);
                app_note_activity();
            }, LV_EVENT_CLICKED, (void *)(intptr_t)i);
        }
        lv_obj_remove_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
        int span = page_count * 16;
        int x = -span / 2 + i * 16 + 8;
        lv_obj_align(dots[i], LV_ALIGN_BOTTOM_MID, x, -16);
        lv_obj_move_foreground(dots[i]);
    }
    for (int i = page_count; i < k_max_pages; i++) {
        if (dots[i]) lv_obj_add_flag(dots[i], LV_OBJ_FLAG_HIDDEN);
    }
    paint_dots();
}

static void on_scroll(lv_event_t *e) {
    (void)e;
    int sx = lv_obj_get_scroll_x(scroller);
    int dx = scroll_seen ? sx - last_scroll : 0;
    last_scroll = sx;
    scroll_seen = true;
    float g = -(float)dx / 14.f;
    if (g > 1.f) g = 1.f;
    if (g < -1.f) g = -1.f;
    for (int i = 0; i < page_count; i++) mark_set_drag(&marks[i], g);
    paint_dots();
    app_note_activity();
}

static void on_scroll_end(lv_event_t *e) {
    (void)e;
    for (int i = 0; i < page_count; i++) mark_clear_drag(&marks[i]);
    int i = active_index();
    if (i >= 0 && i < page_count) mark_bounce(&marks[i]);
    paint_dots();
}

static void build_scroller(void) {
    scroller = lv_obj_create(lv_screen_active());
    lv_obj_set_size(scroller, PB_DISP_W, PB_DISP_H);
    style_flat(scroller);
    lv_obj_add_flag(scroller, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scroller, LV_DIR_HOR);
    lv_obj_set_scroll_snap_x(scroller, LV_SCROLL_SNAP_START);
    lv_obj_set_scrollbar_mode(scroller, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_flex_flow(scroller, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(scroller, 0, 0);
    lv_obj_add_event_cb(scroller, on_scroll, LV_EVENT_SCROLL, NULL);
    lv_obj_add_event_cb(scroller, on_scroll_end, LV_EVENT_SCROLL_END, NULL);

    bool egg = port_egg_unlocked();
    for (unsigned i = 0; i < pb_bot_count; i++) {
        if (pb_bots[i].hidden && !egg) continue;
        add_page(&pb_bots[i]);
    }
    place_dots();
    lv_obj_move_background(scroller);
}

void ui_rebuild_carousel(void) {
    /* Drop the old pages. Marks free their buffers on LV_EVENT_DELETE. */
    if (scroller) {
        lv_obj_delete(scroller);
        scroller = NULL;
    }
    page_count = 0;
    scroll_seen = false;
    last_scroll = 0;
    memset(pages, 0, sizeof pages);
    memset(marks, 0, sizeof marks);
    build_scroller();
    if (page_count > 0) lv_obj_scroll_to_view(pages[page_count - 1], LV_ANIM_ON);
    if (lens_btn) lv_obj_move_foreground(lens_btn);
    if (battery_lbl) lv_obj_move_foreground(battery_lbl);
    if (shade) lv_obj_move_foreground(shade);
}

static void refresh_battery(void) {
    if (!battery_lbl) return;
    int pct = port_battery_percent();
    const char *icon = LV_SYMBOL_BATTERY_FULL;
    char buf[24];
    if (pct < 0) {
        snprintf(buf, sizeof buf, "%s usb", LV_SYMBOL_CHARGE);
        lv_obj_set_style_text_color(battery_lbl, lv_color_hex(MUTED), 0);
    } else {
        if (pct > 80) icon = LV_SYMBOL_BATTERY_FULL;
        else if (pct > 60) icon = LV_SYMBOL_BATTERY_3;
        else if (pct > 40) icon = LV_SYMBOL_BATTERY_2;
        else if (pct > 15) icon = LV_SYMBOL_BATTERY_1;
        else icon = LV_SYMBOL_BATTERY_EMPTY;
        snprintf(buf, sizeof buf, "%s %d%%", icon, pct);
        uint32_t c = pct <= 15 ? 0xE53935 : pct <= 30 ? 0xFFB300 : 0xC8C8CE;
        lv_obj_set_style_text_color(battery_lbl, lv_color_hex(c), 0);
    }
    lv_label_set_text(battery_lbl, buf);
}

static void open_shade(void) {
    if (!shade) return;
    if (lv_obj_has_flag(shade, LV_OBJ_FLAG_HIDDEN)) lv_obj_remove_flag(shade, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(shade, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(shade);
    app_note_activity();
}

bool ui_shade_open(void) {
    return shade && !lv_obj_has_flag(shade, LV_OBJ_FLAG_HIDDEN);
}

void ui_close_shade(void) {
    if (shade) lv_obj_add_flag(shade, LV_OBJ_FLAG_HIDDEN);
}

bool ui_blocks_sleep(void) { return ui_shade_open() || ui_lens_open(); }

static void build_chrome(void) {
    battery_lbl = lv_label_create(lv_screen_active());
    lv_obj_set_style_text_font(battery_lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(battery_lbl, LV_ALIGN_TOP_RIGHT, -10, 8);
    lv_obj_add_flag(battery_lbl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(battery_lbl, [](lv_event_t *e) {
        (void)e;
        open_shade();
    }, LV_EVENT_CLICKED, NULL);
    refresh_battery();
    next_battery_at = port_millis() + 15000;

    lv_obj_t *handle = lv_obj_create(lv_screen_active());
    lv_obj_set_size(handle, 36, 4);
    lv_obj_set_style_radius(handle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(handle, lv_color_hex(HAIR), 0);
    lv_obj_set_style_border_width(handle, 0, 0);
    lv_obj_align(handle, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_add_flag(handle, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(handle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(handle, [](lv_event_t *e) {
        (void)e;
        open_shade();
    }, LV_EVENT_CLICKED, NULL);

    shade = lv_obj_create(lv_screen_active());
    lv_obj_set_size(shade, PB_DISP_W, 148);
    lv_obj_align(shade, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(shade, lv_color_hex(0x1A1A1E), 0);
    lv_obj_set_style_bg_opa(shade, 235, 0);
    lv_obj_set_style_border_width(shade, 0, 0);
    lv_obj_set_style_radius(shade, 0, 0);
    lv_obj_remove_flag(shade, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(shade, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *cap = lv_label_create(shade);
    lv_label_set_text(cap, "brightness");
    lv_obj_set_style_text_font(cap, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(cap, lv_color_hex(MUTED), 0);
    lv_obj_align(cap, LV_ALIGN_TOP_LEFT, 16, 28);

    shade_slider = lv_slider_create(shade);
    lv_obj_set_width(shade_slider, PB_DISP_W - 32);
    lv_obj_align(shade_slider, LV_ALIGN_TOP_MID, 0, 58);
    lv_slider_set_range(shade_slider, 4, 100);
    lv_slider_set_value(shade_slider, port_saved_brightness(), LV_ANIM_OFF);
    lv_obj_set_style_bg_color(shade_slider, lv_color_hex(0x1E88FF), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(shade_slider, lv_color_hex(0x1E88FF), LV_PART_KNOB);
    lv_obj_add_event_cb(shade_slider, [](lv_event_t *e) {
        int v = lv_slider_get_value((lv_obj_t *)lv_event_get_target(e));
        if (brightness_cb) brightness_cb(v);
        else port_set_brightness(v);
        app_note_activity();
    }, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(shade_slider, [](lv_event_t *e) {
        (void)e;
        port_save_brightness(lv_slider_get_value(shade_slider));
    }, LV_EVENT_RELEASED, NULL);

    lv_obj_t *hint = lv_label_create(shade);
    lv_label_set_text(hint, "it dims itself. you can stop hovering.");
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(0x6E6E76), 0);
    lv_obj_align(hint, LV_ALIGN_TOP_LEFT, 16, 100);

    lv_obj_t *close = lv_label_create(shade);
    lv_label_set_text(close, "close");
    lv_obj_set_style_text_font(close, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(close, lv_color_hex(INK), 0);
    lv_obj_align(close, LV_ALIGN_TOP_RIGHT, -16, 28);
    lv_obj_add_flag(close, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(close, [](lv_event_t *e) {
        (void)e;
        ui_close_shade();
    }, LV_EVENT_CLICKED, NULL);

    if (!port_camera_available()) return;
    /* Bottom-left, clear of the dots. Centre lands on (28, H-28), which
       the simulator taps. A ring and a dot, nothing else. */
    lens_btn = lv_obj_create(lv_screen_active());
    lv_obj_set_size(lens_btn, 32, 32);
    lv_obj_set_style_radius(lens_btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(lens_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(lens_btn, lv_color_hex(0xC8C8CE), 0);
    lv_obj_set_style_border_width(lens_btn, 2, 0);
    lv_obj_align(lens_btn, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_obj_remove_flag(lens_btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(lens_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(lens_btn, 8);
    lv_obj_t *pupil = lv_obj_create(lens_btn);
    lv_obj_set_size(pupil, 10, 10);
    lv_obj_set_style_radius(pupil, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(pupil, lv_color_hex(0x1E88FF), 0);
    lv_obj_set_style_border_width(pupil, 0, 0);
    lv_obj_remove_flag(pupil, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(pupil);
    lv_obj_add_event_cb(lens_btn, [](lv_event_t *e) {
        (void)e;
        ui_close_shade();
        ui_open_lens();
    }, LV_EVENT_CLICKED, NULL);
}

static void dismiss_splash(void) {
    if (!splash) return;
    if (splash_timer) {
        lv_timer_delete(splash_timer);
        splash_timer = NULL;
    }
    lv_obj_delete(splash);
    splash = NULL;
    memset(&splash_mark, 0, sizeof splash_mark);
    build_scroller();
    if (lens_btn) lv_obj_move_foreground(lens_btn);
    if (battery_lbl) lv_obj_move_foreground(battery_lbl);
    if (shade) lv_obj_move_foreground(shade);
}

static void show_splash(void) {
    splash = lv_obj_create(lv_screen_active());
    lv_obj_set_size(splash, PB_DISP_W, PB_DISP_H);
    style_flat(splash);
    lv_obj_add_flag(splash, LV_OBJ_FLAG_CLICKABLE);
    const pb_bot_t *hero = pb_bot_count ? &pb_bots[0] : NULL;
    if (hero) {
        mark_create(&splash_mark, splash, hero, mark_px());
        lv_obj_align(splash_mark.root, LV_ALIGN_CENTER, 0, PB_DISP_H >= 400 ? -36 : -20);
        mark_bounce(&splash_mark);
    }
    lv_obj_t *word = lv_label_create(splash);
    lv_label_set_text(word, "pocketbots");
    lv_obj_set_style_text_font(word, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(word, lv_color_hex(INK), 0);
    lv_obj_set_style_text_letter_space(word, 1, 0);
    lv_obj_align(word, LV_ALIGN_CENTER, 0, PB_DISP_H >= 400 ? 96 : 62);

    lv_obj_t *line = lv_label_create(splash);
    lv_label_set_text(line, "mind the keys");
    lv_obj_set_style_text_font(line, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(line, lv_color_hex(MUTED), 0);
    lv_obj_align(line, LV_ALIGN_CENTER, 0, PB_DISP_H >= 400 ? 124 : 88);

    lv_obj_add_event_cb(splash, [](lv_event_t *e) {
        (void)e;
        dismiss_splash();
        app_note_activity();
    }, LV_EVENT_CLICKED, NULL);

    splash_timer = lv_timer_create([](lv_timer_t *t) {
        (void)t;
        dismiss_splash();
    }, 1100, NULL);
    lv_timer_set_repeat_count(splash_timer, 1);
    lv_obj_move_foreground(splash);
}

void ui_set_brightness_cb(void (*fn)(int)) {
    brightness_cb = fn;
}

void ui_init(void) {
    lv_display_t *disp = lv_display_get_default();
    if (disp) {
        lv_theme_default_init(disp, lv_color_hex(0x1E88FF), lv_color_hex(0xFB8C00), true,
                               &lv_font_montserrat_14);
    }
    lv_obj_t *scr = lv_screen_active();
    style_flat(scr);
    build_chrome();
    show_splash();
    lv_timer_create([](lv_timer_t *t) {
        (void)t;
        ui_tick(app_is_dimmed());
    }, 32, NULL);
}

void ui_tick(bool dimmed) {
    uint32_t now = port_millis();
    if (splash && splash_mark.root) mark_step(&splash_mark, now, false);
    for (int i = 0; i < page_count; i++) mark_step(&marks[i], now, dimmed);
    ui_lens_tick(now);
    if (now > next_battery_at) {
        refresh_battery();
        next_battery_at = now + 15000;
    }
}

static void step_page(int dir) {
    if (splash) {
        dismiss_splash();
        return;
    }
    if (page_count <= 0) return;
    int i = active_index();
    i = (i + dir) % page_count;
    if (i < 0) i += page_count;
    lv_obj_scroll_to_view(pages[i], LV_ANIM_ON);
}

void ui_next(void) {
    if (ui_lens_open()) {
        ui_close_lens();
        return;
    }
    step_page(1);
}
void ui_prev(void) {
    if (ui_lens_open()) {
        ui_close_lens();
        return;
    }
    step_page(-1);
}

void ui_on_shake(void) {
    bool hidden = false;
    for (unsigned i = 0; i < pb_bot_count; i++)
        if (pb_bots[i].hidden) hidden = true;
    if (hidden && !port_egg_unlocked()) {
        port_set_egg_unlocked(true);
        if (!splash) ui_rebuild_carousel();
        show_toast("a moth. it lives here now.");
    } else {
        int i = active_index();
        if (i >= 0 && i < page_count) mark_bounce(&marks[i]);
        show_toast("we're awake.");
    }
}

void ui_wake(void) {
    lv_obj_invalidate(lv_screen_active());
}

void ui_sheet(void) {
    /* One frame of every shape, for the README. Not built into the device. */
    lv_obj_t *scr = lv_screen_active();
    style_flat(scr);
    static const char *names[] = {
        "circle", "cloud", "squircle", "plus", "peanut", "blob", "flower",
        "drop", "pill", "triangle", "house", "star", "hexagon"};
    static const uint32_t cols[] = {
        0x1E88FF, 0xFB8C00, 0xE53935, 0xFFB300, 0x43A047, 0x00ACC1, 0x8E24AA,
        0xEC407A, 0xF4F4F5, 0x8D6E63, 0x9E9E9E, 0x1E88FF, 0xFB8C00};
    static pb_bot_t bots[13];
    static pb_mark ms[13];
    const int cell = 150;
    for (int i = 0; i < PB_SHAPE_COUNT && i < 13; i++) {
        bots[i].id = names[i];
        bots[i].name = names[i];
        bots[i].subtitle = "";
        bots[i].shape = (pb_shape_t)i;
        bots[i].color = cols[i];
        bots[i].hidden = false;
        bots[i].image = NULL;
        int col = i % 5;
        int row = i / 5;
        mark_create(&ms[i], scr, &bots[i], 120);
        lv_obj_set_pos(ms[i].root, 12 + col * cell, 8 + row * cell);
        lv_obj_t *lab = lv_label_create(scr);
        lv_label_set_text(lab, names[i]);
        lv_obj_set_style_text_font(lab, &lv_font_montserrat_12, 0);
        lv_obj_set_style_text_color(lab, lv_color_hex(MUTED), 0);
        lv_obj_set_pos(lab, 16 + col * cell, 130 + row * cell);
    }
}
