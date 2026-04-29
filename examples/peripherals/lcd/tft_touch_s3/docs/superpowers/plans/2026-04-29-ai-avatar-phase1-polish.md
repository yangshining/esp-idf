# AI Avatar Phase 1 Polish Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Polish the Phase 1 AI avatar demo by replacing inline switch-case animation logic with static data tables, adding the missing `Uploading` state, improving per-state color contrast, and giving each demo state its own configurable dwell time.

**Architecture:** All changes are confined to `main/ui/ui_config.h` (timing constants) and `main/ui/ui_page_ai.c` (enum, tables, timer callbacks). No new files, no public API changes. The frame table, color table, and text table are `static const` arrays indexed by `ai_demo_state_t`, decoupling data from render logic.

**Tech Stack:** ESP-IDF C, LVGL 9.3, FreeRTOS (via LVGL timer callbacks), `idf.py build` for esp32s3 verification.

---

## File Structure

- Modify: `main/ui/ui_config.h` — remove `UI_AI_DEMO_STEP_MS`, add 5 per-state step constants
- Modify: `main/ui/ui_page_ai.c` — add `UPLOADING` state + `COUNT` sentinel; add frame/color/text tables; rewrite `anim_timer_cb`, `demo_timer_cb`, `ai_page_render`

No tests directory exists for this embedded project. Verification is via `idf.py build` (compile correctness) and optional hardware flash.

---

## Task 1: Update Timing Constants in ui_config.h

**Files:**
- Modify: `main/ui/ui_config.h`

- [ ] **Step 1: Replace UI_AI_DEMO_STEP_MS with per-state constants**

Open `main/ui/ui_config.h`. Remove this line:

```c
#define UI_AI_DEMO_STEP_MS      1600    /* avatar demo state duration */
```

Add these five constants in its place:

```c
#define UI_AI_DEMO_STEP_LISTEN_MS   2000    /* listening state dwell */
#define UI_AI_DEMO_STEP_UPLOAD_MS   1200    /* uploading state dwell */
#define UI_AI_DEMO_STEP_THINK_MS    2000    /* thinking state dwell */
#define UI_AI_DEMO_STEP_SPEAK_MS    2400    /* speaking state dwell */
#define UI_AI_DEMO_STEP_ERROR_MS    1600    /* error state dwell */
```

- [ ] **Step 2: Build to verify the header change compiles**

Run from the `tft_touch_s3` directory with ESP-IDF activated:

```bash
idf.py build
```

Expected: build **fails** with an error like `'UI_AI_DEMO_STEP_MS' undeclared` — because `ui_page_ai.c` still references it. This confirms the constant was removed correctly. Proceed to Task 2 to fix the reference.

- [ ] **Step 3: Do not commit yet** — Task 2 will fix the compile error and then both files are committed together.

---

## Task 2: Rewrite ui_page_ai.c

**Files:**
- Modify: `main/ui/ui_page_ai.c`

This is the core task. Work through the file top-to-bottom.

### Step 1: Update the enum

- [ ] Replace the existing `ai_demo_state_t` enum:

```c
typedef enum {
    AI_DEMO_STATE_IDLE,
    AI_DEMO_STATE_LISTENING,
    AI_DEMO_STATE_THINKING,
    AI_DEMO_STATE_SPEAKING,
    AI_DEMO_STATE_ERROR,
} ai_demo_state_t;
```

With:

```c
typedef enum {
    AI_DEMO_STATE_IDLE,
    AI_DEMO_STATE_LISTENING,
    AI_DEMO_STATE_UPLOADING,
    AI_DEMO_STATE_THINKING,
    AI_DEMO_STATE_SPEAKING,
    AI_DEMO_STATE_ERROR,
    AI_DEMO_STATE_COUNT,
} ai_demo_state_t;
```

`AI_DEMO_STATE_UPLOADING` goes between `LISTENING` and `THINKING`. `AI_DEMO_STATE_COUNT` enables compile-time array sizing.

### Step 2: Add static data tables

- [ ] After the enum, add the color struct typedef and four static tables. Insert this block before the `static lv_obj_t *s_avatar_cont;` line:

