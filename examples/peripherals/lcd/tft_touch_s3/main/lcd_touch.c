/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <sys/lock.h>
#include "freertos/FreeRTOS.h"
#include "driver/ledc.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_touch_xpt2046.h"
#include "lcd_touch.h"

static const char *TAG = "lcd_touch";

#define LCD_HOST            SPI2_HOST
#define LCD_H_RES           240
#define LCD_V_RES           320
#define LCD_CMD_BITS        8
#define LCD_PARAM_BITS      8
#define LCD_BK_LIGHT_ON     CONFIG_EXAMPLE_BK_LIGHT_ON_LEVEL
#define LCD_BK_LIGHT_OFF    (1 - CONFIG_EXAMPLE_BK_LIGHT_ON_LEVEL)

#ifdef CONFIG_EXAMPLE_TOUCH_SWAP_XY
#define EXAMPLE_TOUCH_SWAP_XY 1
#else
#define EXAMPLE_TOUCH_SWAP_XY 0
#endif

#ifdef CONFIG_EXAMPLE_TOUCH_MIRROR_X
#define EXAMPLE_TOUCH_MIRROR_X 1
#else
#define EXAMPLE_TOUCH_MIRROR_X 0
#endif

#ifdef CONFIG_EXAMPLE_TOUCH_MIRROR_Y
#define EXAMPLE_TOUCH_MIRROR_Y 1
#else
#define EXAMPLE_TOUCH_MIRROR_Y 0
#endif

_lock_t lvgl_api_lock;

void lcd_touch_init(lcd_touch_handles_t *out)
{
    _lock_init(&lvgl_api_lock);
    /* Backlight — LEDC PWM init (starts at 0% / off) */
    ledc_timer_config_t ledc_timer = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = LEDC_TIMER_0,
        .duty_resolution = LEDC_TIMER_13_BIT,
        .freq_hz         = 5000,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_ch = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = LEDC_CHANNEL_0,
        .timer_sel  = LEDC_TIMER_0,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_ch));
    /* backlight starts off — duty is already 0 from ledc_channel_config */

    /* SPI bus */
    spi_bus_config_t buscfg = {
        .sclk_io_num   = CONFIG_EXAMPLE_PIN_NUM_SCLK,
        .mosi_io_num   = CONFIG_EXAMPLE_PIN_NUM_MOSI,
        .miso_io_num   = CONFIG_EXAMPLE_PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * 80 * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    /* LCD panel IO */
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = CONFIG_EXAMPLE_PIN_NUM_LCD_DC,
        .cs_gpio_num       = CONFIG_EXAMPLE_PIN_NUM_LCD_CS,
        .pclk_hz           = CONFIG_EXAMPLE_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits      = LCD_CMD_BITS,
        .lcd_param_bits    = LCD_PARAM_BITS,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(LCD_HOST, &io_cfg, &io_handle));
    out->io = io_handle;

    /* ST7789 panel */
    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num  = CONFIG_EXAMPLE_PIN_NUM_LCD_RST,
        .rgb_ele_order   = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel  = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_cfg, &out->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(out->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(out->panel));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(out->panel, true, false));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(out->panel, true));

    /* XPT2046 touch */
    esp_lcd_panel_io_handle_t tp_io = NULL;
    esp_lcd_panel_io_spi_config_t tp_io_cfg = ESP_LCD_TOUCH_IO_SPI_XPT2046_CONFIG(CONFIG_EXAMPLE_PIN_NUM_TOUCH_CS);
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &tp_io_cfg, &tp_io));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .flags = {
            .swap_xy = EXAMPLE_TOUCH_SWAP_XY,
            .mirror_x = EXAMPLE_TOUCH_MIRROR_X,
            .mirror_y = EXAMPLE_TOUCH_MIRROR_Y,
        },
    };
    ESP_LOGI(TAG, "Init XPT2046 touch");
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tp_io, &tp_cfg, &out->touch));

    /* Backlight on */
    lcd_touch_set_brightness(100);
    ESP_LOGI(TAG, "LCD and touch initialised");
}

void lcd_touch_set_brightness(uint8_t pct)
{
    if (pct > 100) pct = 100;
#if CONFIG_EXAMPLE_BK_LIGHT_ON_LEVEL == 0
    uint32_t duty = (uint32_t)(100 - pct) * 8191 / 100;
#else
    uint32_t duty = (uint32_t)pct * 8191 / 100;
#endif
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
