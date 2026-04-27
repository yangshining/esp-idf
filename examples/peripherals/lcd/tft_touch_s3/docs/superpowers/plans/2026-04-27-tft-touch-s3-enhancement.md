# tft_touch_s3 Enhancement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add real-time heap/CPU charts (Home page), PWM backlight with NVS persistence (Settings page), and a simulated "Run Inference" button (AI page) to the tft_touch_s3 ESP32-S3 example.

**Architecture:** Two new utility modules (`sys_stats`, `settings_store`) handle data and persistence independently of UI; four existing UI/hardware files are extended to consume them. All LVGL callbacks remain inside the LVGL task — no new FreeRTOS tasks.

**Tech Stack:** ESP-IDF v5.x, FreeRTOS, LVGL v9.3, ST7789 SPI TFT, XPT2046 touch, LEDC PWM, NVS flash

**Spec:** `docs/superpowers/specs/2026-04-27-tft-touch-s3-enhancement-design.md`

---

## File Map

| Action | Path | Responsibility |
|--------|------|----------------|
| Create | `main/sys_stats.h` | Public API: `sys_stats_init`, `sys_stats_heap_free`, `sys_stats_cpu_load` |
| Create | `main/sys_stats.c` | esp_timer sampling, uxTaskGetSystemState CPU calc, ring buffer |
| Create | `main/settings_store.h` | Public API: init, get/set brightness, get/set rotation |
| Create | `main/settings_store.c` | NVS namespace `"tft_settings"`, keys `"brightness"` / `"rotation"` |
| Modify | `main/lcd_touch.h` | Add `lcd_touch_set_brightness(uint8_t pct)` declaration |
| Modify | `main/lcd_touch.c` | Replace GPIO backlight with LEDC timer+channel; implement set_brightness |
| Modify | `main/main.c` | Add nvs_flash_init, sys_stats_init, settings_store_init before UI init |
| Modify | `main/ui/ui_page_home.c` | Replace static uptime-only content with two lv_chart + uptime label |
| Modify | `main/ui/ui_page_settings.c` | Restore saved values on init; wire slider/rotate to LEDC+NVS |
| Modify | `main/ui/ui_page_ai.c` | Add Run button, one-shot LVGL timer, simulated result |
| Modify | `main/CMakeLists.txt` | Add sys_stats.c and settings_store.c to SRCS |
| Modify | `sdkconfig.defaults.esp32s3` | Add FreeRTOS runtime stats config symbols |

---

## Task 1: sdkconfig — enable FreeRTOS runtime stats

**Files:**
- Modify: `sdkconfig.defaults.esp32s3`

- [ ] **Step 1: Add runtime stats config lines**

Append to `sdkconfig.defaults.esp32s3`:
```
CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER=y
```

The file currently contains only SPIRAM and flash-size defaults. These two lines enable `uxTaskGetSystemState()` to populate `ulRunTimeCounter` fields.

- [ ] **Step 2: Commit**

```bash
git add sdkconfig.defaults.esp32s3
git commit -m "feat(tft_touch_s3): enable FreeRTOS runtime stats for CPU load"
```

---

## Task 2: sys_stats module

**Files:**
- Create: `main/sys_stats.h`
- Create: `main/sys_stats.c`

- [ ] **Step 1: Write `sys_stats.h`**

```c
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void     sys_stats_init(void);
uint32_t sys_stats_heap_free(void);
uint8_t  sys_stats_cpu_load(void);

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: Write `sys_stats.c`**

```c
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <string.h>
#include <sys/lock.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "sys_stats.h"

#define MAX_TASKS 48  /* ESP32-S3 with PSRAM can have >32 tasks */

static _lock_t   s_lock;
static uint32_t  s_heap_free;
static uint8_t   s_cpu_load;
static uint32_t  s_prev_idle_ticks;
static uint32_t  s_prev_total_ticks;

