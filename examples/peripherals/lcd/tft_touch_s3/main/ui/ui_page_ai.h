/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_page_ai_init(lv_obj_t *parent);

/**
 * @brief Update the AI avatar caption and speaking state.
 *        Caller must hold lvgl_api_lock (from lcd_touch.h) before calling.
 * @param label      Assistant caption or reply text.
 * @param confidence Kept for compatibility; ignored by the Phase 1 avatar page.
 */
void ui_ai_update_result(const char *label, float confidence);

#ifdef __cplusplus
}
#endif
