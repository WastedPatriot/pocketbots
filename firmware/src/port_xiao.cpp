#include "port.h"
#include "app.h"

#include "generated/bots.gen.h"
#include "lvgl.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>

#include <stdlib.h>
#include <string.h>

#include "esp_camera.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"

/* Seeed XIAO ESP32-S3 Sense, plus a Waveshare 1.69" ST7789 + CST816.
   The camera lives on the expansion connector. The header pins are free,
   and the radio is switched off: nothing here joins a network.

   Display (header):
     MOSI D10 GPIO9, SCK D8 GPIO7, CS D0 GPIO1, DC D1 GPIO2, RST D3 GPIO4,
     backlight D6 GPIO43.
   Touch:
     SDA D4 GPIO5, SCL D5 GPIO6, INT D9 GPIO8 (an RTC pin, so it can wake).
     Tie the touch RST to 3V3. D2 (GPIO3) is a strap pin and stays unused.
     D11/D12 are the microphone and stay unused.
   The BOOT button is GPIO0.

   ST7789 240×280 panels are a 240×320 controller with the picture starting
   20 rows down. If the image is short or shifted, PB_LCD_GAP_Y. If the
   colours look inverted, PB_LCD_INVERT. If a preview looks false-coloured,
   PB_CAM_SWAP. */

#ifndef PB_LCD_GAP_X
#define PB_LCD_GAP_X 0
#endif
#ifndef PB_LCD_GAP_Y
#define PB_LCD_GAP_Y 20
#endif
#ifndef PB_LCD_INVERT
#define PB_LCD_INVERT 1
#endif
#ifndef PB_LCD_MIRROR_X
#define PB_LCD_MIRROR_X 0
#endif
#ifndef PB_LCD_MIRROR_Y
#define PB_LCD_MIRROR_Y 0
#endif
#ifndef PB_LCD_BGR
#define PB_LCD_BGR 0
#endif
#ifndef PB_SWAP_BYTES
#define PB_SWAP_BYTES 1
#endif
#ifndef PB_CAM_SWAP
#define PB_CAM_SWAP 1
#endif
#ifndef PB_TOUCH_SWAP
#define PB_TOUCH_SWAP 0
#endif
#ifndef PB_TOUCH_MIRROR_X
#define PB_TOUCH_MIRROR_X 0
#endif
#ifndef PB_TOUCH_MIRROR_Y
#define PB_TOUCH_MIRROR_Y 0
#endif
#ifndef PB_WAKE_ON_TOUCH
#define PB_WAKE_ON_TOUCH 1
#endif

static const int PIN_LCD_MOSI = 9;
static const int PIN_LCD_SCK = 7;
static const int PIN_LCD_CS = 1;
static const int PIN_LCD_DC = 2;
static const int PIN_LCD_RST = 4;
static const int PIN_LCD_BL = 43;
static const int PIN_SDA = 5;
static const int PIN_SCL = 6;
static const int PIN_TP_INT = 8;
static const int PIN_BOOT = 0;

static const uint8_t CST816_ADDR = 0x15;

static const int PIN_CAM_XCLK = 10;
static const int PIN_CAM_SIOD = 40;
static const int PIN_CAM_SIOC = 39;
static const int PIN_CAM_Y9 = 48;
static const int PIN_CAM_Y8 = 11;
static const int PIN_CAM_Y7 = 12;
static const int PIN_CAM_Y6 = 14;
static const int PIN_CAM_Y5 = 16;
static const int PIN_CAM_Y4 = 18;
static const int PIN_CAM_Y3 = 17;
static const int PIN_CAM_Y2 = 15;
static const int PIN_CAM_VSYNC = 38;
static const int PIN_CAM_HREF = 47;
static const int PIN_CAM_PCLK = 13;

static esp_lcd_panel_handle_t panel;
static Preferences prefs;
static int brightness = PB_CFG_BRIGHTNESS;
static bool button_down;
static bool cam_ready;
static pixformat_t cam_fmt = PIXFORMAT_RGB565;

void port_log(const char *msg) { Serial.println(msg); }
uint32_t port_millis(void) { return millis(); }
int port_rand(void) { return (int)esp_random(); }

void *port_alloc(size_t n) {
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(n);
    if (p) memset(p, 0, n);
    return p;
}

/* The Sense board charges a cell and does not bring the voltage out to an
   ADC. Reporting a made-up percent would be worse than "usb". */
int port_battery_percent(void) { return -1; }

int port_saved_brightness(void) { return prefs.getUChar("bri", PB_CFG_BRIGHTNESS); }