static void sample_cb(void *arg)
{
    (void)arg;

    /* --- heap --- */
    uint32_t heap = esp_get_free_heap_size();

    /* --- CPU load via idle task runtime --- */
    TaskStatus_t tasks[MAX_TASKS];
    uint32_t total_runtime = 0;
    UBaseType_t n = uxTaskGetSystemState(tasks, MAX_TASKS, &total_runtime);

    uint32_t idle_ticks = 0;
    for (UBaseType_t i = 0; i < n; i++) {
        const char *name = tasks[i].pcTaskName;
        if (strncmp(name, "IDLE", 4) == 0) {
            idle_ticks += tasks[i].ulRunTimeCounter;
        }
    }

    uint32_t idle_delta  = idle_ticks  - s_prev_idle_ticks;
    uint32_t total_delta = total_runtime - s_prev_total_ticks;
    s_prev_idle_ticks  = idle_ticks;
    s_prev_total_ticks = total_runtime;

    uint8_t load = 0;
    if (total_delta > 0) {
        uint32_t idle_pct = (idle_delta * 100) / total_delta;
        load = (idle_pct >= 100) ? 0 : (uint8_t)(100 - idle_pct);
    }

    _lock_acquire(&s_lock);
    s_heap_free = heap;
    s_cpu_load  = load;
    _lock_release(&s_lock);
}

void sys_stats_init(void)
{
    _lock_init(&s_lock);
    s_heap_free = esp_get_free_heap_size();
    s_cpu_load  = 0;
    s_prev_idle_ticks  = 0;
    s_prev_total_ticks = 0;

    const esp_timer_create_args_t args = {
        .callback = sample_cb,
        .name     = "sys_stats",
    };
    esp_timer_handle_t t = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&args, &t));
    ESP_ERROR_CHECK(esp_timer_start_periodic(t, 1000000));
}

uint32_t sys_stats_heap_free(void)
{
    _lock_acquire(&s_lock);
    uint32_t v = s_heap_free;
    _lock_release(&s_lock);
    return v;
}

uint8_t sys_stats_cpu_load(void)
{
    _lock_acquire(&s_lock);
    uint8_t v = s_cpu_load;
    _lock_release(&s_lock);
    return v;
}
```

- [ ] **Step 3: Commit**

```bash
git add main/sys_stats.h main/sys_stats.c
git commit -m "feat(tft_touch_s3): add sys_stats module (heap + CPU sampling)"
```

---

## Task 3: settings_store module

**Files:**
- Create: `main/settings_store.h`
- Create: `main/settings_store.c`

- [ ] **Step 1: Write `settings_store.h`**

```c
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void    settings_store_init(void);

void    settings_store_set_brightness(uint8_t pct);
uint8_t settings_store_get_brightness(void);   /* default 100 */

void    settings_store_set_rotation(uint8_t rot);
uint8_t settings_store_get_rotation(void);     /* default 0 */

#ifdef __cplusplus
}
#endif
```

- [ ] **Step 2: Write `settings_store.c`**

```c
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "settings_store.h"

#define NVS_NS          "tft_settings"
#define KEY_BRIGHTNESS  "brightness"
#define KEY_ROTATION    "rotation"

static const char *TAG = "settings_store";

void settings_store_init(void)
{
    /* nvs_flash_init() must have been called by main.c before this */
}

void settings_store_set_brightness(uint8_t pct)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, KEY_BRIGHTNESS, pct);
    nvs_commit(h);
    nvs_close(h);
}

uint8_t settings_store_get_brightness(void)
{
    nvs_handle_t h;
    uint8_t val = 100;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return val;
    nvs_get_u8(h, KEY_BRIGHTNESS, &val);
    nvs_close(h);
    return val;
}

void settings_store_set_rotation(uint8_t rot)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, KEY_ROTATION, rot);
    nvs_commit(h);
    nvs_close(h);
}

