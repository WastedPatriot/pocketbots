#include "port.h"
#include "app.h"

#include "generated/bots.gen.h"
#include "lvgl.h"

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>
#include <Wire.h>

#include <math.h>
#include <string.h>

/* The radio stays off. A password, if you ever need one, goes in
   include/secrets.h (gitignored). The example file is empty on purpose. */
#if __has_include("secrets.h")
#include "secrets.h"
#else
#include "secrets.example.h"
#endif

#include "esp_lcd_co5300.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"

/* Waveshare ESP32-S3-Touch-AMOLED-1.64.
   V1.1 (silkscreen on the right, Waveshare's "-v2" examples) is the default:
   LCD CS is GPIO46 and the touch interrupt is wired, so light sleep can
   wake on a finger. V1 swaps CS with the IMU interrupt and leaves touch
   INT unconnected. Build with -DPB_BOARD_V11=0 for that board.

   The 20px X gap is the value a working PlatformIO port of this panel
   settled on (trane77). If the picture sits left of the glass, change it. */

#ifndef PB_BOARD_V11
#define PB_BOARD_V11 1
#endif
#ifndef PB_LCD_GAP_X
#define PB_LCD_GAP_X 20
#endif
#ifndef PB_LCD_GAP_Y
#define PB_LCD_GAP_Y 0
#endif
#ifndef PB_SWAP_BYTES
#define PB_SWAP_BYTES 1
#endif
#ifndef PB_LCD_BGR
#define PB_LCD_BGR 0
#endif
#ifndef PB_WAKE_ON_TOUCH
#define PB_WAKE_ON_TOUCH 1
#endif

#if PB_BOARD_V11
static const int PIN_LCD_CS = 46;
static const int PIN_TP_INT = 18;
static const int PIN_IMU_INT = 9;
#else
static const int PIN_LCD_CS = 9;
static const int PIN_TP_INT = -1;
static const int PIN_IMU_INT = 46;
#endif

static const int PIN_LCD_SCK = 10;
static const int PIN_LCD_D0 = 11;
static const int PIN_LCD_D1 = 12;
static const int PIN_LCD_D2 = 13;
static const int PIN_LCD_D3 = 14;
static const int PIN_LCD_RST = 21;
static const int PIN_SDA = 47;
static const int PIN_SCL = 48;
static const int PIN_BAT = 4;
static const int PIN_BOOT = 0;

static const uint8_t FT3168_ADDR = 0x38;
static const uint8_t QMI_WHO = 0x05;

static esp_lcd_panel_handle_t panel;
static Preferences prefs;
static int brightness = PB_CFG_BRIGHTNESS;
static bool button_down;
static bool shake_latched;
static bool imu_ok;
static uint8_t imu_addr;
static float prev_ax, prev_ay, prev_az;
static int shake_run;
static int cached_pct = -1;
static uint32_t next_bat_at;

static const uint8_t k_fe[] = {0x00};
static const uint8_t k_c4[] = {0x80};
static const uint8_t k_35[] = {0x00};
static const uint8_t k_53[] = {0x20};
static const uint8_t k_51[] = {0xFF};
static const uint8_t k_63[] = {0xFF};

/* Command set shared by CO5300 panels on this QSPI glass. Column and row
   windows are sent on every flush, so they are not baked in here. */
static const co5300_lcd_init_cmd_t k_init[] = {
    {0xFE, k_fe, 1, 0},
    {0xC4, k_c4, 1, 0},
    {0x35, k_35, 1, 0},
    {0x53, k_53, 1, 10},
    {0x51, k_51, 1, 0},
    {0x63, k_63, 1, 0},
    {0x11, NULL, 0, 80},
    {0x29, NULL, 0, 10},
};

void port_log(const char *msg) { Serial.println(msg); }
uint32_t port_millis(void) { return millis(); }
int port_rand(void) { return (int)esp_random(); }

void *port_alloc(size_t n) {
    void *p = heap_caps_malloc(n, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!p) p = malloc(n);
    if (p) memset(p, 0, n);
    return p;
}

static bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)addr, (int)len) != (int)len) return false;
    for (size_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
}

static bool i2c_write(uint8_t addr, uint8_t reg, uint8_t val) {
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    return Wire.endTransmission() == 0;
}

static void imu_begin(void) {
    uint8_t candidates[2] = {0x6B, 0x6A};
    for (uint8_t c : candidates) {
        uint8_t who = 0;
        if (i2c_read(c, 0x00, &who, 1) && who == QMI_WHO) {
            imu_addr = c;
            /* Same register values Keybuddy verified on this board. */
            i2c_write(c, 0x02, 0x60);
            i2c_write(c, 0x03, 0x13); /* accel ±4g, 1000 Hz */
            i2c_write(c, 0x08, 0x01); /* accelerometer only */
            imu_ok = true;
            Serial.printf("QMI8658 at 0x%02X\n", c);
            return;
        }
    }
    Serial.println("QMI8658 not found; shake stays a rumour");
}

static void sample_shake(void) {
    if (!imu_ok) return;
    uint8_t raw[6];
    if (!i2c_read(imu_addr, 0x35, raw, 6)) return;
    int16_t rx = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t ry = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t rz = (int16_t)((raw[5] << 8) | raw[4]);
    const float sens = 8192.f;
    float ax = rx / sens, ay = ry / sens, az = rz / sens;
    float delta = fabsf(ax - prev_ax) + fabsf(ay - prev_ay) + fabsf(az - prev_az);
    prev_ax = ax;
    prev_ay = ay;
    prev_az = az;
    /* 3.0g summed, three samples in a row: a deliberate shake, not a fidget.
       Tuned on this board by Keybuddy; a lighter threshold wakes on a tap. */
    if (delta > 3.0f) {
        if (++shake_run >= 3) {
            shake_latched = true;
            shake_run = 0;
        }
    } else {
        shake_run = 0;
    }
}

static void sample_battery(void) {
    int mv = analogReadMilliVolts(PIN_BAT);
    /* Waveshare's divider on GPIO4 is 3:1 (200k over 100k). Their adc_bsp
       and Keybuddy both multiply by 3. A reading under 2.5V means there
       is no cell, or the pin is floating while you run from USB. */
    float v = (mv / 1000.f) * 3.f;
    if (v < 2.5f) {
        cached_pct = -1;
        return;
    }
    int pct = (int)((v - 3.0f) / (4.2f - 3.0f) * 100.f + 0.5f);
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    cached_pct = pct;
}

int port_battery_percent(void) { return cached_pct; }

int port_saved_brightness(void) {
    return prefs.getUChar("bri", PB_CFG_BRIGHTNESS);
}

void port_set_brightness(int percent) {
    if (percent < 1) percent = 1;
    if (percent > 100) percent = 100;
    brightness = percent;
    if (panel) esp_lcd_panel_co5300_set_brightness(panel, (uint8_t)percent);
}

void port_save_brightness(int percent) {
    if (percent < 1) percent = 1;
    if (percent > 100) percent = 100;
    prefs.putUChar("bri", (uint8_t)percent);
}

bool port_egg_unlocked(void) { return prefs.getBool("egg", false); }
void port_set_egg_unlocked(bool on) { prefs.putBool("egg", on); }
bool port_button_down(void) { return button_down; }
bool port_take_shake(void) {
    bool s = shake_latched;
    shake_latched = false;
    return s;
}

void port_poll(void) {
    static uint32_t last_edge;
    static bool raw_was;
    bool raw = digitalRead(PIN_BOOT) == LOW;
    uint32_t now = millis();
    if (raw != raw_was) last_edge = now;
    raw_was = raw;
    if (now - last_edge > 25) button_down = raw;

    static uint32_t last_imu;
    if (now - last_imu > 40) {
        last_imu = now;
        sample_shake();
    }
    if (now > next_bat_at) {
        sample_battery();
        next_bat_at = now + 20000;
    }
}

