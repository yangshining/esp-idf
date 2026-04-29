/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include "lvgl.h"
#include "ui_config.h"
#include "ui_page_ai.h"

typedef enum {
    AI_DEMO_STATE_IDLE,
    AI_DEMO_STATE_LISTENING,
    AI_DEMO_STATE_UPLOADING,
    AI_DEMO_STATE_THINKING,
    AI_DEMO_STATE_SPEAKING,
    AI_DEMO_STATE_ERROR,
    AI_DEMO_STATE_COUNT,
} ai_demo_state_t;

typedef struct {
    lv_palette_t palette;
    uint8_t      lighten;
} state_color_t;

static const char *const s_state_frames[AI_DEMO_STATE_COUNT][4] = {
    /* IDLE      */ {"^_^", "-_-", NULL,  NULL},
    /* LISTENING */ {"o_o", "O_O", NULL,  NULL},
    /* UPLOADING */ {"._.", ">._", "._<", NULL},
    /* THINKING  */ {"-_-", "...", "._.", NULL},
    /* SPEAKING  */ {"^o^", "^O^", NULL,  NULL},
    /* ERROR     */ {"x_x", "x_x", "",   ""},    /* blink: 2 on, 2 off */
};

static const uint8_t s_frame_counts[AI_DEMO_STATE_COUNT] = {2, 2, 3, 3, 2, 4};

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

static lv_obj_t *s_avatar_cont;
static lv_obj_t *s_face_label;
static lv_obj_t *s_status_label;
static lv_obj_t *s_caption_label;
static lv_obj_t *s_wake_btn;
static lv_obj_t *s_wake_btn_label;
static lv_timer_t *s_anim_timer;
static lv_timer_t *s_demo_timer;

static ai_demo_state_t s_demo_state = AI_DEMO_STATE_IDLE;
static uint8_t s_anim_tick;
static char s_caption_text[64];

static void ai_caption_set(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        s_caption_text[0] = '\0';
    } else {
        snprintf(s_caption_text, sizeof(s_caption_text), "%s", text);
    }
}

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

static void ai_state_set(ai_demo_state_t state, const char *caption)
{
    s_demo_state = state;
    ai_caption_set(caption);
}

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
    case AI_DEMO_STATE_UPLOADING:  /* same bobbing motion as LISTENING */
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

static void wake_btn_cb(lv_event_t *e)
{
    (void)e;

    /* Restarting is simpler and keeps repeated taps deterministic. */
    ai_state_set(AI_DEMO_STATE_LISTENING, NULL);
    s_anim_tick = 0;
    lv_timer_set_period(s_demo_timer, UI_AI_DEMO_STEP_LISTEN_MS);
    lv_timer_reset(s_demo_timer);
    lv_timer_resume(s_demo_timer);
    lv_timer_resume(s_anim_timer);
}

void ui_page_ai_init(lv_obj_t *parent)
{
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text_static(title, LV_SYMBOL_IMAGE " AI Assistant");
    lv_obj_set_style_text_align(title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    s_avatar_cont = lv_obj_create(parent);
    lv_obj_set_size(s_avatar_cont, UI_AI_AVATAR_SIZE, UI_AI_AVATAR_SIZE);
    lv_obj_align(s_avatar_cont, LV_ALIGN_TOP_MID, 0, 34);
    lv_obj_set_style_radius(s_avatar_cont, UI_AI_AVATAR_SIZE / 2, 0);
    lv_obj_set_style_border_width(s_avatar_cont, 2, 0);
    lv_obj_set_style_border_color(s_avatar_cont, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_set_style_bg_color(s_avatar_cont, lv_palette_lighten(LV_PALETTE_BLUE, 4), 0);
    lv_obj_set_style_pad_all(s_avatar_cont, 0, 0);

    s_face_label = lv_label_create(s_avatar_cont);
    lv_label_set_text_static(s_face_label, s_state_frames[AI_DEMO_STATE_IDLE][0]);
    lv_obj_set_size(s_face_label, UI_AI_FACE_WIDTH, UI_AI_FACE_HEIGHT);
    lv_obj_set_style_text_font(s_face_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(s_face_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_face_label);

    s_status_label = lv_label_create(parent);
    lv_obj_set_width(s_status_label, UI_AI_STATUS_WIDTH);
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(s_status_label, &lv_font_montserrat_20, 0);
    lv_obj_align(s_status_label, LV_ALIGN_TOP_MID, 0, 170);

    s_caption_label = lv_label_create(parent);
    lv_obj_set_size(s_caption_label, UI_AI_STATUS_WIDTH, 36);
    lv_obj_set_style_text_align(s_caption_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_caption_label, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_caption_label, LV_ALIGN_TOP_MID, 0, 198);

    s_wake_btn = lv_button_create(parent);
    lv_obj_set_size(s_wake_btn, UI_AI_WAKE_BTN_WIDTH, UI_AI_WAKE_BTN_HEIGHT);
    lv_obj_align(s_wake_btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_add_event_cb(s_wake_btn, wake_btn_cb, LV_EVENT_CLICKED, NULL);

    s_wake_btn_label = lv_label_create(s_wake_btn);
    lv_obj_center(s_wake_btn_label);

    s_anim_timer = lv_timer_create(anim_timer_cb, UI_AI_ANIM_PERIOD_MS, NULL);
    s_demo_timer = lv_timer_create(demo_timer_cb, UI_AI_DEMO_STEP_LISTEN_MS, NULL);
    lv_timer_pause(s_demo_timer);

    ai_state_set(AI_DEMO_STATE_IDLE, NULL);
    ai_page_render();
}

void ui_ai_update_result(const char *label, float confidence)
{
    (void)confidence;

    if (s_caption_label == NULL) {
        return;
    }

    if (s_demo_timer != NULL) {
        lv_timer_pause(s_demo_timer);
    }
    if (s_anim_timer != NULL) {
        lv_timer_resume(s_anim_timer);
    }
    ai_state_set(AI_DEMO_STATE_SPEAKING, label);
}