static void apply_backlight(int percent) {
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    uint32_t duty = (uint32_t)percent * 255u / 100u;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
}

void port_set_brightness(int percent) {
    if (percent < 1) percent = 1;
    if (percent > 100) percent = 100;
    brightness = percent;
    apply_backlight(percent);
}

void port_save_brightness(int percent) {
    if (percent < 1) percent = 1;
    if (percent > 100) percent = 100;
    prefs.putUChar("bri", (uint8_t)percent);
}

bool port_egg_unlocked(void) { return prefs.getBool("egg", false); }
void port_set_egg_unlocked(bool on) { prefs.putBool("egg", on); }
bool port_button_down(void) { return button_down; }
bool port_take_shake(void) { return false; }

void port_poll(void) {
    static uint32_t last_edge;
    static bool raw_was;
    bool raw = digitalRead(PIN_BOOT) == LOW;
    uint32_t now = millis();
    if (raw != raw_was) last_edge = now;
    raw_was = raw;
    if (now - last_edge > 25) button_down = raw;
}

static void camera_stop(void) {
    if (!cam_ready) return;
    esp_camera_deinit();
    cam_ready = false;
}

static bool camera_start(pixformat_t fmt, framesize_t size, int quality) {
    if (cam_ready && cam_fmt == fmt) {
        sensor_t *sensor = esp_camera_sensor_get();
        if (sensor && sensor->set_quality) sensor->set_quality(sensor, quality);
        if (sensor && sensor->set_framesize) sensor->set_framesize(sensor, size);
        return true;
    }
    camera_stop();
    camera_config_t c = {};
    c.pin_pwdn = -1;
    c.pin_reset = -1;
    c.pin_xclk = PIN_CAM_XCLK;
    c.pin_sccb_sda = PIN_CAM_SIOD;
    c.pin_sccb_scl = PIN_CAM_SIOC;
    c.pin_d7 = PIN_CAM_Y9;
    c.pin_d6 = PIN_CAM_Y8;
    c.pin_d5 = PIN_CAM_Y7;
    c.pin_d4 = PIN_CAM_Y6;
    c.pin_d3 = PIN_CAM_Y5;
    c.pin_d2 = PIN_CAM_Y4;
    c.pin_d1 = PIN_CAM_Y3;
    c.pin_d0 = PIN_CAM_Y2;
    c.pin_vsync = PIN_CAM_VSYNC;
    c.pin_href = PIN_CAM_HREF;
    c.pin_pclk = PIN_CAM_PCLK;
    c.xclk_freq_hz = 20000000;
    c.ledc_timer = LEDC_TIMER_0;
    c.ledc_channel = LEDC_CHANNEL_0;
    c.pixel_format = fmt;
    c.frame_size = size;
    c.jpeg_quality = quality;
    c.fb_count = 1;
    c.fb_location = CAMERA_FB_IN_PSRAM;
    c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    c.sccb_i2c_port = 0;
    c.jpeg_buffer_size = 0;
    esp_err_t err = esp_camera_init(&c);
    if (err != ESP_OK) {
        Serial.printf("camera: %d\n", (int)err);
        return false;
    }
    cam_ready = true;
    cam_fmt = fmt;
    return true;
}

bool port_camera_available(void) { return true; }

bool port_camera_preview(uint16_t *dst, int w, int h) {
    if (!dst || w < 2 || h < 2) return false;
    if (!camera_start(PIXFORMAT_RGB565, FRAMESIZE_QVGA, 12)) return false;
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb || fb->format != PIXFORMAT_RGB565 || !fb->buf) {
        if (fb) esp_camera_fb_return(fb);
        return false;
    }
    const uint16_t *src = (const uint16_t *)fb->buf;
    int sw = (int)fb->width;
    int sh = (int)fb->height;
    for (int y = 0; y < h; y++) {
        int sy = y * sh / h;
        if (sy >= sh) sy = sh - 1;
        for (int x = 0; x < w; x++) {
            int sx = x * sw / w;
            if (sx >= sw) sx = sw - 1;
            uint16_t p = src[(size_t)sy * (size_t)sw + (size_t)sx];
#if PB_CAM_SWAP
            p = (uint16_t)((p >> 8) | (p << 8));
#endif
            dst[(size_t)y * (size_t)w + (size_t)x] = p;
        }
    }
    esp_camera_fb_return(fb);
    return true;
}