```c
typedef struct {
    lv_palette_t palette;
    uint8_t      lighten;
} state_color_t;

static const char *const s_state_frames[AI_DEMO_STATE_COUNT][4] = {
    /* IDLE      */ {"^_^", "-_-", NULL, NULL},
    /* LISTENING */ {"o_o", "O_O", NULL, NULL},
    /* UPLOADING */ {"._.", ">._", "._<", NULL},
    /* THINKING  */ {"-_-", "...", "._.", NULL},
    /* SPEAKING  */ {"^o^", "^O^", NULL, NULL},
    /* ERROR     */ {"x_x", NULL,  NULL, NULL},
};

static const uint8_t s_frame_counts[AI_DEMO_STATE_COUNT] = {2, 2, 3, 3, 2, 1};

static const state_color_t s_state_colors[AI_DEMO_STATE_COUNT] = {
    /* IDLE      */ {LV_PALETTE_BLUE,   4},
    /* LISTENING */ {LV_PALETTE_GREEN,  3},
    /* UPLOADING */ {LV_PALETTE_PURPLE, 3},
    /* THINKING  */ {LV_PALETTE_AMBER,  3},
    /* SPEAKING  */ {LV_PALETTE_CYAN,   3},
    /* ERROR     */ {LV_PALETTE_RED,    3},
};

static const char *const s_state_status[AI_DEMO_STATE_COUNT] = {
    "Ready", "Listening", "Uploading", "Thinking", "Speaking", "Error",
};

static const char *const s_state_caption[AI_DEMO_STATE_COUNT] = {
    "Tap wake to chat", "Say a command", "Sending audio...",
    "Working on it",    "Here is a reply", "Try again",
};
```

### Step 3: Delete the three helper functions and fix their call site in init

- [ ] Delete these three functions entirely — they are replaced by the tables above:

```c
static const char *ai_state_face(ai_demo_state_t state) { ... }
static const char *ai_state_status(ai_demo_state_t state) { ... }
static const char *ai_state_caption(ai_demo_state_t state) { ... }
```

- [ ] In `ui_page_ai_init()`, find the call to `ai_state_face()` (line ~213):

```c
lv_label_set_text_static(s_face_label, ai_state_face(AI_DEMO_STATE_IDLE));
```

Replace it with a direct table lookup:

```c
lv_label_set_text_static(s_face_label, s_state_frames[AI_DEMO_STATE_IDLE][0]);
```

**Do not build between Step 3 and Step 4** — the old `ai_page_render()` still calls the deleted helpers. Complete Steps 3, 4, 5, 6, 7, 8 before running the build in Step 9.

### Step 4: Rewrite ai_page_render()

- [ ] Replace the existing `ai_page_render()` body:

```c
static void ai_page_render(void)
{
    if (s_face_label == NULL) {
        return;
    }

    lv_label_set_text_static(s_face_label, ai_state_face(s_demo_state));
    lv_label_set_text_static(s_status_label, ai_state_status(s_demo_state));
    lv_label_set_text(s_caption_label,
                      s_caption_text[0] != '\0' ? s_caption_text : ai_state_caption(s_demo_state));

    if (s_wake_btn_label != NULL) {
        lv_label_set_text_static(s_wake_btn_label,
                                 s_demo_state == AI_DEMO_STATE_IDLE ? "Wake Demo" : "Restart");
    }
}
```

With:

```c
static void ai_page_render(void)
{
    if (s_face_label == NULL) {
        return;
    }

    const char *frame = s_state_frames[s_demo_state][s_anim_tick % s_frame_counts[s_demo_state]];
    lv_label_set_text_static(s_face_label, frame);
    lv_label_set_text_static(s_status_label, s_state_status[s_demo_state]);
    lv_label_set_text(s_caption_label,
                      s_caption_text[0] != '\0' ? s_caption_text : s_state_caption[s_demo_state]);

    if (s_wake_btn_label != NULL) {
        lv_label_set_text_static(s_wake_btn_label,
                                 s_demo_state == AI_DEMO_STATE_IDLE ? "Wake Demo" : "Restart");
    }
}
```

### Step 5: Rewrite anim_timer_cb()

- [ ] Replace the existing `anim_timer_cb()`:

