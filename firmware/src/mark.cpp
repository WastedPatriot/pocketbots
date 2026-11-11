#include "mark.h"
#include "port.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

/* Flat picker marks. The distances below are copied from tools/marks.py
   (y-up, unit box). If you nudge a radius, nudge it there too, or the
   PNG path and the on-device path will drift apart. */

static float clampf(float v, float lo, float hi) {
    return v < lo ? lo : v > hi ? hi : v;
}

static float sd_circle(float x, float y, float r) {
    return sqrtf(x * x + y * y) - r;
}

static float sd_box(float x, float y, float hx, float hy) {
    float dx = fabsf(x) - hx;
    float dy = fabsf(y) - hy;
    float ax = dx > 0 ? dx : 0.f;
    float ay = dy > 0 ? dy : 0.f;
    float inside = dx > dy ? dx : dy;
    if (inside > 0) inside = 0.f;
    return sqrtf(ax * ax + ay * ay) + inside;
}

static float sd_roundbox(float x, float y, float hx, float hy, float r) {
    return sd_box(x, y, hx - r, hy - r) - r;
}

static float smin(float a, float b, float k) {
    float h = clampf(0.5f + 0.5f * (b - a) / k, 0.f, 1.f);
    return (b * (1.f - h) + a * h) - k * h * (1.f - h);
}

static float sd_poly(float x, float y, const float *px, const float *py, int n) {
    float d = (x - px[0]) * (x - px[0]) + (y - py[0]) * (y - py[0]);
    float s = 1.f;
    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        float ex = px[j] - px[i];
        float ey = py[j] - py[i];
        float wx = x - px[i];
        float wy = y - py[i];
        float denom = ex * ex + ey * ey;
        float t = denom == 0.f ? 0.f : clampf((wx * ex + wy * ey) / denom, 0.f, 1.f);
        float bx = wx - ex * t;
        float by = wy - ey * t;
        float bd = bx * bx + by * by;
        if (bd < d) d = bd;
        bool c1 = y >= py[i];
        bool c2 = y < py[j];
        bool c3 = ex * wy > ey * wx;
        if ((c1 && c2 && c3) || (!c1 && !c2 && !c3)) s = -s;
    }
    return s * sqrtf(d);
}

static float sd_round_poly(float x, float y, const float *px, const float *py, int n, float rad) {
    float cx = 0.f, cy = 0.f;
    for (int i = 0; i < n; i++) {
        cx += px[i];
        cy += py[i];
    }
    cx /= (float)n;
    cy /= (float)n;
    float sx[8], sy[8];
    for (int i = 0; i < n; i++) {
        float vx = px[i] - cx;
        float vy = py[i] - cy;
        float len = sqrtf(vx * vx + vy * vy);
        if (len < 0.0001f) len = 1.f;
        sx[i] = px[i] - vx / len * rad;
        sy[i] = py[i] - vy / len * rad;
    }
    return sd_poly(x, y, sx, sy, n) - rad;
}

static void ngon(int n, float r, float rot, float *px, float *py) {
    for (int i = 0; i < n; i++) {
        float a = rot + i * 2.f * (float)M_PI / (float)n;
        px[i] = cosf(a) * r;
        py[i] = sinf(a) * r;
    }
}

