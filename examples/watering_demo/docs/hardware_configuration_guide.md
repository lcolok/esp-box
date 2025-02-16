# ESP-BOX 浇水演示项目硬件配置指南

本文档详细说明了浇水演示项目中的硬件配置逻辑，包括 GPIO 配置、ADC 设置以及各个外设的初始化流程。

## 硬件组件概览

项目包含三个主要硬件组件：
1. 水泵控制（数字输出）
2. 土壤湿度传感器（ADC 输入）
3. 电位器（ADC 输入）

## 配置流程

### 1. 水泵控制配置

水泵使用标准 GPIO 输出控制，配置逻辑如下：

```c
gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE,      // 禁用中断
    .mode = GPIO_MODE_OUTPUT,            // 设置为输出模式
    .pin_bit_mask = 1ULL << gpio_num,    // 设置 GPIO 位掩码
    .pull_down_en = GPIO_PULLDOWN_DISABLE, // 禁用下拉
    .pull_up_en = GPIO_PULLUP_DISABLE     // 禁用上拉
};
gpio_config(&io_conf);
```

控制方法：
- 开启：gpio_set_level(gpio_num, active_level)
- 关闭：gpio_set_level(gpio_num, !active_level)

### 2. 土壤湿度传感器配置

土壤湿度传感器使用 ADC2 进行采样，配置过程如下：

```c
// 1. ADC 单元初始化
adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_2,
};
adc_oneshot_new_unit(&init_config, &adc1_handle);

// 2. ADC 通道配置
adc_oneshot_chan_cfg_t config = {
    .bitwidth = adc_width,    // ADC 位宽
    .atten = adc_atten,       // ADC 衰减
};
adc_oneshot_config_channel(adc1_handle, adc_channel, &config);
```

采样特性：
- 采样次数：32 次取平均值
- 电压范围：0-3300mV
- 数值映射：0-100% (湿度)

### 3. 电位器配置

电位器使用 ADC1 的 Channel 0 (GPIO11)，配置过程如下：

```c
// 1. ADC 单元初始化
adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
};
adc_oneshot_new_unit(&init_config, &adc1_handle);

// 2. ADC 通道配置
adc_oneshot_chan_cfg_t config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,
    .atten = ADC_ATTEN_DB_11,
};
adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &config);
```

采样特性：
- 输入范围：0-3.3V
- 原始值：0-4095
- 百分比：0-100%

## ADC 校准

为提高 ADC 读数精度，项目实现了多种校准方法：

1. 双点校准：
```c
if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_TP) == ESP_OK) {
    // 支持双点校准
}
```

2. eFuse 参考电压校准：
```c
if (esp_adc_cal_check_efuse(ESP_ADC_CAL_VAL_EFUSE_VREF) == ESP_OK) {
    // 支持 eFuse 参考电压校准
}
```

3. 曲线拟合校准：
```c
adc_cali_curve_fitting_config_t cali_config = {
    .unit_id = ADC_UNIT_1,
    .atten = ADC_ATTEN,
    .bitwidth = ADC_BITWIDTH,
};
adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
```

## 使用建议

1. GPIO 配置：
   - 确保选择正确的 GPIO 引脚，避免与其他功能冲突
   - 根据实际需求配置上拉/下拉电阻

2. ADC 使用：
   - 建议进行多次采样取平均值以提高精度
   - 使用校准功能提高读数准确性
   - 注意 ADC 通道的选择，确保与硬件设计匹配

3. 性能优化：
   - 根据实际需求调整采样频率
   - 合理设置 ADC 衰减值以匹配输入电压范围
   - 考虑使用中断方式处理数字输入信号

## 注意事项

1. ADC 使用限制：
   - ADC2 在使用 WiFi 时可能受到影响
   - 不同 ESP32 型号的 ADC 通道分配可能不同

2. GPIO 限制：
   - 部分 GPIO 在启动时有特殊用途，选择引脚时需注意
   - 某些 GPIO 可能与其他外设功能复用

3. 校准注意事项：
   - 校准数据可能因温度变化而偏移
   - 建议定期进行校准或存储校准参数