```c
static void anim_timer_cb(lv_timer_t *t)
{
    (void)t;

    if (s_avatar_cont == NULL) {
        return;
    }

    s_anim_tick++;
    int32_t y_offset = 0;
    lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_BLUE, 4);

    switch (s_demo_state) {
    case AI_DEMO_STATE_LISTENING:
        y_offset = (s_anim_tick % 2) == 0 ? -2 : 2;
        bg_color = lv_palette_lighten(LV_PALETTE_GREEN, 4);
        break;
    case AI_DEMO_STATE_THINKING:
        y_offset = (s_anim_tick % 4) - 1;
        bg_color = lv_palette_lighten(LV_PALETTE_AMBER, 4);
        break;
    case AI_DEMO_STATE_SPEAKING:
        y_offset = (s_anim_tick % 2) == 0 ? -3 : 0;
        bg_color = lv_palette_lighten(LV_PALETTE_CYAN, 4);
        break;
    case AI_DEMO_STATE_ERROR:
        bg_color = lv_palette_lighten(LV_PALETTE_RED, 4);
        break;
    case AI_DEMO_STATE_IDLE:
    default:
        y_offset = (s_anim_tick % 8) == 0 ? -1 : 0;
        break;
    }

    lv_obj_set_style_translate_y(s_avatar_cont, y_offset, 0);
    lv_obj_set_style_bg_color(s_avatar_cont, bg_color, 0);
}
```

With the table-driven version:

```c
static void anim_timer_cb(lv_timer_t *t)
{
    (void)t;

    if (s_avatar_cont == NULL) {
        return;
    }

    s_anim_tick++;

    const state_color_t *c = &s_state_colors[s_demo_state];
    lv_obj_set_style_bg_color(s_avatar_cont, lv_palette_lighten(c->palette, c->lighten), 0);
    lv_obj_set_style_border_color(s_avatar_cont, lv_palette_main(c->palette), 0);

    int32_t y_offset = 0;
    switch (s_demo_state) {
    case AI_DEMO_STATE_LISTENING:
    case AI_DEMO_STATE_UPLOADING:
        y_offset = (s_anim_tick % 2) == 0 ? -2 : 2;
        break;
    case AI_DEMO_STATE_THINKING:
        y_offset = (s_anim_tick % 4) - 1;
        break;
    case AI_DEMO_STATE_SPEAKING:
        y_offset = (s_anim_tick % 2) == 0 ? -3 : 0;
        break;
    case AI_DEMO_STATE_IDLE:
        y_offset = (s_anim_tick % 8) == 0 ? -1 : 0;
        break;
    default:
        break;
    }
    lv_obj_set_style_translate_y(s_avatar_cont, y_offset, 0);

    ai_page_render();
}
```

Note: `ai_page_render()` is now called from `anim_timer_cb` so the face frame updates every animation tick. Remove any separate call to `ai_page_render()` from `ai_state_set()` — it is no longer needed there since the anim timer drives updates. Keep `ai_state_set()` only updating `s_demo_state` and calling `ai_caption_set()`.

### Step 6: Rewrite demo_timer_cb()

- [ ] Replace the existing `demo_timer_cb()`:

```c
static void demo_timer_cb(lv_timer_t *t)
{
    switch (s_demo_state) {
    case AI_DEMO_STATE_LISTENING:
        ai_state_set(AI_DEMO_STATE_THINKING, NULL);
        break;
    case AI_DEMO_STATE_THINKING:
        ai_state_set(AI_DEMO_STATE_SPEAKING, NULL);
        break;
    case AI_DEMO_STATE_SPEAKING:
        ai_state_set(AI_DEMO_STATE_ERROR, NULL);
        break;
    case AI_DEMO_STATE_ERROR:
        ai_state_set(AI_DEMO_STATE_IDLE, NULL);
        lv_timer_pause(t);
        break;
    case AI_DEMO_STATE_IDLE:
    default:
        ai_state_set(AI_DEMO_STATE_IDLE, NULL);
        lv_timer_pause(t);
        break;
    }
}
```

With the version that adds `UPLOADING` and sets per-state periods:

