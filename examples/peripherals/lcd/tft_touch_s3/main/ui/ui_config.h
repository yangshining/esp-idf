/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

/* Timer intervals */
#define UI_TIMER_HOME_MS        1000    /* home stats refresh period */
#define UI_TIMER_NET_MS         1000    /* network status refresh period */
#define UI_AI_DEMO_STEP_LISTEN_MS   2000    /* listening state dwell */
#define UI_AI_DEMO_STEP_UPLOAD_MS   1200    /* uploading state dwell */
#define UI_AI_DEMO_STEP_THINK_MS    2000    /* thinking state dwell */
#define UI_AI_DEMO_STEP_SPEAK_MS    2400    /* speaking state dwell */
#define UI_AI_DEMO_STEP_ERROR_MS    1600    /* error state dwell */

/* AI avatar animation */
#define UI_AI_ANIM_PERIOD_MS    120     /* avatar animation tick period */

/* Widget sizes */
#define UI_CHART_POINTS         30
#define UI_CHART_WIDTH          210
#define UI_CHART_HEIGHT         80
#define UI_AI_AVATAR_SIZE       128
#define UI_AI_FACE_WIDTH        96
#define UI_AI_FACE_HEIGHT       64
#define UI_AI_STATUS_WIDTH      210
#define UI_AI_WAKE_BTN_WIDTH    160
#define UI_AI_WAKE_BTN_HEIGHT   40
#define UI_SLIDER_WIDTH         180
#define UI_NET_LABEL_WIDTH      220
#define UI_NET_BTN_WIDTH        160
#define UI_NET_BTN_HEIGHT       40
#define UI_BACKLIGHT_MIN        10   /* lowest slider value — avoids black screen */
