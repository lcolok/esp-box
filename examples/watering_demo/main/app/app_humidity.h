/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include <stdio.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

#define APP_HUMIDITY_MAX_WATCHERS 5  // 统一为5

typedef void (*app_humidity_cb_t)(void *args);

typedef struct {
    void *args;
    app_humidity_cb_t cb;
} app_humidity_watcher_t;

typedef struct {
    int humidity;           // 当前湿度值
    int display_value;      // 用于显示的值，可以快速更新
    
    // ADC 配置
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    adc_oneshot_unit_handle_t adc1_handle;
    adc_cali_handle_t adc_cali_handle;
#else
    esp_adc_cal_characteristics_t *adc_chars;
#endif
    adc_channel_t adc_channel;
    adc_atten_t adc_atten;
    adc_bitwidth_t adc_width;
    
    // 任务和回调
    TaskHandle_t task_handle;
    app_humidity_watcher_t watchers[APP_HUMIDITY_MAX_WATCHERS];
} app_humidity_t;

// API函数
esp_err_t app_humidity_init(void);
esp_err_t app_humidity_add_watcher(app_humidity_cb_t cb, void *args);
esp_err_t app_humidity_del_watcher(app_humidity_cb_t cb, void *args);
int app_humidity_get_value(void);
int app_humidity_get_display_value(void);  // 获取显示值

#ifdef __cplusplus
}
#endif
