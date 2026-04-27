/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "nvs_flash.h"
#include "nvs.h"
#include "settings_store.h"

#define NVS_NS          "tft_settings"
#define KEY_BRIGHTNESS  "brightness"
#define KEY_ROTATION    "rotation"

void settings_store_init(void)
{
    /* nvs_flash_init() must have been called by main.c before this */
}

void settings_store_set_brightness(uint8_t pct)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, KEY_BRIGHTNESS, pct);
    nvs_commit(h);
    nvs_close(h);
}

uint8_t settings_store_get_brightness(void)
{
    nvs_handle_t h;
    uint8_t val = 100;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return val;
    nvs_get_u8(h, KEY_BRIGHTNESS, &val);
    nvs_close(h);
    return val;
}

void settings_store_set_rotation(uint8_t rot)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, KEY_ROTATION, rot);
    nvs_commit(h);
    nvs_close(h);
}

uint8_t settings_store_get_rotation(void)
{
    nvs_handle_t h;
    uint8_t val = 0;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return val;
    nvs_get_u8(h, KEY_ROTATION, &val);
    nvs_close(h);
    return val;
}