void port_sleep(void) {
    if (!panel) return;
    port_log("sleep");
    esp_lcd_panel_disp_sleep(panel, true);

    uint64_t mask = 1ULL << PIN_BOOT;
#if PB_BOARD_V11 && PB_WAKE_ON_TOUCH
    if (PIN_TP_INT >= 0) {
        pinMode(PIN_TP_INT, INPUT_PULLUP);
        mask |= 1ULL << PIN_TP_INT;
    }
#endif
    pinMode(PIN_BOOT, INPUT_PULLUP);
    esp_sleep_enable_ext1_wakeup(mask, ESP_EXT1_WAKEUP_ANY_LOW);
    esp_light_sleep_start();

    esp_lcd_panel_disp_sleep(panel, false);
    esp_lcd_panel_co5300_set_brightness(panel, (uint8_t)brightness);
    port_log("wake");
}

bool port_read_touch(int *x, int *y) {
    uint8_t n = 0;
    if (!i2c_read(FT3168_ADDR, 0x02, &n, 1) || n == 0) return false;
    uint8_t buf[4];
    if (!i2c_read(FT3168_ADDR, 0x03, buf, 4)) return false;
    int tx = ((buf[0] & 0x0F) << 8) | buf[1];
    int ty = ((buf[2] & 0x0F) << 8) | buf[3];
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

static bool display_begin(lv_display_t *disp) {
    /* Written out rather than CO5300_PANEL_BUS_QSPI_CONFIG(). That macro's
       designated initialisers are in the order IDF 5.3 expected. IDF 5.5
       (Arduino 3.3) puts the data lines first, and C++ refuses the reorder. */
    spi_bus_config_t bus = {};
    bus.data0_io_num = PIN_LCD_D0;
    bus.data1_io_num = PIN_LCD_D1;
    bus.sclk_io_num = PIN_LCD_SCK;
    bus.data2_io_num = PIN_LCD_D2;
    bus.data3_io_num = PIN_LCD_D3;
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

    esp_lcd_panel_io_handle_t io = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = CO5300_PANEL_IO_QSPI_CONFIG(PIN_LCD_CS, on_color_done, disp);
    err = esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST, &io_cfg, &io);
    if (err != ESP_OK) {
        Serial.printf("panel io: %d\n", (int)err);
        return false;
    }

    co5300_vendor_config_t vendor = {};
    vendor.init_cmds = k_init;
    vendor.init_cmds_size = sizeof(k_init) / sizeof(k_init[0]);
    vendor.flags.use_qspi_interface = 1;

    esp_lcd_panel_dev_config_t cfg = {};
    cfg.reset_gpio_num = PIN_LCD_RST;
    cfg.rgb_ele_order = PB_LCD_BGR ? LCD_RGB_ELEMENT_ORDER_BGR : LCD_RGB_ELEMENT_ORDER_RGB;
    cfg.bits_per_pixel = 16;
    cfg.vendor_config = &vendor;

    err = esp_lcd_new_panel_co5300(io, &cfg, &panel);
    if (err != ESP_OK) {
        Serial.printf("panel: %d\n", (int)err);
        return false;
    }
    esp_lcd_panel_reset(panel);
    err = esp_lcd_panel_init(panel);
    if (err != ESP_OK) {
        Serial.printf("panel init: %d\n", (int)err);
        return false;
    }
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
#if PB_BOARD_V11
    if (PIN_TP_INT >= 0) pinMode(PIN_TP_INT, INPUT_PULLUP);
    (void)PIN_IMU_INT;
#endif

    analogReadResolution(12);
    analogSetPinAttenuation(PIN_BAT, ADC_11db);
    sample_battery();
    next_bat_at = millis() + 20000;

    Wire.begin(PIN_SDA, PIN_SCL, 300000);
    uint8_t zero = 0x00;
    i2c_write(FT3168_ADDR, 0x00, zero);
    imu_begin();

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
