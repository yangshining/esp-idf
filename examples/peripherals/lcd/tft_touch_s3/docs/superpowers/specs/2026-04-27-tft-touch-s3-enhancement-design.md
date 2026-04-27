# tft_touch_s3 Enhancement Design

**Date:** 2026-04-27  
**Branch:** adapt/edge-ai-lab  
**Scope:** Direction A — quick polish for self-use/debug

---

## Goals

1. **A1** — Home page shows real-time heap and CPU charts (sys_stats module)
2. **A2** — PWM backlight with NVS persistence (settings_store module + LEDC)
3. **A3** — AI page gets a "Run Inference" button with simulated results

Primary audience: developer self-use / debugging. Prioritise correctness and real data over visual polish.

---

## Architecture

### New modules

```
main/
  sys_stats.c / sys_stats.h        heap + CPU sampling, ring buffer
  settings_store.c / settings_store.h  NVS read/write for user prefs
```

### Modified files

| File | Change |
|---|---|
| `main/main.c` | init `nvs_flash`, `sys_stats_init`, `settings_store_init` before UI |
| `main/lcd_touch.c` | replace GPIO backlight with LEDC; export `lcd_touch_set_brightness()` |
| `main/lcd_touch.h` | add `lcd_touch_set_brightness(uint8_t pct)` declaration |
| `main/ui/ui_page_home.c` | replace static label with two lv_chart widgets |
| `main/ui/ui_page_ai.c` | add Run button + simulated inference timer |
| `main/ui/ui_page_settings.c` | connect slider to LEDC + NVS; restore saved values on init |
| `main/CMakeLists.txt` | add `sys_stats.c`, `settings_store.c` to SRCS |

---

## A1 — sys_stats + Home Charts

### sys_stats public API

```c
void     sys_stats_init(void);        // start 1 s sampling timer
uint32_t sys_stats_heap_free(void);   // current free heap, bytes
uint8_t  sys_stats_cpu_load(void);    // CPU load 0-100 (idle-task method)
```

### Implementation notes

- `esp_timer` periodic callback at 1 s interval
- Heap: `esp_get_free_heap_size()`
- CPU load: use `uxTaskGetSystemState()` to retrieve `TaskStatus_t` array; locate the idle task(s) by name (`"IDLE"` or `"IDLE0"`/`"IDLE1"`), compute idle-runtime delta between two consecutive 1 s samples, divide by total-runtime delta → CPU load = `100 - idle_pct`. **Do not use `vTaskGetRunTimeStats()`** (outputs a formatted string, not numeric data).
- Required sdkconfig symbols (both needed on ESP32-S3):
  - `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y`
  - `CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER=y`
- Lock discipline: `sys_stats` sampling callback only writes to the ring buffer under `_lock_t`; the LVGL timer (running inside the LVGL task) acquires the same lock only to read — it must **not** hold `lvgl_api_lock` while holding `sys_stats` lock to avoid deadlock.
- Internal ring buffer: 30 samples, protected by `_lock_t`
- No dynamic allocation after init

### Home page layout (240 × ~280 usable px, portrait)

```
┌──────────────────────────┐
│  ESP32-S3  Edge AI Lab   │  ← static label, top-center
│                          │
│  Heap Free (KB)          │
│  ┌────────────────────┐  │
│  │ lv_chart line      │  │  ~90 px high
│  └────────────────────┘  │
│  CPU Load (%)            │
│  ┌────────────────────┐  │
│  │ lv_chart line      │  │  ~90 px high
│  └────────────────────┘  │
│  Uptime: 42 s            │  ← small label, bottom
└──────────────────────────┘
```

- LVGL timer (1 s) reads `sys_stats_heap_free()` and `sys_stats_cpu_load()`, appends to chart series with `lv_chart_set_next_value()`
- Y-axis for heap: 0 – total heap at boot (read once); for CPU: 0–100

---

## A2 — PWM Backlight + NVS Persistence

### LEDC configuration

| Parameter | Value |
|---|---|
| Timer | `LEDC_TIMER_0` |
| Channel | `LEDC_CHANNEL_0` |
| Frequency | 5 kHz |
| Resolution | 13-bit (0–8191) |
| GPIO | `CONFIG_EXAMPLE_PIN_NUM_BK_LIGHT` |

