/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_ble.h"
#include "protocomm_security.h"
#include "protocomm_ble.h"
#include "app_net_state.h"
#include "app_prov.h"
#include "app_wifi.h"

static const char *TAG = "app_prov";

static bool s_prov_initialized;
static bool s_provisioned;

static void get_service_name(char *service_name, size_t max_len)
{
    uint8_t mac[6] = {};
    const char *prefix = CONFIG_EXAMPLE_PROV_SERVICE_NAME;

    esp_wifi_get_mac(WIFI_IF_STA, mac);
    snprintf(service_name, max_len, "%s-%02X%02X%02X", prefix, mac[3], mac[4], mac[5]);
}

static void prov_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;

    if (event_base == NETWORK_PROV_EVENT) {
        switch (event_id) {
        case NETWORK_PROV_START:
            ESP_LOGI(TAG, "BLE provisioning started");
            app_net_state_set_status(APP_NET_STATUS_PROV_WAITING);
            break;
        case NETWORK_PROV_WIFI_CRED_RECV: {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            ESP_LOGI(TAG, "Received WiFi credentials for SSID %s", (const char *)wifi_sta_cfg->ssid);
            app_net_state_set_status(APP_NET_STATUS_PROV_RECEIVING);
            break;
        }
        case NETWORK_PROV_WIFI_CRED_FAIL: {
            network_prov_wifi_sta_fail_reason_t *reason = (network_prov_wifi_sta_fail_reason_t *)event_data;
            ESP_LOGW(TAG, "Provisioning WiFi connect failed, reason %d", *reason);
            app_net_state_set_wifi_failed(ESP_FAIL);
            network_prov_mgr_reset_wifi_sm_state_on_failure();
            break;
        }
        case NETWORK_PROV_WIFI_CRED_SUCCESS:
            ESP_LOGI(TAG, "Provisioning credentials accepted");
            s_provisioned = true;
            app_net_state_set_provisioned(true);
            app_net_state_set_status(APP_NET_STATUS_PROV_DONE);
            break;
        case NETWORK_PROV_END:
            ESP_LOGI(TAG, "Provisioning ended");
            network_prov_mgr_deinit();
            s_prov_initialized = false;
            break;
        default:
            break;
        }
    } else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT) {
        switch (event_id) {
        case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
            ESP_LOGI(TAG, "Provisioning phone connected over BLE");
            break;
        case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
            ESP_LOGI(TAG, "Provisioning phone disconnected from BLE");
            break;
        default:
            break;
        }
    } else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT) {
        switch (event_id) {
        case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
            ESP_LOGI(TAG, "Provisioning security session established");
            break;
        case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
            ESP_LOGW(TAG, "Provisioning proof-of-possession mismatch");
            break;
        default:
            break;
        }
    }
}

esp_err_t app_prov_init(void)
{
    ESP_RETURN_ON_ERROR(esp_event_handler_register(NETWORK_PROV_EVENT, ESP_EVENT_ANY_ID, prov_event_handler, NULL),
                        TAG, "register provisioning event handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, prov_event_handler, NULL),
                        TAG, "register BLE transport event handler failed");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, prov_event_handler, NULL),
                        TAG, "register security event handler failed");

    return app_prov_start();
}

esp_err_t app_prov_start(void)
{
    network_prov_mgr_config_t config = {
        .network_prov_wifi_conn_cfg = {
            .wifi_conn_attempts = CONFIG_EXAMPLE_WIFI_MAX_RETRY,
        },
        .scheme = network_prov_scheme_ble,
        .scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
    };

    if (!s_prov_initialized) {
        ESP_RETURN_ON_ERROR(network_prov_mgr_init(config), TAG, "network_prov_mgr_init failed");
        s_prov_initialized = true;
    }

    ESP_RETURN_ON_ERROR(network_prov_mgr_is_wifi_provisioned(&s_provisioned), TAG,
                        "read provisioning state failed");
    app_net_state_set_provisioned(s_provisioned);

    if (s_provisioned) {
        ESP_LOGI(TAG, "Already provisioned, starting WiFi STA");
        network_prov_mgr_deinit();
        s_prov_initialized = false;
        return app_wifi_start();
    }

    char service_name[24] = {};
    get_service_name(service_name, sizeof(service_name));

    uint8_t custom_service_uuid[] = {
        0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf,
        0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02,
    };
    network_prov_scheme_ble_set_service_uuid(custom_service_uuid);

    network_prov_security_t security = NETWORK_PROV_SECURITY_1;
    network_prov_security1_params_t *sec_params = (network_prov_security1_params_t *)CONFIG_EXAMPLE_PROV_POP;

    ESP_LOGI(TAG, "Starting BLE provisioning, service name: %s", service_name);
    app_net_state_set_status(APP_NET_STATUS_PROV_WAITING);
    return network_prov_mgr_start_provisioning(security, (const void *)sec_params, service_name, NULL);
}

esp_err_t app_prov_reset(void)
{
    ESP_LOGW(TAG, "Clearing WiFi provisioning data and restarting");
    if (s_prov_initialized) {
        network_prov_mgr_reset_wifi_provisioning();
    }
    ESP_RETURN_ON_ERROR(app_wifi_clear_credentials(), TAG, "clear WiFi credentials failed");
    esp_restart();
    return ESP_OK;
}

bool app_prov_is_provisioned(void)
{
    return s_provisioned;
}
