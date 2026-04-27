/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <unistd.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "driver/spi_master.h"
#include "lvgl.h"
#include "lcd_touch.h"
#include "ui_main.h"
#include "nvs_flash.h"
#include "sys_stats.h"
#include "settings_store.h"
#include "app_net_state.h"
#include "app_wifi.h"
#include "app_prov.h"

static const char *TAG = "main";
#ifdef CONFIG_EXAMPLE_TOUCH_LOG
static int64_t s_last_touch_log_us;
#endif

#define LCD_H_RES               240
#define LCD_V_RES               320
#define LVGL_DRAW_BUF_LINES     20
#define LVGL_TICK_PERIOD_MS     2
#define LVGL_TASK_MIN_DELAY_MS  (1000 / CONFIG_FREERTOS_HZ)
#define LVGL_TASK_MAX_DELAY_MS  500
#define LVGL_TASK_STACK_SIZE    (6 * 1024)
#define LVGL_TASK_PRIORITY      2

static bool notify_flush_ready(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t *edata, void *ctx)
{
    lv_display_flush_ready((lv_display_t *)ctx);
    return false;
}

static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    lv_draw_sw_rgb565_swap(px_map, (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
}

static void lvgl_touch_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    esp_lcd_touch_point_data_t point = {};
    uint8_t cnt = 0;
    esp_lcd_touch_handle_t tp = lv_indev_get_user_data(indev);
    esp_lcd_touch_read_data(tp);
    esp_lcd_touch_get_data(tp, &point, &cnt, 1);
    if (cnt > 0) {
#ifdef CONFIG_EXAMPLE_TOUCH_LOG
        int64_t now_us = esp_timer_get_time();
        if (now_us - s_last_touch_log_us > 200000) {
            ESP_LOGI(TAG, "touch: x=%u y=%u z=%u", point.x, point.y, point.strength);
            s_last_touch_log_us = now_us;
        }
#endif
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_task(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "LVGL task started");
    uint32_t delay_ms = 0;
    while (1) {
        _lock_acquire(&lvgl_api_lock);
        delay_ms = lv_timer_handler();
        _lock_release(&lvgl_api_lock);
        delay_ms = MAX(delay_ms, LVGL_TASK_MIN_DELAY_MS);
        delay_ms = MIN(delay_ms, LVGL_TASK_MAX_DELAY_MS);
        usleep(1000 * delay_ms);
    }
}

void app_main(void)
{
    /* NVS must be initialised before settings_store */
    esp_err_t nvs_ret = nvs_flash_init();
    if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_ret);

    settings_store_init();
    sys_stats_init();
    app_net_state_init();
    ESP_ERROR_CHECK(app_wifi_init());
    ESP_ERROR_CHECK(app_prov_init());

    lcd_touch_handles_t hw = {};
    lcd_touch_init(&hw);

    lv_init();
    lv_display_t *disp = lv_display_create(LCD_H_RES, LCD_V_RES);

    size_t buf_sz = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);
    void *buf1 = spi_bus_dma_memory_alloc(SPI2_HOST, buf_sz, 0);
    void *buf2 = spi_bus_dma_memory_alloc(SPI2_HOST, buf_sz, 0);
    assert(buf1 && buf2);

    lv_display_set_buffers(disp, buf1, buf2, buf_sz, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(disp, hw.panel);   /* must be before xTaskCreate */
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

    /* Register flush-done callback */
    const esp_lcd_panel_io_callbacks_t flush_cbs = {
        .on_color_trans_done = notify_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(hw.io, &flush_cbs, disp));

    const esp_timer_create_args_t tick_args = { .callback = lvgl_tick_cb, .name = "lvgl_tick" };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, disp);
    lv_indev_set_user_data(indev, hw.touch);
    lv_indev_set_read_cb(indev, lvgl_touch_cb);

    BaseType_t ret = xTaskCreate(lvgl_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
    assert(ret == pdPASS);

    _lock_acquire(&lvgl_api_lock);
    ui_main_init(disp, hw.panel);
    _lock_release(&lvgl_api_lock);
}
