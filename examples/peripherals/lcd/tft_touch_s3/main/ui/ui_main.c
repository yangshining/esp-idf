/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "lvgl.h"
#include "esp_lcd_panel_ops.h"
#include "ui_main.h"
#include "ui_page_home.h"
#include "ui_page_ai.h"
#include "ui_page_network.h"
#include "ui_page_settings.h"

static void rotation_cb(lv_event_t *e)
{
    lv_display_t *disp = lv_event_get_target(e);
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    switch (lv_display_get_rotation(disp)) {
    case LV_DISPLAY_ROTATION_0:
        esp_lcd_panel_swap_xy(panel, false);
        esp_lcd_panel_mirror(panel, true, false);
        break;
    case LV_DISPLAY_ROTATION_90:
        esp_lcd_panel_swap_xy(panel, true);
        esp_lcd_panel_mirror(panel, true, true);
        break;
    case LV_DISPLAY_ROTATION_180:
        esp_lcd_panel_swap_xy(panel, false);
        esp_lcd_panel_mirror(panel, false, true);
        break;
    case LV_DISPLAY_ROTATION_270:
        esp_lcd_panel_swap_xy(panel, true);
        esp_lcd_panel_mirror(panel, false, false);
        break;
    }
}

void ui_main_init(lv_display_t *disp, void *panel)
{
    lv_display_set_user_data(disp, panel);
    lv_display_add_event_cb(disp, rotation_cb, LV_EVENT_RESOLUTION_CHANGED, NULL);

    lv_obj_t *scr = lv_display_get_screen_active(disp);
    lv_obj_t *tv = lv_tabview_create(scr);
    lv_tabview_set_tab_bar_size(tv, 40);

    lv_obj_t *tab_home = lv_tabview_add_tab(tv, LV_SYMBOL_HOME " Home");
    lv_obj_t *tab_ai = lv_tabview_add_tab(tv, LV_SYMBOL_IMAGE " AI");
    lv_obj_t *tab_network = lv_tabview_add_tab(tv, "Network");
    lv_obj_t *tab_settings = lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS " Setup");

    ui_page_home_init(tab_home);
    ui_page_ai_init(tab_ai);
    ui_page_network_init(tab_network);
    ui_page_settings_init(tab_settings, disp);
}
