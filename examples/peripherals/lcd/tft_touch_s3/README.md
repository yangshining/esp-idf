| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# TFT Touch S3 - Edge AI Lab Demo

This example drives a 2.4-inch SPI TFT module marked `TFT SPI 240*320`, with an SPI resistive touch controller. The display path uses the ESP-IDF `esp_lcd` ST7789 driver and the touch path uses the XPT2046 component.

The demo renders a three-page LVGL v9 UI:

- Home: chip name and uptime
- AI: result label and confidence bar, updated through `ui_ai_update_result()`
- Settings: screen rotation and backlight on/off slider

## Hardware

- ESP32-S3 development board, such as the XinluCity ESP32-S3 N16R8 board
- 2.4-inch SPI TFT 240x320 module
- XPT2046-compatible resistive touch controller
- USB cable for flashing

The photographed TFT module has these pins:

```text
VCC, GND, CS, RESET, DC, SDI<MOSI>, SCK, LED, SDO<MISO>,
T_CLK, T_CS, T_DIN, T_DO, T_IRQ,
SD_CS, SD_MOSI, SD_MISO, SD_SCK
```

The `SD_*` pins are for the TF card slot. This example does not use the TF card, so leave them unconnected.

## Recommended Wiring

Use the UART/CH340 USB port on the ESP32-S3 board for flashing. Avoid GPIO19/GPIO20 for this demo because they are also the native USB D-/D+ pins on many ESP32-S3 boards.

| TFT module pin | ESP32-S3 pin | Notes |
| -------------- | ------------ | ----- |
| `VCC` | `3V3` | Use 3.3 V first; ESP32-S3 GPIOs are not 5 V tolerant |
| `GND` | `GND` | Common ground |
| `CS` | `GPIO4` | LCD chip select |
| `RESET` | `GPIO3` | LCD reset |
| `DC` | `GPIO5` | LCD data/command |
| `SDI<MOSI>` | `GPIO17` | Shared SPI MOSI |
| `SCK` | `GPIO18` | Shared SPI clock |
| `LED` | `GPIO2` | Backlight control, active high by default |
| `SDO<MISO>` | optional, or `GPIO21` | LCD readback is not required |
| `T_CLK` | `GPIO18` | Touch SPI clock, shared with LCD |
| `T_CS` | `GPIO15` | Touch chip select |
| `T_DIN` | `GPIO17` | Touch SPI MOSI, shared with LCD |
| `T_DO` | `GPIO21` | Touch SPI MISO; required for coordinates |
| `T_IRQ` | unconnected | Current code polls touch data and does not use interrupt |

Default Kconfig values match this table. To change pins, run:

```bash
idf.py menuconfig
```

Then open `Example Configuration`.

## Build

From a PowerShell session:

```powershell
cd E:\code\esp\esp-idf
. .\export.ps1
cd examples\peripherals\lcd\tft_touch_s3
idf.py set-target esp32s3
idf.py build
```

From a Unix-like shell:

```bash
cd /path/to/esp-idf
. ./export.sh
cd examples/peripherals/lcd/tft_touch_s3
idf.py set-target esp32s3
idf.py build
```

The build output is written under:

```text
examples/peripherals/lcd/tft_touch_s3/build
```

## Flash and Monitor

Replace `COMx` with the serial port shown in Device Manager, for example `COM3` or `COM7`.

```powershell
idf.py -p COMx flash monitor
```

You can also flash first and open the monitor later:

```powershell
idf.py -p COMx flash
idf.py -p COMx monitor
```

Exit the monitor with `Ctrl+]`.

Expected log lines include:

```text
I (...) lcd_touch: Init XPT2046 touch
I (...) lcd_touch: LCD and touch initialised
I (...) main: LVGL task started
```

## Configuration Notes

The current defaults are:

| Config option | Default |
| ------------- | ------- |
| `EXAMPLE_PIN_NUM_SCLK` | `18` |
| `EXAMPLE_PIN_NUM_MOSI` | `17` |
| `EXAMPLE_PIN_NUM_MISO` | `21` |
| `EXAMPLE_PIN_NUM_LCD_CS` | `4` |
| `EXAMPLE_PIN_NUM_LCD_DC` | `5` |
| `EXAMPLE_PIN_NUM_LCD_RST` | `3` |
| `EXAMPLE_PIN_NUM_TOUCH_CS` | `15` |
| `EXAMPLE_PIN_NUM_BK_LIGHT` | `2` |
| `EXAMPLE_BK_LIGHT_ON_LEVEL` | `1` |
| `EXAMPLE_LCD_PIXEL_CLOCK_HZ` | `20000000` |

`sdkconfig.defaults.esp32s3` enables octal PSRAM at 80 MHz:

```text
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_SPEED_80M=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
```

This matches ESP32-S3 N16R8 boards with 8 MB PSRAM. If your board has no compatible PSRAM, remove or override those settings.

## Integrating AI Results

Any FreeRTOS task can push inference results to the AI page. LVGL APIs must be called while holding `lvgl_api_lock`:

```c
#include "ui_page_ai.h"
#include "lcd_touch.h"

_lock_acquire(&lvgl_api_lock);
ui_ai_update_result("cat", 0.92f);
_lock_release(&lvgl_api_lock);
```

## Troubleshooting

- Blank white screen: the module may use ILI9341 instead of ST7789, or the SPI pins may be wrong.
- Screen is black: check `LED -> GPIO2` backlight wiring. If the module uses active-low backlight, set `EXAMPLE_BK_LIGHT_ON_LEVEL` to `0` in `idf.py menuconfig`.
- No touch response: check `T_DO -> GPIO21`, `T_DIN -> GPIO17`, `T_CLK -> GPIO18`, and `T_CS -> GPIO15`.
- Touch is mirrored or offset: adjust `.swap_xy`, `.mirror_x`, and `.mirror_y` in `main/lcd_touch.c`.
- Build fails because PSRAM is not found: adjust `sdkconfig.defaults.esp32s3` for your board.
- Do not connect the TF card `SD_*` pins unless SD card support is added to the example.