```c
static void demo_timer_cb(lv_timer_t *t)
{
    switch (s_demo_state) {
    case AI_DEMO_STATE_LISTENING:
        lv_timer_set_period(t, UI_AI_DEMO_STEP_UPLOAD_MS);
        ai_state_set(AI_DEMO_STATE_UPLOADING, NULL);
        break;
    case AI_DEMO_STATE_UPLOADING:
        lv_timer_set_period(t, UI_AI_DEMO_STEP_THINK_MS);
        ai_state_set(AI_DEMO_STATE_THINKING, NULL);
        break;
    case AI_DEMO_STATE_THINKING:
        lv_timer_set_period(t, UI_AI_DEMO_STEP_SPEAK_MS);
        ai_state_set(AI_DEMO_STATE_SPEAKING, NULL);
        break;
    case AI_DEMO_STATE_SPEAKING:
        lv_timer_set_period(t, UI_AI_DEMO_STEP_ERROR_MS);
        ai_state_set(AI_DEMO_STATE_ERROR, NULL);
        break;
    case AI_DEMO_STATE_ERROR:
        ai_state_set(AI_DEMO_STATE_IDLE, NULL);
        lv_timer_pause(t);
        break;
    case AI_DEMO_STATE_IDLE:
    default:
        ai_state_set(AI_DEMO_STATE_IDLE, NULL);
        lv_timer_pause(t);
        break;
    }
}
```

### Step 7: Update ai_state_set() to not call ai_page_render()

- [ ] Find `ai_state_set()`:

```c
static void ai_state_set(ai_demo_state_t state, const char *caption)
{
    s_demo_state = state;
    ai_caption_set(caption);
    ai_page_render();
}
```

Remove the `ai_page_render()` call — the anim timer now drives rendering:

```c
static void ai_state_set(ai_demo_state_t state, const char *caption)
{
    s_demo_state = state;
    ai_caption_set(caption);
}
```

### Step 8: Update ui_page_ai_init() timer creation

- [ ] Find the demo timer creation line in `ui_page_ai_init()`:

```c
s_demo_timer = lv_timer_create(demo_timer_cb, UI_AI_DEMO_STEP_MS, NULL);
```

Replace with:

```c
s_demo_timer = lv_timer_create(demo_timer_cb, UI_AI_DEMO_STEP_LISTEN_MS, NULL);
```

The initial period is `UI_AI_DEMO_STEP_LISTEN_MS` because `Listening` is always the first active state. The timer starts paused immediately after.

### Step 9: Build

- [ ] Run:

```bash
idf.py build
```

Expected: exit code 0, no warnings about undeclared identifiers or out-of-bounds array access. If build fails, check:
- `UI_AI_DEMO_STEP_MS` is fully removed from `ui_config.h`
- `AI_DEMO_STATE_COUNT` is the last enum value (value = 6)
- All table rows have exactly 6 entries

### Step 10: Commit

- [ ] Run:

```bash
git add main/ui/ui_config.h main/ui/ui_page_ai.c
git commit -m "feat: polish AI avatar - frame table, color table, Uploading state, per-state timing"
```

---

## Task 3: Verify and Optional Hardware Test

**Files:**
- Verify: `main/ui/ui_page_ai.c`
- Verify: `main/ui/ui_config.h`

- [ ] **Step 1: Check git status is clean**

```bash
git status --short
```

Expected: only pre-existing changes (`components/esp_wifi/lib` submodule SHA) remain. No unstaged modifications from this implementation.

- [ ] **Step 2: Optional hardware smoke test**

If an ESP32-S3 board is connected:

```bash
idf.py -p COMx flash monitor
```

Expected behavior:
- AI tab shows `^_^` face with "Ready" status and "Tap wake to chat" caption
- Blue background with blue border (IDLE)
- Tapping "Wake Demo" cycles: Listening (green) → Uploading (purple) → Thinking (amber) → Speaking (cyan) → Error (red) → back to Idle (blue)
- Each state has distinct border color matching its background palette
- Face expression animates between frames (e.g. `o_o` ↔ `O_O` during Listening)
- Dwell times feel distinct: Uploading is quicker (1.2s), Speaking is longest (2.4s)

If hardware is not connected, skip this step and report it as not run.

- [ ] **Step 3: Final fix commit if needed**

If any small fixes were required during verification:

```bash
git add main/ui/ui_page_ai.c main/ui/ui_config.h
git commit -m "fix: polish AI avatar - minor verification fixes"
```
