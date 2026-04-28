/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#include <stddef.h>
#include <stdint.h>
#include "lvgl.h"
#include "ui_config.h"
#include "ui_page_ai.h"

typedef enum {
    AI_DEMO_STATE_IDLE,
    AI_DEMO_STATE_LISTENING,
    AI_DEMO_STATE_THINKING,
    AI_DEMO_STATE_SPEAKING,
    AI_DEMO_STATE_ERROR,
} ai_demo_state_t;

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

static const char *ai_state_face(ai_demo_state_t state)
{
    switch (state) {
    case AI_DEMO_STATE_LISTENING:
        return "o_o";
    case AI_DEMO_STATE_THINKING:
        return "-_-";
    case AI_DEMO_STATE_SPEAKING:
        return "^o^";
    case AI_DEMO_STATE_ERROR:
        return "x_x";
    case AI_DEMO_STATE_IDLE:
    default:
        return "^_^";
    }
}

static const char *ai_state_status(ai_demo_state_t state)
{
    switch (state) {
    case AI_DEMO_STATE_LISTENING:
        return "Listening";
    case AI_DEMO_STATE_THINKING:
        return "Thinking";
    case AI_DEMO_STATE_SPEAKING:
        return "Speaking";
    case AI_DEMO_STATE_ERROR:
        return "Error";
    case AI_DEMO_STATE_IDLE:
    default:
        return "Ready";
    }
}

static const char *ai_state_caption(ai_demo_state_t state)
{
    switch (state) {
    case AI_DEMO_STATE_LISTENING:
        return "Say a command";
    case AI_DEMO_STATE_THINKING:
        return "Working on it";
    case AI_DEMO_STATE_SPEAKING:
        return "Here is a reply";
    case AI_DEMO_STATE_ERROR:
        return "Try again";
    case AI_DEMO_STATE_IDLE:
    default:
        return "Tap wake to chat";
    }
}

static void ai_caption_set(const char *text)
{
    if (text == NULL || text[0] == '\0') {
        s_caption_text[0] = '\0';
        return;
    }

    size_t i = 0;
    while (i < (sizeof(s_caption_text) - 1) && text[i] != '\0') {
        s_caption_text[i] = text[i];
        i++;
    }
    s_caption_text[i] = '\0';
}

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

static void ai_state_set(ai_demo_state_t state, const char *caption)
{
    s_demo_state = state;
    ai_caption_set(caption);
    ai_page_render();
}

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

static void wake_btn_cb(lv_event_t *e)
{
    (void)e;

    /* Restarting is simpler and keeps repeated taps deterministic. */
    ai_state_set(AI_DEMO_STATE_LISTENING, NULL);
    s_anim_tick = 0;
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
    lv_label_set_text_static(s_face_label, ai_state_face(AI_DEMO_STATE_IDLE));
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
    lv_obj_set_size(s_caption_label, UI_AI_STATUS_WIDTH, 22);
    lv_obj_set_style_text_align(s_caption_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_caption_label, LV_LABEL_LONG_MODE_DOTS);
    lv_obj_align(s_caption_label, LV_ALIGN_TOP_MID, 0, 202);

    s_wake_btn = lv_button_create(parent);
    lv_obj_set_size(s_wake_btn, UI_AI_WAKE_BTN_WIDTH, UI_AI_WAKE_BTN_HEIGHT);
    lv_obj_align(s_wake_btn, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_add_event_cb(s_wake_btn, wake_btn_cb, LV_EVENT_CLICKED, NULL);

    s_wake_btn_label = lv_label_create(s_wake_btn);
    lv_obj_center(s_wake_btn_label);

    s_anim_timer = lv_timer_create(anim_timer_cb, UI_AI_ANIM_PERIOD_MS, NULL);
    s_demo_timer = lv_timer_create(demo_timer_cb, UI_AI_DEMO_STEP_MS, NULL);
    lv_timer_pause(s_demo_timer);

    ai_state_set(AI_DEMO_STATE_IDLE, NULL);
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
