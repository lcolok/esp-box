#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "app_sensor_display.h"

static const char *TAG = "app_sensor_display";

esp_err_t sensor_display_init_processor(data_processor_t *processor, value_config_t config)
{
    if (!processor) {
        ESP_LOGE(TAG, "Invalid processor pointer");
        return ESP_ERR_INVALID_ARG;
    }

    if (fabsf(config.scale_factor) < 1e-6) {
        ESP_LOGE(TAG, "Invalid scale factor");
        return ESP_ERR_INVALID_ARG;
    }

    // 初始化移动平均窗口
    for (int i = 0; i < 3; i++) {
        processor->values[i] = 0.0f;
    }
    processor->index = 0;
    processor->initialized = false;

    // 复制配置
    processor->config = config;

    ESP_LOGI(TAG, "Initialized processor for %s", config.name);
    return ESP_OK;
}

esp_err_t sensor_display_update_value(data_processor_t *processor, float raw_value, float *processed_value)
{
    if (!processor || !processed_value) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // 应用缩放和偏移
    float scaled_value = raw_value * processor->config.scale_factor + processor->config.offset;

    // 可选的验证
    if (processor->config.validate && !processor->config.validate(scaled_value)) {
        ESP_LOGW(TAG, "Value validation failed for %s: %.2f", processor->config.name, scaled_value);
        return ESP_ERR_INVALID_STATE;
    }

    // 更新移动平均窗口
    processor->values[processor->index] = scaled_value;
    processor->index = (processor->index + 1) % 3;

    // 计算平均值
    float sum = 0.0f;
    for (int i = 0; i < 3; i++) {
        sum += processor->values[i];
    }
    
    if (!processor->initialized) {
        if (processor->index == 0) {
            processor->initialized = true;
        } else {
            sum = sum * 3.0f / (float)processor->index;
        }
    }

    *processed_value = sum / 3.0f;

    ESP_LOGD(TAG, "%s: raw=%.2f, processed=%.2f", 
             processor->config.name, raw_value, *processed_value);

    return ESP_OK;
}

esp_err_t sensor_display_format_value(data_processor_t *processor, float value, char *output, size_t output_size)
{
    if (!processor || !output || output_size == 0) {
        ESP_LOGE(TAG, "Invalid arguments");
        return ESP_ERR_INVALID_ARG;
    }

    // 格式化字符串
    char format[16];
    snprintf(format, sizeof(format), "%%.%df%%s", processor->config.decimal_places);
    
    int written = snprintf(output, output_size, format, value, 
                         processor->config.unit ? processor->config.unit : "");
    
    if (written < 0 || written >= output_size) {
        ESP_LOGE(TAG, "Buffer too small");
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}
