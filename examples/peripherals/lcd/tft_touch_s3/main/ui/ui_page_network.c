/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "esp_err.h"
#include "lvgl.h"
#include "app_net_state.h"
#include "app_prov.h"
#include "ui_config.h"
#include "ui_page_network.h"

static lv_obj_t *s_status_label;
static lv_obj_t *s_ssid_label;
static lv_obj_t *s_ip_label;
static lv_obj_t *s_rssi_label;
static lv_obj_t *s_error_label;

static void network_timer_cb(lv_timer_t *t)
{
    (void)t;
    app_net_state_t state = {};
    app_net_state_get(&state);

    lv_label_set_text_fmt(s_status_label, "Status: %s", app_net_state_status_text(state.status));
    lv_label_set_text_fmt(s_ssid_label, "SSID: %s", state.ssid[0] ? state.ssid : "-");
    lv_label_set_text_fmt(s_ip_label, "IP: %s", state.ip[0] ? state.ip : "-");
    if (state.status == APP_NET_STATUS_WIFI_CONNECTED) {
        lv_label_set_text_fmt(s_rssi_label, "RSSI: %d dBm", state.rssi);
    } else {
        lv_label_set_text_static(s_rssi_label, "RSSI: -");
    }

    if (state.last_error != ESP_OK) {
        lv_label_set_text_fmt(s_error_label, "Last error: %s", esp_err_to_name(state.last_error));
    } else {
        lv_label_set_text_static(s_error_label, "Last error: -");
    }
}

static void reprovision_btn_cb(lv_event_t *e)
{
    (void)e;
    app_prov_reset();
}

void ui_page_network_init(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text_static(title, "Network");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    s_status_label = lv_label_create(parent);
    lv_obj_set_width(s_status_label, UI_NET_LABEL_WIDTH);
    lv_label_set_long_mode(s_status_label, LV_LABEL_LONG_WRAP);
    lv_label_set_text_static(s_status_label, "Status: -");
    lv_obj_align(s_status_label, LV_ALIGN_TOP_LEFT, 10, 36);

    s_ssid_label = lv_label_create(parent);
    lv_obj_set_width(s_ssid_label, UI_NET_LABEL_WIDTH);
    lv_label_set_long_mode(s_ssid_label, LV_LABEL_LONG_DOT);
    lv_label_set_text_static(s_ssid_label, "SSID: -");
    lv_obj_align(s_ssid_label, LV_ALIGN_TOP_LEFT, 10, 68);

    s_ip_label = lv_label_create(parent);
    lv_label_set_text_static(s_ip_label, "IP: -");
    lv_obj_align(s_ip_label, LV_ALIGN_TOP_LEFT, 10, 96);

    s_rssi_label = lv_label_create(parent);
    lv_label_set_text_static(s_rssi_label, "RSSI: -");
    lv_obj_align(s_rssi_label, LV_ALIGN_TOP_LEFT, 10, 124);

    s_error_label = lv_label_create(parent);
    lv_obj_set_width(s_error_label, UI_NET_LABEL_WIDTH);
    lv_label_set_long_mode(s_error_label, LV_LABEL_LONG_DOT);
    lv_label_set_text_static(s_error_label, "Last error: -");
    lv_obj_align(s_error_label, LV_ALIGN_TOP_LEFT, 10, 152);

    lv_obj_t *clear_btn = lv_button_create(parent);
    lv_obj_set_size(clear_btn, UI_NET_BTN_WIDTH, UI_NET_BTN_HEIGHT);
    lv_obj_align(clear_btn, LV_ALIGN_BOTTOM_MID, 0, -14);
    lv_obj_add_event_cb(clear_btn, reprovision_btn_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *clear_label = lv_label_create(clear_btn);
    lv_label_set_text_static(clear_label, "Clear WiFi");
    lv_obj_center(clear_label);

    lv_timer_create(network_timer_cb, UI_TIMER_NET_MS, NULL);
    network_timer_cb(NULL);
}
