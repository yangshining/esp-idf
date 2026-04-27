# AGENTS.md

This file gives Codex guidance for working in this ESP-IDF fork.

## Repository Context

This repository is a fork of Espressif ESP-IDF, the official IoT Development Framework for Espressif SoCs.

- Local path: `E:\code\esp\esp-idf`
- Current branch: `adapt/edge-ai-lab`
- Configured remote: `origin` -> `https://github.com/yangshining/esp-idf.git`
- Expected upstream project: `https://github.com/espressif/esp-idf.git`

At the time this file was updated, only `origin` was configured locally. If work requires syncing from Espressif, first add or verify the upstream remote:

```bash
git remote add upstream https://github.com/espressif/esp-idf.git
git fetch upstream
```

Push adaptation/customization work to `origin adapt/edge-ai-lab`, not to `upstream`.

This branch currently contains an Edge AI Lab display/touch demo at:

```text
examples/peripherals/lcd/tft_touch_s3
```

## Important Local Notes

- `AGENTS.md` documents this fork's local Codex workflow. Keep it aligned with branch-specific conventions.
- Submodules may be uninitialized after clone. `git submodule status --recursive` shows leading `-` for missing submodules.
- Prefer small, focused changes. ESP-IDF is large; avoid broad refactors unless explicitly requested.
- Do not edit generated build outputs, `build/`, `sdkconfig`, dependency lock files, or downloaded managed components unless the task specifically calls for it.

## Environment Setup

ESP-IDF must be installed and activated before using `idf.py`.

On Windows PowerShell:

```powershell
.\install.ps1 esp32,esp32s3
. .\export.ps1
```

On Unix-like shells:

```bash
./install.sh esp32 esp32s3
. ./export.sh
```

After activation, `idf.py` should be available in the same shell session.

If submodules are missing:

```bash
git submodule update --init --recursive
```

For non-GitHub forks, ESP-IDF documents this helper before submodule init:

```bash
tools/set-submodules-to-github.sh
```

## Building a Project

ESP-IDF itself is not built from the repository root. Build an example or application directory that has its own `CMakeLists.txt`.

Typical ESP32-S3 example workflow:

```bash
cd examples/get-started/hello_world
idf.py set-target esp32s3
idf.py build
idf.py -p COM3 flash monitor
```

Use Windows serial ports like `COM3` on Windows, and Unix ports like `/dev/ttyUSB0` or `/dev/cu.usbserial-*` elsewhere.

Common commands:

```bash
idf.py menuconfig
idf.py build
idf.py app
idf.py flash
idf.py monitor
idf.py flash monitor
idf.py app-flash
idf.py erase-flash
idf.py size
```

Supported targets include `esp32`, `esp32s2`, `esp32s3`, `esp32c2`, `esp32c3`, `esp32c5`, `esp32c6`, `esp32c61`, `esp32h2`, and `esp32p4`. Preview/experimental targets may also exist in the current ESP-IDF tree.

## Edge AI Lab TFT Touch Demo

The local customization focus is:

```text
examples/peripherals/lcd/tft_touch_s3
```

This ESP32-S3 demo drives an ST7789 SPI TFT display and an XPT2046 resistive touch controller, with an LVGL 9.3 UI. It also includes a minimal BLE WiFi provisioning flow and WiFi STA status page.

Key files:

- `main/main.c` - app entry point, LVGL task setup, timer setup
- `main/lcd_touch.c` and `main/lcd_touch.h` - LCD, touch, SPI, backlight, and LVGL lock integration
- `main/connectivity/app_net_state.c` and `main/connectivity/app_net_state.h` - shared provisioning/WiFi state model guarded by a FreeRTOS mutex
- `main/connectivity/app_wifi.c` and `main/connectivity/app_wifi.h` - WiFi STA, `esp_netif`, WiFi/IP event handling, credential clearing
- `main/connectivity/app_prov.c` and `main/connectivity/app_prov.h` - BLE provisioning manager integration using `network_provisioning`
- `main/ui/ui_main.c` - tabview creation and page wiring
- `main/ui/ui_page_home.c` - home page
- `main/ui/ui_page_ai.c` - AI result label and confidence bar
- `main/ui/ui_page_network.c` - BLE provisioning and WiFi STA status page
- `main/ui/ui_page_settings.c` - rotation and backlight controls
- `main/Kconfig.projbuild` - GPIO/example configuration
- `partitions.csv` - custom partition table with a 3 MB factory app partition for LVGL + BLE/WiFi
- `main/idf_component.yml` - managed dependencies:
  - `lvgl/lvgl: "9.3.0"`
  - `atanisoft/esp_lcd_touch_xpt2046: "1.0.6"`
  - `espressif/network_provisioning: "^1.2.4"`

Build it with:

