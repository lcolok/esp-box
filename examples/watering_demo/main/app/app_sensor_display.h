#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 数值显示配置结构体
 */
typedef struct {
    float scale_factor;      // 缩放因子
    float offset;           // 偏移量
    int decimal_places;     // 小数位数显示精度
    const char* unit;       // 显示单位
    const char* name;       // 显示名称
    bool (*validate)(float value);  // 可选的验证函数
} value_config_t;

/**
 * @brief 数据处理器结构体
 */
typedef struct {
    float values[3];        // 移动平均窗口
    int index;             // 当前索引
    bool initialized;      // 初始化标志
    value_config_t config; // 处理器配置
} data_processor_t;

/**
 * @brief 初始化数据处理器
 * @param processor 处理器指针
 * @param config 配置结构体
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t sensor_display_init_processor(data_processor_t *processor, value_config_t config);

/**
 * @brief 更新数据并获取处理后的值
 * @param processor 处理器指针
 * @param raw_value 原始值
 * @param processed_value 处理后的值
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t sensor_display_update_value(data_processor_t *processor, float raw_value, float *processed_value);

/**
 * @brief 格式化处理后的值到字符串
 * @param processor 处理器指针
 * @param value 处理后的值
 * @param output 输出缓冲区
 * @param output_size 缓冲区大小
 * @return ESP_OK 成功，其他值表示失败
 */
esp_err_t sensor_display_format_value(data_processor_t *processor, float value, char *output, size_t output_size);

#ifdef __cplusplus
}
#endif
