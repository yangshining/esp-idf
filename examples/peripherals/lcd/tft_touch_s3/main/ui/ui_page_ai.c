/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include "lvgl.h"
#include "ui_page_ai.h"

static lv_obj_t *s_result_label;
static lv_obj_t *s_conf_bar;
static lv_obj_t *s_conf_label;

void ui_page_ai_init(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text_static(title, LV_SYMBOL_IMAGE " AI 推理结果");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    s_result_label = lv_label_create(parent);
    lv_label_set_text_static(s_result_label, "-- 等待推理 --");
    lv_obj_set_style_text_font(s_result_label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_result_label, LV_ALIGN_CENTER, 0, -30);

    s_conf_bar = lv_bar_create(parent);
    lv_obj_set_size(s_conf_bar, 180, 20);
    lv_bar_set_range(s_conf_bar, 0, 100);
    lv_bar_set_value(s_conf_bar, 0, LV_ANIM_OFF);
    lv_obj_align(s_conf_bar, LV_ALIGN_CENTER, 0, 20);

    s_conf_label = lv_label_create(parent);
    lv_label_set_text_static(s_conf_label, "置信度: 0%");
    lv_obj_align(s_conf_label, LV_ALIGN_CENTER, 0, 50);
}

void ui_ai_update_result(const char *label, float confidence)
{
    lv_label_set_text(s_result_label, label);
    int32_t pct = (int32_t)(confidence * 100.0f);
    lv_bar_set_value(s_conf_bar, pct, LV_ANIM_ON);
    lv_label_set_text_fmt(s_conf_label, "置信度: %"PRId32"%%", pct);
}
