/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include <math.h>
#include "esp_random.h"
#include "lvgl.h"
#include "ui_config.h"
#include "ui_page_ai.h"

static lv_obj_t *s_result_label;
static lv_obj_t *s_conf_bar;
static lv_obj_t *s_conf_label;
static lv_obj_t *s_run_btn;
static lv_timer_t *s_bar_timer;

/* Exponential-decay bar interpolation (inspired by Astra UI) */
static float s_bar_cur = 0.0f;
static float s_bar_tgt = 0.0f;

static int32_t s_bar_displayed = -1;

static void bar_anim_cb(lv_timer_t *t)
{
    s_bar_cur += (s_bar_tgt - s_bar_cur) / UI_BAR_ANIM_SPEED;
    if (fabsf(s_bar_tgt - s_bar_cur) < 0.5f) {
        s_bar_cur = s_bar_tgt;
        lv_timer_pause(t);
    }
    int32_t v = (int32_t)s_bar_cur;
    if (v != s_bar_displayed) {
        lv_bar_set_value(s_conf_bar, v, LV_ANIM_OFF);
        lv_label_set_text_fmt(s_conf_label, "Confidence: %"PRId32"%%", v);
        s_bar_displayed = v;
    }
}

static const char *s_labels[] = {
    "Cat", "Dog", "Bird", "Car", "Person", "Plant", "Unknown"
};
#define LABEL_COUNT (sizeof(s_labels) / sizeof(s_labels[0]))

static void inference_done_cb(lv_timer_t *t)
{
    uint32_t idx = esp_random() % LABEL_COUNT;
    /* confidence in range 60-99 */
    uint32_t pct = 60 + (esp_random() % 40);
    float confidence = (float)pct / 100.0f;

    ui_ai_update_result(s_labels[idx], confidence);
    lv_obj_remove_state(s_run_btn, LV_STATE_DISABLED);
    lv_timer_delete(t);
}

static void run_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_add_state(s_run_btn, LV_STATE_DISABLED);
    lv_label_set_text_static(s_result_label, "Running...");
    s_bar_cur = 0.0f;
    s_bar_tgt = 0.0f;
    lv_timer_pause(s_bar_timer);
    lv_bar_set_value(s_conf_bar, 0, LV_ANIM_OFF);
    lv_label_set_text_static(s_conf_label, "Confidence: -");
    lv_timer_create(inference_done_cb, UI_INFERENCE_DELAY_MS, NULL);
}

void ui_page_ai_init(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text_static(title, LV_SYMBOL_IMAGE " AI Result");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    s_result_label = lv_label_create(parent);
    lv_label_set_text_static(s_result_label, "-- Waiting --");
    lv_obj_set_style_text_font(s_result_label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_result_label, LV_ALIGN_CENTER, 0, -40);

    s_conf_bar = lv_bar_create(parent);
    lv_obj_set_size(s_conf_bar, UI_BAR_WIDTH, UI_BAR_HEIGHT);
    lv_bar_set_range(s_conf_bar, 0, 100);
    lv_bar_set_value(s_conf_bar, 0, LV_ANIM_OFF);
    lv_obj_align(s_conf_bar, LV_ALIGN_CENTER, 0, 0);

    s_bar_timer = lv_timer_create(bar_anim_cb, UI_BAR_ANIM_PERIOD_MS, NULL);
    lv_timer_pause(s_bar_timer);

    s_conf_label = lv_label_create(parent);
    lv_label_set_text_static(s_conf_label, "Confidence: 0%");
    lv_obj_align(s_conf_label, LV_ALIGN_CENTER, 0, 28);

    s_run_btn = lv_button_create(parent);
    lv_obj_t *btn_lbl = lv_label_create(s_run_btn);
    lv_label_set_text_static(btn_lbl, LV_SYMBOL_PLAY " Run Inference");
    lv_obj_center(btn_lbl);
    lv_obj_align(s_run_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(s_run_btn, run_btn_cb, LV_EVENT_CLICKED, NULL);
}

void ui_ai_update_result(const char *label, float confidence)
{
    lv_label_set_text(s_result_label, label);
    s_bar_tgt = confidence * 100.0f;
    lv_timer_resume(s_bar_timer);
}
