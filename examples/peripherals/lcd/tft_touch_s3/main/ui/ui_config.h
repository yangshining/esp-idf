/*
 * SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: CC0-1.0
 */
#pragma once

/* ── Timer intervals ─────────────────────────────────────── */
#define UI_TIMER_HOME_MS        1000    /* home stats refresh period */
#define UI_TIMER_NET_MS         1000    /* network status refresh period */
#define UI_INFERENCE_DELAY_MS   1500    /* simulated inference duration */

/* ── AI bar animation (exponential decay) ────────────────── */
#define UI_BAR_ANIM_PERIOD_MS   30      /* interpolation tick period */
#define UI_BAR_ANIM_SPEED       5.0f    /* decay divisor: cur += (tgt-cur)/SPEED */

/* ── Widget sizes ────────────────────────────────────────── */
#define UI_CHART_POINTS         30
#define UI_CHART_WIDTH          210
#define UI_CHART_HEIGHT         80
#define UI_BAR_WIDTH            180
#define UI_BAR_HEIGHT           20
#define UI_SLIDER_WIDTH         180
#define UI_NET_LABEL_WIDTH      220
#define UI_NET_BTN_WIDTH        160
#define UI_NET_BTN_HEIGHT       40