static float shape_sdf(pb_shape_t shape, float x, float y) {
    float px[8], py[8];
    switch (shape) {
    case PB_SHAPE_CIRCLE:
        return sd_circle(x, y, 0.78f);
    case PB_SHAPE_SQUIRCLE:
        return sd_roundbox(x, y, 0.74f, 0.74f, 0.30f);
    case PB_SHAPE_PILL:
        return sd_roundbox(x, y, 0.86f, 0.50f, 0.50f);
    case PB_SHAPE_PLUS: {
        float v = sd_roundbox(x, y, 0.30f, 0.78f, 0.30f);
        float h = sd_roundbox(x, y, 0.78f, 0.30f, 0.30f);
        return smin(v, h, 0.08f);
    }
    case PB_SHAPE_HEXAGON:
        ngon(6, 0.88f, (float)M_PI / 2.f, px, py);
        return sd_round_poly(x, y, px, py, 6, 0.26f);
    case PB_SHAPE_TRIANGLE:
        ngon(3, 0.92f, (float)M_PI / 2.f, px, py);
        return sd_round_poly(x, y, px, py, 3, 0.22f);
    case PB_SHAPE_HOUSE: {
        const float hx[5] = {0.f, 0.72f, 0.72f, -0.72f, -0.72f};
        const float hy[5] = {0.86f, 0.18f, -0.78f, -0.78f, 0.18f};
        return sd_round_poly(x, y, hx, hy, 5, 0.16f);
    }
    case PB_SHAPE_STAR:
        for (int i = 0; i < 8; i++) {
            float a = (float)M_PI / 2.f + i * (float)M_PI / 4.f;
            float rad = (i % 2 == 0) ? 0.86f : 0.40f;
            px[i] = cosf(a) * rad;
            py[i] = sinf(a) * rad;
        }
        return sd_round_poly(x, y, px, py, 8, 0.12f);
    case PB_SHAPE_PEANUT:
        return smin(sd_circle(x - 0.30f, y, 0.50f), sd_circle(x + 0.30f, y, 0.50f), 0.28f);
    case PB_SHAPE_CLOUD: {
        float d = sd_roundbox(x, y - 0.10f, 0.62f, 0.36f, 0.32f);
        d = smin(d, sd_circle(x - 0.30f, y + 0.10f, 0.34f), 0.14f);
        d = smin(d, sd_circle(x + 0.26f, y + 0.14f, 0.38f), 0.14f);
        d = smin(d, sd_circle(x - 0.02f, y + 0.30f, 0.28f), 0.12f);
        return d;
    }
    case PB_SHAPE_BLOB: {
        float d = sd_circle(x - 0.06f, y + 0.08f, 0.50f);
        d = smin(d, sd_circle(x + 0.30f, y - 0.10f, 0.42f), 0.30f);
        d = smin(d, sd_circle(x - 0.32f, y - 0.16f, 0.38f), 0.28f);
        d = smin(d, sd_circle(x + 0.02f, y + 0.36f, 0.30f), 0.22f);
        return d;
    }
    case PB_SHAPE_FLOWER: {
        float d = sd_circle(x, y, 0.34f);
        for (int i = 0; i < 6; i++) {
            float a = (float)M_PI / 2.f + i * (float)M_PI / 3.f;
            d = smin(d, sd_circle(x + cosf(a) * 0.40f, y + sinf(a) * 0.40f, 0.32f), 0.10f);
        }
        return d;
    }
    case PB_SHAPE_DROP: {
        float body = sd_circle(x, y - 0.16f, 0.52f);
        const float tx[3] = {0.f, 0.46f, -0.46f};
        const float ty[3] = {0.88f, -0.02f, -0.02f};
        return smin(body, sd_poly(x, y, tx, ty, 3), 0.20f);
    }
    default:
        return sd_circle(x, y, 0.78f);
    }
}

/* LVGL ARGB8888 on little-endian is stored B, G, R, A. */
static void render_mask(uint8_t *px, int size, pb_shape_t shape, uint32_t color) {
    uint8_t cr = (color >> 16) & 255;
    uint8_t cg = (color >> 8) & 255;
    uint8_t cb = color & 255;
    float step = 2.f / (float)size;
    for (int py = 0; py < size; py++) {
        float y = 1.f - (py + 0.5f) * step;
        uint8_t *row = px + (size_t)py * (size_t)size * 4;
        for (int xi = 0; xi < size; xi++) {
            float x = -1.f + (xi + 0.5f) * step;
            float d = shape_sdf(shape, x, y);
            float cover = clampf(0.5f - d / step, 0.f, 1.f);
            uint8_t *p = row + xi * 4;
            p[0] = cb;
            p[1] = cg;
            p[2] = cr;
            p[3] = (uint8_t)(cover * 255.f + 0.5f);
        }
    }
}

static void on_delete(lv_event_t *e) {
    pb_mark *m = (pb_mark *)lv_event_get_user_data(e);
    if (m->mask) {
        /* port_alloc has no matching free wrapper; the libc free matches
           both malloc and heap_caps_malloc. */
        free(m->mask);
        m->mask = NULL;
    }
}

