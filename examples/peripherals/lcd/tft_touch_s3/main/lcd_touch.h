/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <sys/lock.h>
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Shared LVGL API mutex — include this header and use _lock_acquire/release */
extern _lock_t lvgl_api_lock;

typedef struct {
    esp_lcd_panel_handle_t    panel;
    esp_lcd_touch_handle_t    touch;
    esp_lcd_panel_io_handle_t io;    /* used by main.c to register flush callback */
} lcd_touch_handles_t;

/**
 * @brief Initialize SPI bus, ST7789 panel, and XPT2046 touch controller.
 *        Also initialises lvgl_api_lock.
 * @param[out] out  Populated with panel and touch handles on success.
 */
void lcd_touch_init(lcd_touch_handles_t *out);

/**
 * @brief Set backlight brightness via LEDC PWM.
 * @param pct  Brightness percentage 0-100.
 */
void lcd_touch_set_brightness(uint8_t pct);

#ifdef __cplusplus
}
#endif
