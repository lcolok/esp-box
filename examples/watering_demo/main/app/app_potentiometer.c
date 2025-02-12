#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "app_potentiometer.h"

static const char *TAG = "app_potentiometer";

static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle = NULL;
static int adc_raw;

#define ADC_POTENTIOMETER_CHANNEL ADC_CHANNEL_0  // GPIO11 maps to ADC1_CH0 on ESP32-S3
#define ADC_ATTEN               ADC_ATTEN_DB_11
#define ADC_BITWIDTH           ADC_BITWIDTH_DEFAULT  // Use default bit width

static bool adc_calibration_init(void)
{
    esp_err_t ret;
    bool cali_enable = false;

    adc_cali_handle_t handle = NULL;
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .atten = ADC_ATTEN,
        .bitwidth = ADC_BITWIDTH,
    };
    ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
    if (ret == ESP_OK) {
        cali_enable = true;
        adc1_cali_handle = handle;
        ESP_LOGI(TAG, "ADC calibration success");
    } else {
        ESP_LOGW(TAG, "ADC calibration failed");
    }

    return cali_enable;
}

esp_err_t app_potentiometer_init(void)
{
    esp_err_t ret;
    
    // ADC1 Init
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ret = adc_oneshot_new_unit(&init_config1, &adc1_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC1 init failed");
        return ret;
    }

    // ADC1 Config
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH,
        .atten = ADC_ATTEN,
    };
    ret = adc_oneshot_config_channel(adc1_handle, ADC_POTENTIOMETER_CHANNEL, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC1 channel config failed");
        return ret;
    }

    // ADC Calibration Init
    adc_calibration_init();

    ESP_LOGI(TAG, "ADC1 init and config success");
    return ESP_OK;
}

potentiometer_data_t app_potentiometer_read_value(void)
{
    potentiometer_data_t data = {0};
    esp_err_t ret;

    // Read raw ADC value
    ret = adc_oneshot_read(adc1_handle, ADC_POTENTIOMETER_CHANNEL, &adc_raw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "ADC1 read failed");
        return data;
    }

    data.adc_raw = adc_raw;
    data.percentage = ((float)adc_raw / 4095.0) * 100.0;

    // Convert to voltage
    if (adc1_cali_handle) {
        int voltage_mv;
        ret = adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage_mv);
        if (ret == ESP_OK) {
            data.voltage = voltage_mv / 1000.0f;  // Convert mV to V
        }
    } else {
        // If calibration failed, use simple conversion
        data.voltage = (adc_raw * 3.3f) / 4095.0f;
    }

    ESP_LOGI(TAG, "ADC Raw: %d, Voltage: %.2fV, Percentage: %.1f%%", 
             data.adc_raw, data.voltage, data.percentage);

    return data;
}
