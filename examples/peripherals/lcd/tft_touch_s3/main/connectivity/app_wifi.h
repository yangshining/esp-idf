/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_wifi_init(void);
esp_err_t app_wifi_start(void);
esp_err_t app_wifi_clear_credentials(void);

#ifdef __cplusplus
}
#endif