uint8_t settings_store_get_rotation(void)
{
    nvs_handle_t h;
    uint8_t val = 0;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return val;
    nvs_get_u8(h, KEY_ROTATION, &val);
    nvs_close(h);
    return val;
}
```

- [ ] **Step 3: Commit**

```bash
git add main/settings_store.h main/settings_store.c
git commit -m "feat(tft_touch_s3): add settings_store module (NVS persistence)"
```

---

## Task 4: LEDC backlight refactor in lcd_touch

**Files:**
- Modify: `main/lcd_touch.h`
- Modify: `main/lcd_touch.c`

- [ ] **Step 1: Add `lcd_touch_set_brightness` to `lcd_touch.h`**

After the existing `lcd_touch_init` declaration, add:
```c
/**
 * @brief Set backlight brightness via LEDC PWM.
 * @param pct  Brightness percentage 0–100.
 */
void lcd_touch_set_brightness(uint8_t pct);
```

- [ ] **Step 2: Replace GPIO backlight with LEDC in `lcd_touch.c`**

Add includes at top (after existing includes):
```c
#include "driver/ledc.h"
```

Three changes are needed inside `lcd_touch_init()`:

**Change A** — replace the GPIO backlight block near the top of the function (the `gpio_config_t` block followed by `gpio_set_level(..., LCD_BK_LIGHT_OFF)`):
```c
/* BEFORE — remove these four lines: */
gpio_config_t bk_cfg = {
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = 1ULL << CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT,
};
ESP_ERROR_CHECK(gpio_config(&bk_cfg));
gpio_set_level(CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_OFF);

/* AFTER — replace with: */
ledc_timer_config_t ledc_timer = {
    .speed_mode      = LEDC_LOW_SPEED_MODE,
    .timer_num       = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .freq_hz         = 5000,
    .clk_cfg         = LEDC_AUTO_CLK,
};
ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

ledc_channel_config_t ledc_ch = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel    = LEDC_CHANNEL_0,
    .timer_sel  = LEDC_TIMER_0,
    .intr_type  = LEDC_INTR_DISABLE,
    .gpio_num   = CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT,
    .duty       = 0,
    .hpoint     = 0,
};
ESP_ERROR_CHECK(ledc_channel_config(&ledc_ch));
/* backlight starts off — duty is already 0 from ledc_channel_config */
```

**Change B** — near the bottom of `lcd_touch_init()`, replace the single remaining `gpio_set_level` call that turns backlight on:
```c
/* BEFORE: */
gpio_set_level(CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT, LCD_BK_LIGHT_ON);

/* AFTER: */
lcd_touch_set_brightness(100);
```

Also remove the `#include "driver/gpio.h"` line only if `gpio.h` is no longer used elsewhere in the file (check first — it may still be needed for touch INT/RST pins set to -1, which don't use GPIO directly, so it is safe to remove).

- [ ] **Step 3: Implement `lcd_touch_set_brightness()`**

Add after `lcd_touch_init()`:
```c
void lcd_touch_set_brightness(uint8_t pct)
{
    if (pct > 100) pct = 100;
#if CONFIG_EXAMPLE_BK_LIGHT_ON_LEVEL == 0
    uint32_t duty = (uint32_t)(100 - pct) * 8191 / 100;
#else
    uint32_t duty = (uint32_t)pct * 8191 / 100;
#endif
    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0);
}
```

- [ ] **Step 4: Commit**

> Note: Do not run `idf.py build` yet — `sys_stats.c` and `settings_store.c` are not registered in CMakeLists until Task 5. Build verification happens in Task 5.

```bash
git add main/lcd_touch.h main/lcd_touch.c
git commit -m "feat(tft_touch_s3): replace GPIO backlight with LEDC PWM, add set_brightness()"
```

---

## Task 5: CMakeLists.txt — register new sources

**Files:**
- Modify: `main/CMakeLists.txt`

- [ ] **Step 1: Add new sources**

Replace the SRCS list:
```cmake
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
)
```

- [ ] **Step 2: Verify the build compiles all new modules**

```bash
idf.py build 2>&1 | tail -20
```
Expected: no errors. This is the first `idf.py build` that includes `sys_stats.c` and `settings_store.c`.

- [ ] **Step 3: Commit**

```bash
git add main/CMakeLists.txt
git commit -m "build(tft_touch_s3): register sys_stats and settings_store sources"
```

---

## Task 6: main.c — init sequence

**Files:**
- Modify: `main/main.c`

