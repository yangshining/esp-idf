| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# TFT Touch S3 — Edge AI Lab Demo

This example targets ESP32-S3 and drives an ST7789 SPI TFT display (240×320) together with an XPT2046 resistive touch controller. It uses LVGL v9.3 to render a three-page tabview UI and exposes a `ui_ai_update_result()` API for plugging in inference results from an AI task.

## What This Example Does

- Initializes an ST7789 display and XPT2046 touch controller on a shared SPI2 bus
- Runs LVGL in a dedicated FreeRTOS task protected by a mutex
- Displays a three-page tabview:
  - **Home** — chip name and live uptime counter (refreshed every second)
  - **AI** — placeholder card with a label and confidence bar; updated via `ui_ai_update_result()`
  - **Settings** — screen rotation button (0°/90°/180°/270°) and backlight on/off slider
- Exposes `ui_ai_update_result(const char *label, float confidence)` for external AI tasks

## Key Components Used

### ESP-IDF Components

- **esp_lcd**: ST7789 panel driver and SPI panel IO
- **driver/spi_master**: SPI2 bus shared by LCD and touch controller
- **esp_timer**: 2 ms periodic tick for LVGL

### External Components (Component Registry)

- **lvgl/lvgl 9.3.0**: Graphics library
- **atanisoft/esp_lcd_touch_xpt2046 1.0.6**: XPT2046 resistive touch driver

## Hardware Requirements

- ESP32-S3 development board
- ST7789 SPI TFT display, 240×320
- XPT2046 resistive touch controller (often integrated on the same display module)
- USB cable for flashing

## Hardware Connections

Default pin assignments (configurable via `idf.py menuconfig` → "Example Configuration"):

| Signal    | GPIO | Notes                        |
|-----------|------|------------------------------|
| SCLK      | 18   | Shared by LCD and touch       |
| MOSI      | 19   | Shared by LCD and touch       |
| MISO      | 21   | Touch read-back only          |
| LCD CS    | 4    | ST7789 chip select            |
| LCD DC    | 5    | Data/command select           |
| LCD RST   | 3    | Display reset                 |
| Touch CS  | 15   | XPT2046 chip select           |
| Backlight | 2    | Active-low (low = on)         |

```text
ESP32-S3 Board                       Display Module
+--------------------+           +----------------------+
|               GND  +---------->| GND                  |
|               3V3  +---------->| VCC                  |
|         SCLK (18)  +---------->| SCL / CLK            |
|         MOSI (19)  +---------->| MOSI / SDA           |
|         MISO (21)  |<----------+ MISO / SDO           |
|       LCD DC  (5)  +---------->| DC / RS (LCD)        |
|      LCD RST  (3)  +---------->| RST (LCD)            |
|       LCD CS  (4)  +---------->| CS (LCD)             |
|     Touch CS (15)  +---------->| CS (Touch / XPT2046) |
|    Backlight  (2)  +---------->| BLK                  |
+--------------------+           +----------------------+
```

## Getting Started

### Set Target and Build

```bash
cd examples/peripherals/lcd/tft_touch_s3

idf.py set-target esp32s3
idf.py build
```

### Flash and Monitor

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

### Configuration

Run `idf.py menuconfig` and navigate to **Example Configuration** to adjust GPIO pin numbers.

PSRAM (SPIRAM) is enabled by default in `sdkconfig.defaults.esp32s3` for ESP32-S3 boards with octal PSRAM at 80 MHz. If your board does not have PSRAM, remove or override those settings.

## Integrating AI Results

Any FreeRTOS task can push inference results to the AI page:

```c
#include "ui_page_ai.h"
#include "lcd_touch.h"   /* for lvgl_api_lock */

// From your AI inference task:
_lock_acquire(&lvgl_api_lock);
ui_ai_update_result("cat", 0.92f);
_lock_release(&lvgl_api_lock);
```

The bar and label on the AI page update immediately.

## Expected Console Output

```text
I (307) main: LCD touch initialized
I (317) main: LVGL task started
I (327) ui_main: tabview created, 3 pages registered
```

## Troubleshooting

- **Blank screen**: Check MOSI/SCLK wiring and LCD CS/DC/RST connections.
- **No touch response**: Verify MISO and Touch CS wiring. Try toggling `mirror_y` in `lcd_touch.c`.
- **Build error — PSRAM not found**: If your board lacks PSRAM, comment out the SPIRAM lines in `sdkconfig.defaults.esp32s3`.
- **LVGL assertion / stack overflow**: Increase `LVGL_TASK_STACK_SIZE` in `main.c`.

For other issues, open a ticket in the project repository.