```bash
cd examples/peripherals/lcd/tft_touch_s3
idf.py set-target esp32s3
idf.py build
```

The ESP32-S3 defaults enable PSRAM:

```text
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_BT_ENABLED=y
CONFIG_BT_NIMBLE_ENABLED=y
CONFIG_ESP_PROTOCOMM_SUPPORT_SECURITY_VERSION_1=y
```

If the target board has no compatible PSRAM, adjust or override `sdkconfig.defaults.esp32s3`.

When updating LVGL UI from another FreeRTOS task, guard LVGL calls with the exported lock from `lcd_touch.h` before calling `ui_ai_update_result()`. WiFi/BLE event handlers should not call LVGL directly; update `app_net_state` and let an LVGL-side timer/page render the state.

BLE provisioning defaults:

```text
CONFIG_EXAMPLE_PROV_SERVICE_NAME="edge-ai-lab"
CONFIG_EXAMPLE_PROV_POP="abcd1234"
CONFIG_EXAMPLE_WIFI_MAX_RETRY=5
```

If a build fails with `components/esp_wifi/lib/esp32s3/libcore.a` missing, initialize the WiFi binary submodule:

```bash
git submodule update --init --recursive -- components/esp_wifi/lib
```

## Architecture Overview

ESP-IDF uses CMake plus Ninja, wrapped by `idf.py`.

Important build files:

- `tools/cmake/project.cmake` - project setup, component discovery, configuration integration
- `tools/cmake/component.cmake` - component registration macros
- `tools/cmake/build.cmake` - build property management
- `tools/cmake/kconfig.cmake` - Kconfig to `sdkconfig` to `sdkconfig.h`

Reusable framework code lives in `components/`. Most components have:

- `CMakeLists.txt` with `idf_component_register(...)`
- optional `Kconfig` or `Kconfig.projbuild`
- public headers under `include/`
- tests under `test_apps/` or related directories

Major component areas include:

- `freertos` - RTOS kernel integration
- `esp_system` - system init, panic, reset, and startup behavior
- `soc` - SoC-specific register definitions and capabilities
- `hal` and `esp_hal_*` - hardware abstraction layers
- `driver` and `esp_driver_*` - peripheral drivers
- `esp_lcd` - LCD panel and IO APIs
- `esp_wifi`, `esp_netif`, `esp_netif_stack`, `lwip` - networking
- `nvs_flash` - non-volatile storage
- `bootloader`, `bootloader_support` - bootloader infrastructure
- `heap` - memory allocation, tracing, and poisoning support

## Kconfig and Configuration

`idf.py menuconfig` writes `sdkconfig` in the project directory. Avoid committing local `sdkconfig` changes unless the project intentionally tracks them.

Use:

- `Kconfig` for component-wide options
- `Kconfig.projbuild` for project-level options injected by a component/example
- `sdkconfig.defaults` and `sdkconfig.defaults.<target>` for example defaults
- root `sdkconfig.rename` for compatibility mappings when config symbols are renamed

## Tests and Verification

Tests use `pytest` with `pytest-embedded`; many require real hardware or QEMU.

From a project or test app directory:

```bash
pytest pytest_*.py -m generic
pytest pytest_*.py --target esp32s3 -p COM3
```

Host-side tests can use the `linux` target when supported by that project:

```bash
idf.py set-target linux
idf.py build
./build/<app_name>.elf
```

For narrow example changes, a targeted `idf.py build` for the affected example is usually the best verification. For component changes, build the closest `components/<name>/test_apps/...` project or affected examples.

## Style and Linting

C/C++ formatting uses ESP-IDF's `tools/format.sh`:

```bash
tools/format.sh path/to/file.c
```

Python formatting and linting use Ruff:

```bash
ruff check --fix .
ruff format .
```

Pre-commit can run the configured checks:

```bash
pre-commit run --all-files
```

Project conventions:

- C/C++ style follows ESP-IDF formatting rules: OTBS, 4-space indent, and restrained comments.
- Python style is configured in `ruff.toml`: single quotes, 120-character line length, Python 3.10+ target.
- Keep public API changes documented and update examples/tests when behavior changes.

## Working Rules for Codex

- Read the nearest component/example files before editing; ESP-IDF patterns vary by area.
- Prefer `rg` for search when available. If it fails on this Windows checkout, use PowerShell `Get-ChildItem` and `Select-String`.
- Do not assume hardware is connected. If flashing or hardware tests are needed, state the required port and target.
- Avoid changing submodule SHAs unless the user explicitly asks for dependency updates.
- Avoid broad formatting over unrelated files.
- When changing the TFT touch demo, keep `README.md`, `Kconfig.projbuild`, GPIO defaults, and UI source files consistent.
- When adding managed components, update `idf_component.yml` and verify that the example still resolves dependencies through `idf.py build`.