- [ ] **Step 1: Add includes**

After the existing includes in `main.c`, add:
```c
#include "nvs_flash.h"
#include "sys_stats.h"
#include "settings_store.h"
```

- [ ] **Step 2: Add init calls at top of `app_main()`**

Insert before `lcd_touch_handles_t hw = {};`:
```c
/* NVS must be initialised before settings_store */
esp_err_t nvs_ret = nvs_flash_init();
if (nvs_ret == ESP_ERR_NVS_NO_FREE_PAGES || nvs_ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    nvs_ret = nvs_flash_init();
}
ESP_ERROR_CHECK(nvs_ret);

settings_store_init();
sys_stats_init();
```

- [ ] **Step 3: Build**

```bash
idf.py build 2>&1 | tail -20
```
Expected: no errors.

- [ ] **Step 4: Commit**

```bash
git add main/main.c
git commit -m "feat(tft_touch_s3): init NVS, settings_store, sys_stats in app_main"
```

---

## Task 7: Home page — heap + CPU charts

**Files:**
- Modify: `main/ui/ui_page_home.c`

- [ ] **Step 1: Rewrite `ui_page_home.c`**

Replace the entire file content:

```c
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include "sys_stats.h"
#include "ui_page_home.h"

#define CHART_POINTS 30

static lv_obj_t *s_uptime_label;
static lv_obj_t *s_heap_chart;
static lv_obj_t *s_cpu_chart;
static lv_chart_series_t *s_heap_ser;
static lv_chart_series_t *s_cpu_ser;

/* Called from LVGL task — no lvgl_api_lock needed here.
   sys_stats_* acquire their own internal lock briefly; never hold lvgl_api_lock
   while calling them to avoid lock-order inversion. */
static void stats_timer_cb(lv_timer_t *t)
{
    (void)t;
    uint32_t seconds = (uint32_t)(esp_timer_get_time() / 1000000ULL);
    lv_label_set_text_fmt(s_uptime_label, LV_SYMBOL_REFRESH " %"PRIu32" s", seconds);

    lv_chart_set_next_value(s_heap_chart, s_heap_ser,
                            (int32_t)(sys_stats_heap_free() / 1024));
    lv_chart_set_next_value(s_cpu_chart,  s_cpu_ser,
                            (int32_t)sys_stats_cpu_load());
}

void ui_page_home_init(lv_obj_t *parent)
{
    /* Title */
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text_static(title, LV_SYMBOL_HOME " ESP32-S3  Edge AI Lab");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 6);

    /* Heap chart */
    lv_obj_t *heap_lbl = lv_label_create(parent);
    lv_label_set_text_static(heap_lbl, "Heap Free (KB)");
    lv_obj_align(heap_lbl, LV_ALIGN_TOP_MID, 0, 30);

    s_heap_chart = lv_chart_create(parent);
    lv_obj_set_size(s_heap_chart, 210, 80);
    lv_obj_align(s_heap_chart, LV_ALIGN_TOP_MID, 0, 48);
    lv_chart_set_type(s_heap_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_heap_chart, CHART_POINTS);
    int32_t total_kb = (int32_t)(esp_get_free_heap_size() / 1024 + 10);
    lv_chart_set_range(s_heap_chart, LV_CHART_AXIS_PRIMARY_Y, 0, total_kb);
    lv_chart_set_div_line_count(s_heap_chart, 3, 0);
    s_heap_ser = lv_chart_add_series(s_heap_chart, lv_palette_main(LV_PALETTE_GREEN),
                                     LV_CHART_AXIS_PRIMARY_Y);

    /* CPU chart */
    lv_obj_t *cpu_lbl = lv_label_create(parent);
    lv_label_set_text_static(cpu_lbl, "CPU Load (%)");
    lv_obj_align(cpu_lbl, LV_ALIGN_TOP_MID, 0, 138);

    s_cpu_chart = lv_chart_create(parent);
    lv_obj_set_size(s_cpu_chart, 210, 80);
    lv_obj_align(s_cpu_chart, LV_ALIGN_TOP_MID, 0, 156);
    lv_chart_set_type(s_cpu_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(s_cpu_chart, CHART_POINTS);
    lv_chart_set_range(s_cpu_chart, LV_CHART_AXIS_PRIMARY_Y, 0, 100);
    lv_chart_set_div_line_count(s_cpu_chart, 3, 0);
    s_cpu_ser = lv_chart_add_series(s_cpu_chart, lv_palette_main(LV_PALETTE_RED),
                                    LV_CHART_AXIS_PRIMARY_Y);

    /* Uptime label at bottom */
    s_uptime_label = lv_label_create(parent);
    lv_label_set_text_static(s_uptime_label, LV_SYMBOL_REFRESH " 0 s");
    lv_obj_align(s_uptime_label, LV_ALIGN_BOTTOM_MID, 0, -6);

    lv_timer_create(stats_timer_cb, 1000, NULL);
}
```

