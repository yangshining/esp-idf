#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void ui_page_ai_init(lv_obj_t *parent);

/**
 * @brief Update AI inference result display.
 *        Caller must hold lvgl_api_lock (from lcd_touch.h) before calling.
 * @param label      Classification label string (e.g. "cat")
 * @param confidence Confidence score, 0.0 to 1.0
 */
void ui_ai_update_result(const char *label, float confidence);

#ifdef __cplusplus
}
#endif