void mark_create(pb_mark *m, lv_obj_t *parent, const pb_bot_t *bot, int size) {
    memset(m, 0, sizeof(*m));
    m->size = size;
    m->phase = (float)(port_rand() % 2000);
    m->next_blink_at = port_millis() + 600u + (uint32_t)(port_rand() % 900);
    m->idle_glance_at = port_millis() + 800u;

    m->root = lv_obj_create(parent);
    lv_obj_set_size(m->root, size, size);
    lv_obj_remove_flag(m->root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(m->root, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(m->root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(m->root, 0, 0);
    lv_obj_set_style_pad_all(m->root, 0, 0);
    lv_obj_set_style_transform_pivot_x(m->root, size / 2, 0);
    lv_obj_set_style_transform_pivot_y(m->root, size / 2, 0);
    lv_obj_add_event_cb(m->root, on_delete, LV_EVENT_DELETE, m);

    m->body = lv_image_create(m->root);
    if (bot->image) {
        lv_image_set_src(m->body, bot->image);
        m->procedural = false;
    } else {
        size_t bytes = (size_t)size * (size_t)size * 4;
        m->mask = (uint8_t *)port_alloc(bytes);
        if (m->mask) {
            render_mask(m->mask, size, bot->shape, bot->color);
            memset(&m->dsc, 0, sizeof(m->dsc));
            m->dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
            m->dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
            m->dsc.header.w = size;
            m->dsc.header.h = size;
            m->dsc.header.stride = size * 4;
            m->dsc.data_size = (uint32_t)bytes;
            m->dsc.data = m->mask;
            lv_image_set_src(m->body, &m->dsc);
        }
        m->procedural = true;
    }
    lv_obj_center(m->body);
    lv_image_set_pivot(m->body, size / 2, size / 2);
    m->base_scale = 256;
    /* A dropped-in PNG is whatever size it is. Scale it into the box. */
    if (bot->image) {
        int iw = bot->image->header.w;
        int ih = bot->image->header.h;
        if (iw > 0 && ih > 0) {
            m->base_scale = (size * 256) / (iw > ih ? iw : ih);
            lv_image_set_scale(m->body, (uint32_t)m->base_scale);
        }
    }

    if (m->procedural) {
        int ew = size * 115 / 1000;
        int eh = size * 205 / 1000;
        if (ew < 4) ew = 4;
        if (eh < 6) eh = 6;
        int spread = size * 105 / 1000;
        int yoff = (size * 455 / 1000) - (size / 2);
        lv_color_t eye = lv_color_hex(0x16161A);
        for (int s = -1; s <= 1; s += 2) {
            lv_obj_t *eye_o = lv_obj_create(m->root);
            lv_obj_set_size(eye_o, ew, eh);
            lv_obj_remove_flag(eye_o, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_remove_flag(eye_o, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_bg_color(eye_o, eye, 0);
            lv_obj_set_style_bg_opa(eye_o, LV_OPA_COVER, 0);
            lv_obj_set_style_border_width(eye_o, 0, 0);
            lv_obj_set_style_radius(eye_o, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_pad_all(eye_o, 0, 0);
            lv_obj_align(eye_o, LV_ALIGN_CENTER, s * spread, yoff);
            m->eye_h = eh;
            if (s < 0) m->eye_l = eye_o;
            else m->eye_r = eye_o;
        }
    }
}

void mark_bounce(pb_mark *m) {
    if (m) m->bounce = 1.f;
}

void mark_set_drag(pb_mark *m, float glance) {
    if (!m) return;
    m->dragging = true;
    m->glance_target = glance;
}

void mark_clear_drag(pb_mark *m) {
    if (m) m->dragging = false;
}

void mark_step(pb_mark *m, uint32_t now, bool dimmed) {
    if (!m || !m->root) return;
    float tempo = dimmed ? 0.45f : 1.f;
    float t = (now + m->phase) / 1000.f * tempo;
    float bob = sinf(t * 2.6f);
    /* Up on the rise, squashed on the landing. Volume stays put. */
    float sy = 1.f + bob * 0.05f;
    float sx = 1.f / sy;
    float pop = 0.f;
    if (m->bounce > 0.f) {
        pop = sinf(m->bounce * (float)M_PI);
        m->bounce -= dimmed ? 0.05f : 0.09f;
        if (m->bounce < 0.f) m->bounce = 0.f;
    }
    /* Scale the image itself. An object-level transform on the ARGB buffer
       makes LVGL build a layer and, on this version, never come back. */
    int sx_i = (int)((float)m->base_scale * sx * (1.f + pop * 0.04f));
    int sy_i = (int)((float)m->base_scale * sy * (1.f + pop * 0.10f));
    if (sx_i < 32) sx_i = 32;
    if (sy_i < 32) sy_i = 32;
    lv_image_set_scale_x(m->body, (uint32_t)sx_i);
    lv_image_set_scale_y(m->body, (uint32_t)sy_i);
    lv_image_set_rotation(m->body, (int32_t)(sinf(t * 1.7f) * 18.f));
    lv_obj_set_style_translate_y(m->root, (int)(bob * (dimmed ? 3.f : 6.f)), 0);

    if (!m->dragging && now > m->idle_glance_at) {
        int r = port_rand() % 5;
        m->glance_target = r == 0 ? -0.9f : r == 1 ? 0.9f : 0.f;
        m->idle_glance_at = now + (dimmed ? 1800u : 900u) + (uint32_t)(port_rand() % 1600);
    }
    m->glance += (m->glance_target - m->glance) * 0.30f;

    if (!m->eye_l) return;
    int gx = (int)(m->glance * (m->size * 0.055f));
    lv_obj_set_style_translate_x(m->eye_l, gx, 0);
    lv_obj_set_style_translate_x(m->eye_r, gx, 0);

    float bh = 1.f;
    if (m->blinks_left > 0) {
        uint32_t dt = now - m->blink_t0;
        if (dt < 80) bh = 1.f - (dt / 80.f) * 0.88f;
        else if (dt < 140) bh = 0.12f;
        else if (dt < 230) bh = 0.12f + ((dt - 140) / 90.f) * 0.88f;
        else {
            m->blinks_left--;
            m->blink_t0 = now;
            if (m->blinks_left == 0) m->next_blink_at = now + 2200u + (uint32_t)(port_rand() % 2800);
        }
    } else if (now > m->next_blink_at) {
        m->blinks_left = (port_rand() % 7 == 0) ? 2 : 1;
        m->blink_t0 = now;
    }
    int eh = (int)((float)m->eye_h * bh);
    if (eh < 2) eh = 2;
    int spread = m->size * 105 / 1000;
    int yoff = (m->size * 455 / 1000) - (m->size / 2);
    lv_obj_set_height(m->eye_l, eh);
    lv_obj_set_height(m->eye_r, eh);
    lv_obj_align(m->eye_l, LV_ALIGN_CENTER, -spread, yoff);
    lv_obj_align(m->eye_r, LV_ALIGN_CENTER, spread, yoff);
}
