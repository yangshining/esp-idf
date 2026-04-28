# Avatar AI Page Phase 1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current simulated classification AI page with a dynamic local voice-assistant avatar prototype.

**Architecture:** Phase 1 is UI-only and does not add audio, HTTPS, backend, or assistant service modules yet. `ui_page_ai.c` owns a small local demo state machine that cycles through assistant-like states, while `ui_config.h` stores new layout and animation constants. This gives a visible, testable foundation for later real `assistant_state` integration without changing the current connectivity architecture.

**Tech Stack:** ESP-IDF C, LVGL 9.3, FreeRTOS timers through LVGL timers, existing `idf.py build` verification for `esp32s3`.

---

## File Structure

- Modify `main/ui/ui_page_ai.c`
  - Replace the classification result label, confidence bar, and random inference timer with a cartoon avatar UI.
  - Keep public `ui_page_ai_init()` and `ui_ai_update_result()` available so existing callers still compile.
  - Add a local `assistant_demo_state_t` enum for `Idle`, `Listening`, `Thinking`, `Speaking`, and `Error`.
  - Use an LVGL timer for lightweight animation ticks.
  - Use a button to cycle through local demo states.

- Modify `main/ui/ui_config.h`
  - Remove or leave unused inference constants only if doing so is safe.
  - Add UI constants for avatar dimensions, animation period, status/caption width, and wake button dimensions.

- Modify `README.md`
  - Update the AI page description from classification demo to voice assistant avatar prototype.
  - Update the "Integrating AI Results" section to describe the Phase 1 mock UI and future assistant-state path.

- Modify `AGENTS.md`
  - Update the AI page description if it still says the page is only an AI result/confidence bar.
  - Mention that Phase 1 is UI-only and later phases should route non-LVGL work through assistant state.

- Modify `CLAUDE.md` if present
  - Keep module responsibilities aligned with `AGENTS.md`.

## Task 1: Inspect Existing UI Contracts

**Files:**
- Read: `main/ui/ui_page_ai.h`
- Read: `main/ui/ui_page_ai.c`
- Read: `main/ui/ui_config.h`
- Read: `main/ui/ui_main.c`

- [ ] **Step 1: Confirm public AI page API**

Run:

```powershell
Get-Content -LiteralPath main\ui\ui_page_ai.h
```

Expected:

- `ui_page_ai_init()` exists.
- `ui_ai_update_result()` exists or the equivalent public API is visible.

- [ ] **Step 2: Search for callers**

Run:

```powershell
rg "ui_ai_update_result|ui_page_ai_init" -n .
```

Expected:

- `ui_page_ai_init()` is called from `main/ui/ui_main.c`.
- `ui_ai_update_result()` may be referenced in docs and the current AI page implementation.

- [ ] **Step 3: Commit nothing**

This task is read-only.

## Task 2: Add Avatar UI Constants

**Files:**
- Modify: `main/ui/ui_config.h`

- [ ] **Step 1: Add constants for avatar layout and animation**

Add constants similar to:

```c
#define UI_AI_AVATAR_SIZE       128
#define UI_AI_FACE_WIDTH        96
#define UI_AI_FACE_HEIGHT       64
#define UI_AI_STATUS_WIDTH      210
#define UI_AI_WAKE_BTN_WIDTH    160
#define UI_AI_WAKE_BTN_HEIGHT   40
#define UI_AI_ANIM_PERIOD_MS    120
#define UI_AI_DEMO_STEP_MS      1600
```

Keep names under the existing `UI_AI_` or `UI_` style and avoid inline magic numbers in `ui_page_ai.c`.

- [ ] **Step 2: Build check after header edit**

Run:

```powershell
idf.py build
```

Expected:

- Build still succeeds.
- If ESP-IDF is not activated, stop and report that `. .\export.ps1` is required from the ESP-IDF root before building.

- [ ] **Step 3: Commit**

Run:

```powershell
git add main/ui/ui_config.h
git commit -m "feat: add AI avatar UI constants"
```

## Task 3: Replace AI Page With Local Avatar Prototype

**Files:**
- Modify: `main/ui/ui_page_ai.c`

- [ ] **Step 1: Remove classification-only widgets from the page implementation**

Replace the confidence bar, random label list, and simulated inference callbacks with avatar-specific objects:

```c
static lv_obj_t *s_avatar;
static lv_obj_t *s_face_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_caption_label;
static lv_obj_t *s_wake_btn;
static lv_timer_t *s_anim_timer;
static lv_timer_t *s_demo_timer;
```

- [ ] **Step 2: Add local demo state enum**

Add:

```c
typedef enum {
    AI_DEMO_STATE_IDLE,
    AI_DEMO_STATE_LISTENING,
    AI_DEMO_STATE_THINKING,
    AI_DEMO_STATE_SPEAKING,
    AI_DEMO_STATE_ERROR,
} ai_demo_state_t;
```

Keep this private to `ui_page_ai.c`; Phase 2 will replace it with a shared assistant state module.

- [ ] **Step 3: Add a state render function**

Create a helper that maps state to short UI text and avatar expression:

```c
static void render_state(ai_demo_state_t state)
{
    switch (state) {
    case AI_DEMO_STATE_IDLE:
        lv_label_set_text_static(s_face_label, "^_^");
        lv_label_set_text_static(s_status_label, "Ready");
        lv_label_set_text_static(s_caption_label, "Tap to talk");
        break;
    case AI_DEMO_STATE_LISTENING:
        lv_label_set_text_static(s_face_label, "o_o");
        lv_label_set_text_static(s_status_label, "Listening");
        lv_label_set_text_static(s_caption_label, "Ask me something");
        break;
    case AI_DEMO_STATE_THINKING:
        lv_label_set_text_static(s_face_label, "-_-");
        lv_label_set_text_static(s_status_label, "Thinking");
        lv_label_set_text_static(s_caption_label, "Working on it");
        break;
    case AI_DEMO_STATE_SPEAKING:
        lv_label_set_text_static(s_face_label, "^o^");
        lv_label_set_text_static(s_status_label, "Speaking");
        lv_label_set_text_static(s_caption_label, "Playing answer");
        break;
    case AI_DEMO_STATE_ERROR:
        lv_label_set_text_static(s_face_label, "x_x");
        lv_label_set_text_static(s_status_label, "Error");
        lv_label_set_text_static(s_caption_label, "Try again");
        break;
    }
}
```

Final text can be adjusted, but keep it short for the 240x320 screen.

- [ ] **Step 4: Add animation timer**

Use `UI_AI_ANIM_PERIOD_MS` to create a small animation tick. Keep it simple:

- Idle: subtle avatar vertical offset or border color toggle.
- Listening: pulse outline or ring.
- Thinking: cycle caption suffix.
- Speaking: alternate mouth expression.
- Error: no aggressive flashing.

Do not allocate large images or fonts in Phase 1.

- [ ] **Step 5: Add wake/demo button behavior**

Button click should start a local demo sequence:

```text
Idle -> Listening -> Thinking -> Speaking -> Idle
```

Use an LVGL timer with `UI_AI_DEMO_STEP_MS`. If clicked while active, either restart the sequence or ignore the click; document the choice in code if not obvious.

- [ ] **Step 6: Preserve `ui_ai_update_result()` compatibility**

Keep the function defined. For Phase 1, make it update the caption and set the state to `Speaking` or `Idle` without reintroducing the confidence bar:

```c
void ui_ai_update_result(const char *label, float confidence)
{
    (void)confidence;
    if (label != NULL && label[0] != '\0') {
        lv_label_set_text(s_caption_label, label);
    }
    set_demo_state(AI_DEMO_STATE_SPEAKING);
}
```

Guard against `s_caption_label == NULL` if there is any chance this is called before page initialization.

- [ ] **Step 7: Build**

Run:

```powershell
idf.py build
```

Expected:

- No compile errors.
- No missing symbol for `ui_ai_update_result()`.

- [ ] **Step 8: Commit**

Run:

```powershell
git add main/ui/ui_page_ai.c
git commit -m "feat: prototype AI assistant avatar page"
```

## Task 4: Update User-Facing Docs

**Files:**
- Modify: `README.md`
- Modify: `AGENTS.md`
- Modify: `CLAUDE.md` if present

- [ ] **Step 1: Update README AI page summary**

Change the AI bullet from classification result/confidence bar to:

```markdown
- **AI:** Phase 1 voice-assistant avatar prototype with local simulated listening, thinking, speaking, and error states; later phases will connect audio hardware and a backend proxy.
```

- [ ] **Step 2: Update README integration section**

Replace or revise "Integrating AI Results" so it says:

- Phase 1 is local UI simulation only.
- Later work should add `assistant_state` and have non-LVGL tasks update state instead of calling LVGL directly.
- `ui_ai_update_result()` remains only as a compatibility hook.

- [ ] **Step 3: Update AGENTS.md**

Update `main/ui/ui_page_ai.c` responsibility to mention the avatar prototype instead of confidence bar classification.

Add a short note:

```markdown
Phase 1 of the voice assistant is UI-only. Future audio/backend tasks should add an assistant state module and keep network/audio work out of LVGL callbacks.
```

- [ ] **Step 4: Update CLAUDE.md if present**

Run:

```powershell
Test-Path -LiteralPath CLAUDE.md
```

If true, mirror the same module responsibility update.

- [ ] **Step 5: Build after docs-only changes only if code changed since last build**

If Task 3 already built and this task only changes docs, no build is required. Otherwise run:

```powershell
idf.py build
```

- [ ] **Step 6: Commit**

Run:

```powershell
git add README.md AGENTS.md CLAUDE.md
git commit -m "docs: describe AI assistant avatar prototype"
```

If `CLAUDE.md` does not exist, omit it from `git add`.

## Task 5: Final Verification

**Files:**
- Verify: `main/ui/ui_page_ai.c`
- Verify: `main/ui/ui_config.h`
- Verify: `README.md`
- Verify: `AGENTS.md`
- Verify: `CLAUDE.md` if present

- [ ] **Step 1: Run final build**

Run:

```powershell
idf.py build
```

Expected:

- Exit code 0.
- The `tft_touch_s3` app builds for `esp32s3`.

- [ ] **Step 2: Check git status**

Run:

```powershell
git status --short
```

Expected:

- Only known pre-existing changes may remain, such as `components/esp_wifi/lib` or `.superpowers/`.
- No unstaged changes from this implementation.

- [ ] **Step 3: Optional hardware smoke test**

If hardware is connected, flash and monitor:

```powershell
idf.py -p COMx flash monitor
```

Expected:

- The AI tab shows the avatar.
- Tapping the button cycles through listening, thinking, speaking, and idle states.
- Touch remains responsive.

If hardware is not connected, skip this step and report it as not run.

- [ ] **Step 4: Final commit if needed**

If any verification-only fixes were needed, commit them:

```powershell
git add main/ui/ui_page_ai.c main/ui/ui_config.h README.md AGENTS.md CLAUDE.md
git commit -m "fix: polish AI avatar page prototype"
```
