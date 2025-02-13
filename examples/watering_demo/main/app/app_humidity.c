/*
 * SPDX-FileCopyrightText: 2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */


#include <stdio.h>
#include "esp_log.h"
#include "esp_check.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "app_humidity.h"

static const char *TAG = "app_humidity";

#define DEFAULT_VREF    1100
#define APP_HUMIDITY_ADC_MAX_INPUT_V 3300

static app_humidity_t s_humidity = {0};

static app_humidity_t *humidity_ref(void)
{
    return &s_humidity;
}

// 添加校准数据结构
typedef struct {
    int min_voltage;
    int max_voltage;
    bool calibrated;
} adc_calibration_data_t;

// 使用实测的固定校准值
static const adc_calibration_data_t FACTORY_CAL = {
    .min_voltage = 0,    // 实测最小电压
    .max_voltage = 3121, // 实测最大电压
    .calibrated = true
};

static adc_calibration_data_t s_cal_data = {
    .min_voltage = 0,
    .max_voltage = 3121,
    .calibrated = true
};

static int voltage2humidity(int v)
{
    int h;
    float p;
    int max_h = 100;
    int min_h = 0;

    // 使用固定的校准范围
    int voltage_range = FACTORY_CAL.max_voltage;

    // 线性映射
    if (v <= FACTORY_CAL.min_voltage) {
        h = max_h;
    } else if (v >= FACTORY_CAL.max_voltage) {
        h = min_h;
    } else {
        p = 1.0 - (float)v / (float)voltage_range;
        h = (int)(p * (max_h - min_h));
    }

    // 确保范围在0-100之间
    if (h > max_h) h = max_h;
    if (h < min_h) h = min_h;

    // 只在达到极值时记录日志
    if (h == 0 || h == 100) {
        ESP_LOGI(TAG, "Reached %d%% (voltage: %d mV)", h, v);
    }

    return h;
}

static int app_humidity_drive_read_value(app_humidity_t *ref)
{
    uint32_t adc_reading = 0;
#define NO_OF_SAMPLES 32
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    int adc_raw = 0;
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(ref->adc1_handle, ref->adc_channel, &adc_raw));
        adc_reading += adc_raw;
    }
#else
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw(ref->adc_channel);
    }
#endif
    adc_reading /= NO_OF_SAMPLES;

    int voltage;
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (ref->adc_cali_handle) {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(ref->adc_cali_handle, adc_reading, &voltage));
    } else {
        voltage = adc_reading * APP_HUMIDITY_ADC_MAX_INPUT_V / 4095;
    }
#else
    voltage = adc_reading * APP_HUMIDITY_ADC_MAX_INPUT_V / 4095;
#endif

    return voltage2humidity(voltage);
}

static esp_err_t app_humidity_drive_init(app_humidity_t *ref)
{
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    if (NULL == ref->adc1_handle) {
        //ADC2 Init
        adc_oneshot_unit_init_cfg_t init_config = {
            .unit_id = ADC_UNIT_2,
        };
        if (adc_oneshot_new_unit(&init_config, &ref->adc1_handle) != ESP_OK) {
            ESP_LOGW(TAG, "adc oneshot new unit fail!");
        }

        //ADC2 Config
        adc_oneshot_chan_cfg_t oneshot_config = {
            .bitwidth = ref->adc_width,
            .atten = ref->adc_atten,
        };
        if (adc_oneshot_config_channel(ref->adc1_handle, ref->adc_channel, &oneshot_config) != ESP_OK) {
            ESP_LOGW(TAG, "adc oneshot config channel fail!");
        }
    }
#else
    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
        ESP_LOGI(TAG, "eFuse Two Point: Supported");
    } else {
        ESP_LOGW(TAG, "eFuse Two Point: NOT supported");
    }

    if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
        ESP_LOGI(TAG, "eFuse Vref: Supported");
    } else {
        ESP_LOGW(TAG, "eFuse Vref: NOT supported");
    }
    /** Configure ADC */
    adc1_config_width(ref->adc_width);
    /** initialize adc channel */
    adc1_config_channel_atten(ref->adc_channel, ref->adc_atten);
    /** Characterize ADC */
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ref->adc_atten, ref->adc_width, DEFAULT_VREF, ref->adc_chars);

    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "Characterized using Two Point Value");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "Characterized using eFuse Vref");
    } else {
        ESP_LOGI(TAG, "Characterized using Default Vref");
    }
