# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is an ESP32-S3 example project (`tft_touch_s3`) within the ESP-IDF repository. It drives an ST7789 SPI TFT display (240×320) with an XPT2046 resistive touch controller using LVGL v9.3, presenting a three-page tabview UI. It is designed as a starting point for Edge AI Lab demos where an inference task pushes results to the AI page.

External component dependencies are declared in `main/idf_component.yml` and fetched automatically during build:
- `lvgl/lvgl 9.3.0`
- `atanisoft/esp_lcd_touch_xpt2046 1.0.6`

### New modules (added 2026-04-27)

- `main/sys_stats.c/.h` — heap and CPU load sampling via `esp_timer` + `uxTaskGetSystemState`. Call `sys_stats_init()` once; then `sys_stats_heap_free()` and `sys_stats_cpu_load()` are safe to read from any context (internal `_lock_t`).
- `main/settings_store.c/.h` — NVS-backed persistence for brightness and rotation. Namespace `"tft_settings"`, keys `"brightness"` (u8, default 100) and `"rotation"` (u8, default 0). `nvs_flash_init()` must be called by the caller before `settings_store_init()`.

Backlight is now driven by **LEDC PWM** (LEDC_TIMER_0 / LEDC_CHANNEL_0, 5 kHz, 13-bit). Use `lcd_touch_set_brightness(uint8_t pct)` instead of `gpio_set_level`. Active-low boards are handled automatically via the `CONFIG_EXAMPLE_BK_LIGHT_ON_LEVEL` Kconfig symbol.

## Build Commands

All commands must be run from this project directory (`examples/peripherals/lcd/tft_touch_s3`) with ESP-IDF activated (`. $IDF_PATH/export.sh`).

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
idf.py menuconfig   # navigate to "TFT Touch S3 Example Configuration" for GPIO pin overrides
```

PSRAM is enabled by default in `sdkconfig.defaults.esp32s3` (octal, 80 MHz). If the board lacks PSRAM, remove the three `CONFIG_SPIRAM*` lines from that file.

## Architecture

### Layered structure

```
app_main (main.c)
  ├─ nvs_flash_init()           NVS (must be first)
  ├─ settings_store_init()      persistence layer → settings_store.c
  ├─ sys_stats_init()           stats sampling    → sys_stats.c
  ├─ lcd_touch_init()           hardware layer    → lcd_touch.c / lcd_touch.h
  └─ LVGL init + task           middleware         → main.c
       └─ ui_main_init()        UI root            → ui/ui_main.c
             ├─ ui_page_home_init()    Home tab    → ui/ui_page_home.c
             ├─ ui_page_ai_init()      AI tab      → ui/ui_page_ai.c
             └─ ui_page_settings_init() Settings   → ui/ui_page_settings.c
```

### Threading model

LVGL runs in a dedicated FreeRTOS task (`lvgl_task`, priority 2, 6 KB stack). All LVGL API calls from other tasks **must** be wrapped with the shared mutex declared in `lcd_touch.h`:

```c
_lock_acquire(&lvgl_api_lock);
/* LVGL calls here */
_lock_release(&lvgl_api_lock);
```

`lvgl_api_lock` is defined in `lcd_touch.c` and initialized by `lcd_touch_init()`.

LVGL ticks are driven by an `esp_timer` periodic callback firing every 2 ms (`lvgl_tick_cb` → `lv_tick_inc`). DMA draw buffers are allocated with `spi_bus_dma_memory_alloc` for zero-copy SPI transfers.

### AI integration point

`ui_ai_update_result(const char *label, float confidence)` (declared in `ui/ui_page_ai.h`) updates the result label and confidence bar on the AI tab. Must be called under `lvgl_api_lock`. The function is intentionally thin — it only updates static LVGL widgets; all inference logic belongs in a separate FreeRTOS task.

### Rotation

Screen rotation is handled in `ui/ui_main.c` via an LVGL `LV_EVENT_RESOLUTION_CHANGED` callback. The callback keeps the `esp_lcd` panel (`swap_xy` / `mirror`) in sync with LVGL's software rotation so the physical pixel order matches. Triggered by the Settings page rotate button (`ui_page_settings.c`).

### GPIO / Kconfig

All GPIO numbers and the LCD SPI clock are Kconfig symbols under menu `"TFT Touch S3 Example Configuration"` in `main/Kconfig.projbuild`. Defaults match the wiring table in `README.md`. Backlight defaults to active-high (`EXAMPLE_BK_LIGHT_ON_LEVEL = 1`) and is driven by LEDC PWM — see `lcd_touch_set_brightness()`.

`sdkconfig.defaults.esp32s3` now also includes:
```
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER=y
```
These are required for `sys_stats` CPU load calculation via `uxTaskGetSystemState()`.
