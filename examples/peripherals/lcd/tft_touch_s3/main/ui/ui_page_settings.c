/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "lvgl.h"
#include "app_net_state.h"
#include "app_prov.h"
#include "lcd_touch.h"
#include "settings_store.h"
#include "ui_config.h"
#include "ui_page_settings.h"

#if UI_BACKLIGHT_MIN < 0 || UI_BACKLIGHT_MIN > 100
#error "UI_BACKLIGHT_MIN must be in range [0, 100]"
#endif

static lv_display_rotation_t s_rotation = LV_DISPLAY_ROTATION_0;
static lv_obj_t *s_net_status_label;
static lv_obj_t *s_net_ssid_label;
static lv_obj_t *s_net_ip_label;

static void settings_network_timer_cb(lv_timer_t *t)
{
    (void)t;

    app_net_state_t state = {};
    app_net_state_get(&state);

    lv_label_set_text_fmt(s_net_status_label, "WiFi: %s", app_net_state_status_text(state.status));
    lv_label_set_text_fmt(s_net_ssid_label, "SSID: %s", state.ssid[0] ? state.ssid : "-");
    lv_label_set_text_fmt(s_net_ip_label, "IP: %s", state.ip[0] ? state.ip : "-");
}

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

static void clear_wifi_btn_cb(lv_event_t *e)
{
    (void)e;
    app_prov_reset();
}

void ui_page_settings_init(lv_obj_t *parent, lv_display_t *disp)
{
    /* Rotate button */
    lv_obj_t *rot_btn = lv_button_create(parent);
    lv_obj_set_size(rot_btn, UI_NET_BTN_WIDTH, UI_NET_BTN_HEIGHT);
    lv_obj_t *rot_lbl = lv_label_create(rot_btn);
    lv_label_set_text_static(rot_lbl, LV_SYMBOL_REFRESH " Rotate");
    lv_obj_center(rot_lbl);
    lv_obj_align(rot_btn, LV_ALIGN_TOP_MID, 0, 12);
    lv_obj_add_event_cb(rot_btn, rotate_btn_cb, LV_EVENT_CLICKED, disp);

    /* Backlight label + slider */
    lv_obj_t *bk_label = lv_label_create(parent);
    lv_label_set_text_static(bk_label, "Backlight");
    lv_obj_align(bk_label, LV_ALIGN_TOP_MID, 0, 56);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_width(slider, UI_SLIDER_WIDTH);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 82);
    lv_slider_set_range(slider, UI_BACKLIGHT_MIN, 100);
    lv_obj_add_event_cb(slider, backlight_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_net_status_label = lv_label_create(parent);
    lv_obj_set_width(s_net_status_label, UI_NET_LABEL_WIDTH);
    lv_label_set_long_mode(s_net_status_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_net_status_label, "WiFi: -");
    lv_obj_align(s_net_status_label, LV_ALIGN_TOP_LEFT, 10, 118);

    s_net_ssid_label = lv_label_create(parent);
    lv_obj_set_width(s_net_ssid_label, UI_NET_LABEL_WIDTH);
    lv_label_set_long_mode(s_net_ssid_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_net_ssid_label, "SSID: -");
    lv_obj_align(s_net_ssid_label, LV_ALIGN_TOP_LEFT, 10, 144);

    s_net_ip_label = lv_label_create(parent);
    lv_obj_set_width(s_net_ip_label, UI_NET_LABEL_WIDTH);
    lv_label_set_long_mode(s_net_ip_label, LV_LABEL_LONG_DOT);
    lv_label_set_text(s_net_ip_label, "IP: -");
    lv_obj_align(s_net_ip_label, LV_ALIGN_TOP_LEFT, 10, 170);

    lv_obj_t *clear_btn = lv_button_create(parent);
    lv_obj_set_size(clear_btn, UI_NET_BTN_WIDTH, UI_NET_BTN_HEIGHT);
    lv_obj_align(clear_btn, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_event_cb(clear_btn, clear_wifi_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text_static(clear_label, "Clear WiFi");
    lv_obj_center(clear_label);

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

    lv_timer_create(settings_network_timer_cb, UI_TIMER_NET_MS, NULL);
    settings_network_timer_cb(NULL);
}