#endif
    return ESP_OK;
}

static void humidity_task(void *pvParam)
{
    app_humidity_t *ref = pvParam;

#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
    ref->adc_channel = ADC_CHANNEL_0;
    ref->adc_atten   = ADC_ATTEN_DB_11;//for box s3
    ref->adc_width   = SOC_ADC_RTC_MAX_BITWIDTH;
#else
    ref->adc_channel = ADC2_CHANNEL_0;
    ref->adc_atten   = ADC_ATTEN_DB_11;//for box s3
    ref->adc_width   = ADC_WIDTH_BIT_DEFAULT;
    ref->adc_chars   = calloc(1, sizeof(esp_adc_cal_characteristics_t));
#endif
    app_humidity_drive_init(ref);

    int cur_value = app_humidity_drive_read_value(ref);
    ref->humidity = cur_value;      // MQTT通知值
    ref->display_value = cur_value; // 显示值
    vTaskDelay(pdMS_TO_TICKS(5000));//wait ui

    TickType_t last_notify_time = xTaskGetTickCount();
    const TickType_t notify_interval = pdMS_TO_TICKS(500);  // MQTT通知间隔增加到500ms

    for (;;) {
        int value = app_humidity_drive_read_value(ref);
        
        // 立即更新显示值
        ref->display_value = value;
        
        // MQTT通知使用节流和平滑处理
        TickType_t now = xTaskGetTickCount();
        if ((now - last_notify_time) >= notify_interval) {
            // 如果变化超过阈值才更新MQTT
            if (abs(value - ref->humidity) >= 2) {  // 添加阈值判断
                ref->humidity = value;
                for (int i = 0; i < APP_HUMIDITY_MAX_WATCHERS; i++) {
                    if (ref->watchers[i].cb) {
                        ref->watchers[i].cb(ref->watchers[i].args);
                    }
                }
            }
            last_notify_time = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));  // 显示保持高刷新率
    }
}

esp_err_t app_humidity_init(void)
{
    app_humidity_t *ref = humidity_ref();
    ESP_RETURN_ON_FALSE(ref->task_handle == NULL, ESP_FAIL, TAG, "already init");

    BaseType_t ret_val = xTaskCreatePinnedToCore(
                             (TaskFunction_t)        humidity_task,
                             (const char *const)    "RH Task",
                             (const uint32_t)        5 * 512,
                             (void *const)          ref,
                             (UBaseType_t)           1,
                             (TaskHandle_t *const)  & (ref->task_handle),
                             (const BaseType_t)      0);
    ESP_ERROR_CHECK(ret_val == pdPASS ? ESP_OK : ESP_FAIL);
    return ESP_OK;
}

// 获取显示值的函数
int app_humidity_get_display_value(void)
{
    app_humidity_t *ref = humidity_ref();
    return ref->display_value;
}

// 原有的获取值函数（用于MQTT）保持不变
int app_humidity_get_value(void)
{
    app_humidity_t *ref = humidity_ref();
    return ref->humidity;
}

esp_err_t app_humidity_add_watcher(app_humidity_cb_t cb, void *args)
{
    app_humidity_t *ref = humidity_ref();

    for (int i = 0; i < APP_HUMIDITY_MAX_WATCHERS; i++) {
        if (ref->watchers[i].cb == NULL) {
            ref->watchers[i].cb = cb;
            ref->watchers[i].args = args;
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

esp_err_t app_humidity_del_watcher(app_humidity_cb_t cb, void *args)
{
    app_humidity_t *ref = humidity_ref();

    for (int i = 0; i < APP_HUMIDITY_MAX_WATCHERS; i++) {
        if (ref->watchers[i].cb == cb) {
            ref->watchers[i].cb = NULL;
            ref->watchers[i].args = NULL;
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}
