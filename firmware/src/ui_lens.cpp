#include "ui.h"

#include "app.h"
#include "port.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

/* Viewfinder and the on-device roll. Nothing here leaves the device.
   The shutter and the keep pill share a centre at (W/2, H-42) so the
   simulator can hit both with one coordinate. The lens button that
   opens this is in ui.cpp, at (21, H-19). */

#define BG 0x141416
#define INK 0xF2F2F3
#define MUTED 0x8E8E96

enum { S_OFF = 0, S_LIVE, S_FROZEN, S_ROLL };

static int state;
static lv_obj_t *layer;
static lv_obj_t *clip;
static lv_obj_t *photo;
static lv_obj_t *shutter;
static lv_obj_t *keep;
static lv_obj_t *again;
static lv_obj_t *drop;
static lv_obj_t *roll_btn;
static lv_obj_t *status;
static uint16_t *preview;
static lv_image_dsc_t dsc;
static uint32_t last_grab;

static void hide(lv_obj_t *o, bool on) {
    if (!o) return;
    if (on) lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static void show_pixels(const uint16_t *px, int w, int h) {
    memset(&dsc, 0, sizeof dsc);
    dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    dsc.header.w = w;
    dsc.header.h = h;
    dsc.header.stride = w * 2;
    dsc.data_size = (uint32_t)w * (uint32_t)h * 2u;
    dsc.data = (const uint8_t *)px;
    lv_image_set_src(photo, &dsc);
    lv_obj_invalidate(photo);
}

static lv_obj_t *pill(lv_obj_t *parent, const char *text, int w, uint32_t bg) {
    lv_obj_t *b = lv_obj_create(parent);
    lv_obj_set_size(b, w, 40);
    lv_obj_set_style_radius(b, 20, 0);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(b, 0, 0);
    lv_obj_set_style_pad_all(b, 0, 0);
    lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *lab = lv_label_create(b);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_font(lab, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lab, lv_color_hex(INK), 0);
    lv_obj_remove_flag(lab, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(lab);
    return b;
}

static void show_roll(void) {
    state = S_ROLL;
    hide(shutter, true);
    hide(keep, true);
    hide(again, true);
    hide(roll_btn, true);
    hide(drop, false);
    int n = port_gallery_count();
    if (n <= 0) {
        hide(clip, true);
        lv_obj_align(status, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(status, "nothing in the roll");
        return;
    }
    const uint16_t *px = NULL;
    int w = 0, h = 0;
    if (!port_gallery_frame(0, &px, &w, &h) || !px) {
        hide(clip, true);
        lv_obj_align(status, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(status, "nothing in the roll");
        return;
    }
    hide(clip, false);
    show_pixels(px, w, h);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -70);
    size_t bytes = port_gallery_jpeg_size(0);
    char buf[40];
    if (bytes == 0) snprintf(buf, sizeof buf, "no jpeg this time");
    else if (bytes < 1024) snprintf(buf, sizeof buf, "jpeg, %u bytes", (unsigned)bytes);
    else snprintf(buf, sizeof buf, "jpeg, %u kb", (unsigned)((bytes + 1023) / 1024));
    lv_label_set_text(status, buf);
}

static void show_live(void) {
    state = S_LIVE;
    last_grab = 0;
    hide(layer, false);
    hide(clip, false);
    hide(shutter, false);
    hide(keep, true);
    hide(again, true);
    hide(drop, true);
    hide(roll_btn, false);
    lv_label_set_text(status, "lens");
    if (preview && port_camera_preview(preview, PB_FRAME_W, PB_FRAME_H)) show_pixels(preview, PB_FRAME_W, PB_FRAME_H);
    else lv_label_set_text(status, "the lens didn't wake");
    lv_obj_move_foreground(layer);
}

static void show_frozen(void) {
    state = S_FROZEN;
    hide(shutter, true);
    hide(keep, false);
    hide(again, false);
    hide(drop, true);
    hide(roll_btn, true);
    lv_label_set_text(status, "keep it?");
}

static void ensure_layer(void) {
    if (layer) return;
    layer = lv_obj_create(lv_screen_active());
    lv_obj_set_size(layer, PB_DISP_W, PB_DISP_H);
    lv_obj_set_style_bg_color(layer, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(layer, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(layer, 0, 0);
    lv_obj_set_style_pad_all(layer, 0, 0);
    lv_obj_set_style_radius(layer, 0, 0);
    lv_obj_remove_flag(layer, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(layer, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *back = lv_label_create(layer);
    lv_label_set_text(back, "back");
    lv_obj_set_style_text_font(back, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(back, lv_color_hex(INK), 0);
    lv_obj_align(back, LV_ALIGN_TOP_LEFT, 12, 10);
    lv_obj_add_flag(back, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(back, [](lv_event_t *e) {
        (void)e;
        ui_close_lens();
    }, LV_EVENT_CLICKED, NULL);

    roll_btn = lv_label_create(layer);
    lv_label_set_text(roll_btn, "roll");
    lv_obj_set_style_text_font(roll_btn, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(roll_btn, lv_color_hex(INK), 0);
    lv_obj_align(roll_btn, LV_ALIGN_TOP_RIGHT, -12, 10);
    lv_obj_add_flag(roll_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(roll_btn, [](lv_event_t *e) {
        (void)e;
        show_roll();
        app_note_activity();
    }, LV_EVENT_CLICKED, NULL);

    status = lv_label_create(layer);
    lv_obj_set_style_text_font(status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(status, lv_color_hex(MUTED), 0);
    lv_obj_align(status, LV_ALIGN_BOTTOM_MID, 0, -70);

    clip = lv_obj_create(layer);
    lv_obj_set_size(clip, PB_FRAME_W, PB_FRAME_H);
    lv_obj_align(clip, LV_ALIGN_CENTER, 0, -24);
    lv_obj_set_style_radius(clip, 16, 0);
    lv_obj_set_style_clip_corner(clip, true, 0);
    lv_obj_set_style_bg_color(clip, lv_color_hex(0x101014), 0);
    lv_obj_set_style_border_width(clip, 0, 0);
    lv_obj_set_style_pad_all(clip, 0, 0);
    lv_obj_remove_flag(clip, LV_OBJ_FLAG_SCROLLABLE);

    photo = lv_image_create(clip);
    lv_obj_center(photo);

    shutter = lv_obj_create(layer);
    lv_obj_set_size(shutter, 52, 52);
    lv_obj_align(shutter, LV_ALIGN_BOTTOM_MID, 0, -16);
    lv_obj_set_style_radius(shutter, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(shutter, lv_color_hex(INK), 0);
    lv_obj_set_style_border_width(shutter, 0, 0);
    lv_obj_remove_flag(shutter, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(shutter, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_t *ring = lv_obj_create(shutter);
    lv_obj_set_size(ring, 38, 38);
    lv_obj_set_style_radius(ring, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(ring, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(ring, lv_color_hex(BG), 0);
    lv_obj_set_style_border_width(ring, 3, 0);
    lv_obj_remove_flag(ring, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(ring);
    lv_obj_add_event_cb(shutter, [](lv_event_t *e) {
        (void)e;
        if (state != S_LIVE || !preview) return;
        show_frozen();
        app_note_activity();
    }, LV_EVENT_CLICKED, NULL);

    keep = pill(layer, "keep", 96, 0x1E88FF);
    lv_obj_align(keep, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_add_event_cb(keep, [](lv_event_t *e) {
        (void)e;
        if (state != S_FROZEN || !preview) return;
        int n = port_camera_snap(preview, PB_FRAME_W, PB_FRAME_H);
        if (n < 0) lv_label_set_text(status, "that one didn't stick");
        else show_roll();
        app_note_activity();
    }, LV_EVENT_CLICKED, NULL);

    again = pill(layer, "again", 64, 0x2A2A30);
    lv_obj_align(again, LV_ALIGN_BOTTOM_MID, -84, -22);
    lv_obj_add_event_cb(again, [](lv_event_t *e) {
        (void)e;
        show_live();
        app_note_activity();
    }, LV_EVENT_CLICKED, NULL);

    drop = pill(layer, "drop", 72, 0x2A2A30);
    lv_obj_align(drop, LV_ALIGN_BOTTOM_MID, 0, -22);
    lv_obj_add_event_cb(drop, [](lv_event_t *e) {
        (void)e;
        if (port_gallery_count() > 0) port_gallery_drop(0);
        show_roll();
        app_note_activity();
    }, LV_EVENT_CLICKED, NULL);
}

void ui_open_lens(void) {
    if (!port_camera_available()) return;
    if (!preview) preview = (uint16_t *)port_alloc((size_t)PB_FRAME_W * PB_FRAME_H * 2);
    ensure_layer();
    show_live();
    app_note_activity();
}

void ui_close_lens(void) {
    state = S_OFF;
    hide(layer, true);
}

bool ui_lens_open(void) { return state != S_OFF; }

void ui_lens_tick(uint32_t now) {
    if (state != S_LIVE || !preview || !photo) return;
    if (last_grab != 0 && now - last_grab < 80) return;
    last_grab = now;
    if (!port_camera_preview(preview, PB_FRAME_W, PB_FRAME_H)) {
        lv_label_set_text(status, "the lens didn't wake");
        return;
    }
    show_pixels(preview, PB_FRAME_W, PB_FRAME_H);
    lv_label_set_text(status, "lens");
}