static bool grab_jpeg(uint8_t **out, size_t *out_len) {
    *out = NULL;
    *out_len = 0;
    const int qualities[] = {16, 28, 40};
    uint8_t *best = NULL;
    size_t best_len = 0;
    for (int q : qualities) {
        if (!camera_start(PIXFORMAT_JPEG, FRAMESIZE_QVGA, q)) break;
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) continue;
        esp_camera_fb_return(fb);
        fb = esp_camera_fb_get();
        if (!fb) continue;
        bool jpeg = fb->format == PIXFORMAT_JPEG && fb->buf && fb->len > 0;
        if (jpeg && (best_len == 0 || fb->len < best_len)) {
            uint8_t *copy = (uint8_t *)malloc(fb->len);
            if (copy) {
                memcpy(copy, fb->buf, fb->len);
                free(best);
                best = copy;
                best_len = fb->len;
            }
        }
        bool fits = jpeg && fb->len <= PB_JPEG_MAX;
        esp_camera_fb_return(fb);
        if (fits) break;
    }
    if (!best || best_len > PB_JPEG_MAX) {
        free(best);
        return false;
    }
    *out = best;
    *out_len = best_len;
    return true;
}

int port_camera_snap(const uint16_t *frame, int w, int h) {
    if (!frame || w != PB_FRAME_W || h != PB_FRAME_H) return -1;
    uint8_t *jpeg = NULL;
    size_t len = 0;
    grab_jpeg(&jpeg, &len);
    int n = gallery_push(frame, jpeg, len);
    free(jpeg);
    return n;
}

void port_sleep(void) {
    if (!panel) return;
    port_log("sleep");
    camera_stop();
    apply_backlight(0);
    esp_lcd_panel_disp_on_off(panel, false);

    uint64_t mask = 1ULL << PIN_BOOT;
#if PB_WAKE_ON_TOUCH
    pinMode(PIN_TP_INT, INPUT_PULLUP);
    mask |= 1ULL << PIN_TP_INT;
#endif
    pinMode(PIN_BOOT, INPUT_PULLUP);
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_light_sleep_start();

    esp_lcd_panel_disp_on_off(panel, true);
    apply_backlight(brightness);
    port_log("wake");
}

static bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)addr, (int)len) != (int)len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

bool port_read_touch(int *x, int *y) {
    uint8_t n = 0;
    if (!i2c_read(CST816_ADDR, 0x02, &n, 1)) return false;
    n &= 0x0F;
    if (n == 0) return false;
    uint8_t buf[4];
    if (!i2c_read(CST816_ADDR, 0x03, buf, 4)) return false;
    int tx = ((buf[0] & 0x0F) << 8) | buf[1];
    int ty = ((buf[2] & 0x0F) << 8) | buf[3];
#if PB_TOUCH_SWAP
    int swap = tx;
    tx = ty;
    ty = swap;
#endif
#if PB_TOUCH_MIRROR_X
    tx = (PB_DISP_W - 1) - tx;
#endif
#if PB_TOUCH_MIRROR_Y
    ty = (PB_DISP_H - 1) - ty;
#endif
    if (tx < 0) tx = 0;
    if (ty < 0) ty = 0;
    if (tx > PB_DISP_W - 1) tx = PB_DISP_W - 1;
    if (ty > PB_DISP_H - 1) ty = PB_DISP_H - 1;
    *x = tx;
    *y = ty;
    return true;
}

static bool IRAM_ATTR on_color_done(esp_lcd_panel_io_handle_t, esp_lcd_panel_io_event_data_t *, void *ctx) {
    lv_display_flush_ready((lv_display_t *)ctx);
    return false;
}

static void flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px) {
    (void)disp;
    int x1 = area->x1;
    int y1 = area->y1;
    int x2 = area->x2 + 1;
    int y2 = area->y2 + 1;
#if PB_SWAP_BYTES
    uint16_t *src = (uint16_t *)px;
    size_t n = (size_t)(x2 - x1) * (size_t)(y2 - y1);
    for (size_t i = 0; i < n; i++) src[i] = (uint16_t)((src[i] >> 8) | (src[i] << 8));
#endif
    esp_lcd_panel_draw_bitmap(panel, x1, y1, x2, y2, px);
}

