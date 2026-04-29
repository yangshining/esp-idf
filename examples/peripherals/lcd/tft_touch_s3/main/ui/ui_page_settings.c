/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "lvgl.h"
#include "lcd_touch.h"
#include "settings_store.h"
#include "ui_config.h"
#include "ui_page_settings.h"

#if UI_BACKLIGHT_MIN < 0 || UI_BACKLIGHT_MIN > 100
#error "UI_BACKLIGHT_MIN must be in range [0, 100]"
#endif

static lv_display_rotation_t s_rotation = LV_DISPLAY_ROTATION_0;

static void rotate_btn_cb(lv_event_t *e)
{
    lv_display_t *disp = lv_event_get_user_data(e);
    s_rotation = (lv_display_rotation_t)((s_rotation + 1) % 4);
    lv_display_set_rotation(disp, s_rotation);
    settings_store_set_rotation((uint8_t)s_rotation);
}

static void backlight_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t val = (uint8_t)lv_slider_get_value(slider);
    lcd_touch_set_brightness(val);
    settings_store_set_brightness(val);
}

void ui_page_settings_init(lv_obj_t *parent, lv_display_t *disp)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text_static(title, LV_SYMBOL_SETTINGS " Settings");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    /* Rotate button */
    lv_obj_t *rot_btn = lv_button_create(parent);
    lv_obj_t *rot_lbl = lv_label_create(rot_btn);
    lv_label_set_text_static(rot_lbl, LV_SYMBOL_REFRESH " Rotate");
    lv_obj_center(rot_lbl);
    lv_obj_align(rot_btn, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_add_event_cb(rot_btn, rotate_btn_cb, LV_EVENT_CLICKED, disp);

    /* Backlight label + slider */
    lv_obj_t *bk_label = lv_label_create(parent);
    lv_label_set_text_static(bk_label, "Backlight");
    lv_obj_align(bk_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_width(slider, UI_SLIDER_WIDTH);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 20);
    lv_slider_set_range(slider, UI_BACKLIGHT_MIN, 100);
    lv_obj_add_event_cb(slider, backlight_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Restore saved values */
    uint8_t saved_brt = settings_store_get_brightness();
    if (saved_brt < UI_BACKLIGHT_MIN) {
        saved_brt = UI_BACKLIGHT_MIN;
    }
    lv_slider_set_value(slider, saved_brt, LV_ANIM_OFF);
    lcd_touch_set_brightness(saved_brt);

    uint8_t saved_rot = settings_store_get_rotation();
    if (saved_rot < 4) {
        s_rotation = (lv_display_rotation_t)saved_rot;
        lv_display_set_rotation(disp, s_rotation);
    }
}
