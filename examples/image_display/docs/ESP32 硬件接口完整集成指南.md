# ESP32 硬件接口完整集成指南

## 1. GPIO配置与使用

### 1.1 基础GPIO配置
```c
gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE,      // 中断类型
    .mode = GPIO_MODE_OUTPUT,            // GPIO模式
    .pin_bit_mask = 1ULL << gpio_num,    // GPIO位掩码
    .pull_down_en = GPIO_PULLDOWN_DISABLE, // 下拉配置
    .pull_up_en = GPIO_PULLUP_DISABLE     // 上拉配置
};
gpio_config(&io_conf);
```

### 1.2 中断处理配置
```c
// 安装GPIO ISR服务
gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

// 中断处理函数示例
void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    // 处理中断
}

// 添加GPIO中断处理程序
gpio_isr_handler_add(gpio_num, gpio_isr_handler, (void*) gpio_num);
```

## 2. ADC配置与优化

### 2.1 单次采样配置
```c
// ADC单元初始化
adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_1,
    .ulp_mode = ADC_ULP_MODE_DISABLE,
};
adc_oneshot_new_unit(&init_config, &adc_handle);

// ADC通道配置
adc_oneshot_chan_cfg_t config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,    // 12位分辨率
    .atten = ADC_ATTEN_DB_12,           // 12dB衰减（0-3.3V范围）
};
adc_oneshot_config_channel(adc_handle, adc_channel, &config);
```

### 2.2 优化的多次采样实现
```c
#define NO_OF_SAMPLES 32   // 平均值采样次数

static int read_adc_value(adc_oneshot_unit_handle_t adc_handle, adc_channel_t channel) {
    int adc_reading = 0;
    int adc_raw = 0;
    
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, channel, &adc_raw));
        adc_reading += adc_raw;
    }
    
    adc_reading /= NO_OF_SAMPLES;
    return adc_reading;
}
```

### 2.3 连续采样配置
```c
adc_continuous_handle_cfg_t adc_config = {
    .max_store_buf_size = 1024,
    .conv_frame_size = 100,
};
adc_continuous_new_handle(&adc_config, &handle);

adc_continuous_config_t dig_cfg = {
    .sample_freq_hz = 20 * 1000, // 20kHz
    .conv_mode = ADC_CONV_SINGLE_UNIT_1,
    .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
};
adc_continuous_config(handle, &dig_cfg);
```

### 2.4 ADC使用注意事项
1. GPIO与ADC通道映射
   - ESP32-S3的GPIO11对应ADC2_CHANNEL_0，不是ADC1
   - 使用ADC前需要确认GPIO与ADC单元和通道的对应关系
   - 可以通过ESP-IDF的头文件或技术规格书查询对应关系

2. ADC配置最佳实践
   - 使用ADC_ATTEN_DB_12而不是已弃用的ADC_ATTEN_DB_11
   - 建议使用多次采样取平均值来提高读数稳定性
   - 电压转换时注意参考电压值（通常为3.3V）

3. 常见问题解决
   - 读数不稳定：增加采样次数，检查接地
   - 值异常：验证衰减设置，检查供电
   - 编译错误：确保使用正确的头文件 (esp_adc/adc_oneshot.h)
   - 变量类型：ADC读取函数需要使用int*类型而不是uint32_t*

4. 代码示例
```c
// GPIO11 ADC读取完整示例
#include "esp_adc/adc_oneshot.h"

#define ADC_UNIT ADC_UNIT_2
#define ADC_CHANNEL ADC_CHANNEL_0  // GPIO11 maps to ADC2_CHANNEL_0
#define NO_OF_SAMPLES 32   // 平均采样次数

static adc_oneshot_unit_handle_t adc2_handle;

// 初始化ADC
static void init_adc(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc2_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC_CHANNEL, &config));
}

// 读取ADC值
static int read_adc_value(void) {
    int adc_reading = 0;
    int adc_raw = 0;
    
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, ADC_CHANNEL, &adc_raw));
        adc_reading += adc_raw;
    }
    
    adc_reading /= NO_OF_SAMPLES;
    return adc_reading;
}
```

## 3. 串行接口配置

### 3.1 I2C配置
```c
i2c_config_t conf = {
    .mode = I2C_MODE_MASTER,
    .sda_io_num = sda_gpio,
    .scl_io_num = scl_gpio,
    .sda_pullup_en = GPIO_PULLUP_ENABLE,
    .scl_pullup_en = GPIO_PULLUP_ENABLE,
    .master.clk_speed = 400000,  // 400 KHz
};
i2c_param_config(i2c_num, &conf);
i2c_driver_install(i2c_num, conf.mode, 0, 0, 0);
```

## 9. 常见问题解决

### 9.1 ADC问题
- 读数不稳定：增加采样次数，检查接地
- 值异常：验证衰减设置，检查供电
- 通道映射错误：仔细核对GPIO与ADC通道的对应关系
- 编译错误：确保使用正确的头文件和变量类型

### 9.2 通信问题
- 通信失败：检查连接和时序
- 数据错误：验证协议配置
- 性能不足：优化传输参数

## 10. 故障排除和最佳实践

### 10.1 ADC故障排除
1. 读数不稳定
   - 增加采样次数
   - 检查接地
   - 使用多次采样取平均值

2. 值异常
   - 验证衰减设置
   - 检查供电
   - 确保ADC通道配置正确

3. 通道映射错误
   - 仔细核对GPIO与ADC通道的对应关系
   - 使用ESP-IDF的头文件或技术规格书查询对应关系

### 10.2 通信故障排除
1. 通信失败
   - 检查连接和时序
   - 验证协议配置
   - 确保设备地址正确

2. 数据错误
   - 验证协议配置
   - 检查数据格式
   - 确保数据缓冲区大小足够

3. 性能不足
   - 优化传输参数
   - 使用DMA传输数据
   - 确保设备工作在最佳状态

### 10.3 编程最佳实践
1. 使用正确的头文件
2. 验证变量类型
3. 确保函数调用正确
4. 使用ESP_ERROR_CHECK宏检查错误