/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void    settings_store_init(void);

void    settings_store_set_brightness(uint8_t pct);
uint8_t settings_store_get_brightness(void);   /* default 100 */

void    settings_store_set_rotation(uint8_t rot);
uint8_t settings_store_get_rotation(void);     /* default 0 */

#ifdef __cplusplus
}
#endif
