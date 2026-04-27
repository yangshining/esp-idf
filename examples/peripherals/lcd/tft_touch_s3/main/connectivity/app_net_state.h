/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_NET_STATUS_IDLE,
    APP_NET_STATUS_PROV_WAITING,
    APP_NET_STATUS_PROV_RECEIVING,
    APP_NET_STATUS_PROV_DONE,
    APP_NET_STATUS_WIFI_CONNECTING,
    APP_NET_STATUS_WIFI_CONNECTED,
    APP_NET_STATUS_WIFI_FAILED,
} app_net_status_t;

typedef struct {
    app_net_status_t status;
    char ssid[33];
    char ip[16];
    int8_t rssi;
    esp_err_t last_error;
    bool provisioned;
} app_net_state_t;

void app_net_state_init(void);
void app_net_state_get(app_net_state_t *out_state);
void app_net_state_set_status(app_net_status_t status);
void app_net_state_set_provisioned(bool provisioned);
void app_net_state_set_wifi_connected(const char *ssid, const char *ip, int8_t rssi);
void app_net_state_set_wifi_failed(esp_err_t err);
const char *app_net_state_status_text(app_net_status_t status);

#ifdef __cplusplus
}
#endif
