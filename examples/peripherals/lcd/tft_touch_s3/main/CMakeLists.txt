idf_component_register(
    SRCS
        "main.c"
        "lcd_touch.c"
        "sys_stats.c"
        "settings_store.c"
        "ui/ui_main.c"
        "ui/ui_page_home.c"
        "ui/ui_page_ai.c"
        "ui/ui_page_settings.c"
    INCLUDE_DIRS "." "ui"
    PRIV_REQUIRES nvs_flash esp_driver_ledc esp_driver_spi esp_driver_gpio esp_lcd
)
