idf_component_register(
    SRCS
        "main.c"
        "lcd_touch.c"
        "sys_stats.c"
        "settings_store.c"
        "connectivity/app_net_state.c"
        "connectivity/app_wifi.c"
        "connectivity/app_prov.c"
        "ui/ui_main.c"
        "ui/ui_page_home.c"
        "ui/ui_page_ai.c"
        "ui/ui_page_network.c"
        "ui/ui_page_settings.c"
    INCLUDE_DIRS "." "ui" "connectivity"
    PRIV_REQUIRES nvs_flash esp_driver_ledc esp_driver_spi esp_driver_gpio esp_lcd esp_wifi esp_netif esp_event bt protocomm network_provisioning
)
