/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "app_net_state.h"

static SemaphoreHandle_t s_state_lock;
static app_net_state_t s_state = {
    .status = APP_NET_STATUS_IDLE,
    .ssid = "",
    .ip = "",
    .rssi = 0,
    .last_error = ESP_OK,
    .provisioned = false,
};

static void copy_str(char *dst, size_t dst_size, const char *src)
{
    if (!dst || dst_size == 0) {
        return;
    }
    if (!src) {
        src = "";
    }
    strlcpy(dst, src, dst_size);
}

void app_net_state_init(void)
{
    if (!s_state_lock) {
        s_state_lock = xSemaphoreCreateMutex();
    }
}

void app_net_state_get(app_net_state_t *out_state)
{
    if (!out_state) {
        return;
    }
    if (s_state_lock) {
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
    }
    *out_state = s_state;
    if (s_state_lock) {
        xSemaphoreGive(s_state_lock);
    }
}

void app_net_state_set_status(app_net_status_t status)
{
    if (s_state_lock) {
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
    }
    s_state.status = status;
    if (s_state_lock) {
        xSemaphoreGive(s_state_lock);
    }
}

void app_net_state_set_provisioned(bool provisioned)
{
    if (s_state_lock) {
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
    }
    s_state.provisioned = provisioned;
    if (s_state_lock) {
        xSemaphoreGive(s_state_lock);
    }
}

void app_net_state_set_wifi_connected(const char *ssid, const char *ip, int8_t rssi)
{
    if (s_state_lock) {
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
    }
    s_state.status = APP_NET_STATUS_WIFI_CONNECTED;
    copy_str(s_state.ssid, sizeof(s_state.ssid), ssid);
    copy_str(s_state.ip, sizeof(s_state.ip), ip);
    s_state.rssi = rssi;
    s_state.last_error = ESP_OK;
    s_state.provisioned = true;
    if (s_state_lock) {
        xSemaphoreGive(s_state_lock);
    }
}

void app_net_state_set_wifi_failed(esp_err_t err)
{
    if (s_state_lock) {
        xSemaphoreTake(s_state_lock, portMAX_DELAY);
    }
    s_state.status = APP_NET_STATUS_WIFI_FAILED;
    s_state.ip[0] = '\0';
    s_state.rssi = 0;
    s_state.last_error = err;
    if (s_state_lock) {
        xSemaphoreGive(s_state_lock);
    }
}

const char *app_net_state_status_text(app_net_status_t status)
{
    switch (status) {
    case APP_NET_STATUS_IDLE:
        return "Idle";
    case APP_NET_STATUS_PROV_WAITING:
        return "Waiting for BLE provisioning";
    case APP_NET_STATUS_PROV_RECEIVING:
        return "Receiving WiFi credentials";
    case APP_NET_STATUS_PROV_DONE:
        return "Provisioning complete";
    case APP_NET_STATUS_WIFI_CONNECTING:
        return "Connecting to WiFi";
    case APP_NET_STATUS_WIFI_CONNECTED:
        return "Connected";
    case APP_NET_STATUS_WIFI_FAILED:
        return "Connection failed";
    default:
        return "Unknown";
    }
}
