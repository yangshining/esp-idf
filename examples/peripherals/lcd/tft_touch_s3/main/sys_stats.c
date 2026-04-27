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
