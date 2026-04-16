/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "driver/gpio.h"
#include "lvgl.h"
#include "ui_page_settings.h"
#include "lcd_touch.h"   /* for CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT */

static lv_display_rotation_t s_rotation = LV_DISPLAY_ROTATION_0;

static void rotate_btn_cb(lv_event_t *e)
{
    lv_display_t *disp = lv_event_get_user_data(e);
    s_rotation = (s_rotation + 1) % 4;
    lv_display_set_rotation(disp, s_rotation);
}

static void backlight_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t val = lv_slider_get_value(slider);
    /* Simple on/off: off when slider < 50, on when >= 50 */
    gpio_set_level(CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT, val < 50 ? 1 : 0);
}

void ui_page_settings_init(lv_obj_t *parent, lv_display_t *disp)
{
    lv_obj_t *rot_btn = lv_button_create(parent);
    lv_obj_t *rot_lbl = lv_label_create(rot_btn);
    lv_label_set_text_static(rot_lbl, LV_SYMBOL_REFRESH " 旋转屏幕");
    lv_obj_align(rot_btn, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_add_event_cb(rot_btn, rotate_btn_cb, LV_EVENT_CLICKED, disp);

    lv_obj_t *bk_label = lv_label_create(parent);
    lv_label_set_text_static(bk_label, "背光亮度");
    lv_obj_align(bk_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_width(slider, 180);
    lv_slider_set_value(slider, 100, LV_ANIM_OFF);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_event_cb(slider, backlight_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);
}
