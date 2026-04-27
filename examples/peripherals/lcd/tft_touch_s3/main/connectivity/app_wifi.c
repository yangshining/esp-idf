/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/inet.h"
#include "app_net_state.h"
#include "app_wifi.h"

static const char *TAG = "app_wifi";

static esp_netif_t *s_sta_netif;
static bool s_wifi_initialized;
static int s_retry_count;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    (void)event_data;

    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            s_retry_count = 0;
            app_net_state_set_status(APP_NET_STATUS_WIFI_CONNECTING);
            esp_wifi_connect();
            break;
        case WIFI_EVENT_STA_DISCONNECTED:
            if (s_retry_count < CONFIG_EXAMPLE_WIFI_MAX_RETRY) {
                s_retry_count++;
                app_net_state_set_status(APP_NET_STATUS_WIFI_CONNECTING);
                esp_wifi_connect();
            } else {
                ESP_LOGW(TAG, "WiFi connection failed after %d retries", s_retry_count);
                app_net_state_set_wifi_failed(ESP_ERR_WIFI_CONN);
            }
            break;
        default:
            break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        wifi_ap_record_t ap_info = {};
        char ssid[33] = "";
        int8_t rssi = 0;
        char ip[16] = "";

        esp_ip4addr_ntoa(&event->ip_info.ip, ip, sizeof(ip));
        if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
            memcpy(ssid, ap_info.ssid, sizeof(ap_info.ssid));
            ssid[sizeof(ssid) - 1] = '\0';
            rssi = ap_info.rssi;
        }
        s_retry_count = 0;
        ESP_LOGI(TAG, "Connected to %s, IP %s, RSSI %d", ssid, ip, rssi);
        app_net_state_set_wifi_connected(ssid, ip, rssi);
    }
}

esp_err_t app_wifi_init(void)
{
    if (s_wifi_initialized) {
        return ESP_OK;
    }

    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "esp_wifi_init failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL),
                        TAG, "register WiFi event handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL),
                        TAG, "register IP event handler failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set WiFi STA mode failed");

    s_wifi_initialized = true;
    return ESP_OK;
}

esp_err_t app_wifi_start(void)
{
    if (!s_wifi_initialized) {
        ESP_RETURN_ON_ERROR(app_wifi_init(), TAG, "app_wifi_init failed");
    }
    app_net_state_set_status(APP_NET_STATUS_WIFI_CONNECTING);
    return esp_wifi_start();
}

esp_err_t app_wifi_clear_credentials(void)
{
    if (!s_wifi_initialized) {
        ESP_RETURN_ON_ERROR(app_wifi_init(), TAG, "app_wifi_init failed");
    }
    return esp_wifi_restore();
}