`lcd_touch_set_brightness(uint8_t pct)` converts 0–100 → duty, accounting for active-low backlight (`LCD_BK_LIGHT_ON = 0` by default on this board):
- If `CONFIG_EXAMPLE_BK_LIGHT_ON_LEVEL == 1` (active-high): `duty = pct * 8191 / 100`
- If `CONFIG_EXAMPLE_BK_LIGHT_ON_LEVEL == 0` (active-low): `duty = (100 - pct) * 8191 / 100`

`ledc_channel_config_t` must include `.hpoint = 0` (explicit zero-init of all fields avoids strict-init compiler warnings).

`lcd_touch_init()` calls `ledc_timer_config` and `ledc_channel_config` instead of bare `gpio_config`. Initial duty set to 0 (backlight off) during init, restored to saved brightness after panel ready.

### settings_store public API

```c
void    settings_store_init(void);                  // open NVS namespace
void    settings_store_set_brightness(uint8_t pct);
uint8_t settings_store_get_brightness(void);        // default 100
void    settings_store_set_rotation(uint8_t rot);   // 0-3
uint8_t settings_store_get_rotation(void);          // default 0
```

- NVS namespace: `"tft_settings"`
- Keys: `"brightness"` (u8), `"rotation"` (u8)
- `nvs_flash_init()` called once in `main.c` before `settings_store_init()`, with the standard erase-and-retry pattern:
  ```c
  esp_err_t ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
  }
  ESP_ERROR_CHECK(ret);
  ```

### Settings page changes

- `ui_page_settings_init()` reads saved brightness → sets slider position and calls `lcd_touch_set_brightness()`
- `ui_page_settings_init()` reads saved rotation → applies `lv_display_set_rotation()` (fires `rotation_cb` automatically)
- `backlight_slider_cb`: call `lcd_touch_set_brightness(val)` then `settings_store_set_brightness(val)`
- `rotate_btn_cb`: after updating `s_rotation`, call `settings_store_set_rotation(s_rotation)`

---

## A3 — Simulated Inference (AI Page)

### UI changes

```
┌──────────────────────────┐
│  🖼 AI Result            │  ← title
│                          │
│       Cat                │  ← result label, font_montserrat_20
│  ████████░░  85%         │  ← confidence bar + label
│                          │
│  [ Run Inference ]       │  ← new button, bottom-center
└──────────────────────────┘
```

### Interaction flow

1. User taps "Run Inference"
2. Button disabled; result label set to "Running..."
3. `lv_timer_create()` one-shot timer, 1500 ms
4. Timer fires: pick random label from `s_labels[]`, generate random confidence 0.60–0.99
5. Call `ui_ai_update_result(label, confidence)`
6. Re-enable button

### Implementation notes

- Random: `esp_random()` (hardware RNG), not `rand()`
- Labels array (extendable):
  ```c
  static const char *s_labels[] = {
      "Cat", "Dog", "Bird", "Car", "Person", "Plant", "Unknown"
  };
  ```
- One-shot timer: delete in callback with `lv_timer_delete(t)` (more idiomatic in LVGL v9 than `set_repeat_count`)
- All timer callbacks run inside the LVGL task — **no `lvgl_api_lock` needed here**, and no separate FreeRTOS task required. If `ui_ai_update_result` is ever called from outside the LVGL task, the caller must hold `lvgl_api_lock` (per CLAUDE.md threading rules).

---

## Sequencing / Build Order

1. `sys_stats` module (no UI dependencies)
2. `settings_store` module (no UI dependencies)
3. `lcd_touch.c` LEDC refactor
4. `main.c` init sequence update
5. `ui_page_home.c` chart integration
6. `ui_page_settings.c` persistence wiring
7. `ui_page_ai.c` Run button + simulated inference
8. `CMakeLists.txt` update
9. `sdkconfig.defaults.esp32s3` add `CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y` and `CONFIG_FREERTOS_RUN_TIME_STATS_USING_ESP_TIMER=y`

---

## Out of Scope

- Real ML inference (covered by Direction B)
- WiFi / MQTT (Direction C)
- Touch gesture recognition
- Multi-point touch
