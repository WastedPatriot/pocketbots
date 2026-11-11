#include "port.h"
#include "app.h"
#include "ui.h"
#include "generated/bots.gen.h"

#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define PB_DISP_W 280
#define PB_DISP_H 456

static uint32_t sim_ms;
static int sim_frame;
static bool dumping;
static bool sheet;
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

/* A scripted finger, so the dumped GIF actually changes cards. */
bool port_read_touch(int *x, int *y) {
    if (sheet) return false;
    /* Splash occupies the first 1.1s. Then Cyber idles, blinks, and we
       drag left onto Vik. */
    if (sim_frame >= 50 && sim_frame <= 78) {
        float t = (sim_frame - 50) / 28.f;
        *x = (int)(230 - t * 180);
        *y = 210;
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
    int frames = dumping ? 100 : 30;
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
