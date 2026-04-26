/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <sys/lock.h>
#include "freertos/FreeRTOS.h"
#include "driver/gpio.h"
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

_lock_t lvgl_api_lock;

void lcd_touch_init(lcd_touch_handles_t *out)
{
    /* Backlight off during init */
    gpio_config_t bk_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_cfg));
    gpio_set_level(CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_OFF);

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
        .flags = { .swap_xy = 0, .mirror_x = 0, .mirror_y = 1 },
    };
    ESP_LOGI(TAG, "Init XPT2046 touch");
    ESP_ERROR_CHECK(esp_lcd_touch_new_spi_xpt2046(tp_io, &tp_cfg, &out->touch));

    /* Backlight on */
    gpio_set_level(CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON);
    ESP_LOGI(TAG, "LCD and touch initialised");
}
