/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "lvgl.h"
#include "ui_page_home.h"

static lv_obj_t *s_uptime_label;

static void uptime_timer_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t seconds = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    lv_label_set_text_fmt(s_uptime_label, LV_SYMBOL_REFRESH " Uptime: %"PRIu32" s", seconds);
}

void ui_page_home_init(lv_obj_t *parent)
{
    lv_obj_t *chip_label = lv_label_create(parent);
    lv_label_set_text_static(chip_label, LV_SYMBOL_HOME " ESP32-S3\nEdge AI Lab");
    lv_obj_set_style_text_align(chip_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(chip_label, LV_ALIGN_TOP_MID, 0, 20);

    s_uptime_label = lv_label_create(parent);
    lv_label_set_text_static(s_uptime_label, LV_SYMBOL_REFRESH " Uptime: 0 s");
    lv_obj_align(s_uptime_label, LV_ALIGN_CENTER, 0, 20);

    lv_timer_create(uptime_timer_cb, 1000, NULL);
}
