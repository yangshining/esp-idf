/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t app_prov_init(void);
esp_err_t app_prov_start(void);
esp_err_t app_prov_reset(void);
bool app_prov_is_provisioned(void);

#ifdef __cplusplus
}
#endif