static void indev_cb(lv_indev_t *, lv_indev_data_t *data) {
    int x, y;
    if (port_read_touch(&x, &y)) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void backlight_begin(void) {
    ledc_timer_config_t timer = {};
    timer.speed_mode = LEDC_LOW_SPEED_MODE;
    timer.duty_resolution = LEDC_TIMER_8_BIT;
    timer.timer_num = LEDC_TIMER_1;
    timer.freq_hz = 5000;
    timer.clk_cfg = LEDC_AUTO_CLK;
    ledc_timer_config(&timer);

    ledc_channel_config_t ch = {};
    ch.gpio_num = PIN_LCD_BL;
    ch.speed_mode = LEDC_LOW_SPEED_MODE;
    ch.channel = LEDC_CHANNEL_1;
    ch.intr_type = LEDC_INTR_DISABLE;
    ch.timer_sel = LEDC_TIMER_1;
    ch.duty = 0;
    ch.hpoint = 0;
    ledc_channel_config(&ch);
}

static bool display_begin(lv_display_t *disp) {
    spi_bus_config_t bus = {};
    bus.mosi_io_num = PIN_LCD_MOSI;
    bus.miso_io_num = -1;
    bus.sclk_io_num = PIN_LCD_SCK;
    bus.quadwp_io_num = -1;
    bus.quadhd_io_num = -1;
    bus.data4_io_num = -1;
    bus.data5_io_num = -1;
    bus.data6_io_num = -1;
    bus.data7_io_num = -1;
    bus.max_transfer_sz = PB_DISP_W * 48 * 2;
    esp_err_t err = spi_bus_initialize(SPI2_HOST, &bus, SPI_DMA_CH_AUTO);
    if (err != ESP_OK) {
        Serial.printf("spi bus: %d\n", (int)err);
        return false;
    }

    esp_lcd_panel_io_spi_config_t io_cfg = {};
    io_cfg.cs_gpio_num = PIN_LCD_CS;
    io_cfg.dc_gpio_num = PIN_LCD_DC;
    io_cfg.spi_mode = 0;
    io_cfg.pclk_hz = 40 * 1000 * 1000;
    io_cfg.trans_queue_depth = 10;
    io_cfg.on_color_trans_done = on_color_done;
    io_cfg.user_ctx = disp;
    io_cfg.lcd_cmd_bits = 8;
    io_cfg.lcd_param_bits = 8;

    esp_lcd_panel_io_handle_t io = NULL;
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io);
    if (err != ESP_OK) {
        Serial.printf("panel io: %d\n", (int)err);
        return false;
    }

    esp_lcd_panel_dev_config_t cfg = {};
    cfg.reset_gpio_num = PIN_LCD_RST;
    cfg.rgb_ele_order = PB_LCD_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB;
    cfg.bits_per_pixel = 16;

    err = esp_lcd_new_panel_st7789(io, &cfg, &panel);
    if (err != ESP_OK || !panel) {
        Serial.printf("panel: %d\n", (int)err);
        return false;
    }
    esp_lcd_panel_reset(panel);
    err = esp_lcd_panel_init(panel);
    if (err != ESP_OK) {
        Serial.printf("panel init: %d\n", (int)err);
        return false;
    }
    esp_lcd_panel_invert_color(panel, PB_LCD_INVERT);
    esp_lcd_panel_swap_xy(panel, false);
    esp_lcd_panel_mirror(panel, PB_LCD_MIRROR_X, PB_LCD_MIRROR_Y);
    esp_lcd_panel_set_gap(panel, PB_LCD_GAP_X, PB_LCD_GAP_Y);
    esp_lcd_panel_disp_on_off(panel, true);
    return true;
}

void setup() {
    Serial.begin(115200);
    delay(40);
    Serial.println("pocketbots");
    WiFi.mode(WIFI_OFF);
    btStop();

    prefs.begin("pocketbots", false);
    pinMode(PIN_BOOT, INPUT_PULLUP);
    pinMode(PIN_TP_INT, INPUT_PULLUP);

    backlight_begin();
    Wire.begin(PIN_SDA, PIN_SCL, 300000);

    lv_init();
    lv_display_t *disp = lv_display_create(PB_DISP_W, PB_DISP_H);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    size_t buf_sz = PB_DISP_W * 48 * 2;
    void *b1 = heap_caps_malloc(buf_sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    void *b2 = heap_caps_malloc(buf_sz, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (!b1 || !b2) {
        Serial.println("no DMA buffer");
        return;
    }
    lv_display_set_buffers(disp, b1, b2, buf_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, flush_cb);

    if (!display_begin(disp)) {
        Serial.println("display failed; serial is all you get");
    } else {
        port_set_brightness(port_saved_brightness());
    }

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, indev_cb);
    lv_indev_set_long_press_time(indev, 500);

    app_init();
}

void loop() {
    static uint32_t last;
    uint32_t now = millis();
    uint32_t step = now - last;
    if (step > 50) step = 50;
    lv_tick_inc(step);
    last = now;
    app_loop();
    delay(2);
}
