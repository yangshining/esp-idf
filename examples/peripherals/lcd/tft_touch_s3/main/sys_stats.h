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
