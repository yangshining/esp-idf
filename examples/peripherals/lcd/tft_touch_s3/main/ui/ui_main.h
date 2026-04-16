/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the top-level tabview and initialise all pages.
 * @param disp   Active LVGL display handle (used to bind rotation callback).
 * @param panel  esp_lcd panel handle (passed to rotation callback).
 */
void ui_main_init(lv_display_t *disp, void *panel);

#ifdef __cplusplus
}
#endif
