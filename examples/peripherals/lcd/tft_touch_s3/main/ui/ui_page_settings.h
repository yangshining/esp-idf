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
 * @param parent  Tab container object.
 * @param disp    Active LVGL display handle (needed by rotation button).
 */
void ui_page_settings_init(lv_obj_t *parent, lv_display_t *disp);

#ifdef __cplusplus
}
#endif
