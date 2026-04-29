# AI Avatar Phase 1 Polish Design

## Context

Phase 1 of the voice AI assistant avatar is complete. `ui_page_ai.c` implements a local demo state machine with `Idle`, `Listening`, `Thinking`, `Speaking`, and `Error` states. The animation uses a single LVGL timer with an `s_anim_tick` counter driving per-state background color and Y-offset changes. Face expressions are ASCII emoticons set inline inside the switch-case. Demo state duration is uniform at `UI_AI_DEMO_STEP_MS` (1600 ms).

This document covers the Phase 1 polish work: richer multi-frame animation, improved color contrast, addition of the missing `Uploading` state, per-state demo timing, and factoring animation data out of logic into static tables.

## Goals

- Introduce multi-frame ASCII emoticon animation driven by a static frame table, not inline switch-case logic.
- Add `Uploading` state to both the enum and the demo sequence.
- Improve per-state color contrast by also changing the border color in addition to the background.
- Give each demo state its own configurable dwell time via `ui_config.h` constants.
- Keep all changes inside `main/ui/ui_page_ai.c` and `main/ui/ui_config.h`; no new files, no API changes.
- Remain buildable for `esp32s3` with `idf.py build`.

## Non-Goals

- No `lv_anim_t` easing or opacity transitions (reserved for Phase 2+).
- No real audio, backend, or `assistant_state` module changes.
- No changes to `ui_page_ai.h` public API.
- No changes outside `main/ui/`.

## State Enum

Add `AI_DEMO_STATE_UPLOADING` between `LISTENING` and `THINKING`:

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

`AI_DEMO_STATE_COUNT` enables compile-time array sizing.

## Frame Table

Replace per-state inline emoticon strings with a static 2-D frame table and a frame-count array:

```c
static const char *const s_state_frames[AI_DEMO_STATE_COUNT][4] = {
    /* IDLE      */ {"^_^", "-_-", NULL, NULL},
    /* LISTENING */ {"o_o", "O_O", NULL, NULL},
    /* UPLOADING */ {"._.", ">._", "._<", NULL},
    /* THINKING  */ {"-_-", "...", "._.", NULL},
    /* SPEAKING  */ {"^o^", "^O^", NULL, NULL},
    /* ERROR     */ {"x_x", NULL,  NULL, NULL},
};
static const uint8_t s_frame_counts[AI_DEMO_STATE_COUNT] = {2, 2, 3, 3, 2, 1};
```

`anim_timer_cb` indexes into the table with `s_anim_tick % s_frame_counts[s_demo_state]`. No switch-case on frame content.

## Color Table

Add a static color-pair table for background and border. Each entry maps to an `lv_palette_t` and lighten level:

```c
typedef struct {
    lv_palette_t palette;
    uint8_t      lighten;
} state_color_t;

static const state_color_t s_state_colors[AI_DEMO_STATE_COUNT] = {
    /* IDLE      */ {LV_PALETTE_BLUE,   4},
    /* LISTENING */ {LV_PALETTE_GREEN,  3},
    /* UPLOADING */ {LV_PALETTE_PURPLE, 3},
    /* THINKING  */ {LV_PALETTE_AMBER,  3},
    /* SPEAKING  */ {LV_PALETTE_CYAN,   3},
    /* ERROR     */ {LV_PALETTE_RED,    3},
};
```

`anim_timer_cb` calls:
```c
lv_obj_set_style_bg_color(s_avatar_cont,
    lv_palette_lighten(c->palette, c->lighten), 0);
lv_obj_set_style_border_color(s_avatar_cont,
    lv_palette_main(c->palette), 0);
```

Border color was previously fixed; this adds per-state contrast.

## Status and Caption Table

Replace the three `ai_state_*()` helper functions with two static string arrays:

```c
static const char *const s_state_status[AI_DEMO_STATE_COUNT] = {
    "Ready", "Listening", "Uploading", "Thinking", "Speaking", "Error",
};
static const char *const s_state_caption[AI_DEMO_STATE_COUNT] = {
    "Tap wake to chat", "Say a command", "Sending audio...",
    "Working on it",    "Here is a reply", "Try again",
};
```

`ai_page_render()` reads from these arrays directly.

## Per-State Demo Timing

Replace uniform `UI_AI_DEMO_STEP_MS` with individual constants in `ui_config.h`:

```c
#define UI_AI_DEMO_STEP_LISTEN_MS   2000
#define UI_AI_DEMO_STEP_UPLOAD_MS   1200
#define UI_AI_DEMO_STEP_THINK_MS    2000
#define UI_AI_DEMO_STEP_SPEAK_MS    2400
#define UI_AI_DEMO_STEP_ERROR_MS    1600
```

`demo_timer_cb` calls `lv_timer_set_period(t, UI_AI_DEMO_STEP_xxx_MS)` before each state transition so each state dwells for its own duration. `UI_AI_DEMO_STEP_MS` is removed from `ui_config.h`.

The demo timer is created paused with `UI_AI_DEMO_STEP_LISTEN_MS` as the initial period (the first active state is always `Listening`). The period is updated by `demo_timer_cb` on each subsequent transition.

## Demo Sequence

Updated full cycle:

```
Idle → Listening → Uploading → Thinking → Speaking → Error → Idle
```

Button click always restarts from `Listening` regardless of current state.

## Wake Button Label

- In `Idle`: label is `"Wake Demo"`
- In any other state: label is `"Restart"`

Behavior unchanged from Phase 1.

## Files Changed

| File | Change |
|------|--------|
| `main/ui/ui_config.h` | Remove `UI_AI_DEMO_STEP_MS`; add 5 per-state step constants |
| `main/ui/ui_page_ai.c` | Add `UPLOADING` to enum; replace helper functions with frame/color/text tables; update `anim_timer_cb`, `demo_timer_cb`, `ai_page_render` |

## Constraints

- Array sizes are compile-time: `[AI_DEMO_STATE_COUNT][4]` for frames, `[AI_DEMO_STATE_COUNT]` for others.
- `ui_ai_update_result()` signature and behavior unchanged.
- All LVGL calls remain on the LVGL task side (timer callbacks run under LVGL task).
- No heap allocation; all tables are `static const`.
