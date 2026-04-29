# AGENTS.md

This file gives Codex guidance for working inside the `tft_touch_s3` ESP-IDF example.

## Project Scope

This directory is an ESP32-S3 Edge AI Lab demo. It drives:

- ST7789 SPI TFT display, 240x320
- XPT2046 resistive touch controller
- LVGL 9.3 two-tab AI assistant UI (`AI` and `Setup`)
- BLE WiFi provisioning through `espressif/network_provisioning`
- WiFi STA status display and credential clearing on the Setup page

Keep changes focused on this example unless the user explicitly asks for framework-level ESP-IDF changes.

## Key Files

- `main/main.c` - app entry point, NVS setup, connectivity startup, LVGL task/tick setup
- `main/lcd_touch.c` and `main/lcd_touch.h` - LCD, touch, SPI, backlight PWM, and LVGL lock integration
- `main/sys_stats.c` and `main/sys_stats.h` - heap and CPU load sampling
- `main/settings_store.c` and `main/settings_store.h` - NVS-backed brightness and rotation settings
- `main/connectivity/app_net_state.c` and `.h` - shared provisioning/WiFi state model guarded by a FreeRTOS mutex
- `main/connectivity/app_wifi.c` and `.h` - WiFi STA, `esp_netif`, WiFi/IP event handling, credential clearing
- `main/connectivity/app_prov.c` and `.h` - BLE provisioning manager integration
- `main/ui/ui_config.h` - central UI constants (timer periods per avatar state, animation parameters, widget dimensions); add new UI constants here instead of inline magic numbers
- `main/ui/ui_main.c` - LVGL two-tab tabview creation and page wiring
- `main/ui/ui_page_ai.c` - Phase 1 voice-assistant avatar prototype; table-driven demo state machine with Idle/Listening/Uploading/Thinking/Speaking/Error states, per-state timing and color/frame tables
- `main/ui/ui_page_settings.c` - rotation, backlight, WiFi status, and Clear WiFi controls
- `main/ui/ui_page_home.c` - legacy heap/CPU charts and uptime page, currently not mounted in `ui_main.c`
- `main/ui/ui_page_network.c` - legacy standalone BLE provisioning and WiFi STA status page, currently not mounted in `ui_main.c`
- `main/Kconfig.projbuild` - GPIO, touch, provisioning, and WiFi retry configuration
- `main/idf_component.yml` - managed dependencies
- `partitions.csv` - custom 3 MB factory app partition

## Build And Verification

ESP-IDF must be activated before building.

```powershell
idf.py set-target esp32s3
idf.py build
```

For hardware verification:

```powershell
idf.py -p COMx flash monitor
```

If build fails with a missing WiFi binary library such as `components/esp_wifi/lib/esp32s3/libcore.a`, initialize the ESP-IDF WiFi binary submodule:

```powershell
git submodule update --init --recursive -- components/esp_wifi/lib
```

Do not commit `build/`, `sdkconfig`, `sdkconfig.old`, downloaded `managed_components/`, or submodule SHA changes unless the user explicitly asks.

## Connectivity Rules

- `nvs_flash_init()` must run before settings, WiFi, or provisioning code.
- BLE provisioning defaults are controlled by Kconfig:
  - `CONFIG_EXAMPLE_PROV_SERVICE_NAME="edge-ai-lab"`
  - `CONFIG_EXAMPLE_PROV_POP="abcd1234"`
  - `CONFIG_EXAMPLE_WIFI_MAX_RETRY=5`
- WiFi/BLE event handlers must not call LVGL directly.
- Event handlers should update `app_net_state`; LVGL pages should read that state from LVGL task context.
- The Setup page currently uses an LVGL timer to refresh status labels from `app_net_state`.
- The **Clear WiFi** button calls provisioning reset/credential clearing and restarts the board.
- Phase 1 AI is UI-only; future audio/backend tasks should add `assistant_state` and keep network/audio work out of LVGL callbacks.

## LVGL Threading

LVGL runs in the dedicated `lvgl_task`. Any LVGL call from another FreeRTOS task must be guarded with the shared lock declared in `lcd_touch.h`:

```c
_lock_acquire(&lvgl_api_lock);
/* LVGL calls */
_lock_release(&lvgl_api_lock);
```

The AI page API `ui_ai_update_result()` must be called under this lock if invoked outside LVGL task context.

## Configuration Notes

- GPIO defaults should stay consistent across `README.md`, `Kconfig.projbuild`, and wiring assumptions.
- `sdkconfig.defaults.esp32s3` enables PSRAM, Bluetooth, NimBLE, and FreeRTOS runtime stats.
- `sdkconfig.defaults` enables LVGL defaults, XPT2046 defaults, the custom partition table, and protocomm security v1.
- The current app binary needs the custom 3 MB factory partition because LVGL plus BLE/WiFi is larger than the stock small app partition.

## Documentation Rules

When changing this example:

- Update `README.md` for user-facing behavior, build steps, hardware notes, and troubleshooting.
- Update this `AGENTS.md` and `CLAUDE.md` when module responsibilities or workflow rules change.
- Keep connectivity architecture documented: event handlers update state, UI renders state.
