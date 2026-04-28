/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "sys_stats.h"
#include "ui_config.h"
#include "ui_page_home.h"

static lv_obj_t *s_uptime_label;
static lv_obj_t *s_heap_chart;
static lv_obj_t *s_cpu_chart;
static lv_chart_series_t *s_heap_ser;
static lv_chart_series_t *s_cpu_ser;

/* Called from LVGL task — no lvgl_api_lock needed here.
   sys_stats_* acquire their own internal lock briefly; never hold lvgl_api_lock
   while calling them to avoid lock-order inversion. */
static void stats_timer_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t seconds = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    lv_label_set_text_fmt(s_uptime_label, LV_SYMBOL_REFRESH " %"PRIu32" s", seconds);

    lv_chart_set_next_value(s_heap_chart, s_heap_ser,
                            (int32_t)(sys_stats_heap_free() / 1024));
    lv_chart_set_next_value(s_cpu_chart,  s_cpu_ser,
                            (int32_t)sys_stats_cpu_load());
}

void ui_page_home_init(lv_obj_t *parent)
{
    /* Title */
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text_static(title, LV_SYMBOL_HOME " ESP32-S3  Edge AI Lab");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    /* Heap chart */
    lv_obj_t *heap_lbl = lv_label_create(parent);
    lv_label_set_text_static(heap_lbl, "Heap Free (KB)");
    lv_obj_align(heap_lbl, LV_ALIGN_TOP_MID, 0, 30);

    s_heap_chart = lv_chart_create(parent);
    lv_obj_set_size(s_heap_chart, UI_CHART_WIDTH, UI_CHART_HEIGHT);
    lv_obj_align(s_heap_chart, LV_ALIGN_TOP_MID, 0, 48);
    lv_chart_set_type(s_heap_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_heap_chart, UI_CHART_POINTS);
    int32_t total_kb = (int32_t)(esp_get_free_heap_size() / 1024 + 10);
    lv_chart_set_range(s_heap_chart, LV_CHART_AXIS_PRIMARY_Y, 0, total_kb);
    lv_chart_set_div_line_count(s_heap_chart, 3, 0);
    s_heap_ser = lv_chart_add_series(s_heap_chart, lv_palette_main(LV_PALETTE_GREEN),
                                     LV_CHART_AXIS_PRIMARY_Y);

    /* CPU chart */
    lv_obj_t *cpu_lbl = lv_label_create(parent);
    lv_label_set_text_static(cpu_lbl, "CPU Load (%)");
    lv_obj_align(cpu_lbl, LV_ALIGN_TOP_MID, 0, 138);

    s_cpu_chart = lv_chart_create(parent);
    lv_obj_set_size(s_cpu_chart, UI_CHART_WIDTH, UI_CHART_HEIGHT);
    lv_obj_align(s_cpu_chart, LV_ALIGN_TOP_MID, 0, 156);
    lv_chart_set_type(s_cpu_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_cpu_chart, UI_CHART_POINTS);
    lv_chart_set_range(s_cpu_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(s_cpu_chart, 3, 0);
    s_cpu_ser = lv_chart_add_series(s_cpu_chart, lv_palette_main(LV_PALETTE_RED),
                                    LV_CHART_AXIS_PRIMARY_Y);

    /* Uptime label at bottom */
    s_uptime_label = lv_label_create(parent);
    lv_label_set_text_static(s_uptime_label, LV_SYMBOL_REFRESH " 0 s");
    lv_obj_align(s_uptime_label, LV_ALIGN_BOTTOM_MID, 0, -6);

    lv_timer_create(stats_timer_cb, UI_TIMER_HOME_MS, NULL);
}
