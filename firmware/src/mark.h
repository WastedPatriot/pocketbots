#pragma once

#include "generated/bots.gen.h"
#include "lvgl.h"

/* A drawn picker-mark: flat shape, two eyes, idle motion.
   Geometry matches tools/marks.py. */

struct pb_mark {
    lv_obj_t *root;
    lv_obj_t *body;
    lv_obj_t *eye_l;
    lv_obj_t *eye_r;
    lv_image_dsc_t dsc;
    uint8_t *mask;
    bool procedural;
    int size;
    float phase;
    float glance;
    float glance_target;
    bool dragging;
    uint32_t next_blink_at;
    uint32_t blink_t0;
    int blinks_left;
    float bounce;
    uint32_t idle_glance_at;
    int base_scale;
    int eye_h;
};

void mark_create(pb_mark *m, lv_obj_t *parent, const pb_bot_t *bot, int size);
void mark_step(pb_mark *m, uint32_t now, bool dimmed);
void mark_bounce(pb_mark *m);
void mark_set_drag(pb_mark *m, float glance);
void mark_clear_drag(pb_mark *m);