- [ ] **Step 2: Build**

```bash
idf.py build 2>&1 | tail -20
```
Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add main/ui/ui_page_home.c
git commit -m "feat(tft_touch_s3): home page shows heap + CPU charts via sys_stats"
```

---

## Task 8: Settings page — LEDC + NVS wiring

**Files:**
- Modify: `main/ui/ui_page_settings.c`

- [ ] **Step 1: Rewrite `ui_page_settings.c`**

Replace the entire file content:

```c
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include "lvgl.h"
#include "lcd_touch.h"
#include "settings_store.h"
#include "ui_page_settings.h"

static lv_display_rotation_t s_rotation = LV_DISPLAY_ROTATION_0;

static void rotate_btn_cb(lv_event_t *e)
{
    lv_display_t *disp = lv_event_get_user_data(e);
    s_rotation = (lv_display_rotation_t)((s_rotation + 1) % 4);
    lv_display_set_rotation(disp, s_rotation);
    settings_store_set_rotation((uint8_t)s_rotation);
}

static void backlight_slider_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    uint8_t val = (uint8_t)lv_slider_get_value(slider);
    lcd_touch_set_brightness(val);
    settings_store_set_brightness(val);
}

void ui_page_settings_init(lv_obj_t *parent, lv_display_t *disp)
{
    /* Rotate button */
    lv_obj_t *rot_btn = lv_button_create(parent);
    lv_obj_t *rot_lbl = lv_label_create(rot_btn);
    lv_label_set_text_static(rot_lbl, LV_SYMBOL_REFRESH " Rotate");
    lv_obj_center(rot_lbl);
    lv_obj_align(rot_btn, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_add_event_cb(rot_btn, rotate_btn_cb, LV_EVENT_CLICKED, disp);

    /* Backlight label + slider */
    lv_obj_t *bk_label = lv_label_create(parent);
    lv_label_set_text_static(bk_label, "Backlight");
    lv_obj_align(bk_label, LV_ALIGN_CENTER, 0, -10);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_width(slider, 180);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_event_cb(slider, backlight_slider_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* Restore saved values */
    uint8_t saved_brt = settings_store_get_brightness();
    lv_slider_set_value(slider, saved_brt, LV_ANIM_OFF);
    lcd_touch_set_brightness(saved_brt);

    uint8_t saved_rot = settings_store_get_rotation();
    if (saved_rot < 4) {
        s_rotation = (lv_display_rotation_t)saved_rot;
        lv_display_set_rotation(disp, s_rotation);
    }
}
```

- [ ] **Step 2: Build**

```bash
idf.py build 2>&1 | tail -20
```
Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add main/ui/ui_page_settings.c
git commit -m "feat(tft_touch_s3): settings page uses LEDC brightness + NVS persistence"
```

---

## Task 9: AI page — Run Inference button

**Files:**
- Modify: `main/ui/ui_page_ai.c`

- [ ] **Step 1: Rewrite `ui_page_ai.c`**

Replace the entire file content:

```c
/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <inttypes.h>
#include "esp_random.h"
#include "lvgl.h"
#include "ui_page_ai.h"

static lv_obj_t *s_result_label;
static lv_obj_t *s_conf_bar;
static lv_obj_t *s_conf_label;
static lv_obj_t *s_run_btn;

static const char *s_labels[] = {
    "Cat", "Dog", "Bird", "Car", "Person", "Plant", "Unknown"
};
#define LABEL_COUNT (sizeof(s_labels) / sizeof(s_labels[0]))

static void inference_done_cb(lv_timer_t *t)
{
    uint32_t idx = esp_random() % LABEL_COUNT;
    /* confidence in range 60–99 */
    uint32_t pct = 60 + (esp_random() % 40);
    float confidence = (float)pct / 100.0f;

    ui_ai_update_result(s_labels[idx], confidence);
    lv_obj_remove_state(s_run_btn, LV_STATE_DISABLED);
    lv_timer_delete(t);
}

static void run_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_add_state(s_run_btn, LV_STATE_DISABLED);
    lv_label_set_text_static(s_result_label, "Running...");
    lv_bar_set_value(s_conf_bar, 0, LV_ANIM_OFF);
    lv_label_set_text_static(s_conf_label, "Confidence: -");
    lv_timer_create(inference_done_cb, 1500, NULL);
}

void ui_page_ai_init(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text_static(title, LV_SYMBOL_IMAGE " AI Result");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);

    s_result_label = lv_label_create(parent);
    lv_label_set_text_static(s_result_label, "-- Waiting --");
    lv_obj_set_style_text_font(s_result_label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_result_label, LV_ALIGN_CENTER, 0, -40);

    s_conf_bar = lv_bar_create(parent);
    lv_obj_set_size(s_conf_bar, 180, 20);
    lv_bar_set_range(s_conf_bar, 0, 100);
    lv_bar_set_value(s_conf_bar, 0, LV_ANIM_OFF);
    lv_obj_align(s_conf_bar, LV_ALIGN_CENTER, 0, 0);

    s_conf_label = lv_label_create(parent);
    lv_label_set_text_static(s_conf_label, "Confidence: 0%");
    lv_obj_align(s_conf_label, LV_ALIGN_CENTER, 0, 28);

    s_run_btn = lv_button_create(parent);
    lv_obj_t *btn_lbl = lv_label_create(s_run_btn);
    lv_label_set_text_static(btn_lbl, LV_SYMBOL_PLAY " Run Inference");
    lv_obj_center(btn_lbl);
    lv_obj_align(s_run_btn, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(s_run_btn, run_btn_cb, LV_EVENT_CLICKED, NULL);
}

void ui_ai_update_result(const char *label, float confidence)
{
    lv_label_set_text(s_result_label, label);
    int32_t pct = (int32_t)(confidence * 100.0f);
    lv_bar_set_value(s_conf_bar, pct, LV_ANIM_ON);
    lv_label_set_text_fmt(s_conf_label, "Confidence: %"PRId32"%%", pct);
}
```

- [ ] **Step 2: Build**

```bash
idf.py build 2>&1 | tail -20
```
Expected: no errors.

- [ ] **Step 3: Commit**

```bash
git add main/ui/ui_page_ai.c
git commit -m "feat(tft_touch_s3): AI page adds Run Inference button with simulated result"
```

---

## Task 10: Final build verification

- [ ] **Step 1: Clean build**

```bash
idf.py build 2>&1 | grep -E "error:|warning:" | head -30
```
Expected: no errors; warnings should be zero or only from third-party managed components.

- [ ] **Step 2: Check binary size**

```bash
idf.py size
```
Note the app binary size. PSRAM is enabled so heap headroom is large, but verify the flash fits within the 16 MB default partition.

- [ ] **Step 3: Flash and smoke-test (if hardware available)**

```bash
idf.py -p /dev/ttyUSB0 flash monitor
```

Check:
- Home tab: two charts animate every second, uptime ticks
- Settings tab: slider moves backlight smoothly (not just on/off); rotation persists after reboot
- AI tab: "Run Inference" button disables for ~1.5 s then shows a random result with animated bar

- [ ] **Step 4: Final commit if any tweaks needed**

```bash
git add -p
git commit -m "fix(tft_touch_s3): post-flash adjustments"
```
