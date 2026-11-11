#include "port.h"
#include "app.h"
#include "ui.h"
#include "generated/bots.gen.h"

#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint32_t sim_ms;
static int sim_frame;
static bool dumping;
static bool sheet;
static bool lens_mode;
static int saved_bri = PB_CFG_BRIGHTNESS;
static bool egg;
static int battery = 76;

static uint16_t *fb;
static int fb_w;
static int fb_h;

void port_log(const char *msg) { fprintf(stderr, "pocketbots: %s\n", msg); }
uint32_t port_millis(void) { return sim_ms; }
int port_rand(void) { return rand(); }
void *port_alloc(size_t n) { return calloc(1, n); }
int port_battery_percent(void) { return battery; }
int port_saved_brightness(void) { return saved_bri; }
void port_set_brightness(int p) { saved_bri = p; }
void port_save_brightness(int p) { saved_bri = p; }
bool port_egg_unlocked(void) { return egg; }
void port_set_egg_unlocked(bool on) { egg = on; }
bool port_button_down(void) { return false; }
bool port_take_shake(void) { return false; }
void port_poll(void) {}
void port_sleep(void) { port_log("sleep (sim, skipped)"); }

#include "sim_jpeg.inc"

static uint16_t pack565(int r, int g, int b) {
    return (uint16_t)(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

/* A small room, so the viewfinder is obviously a camera and not a second bot. */
bool port_camera_available(void) { return true; }

bool port_camera_preview(uint16_t *dst, int w, int h) {
    if (!dst || w < 8 || h < 8) return false;
    int drift = (sim_frame % 24) - 12;
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            bool floor = y > h * 58 / 100;
            int r, g, b;
            if (!floor) {
                int t = y * 255 / (h * 58 / 100 + 1);
                r = 22 + t / 9;
                g = 34 + t / 6;
                b = 62 + t / 5;
            } else {
                r = 42;
                g = 36;
                b = 30;
            }
            int wx = w * 62 / 100;
            int wy = h * 18 / 100;
            if (x > wx && x < wx + w / 5 && y > wy && y < wy + h / 4) {
                r = 210;
                g = 196;
                b = 150;
            }
            int cx = w / 2 + drift;
            int cy = h * 48 / 100;
            int dx = x - cx;
            int dy = y - cy;
            if (dx * dx + dy * dy * 2 < (h / 5) * (h / 5)) {
                r = 30;
                g = 136;
                b = 255;
            }
            dst[(size_t)y * (size_t)w + (size_t)x] = pack565(r, g, b);
        }
    }
    return true;
}

int port_camera_snap(const uint16_t *frame, int w, int h) {
    if (!frame || w != PB_FRAME_W || h != PB_FRAME_H) return -1;
    return gallery_push(frame, k_sim_jpeg, k_sim_jpeg_len);
}

/* A scripted finger. dump swipes the carousel. lens opens the viewfinder,
   taps the shutter, then keeps the frame. Coordinates match ui.cpp. */
bool port_read_touch(int *x, int *y) {
    if (sheet) return false;
    if (lens_mode) {
        if (sim_frame >= 28 && sim_frame <= 30) {
            *x = 28;
            *y = PB_DISP_H - 28;
            return true;
        }
        if ((sim_frame >= 44 && sim_frame <= 46) || (sim_frame >= 60 && sim_frame <= 62)) {
            *x = PB_DISP_W / 2;
            *y = PB_DISP_H - 42;
            return true;
        }
        return false;
    }
    /* Splash occupies the first 1.1s. Then Cyber idles, and we drag onto Vik.
       y sits on the mark, clear of the lens button. */
    if (sim_frame >= 50 && sim_frame <= 78) {
        float t = (sim_frame - 50) / 28.f;
        *x = (int)(190 - t * 150);
        *y = PB_DISP_H / 2 - 14;
        return true;
    }
    return false;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
    int w = lv_area_get_width(area);
    uint16_t *src = (uint16_t *)px;
    for (int y = area->y1; y <= area->y2; y++) {
        memcpy(&fb[(size_t)y * fb_w + area->x1], src, (size_t)w * 2);
        src += w;
    }
    lv_display_flush_ready(disp);
}

static void indev_cb(lv_indev_t *indev, lv_indev_data_t *data) {
    (void)indev;
    int x = 0, y = 0;
    if (port_read_touch(&x, &y)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void write_ppm(int index) {
    char path[128];
    snprintf(path, sizeof path, "frames/frame_%03d.ppm", index);
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fprintf(f, "P6\n%d %d\n255\n", fb_w, fb_h);
    for (int i = 0; i < fb_w * fb_h; i++) {
        uint16_t c = fb[i];
        uint8_t r = (c >> 11) & 0x1F;
        uint8_t g = (c >> 5) & 0x3F;
        uint8_t b = c & 0x1F;
        uint8_t rgb[3] = {(uint8_t)(r * 255 / 31), (uint8_t)(g * 255 / 63), (uint8_t)(b * 255 / 31)};
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}

static int framebuffer_varied(void) {
    if (!fb) return 0;
    uint16_t first = fb[0];
    int diff = 0;
    int n = fb_w * fb_h;
    for (int i = 0; i < n; i += 17) {
        if (fb[i] != first) diff++;
    }
    return diff;
}

int main(int argc, char **argv) {
    const char *mode = argc > 1 ? argv[1] : "run";
    dumping = strcmp(mode, "dump") == 0;
    sheet = strcmp(mode, "sheet") == 0;
    lens_mode = strcmp(mode, "lens") == 0;
    if (lens_mode) dumping = true;
    srand(1);

    fb_w = sheet ? 780 : PB_DISP_W;
    fb_h = sheet ? 520 : PB_DISP_H;
    fb = (uint16_t *)calloc((size_t)fb_w * fb_h, 2);
    if (!fb) return 2;

    lv_init();
    lv_display_t *disp = lv_display_create(fb_w, fb_h);
    static uint16_t buf[780 * 40];
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(disp, buf, NULL, sizeof buf, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);
    lv_theme_default_init(disp, lv_color_hex(0x1E88FF), lv_color_hex(0xFB8C00), true,
                           &lv_font_montserrat_14);

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indev_cb);
    lv_indev_set_long_press_time(indev, 500);

    if (sheet) {
        ui_sheet();
        lv_tick_inc(50);
        sim_ms += 50;
        lv_timer_handler();
        lv_tick_inc(50);
        sim_ms += 50;
        lv_timer_handler();
        if (dumping || sheet) write_ppm(0);
        int varied = framebuffer_varied();
        fprintf(stderr, "sheet varied=%d\n", varied);
        return varied > 20 ? 0 : 1;
    }

    app_init();
    int frames = lens_mode ? 80 : dumping ? 100 : 30;
    if (dumping) {
        if (system("mkdir -p frames") != 0) return 2;
    }
    for (sim_frame = 0; sim_frame < frames; sim_frame++) {
        lv_tick_inc(50);
        sim_ms += 50;
        app_loop();
        if (dumping) write_ppm(sim_frame);
    }
    int varied = framebuffer_varied();
    fprintf(stderr, "sim frames=%d varied=%d brightness=%d\n", frames, varied, saved_bri);
    return varied > 30 ? 0 : 1;
}
