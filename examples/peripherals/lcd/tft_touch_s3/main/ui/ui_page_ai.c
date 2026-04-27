/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include "esp_random.h"
#include "lvgl.h"
#include "ui_page_ai.h"

static lv_obj_t *s_result_label;
static lv_obj_t *s_conf_bar;
static lv_obj_t *s_conf_label;
static lv_obj_t *s_run_btn;

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
    lv_bar_set_value(s_conf_bar, 0, LV_ANIM_OFF);
    lv_label_set_text_static(s_conf_label, "Confidence: -");
    lv_timer_create(inference_done_cb, 1500, NULL);
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
    lv_obj_set_size(s_conf_bar, 180, 20);
    lv_bar_set_range(s_conf_bar, 0, 100);
    lv_bar_set_value(s_conf_bar, 0, LV_ANIM_OFF);
    lv_obj_align(s_conf_bar, LV_ALIGN_CENTER, 0, 0);

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
    int32_t pct = (int32_t)(confidence * 100.0f);
    lv_bar_set_value(s_conf_bar, pct, LV_ANIM_ON);
    lv_label_set_text_fmt(s_conf_label, "Confidence: %"PRId32"%%", pct);
}
