# ESP32 硬件接口配置指南

本文档提供了 ESP32 系列芯片常用硬件接口的配置方法和最佳实践。

## 1. 数字接口 (GPIO)

### 1.1 基本 GPIO 配置

```c
gpio_config_t io_conf = {
    .intr_type = GPIO_INTR_DISABLE,      // 中断类型
    .mode = GPIO_MODE_OUTPUT,            // GPIO 模式
    .pin_bit_mask = 1ULL << gpio_num,    // GPIO 位掩码
    .pull_down_en = GPIO_PULLDOWN_DISABLE, // 下拉配置
    .pull_up_en = GPIO_PULLUP_DISABLE     // 上拉配置
};
gpio_config(&io_conf);
```

#### GPIO 模式选项
- 输入模式：`GPIO_MODE_INPUT`
- 输出模式：`GPIO_MODE_OUTPUT`
- 输入输出模式：`GPIO_MODE_INPUT_OUTPUT`
- 开漏输出：`GPIO_MODE_OUTPUT_OD`
- 开漏输入输出：`GPIO_MODE_INPUT_OUTPUT_OD`

#### 中断类型选项
- 禁用中断：`GPIO_INTR_DISABLE`
- 上升沿触发：`GPIO_INTR_POSEDGE`
- 下降沿触发：`GPIO_INTR_NEGEDGE`
- 双边沿触发：`GPIO_INTR_ANYEDGE`
- 低电平触发：`GPIO_INTR_LOW_LEVEL`
- 高电平触发：`GPIO_INTR_HIGH_LEVEL`

### 1.2 GPIO 中断处理

```c
// 安装 GPIO ISR 服务
gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

// 添加 GPIO 中断处理程序
gpio_isr_handler_add(gpio_num, gpio_isr_handler, (void*) gpio_num);

// 中断处理函数示例
void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    // 处理中断
}
```

## 2. 模拟接口 (ADC)

### 2.1 单次采样 ADC 配置

```c
// ADC 单元初始化
adc_oneshot_unit_init_cfg_t init_config = {
    .unit_id = ADC_UNIT_1,               // ADC 单元
    .ulp_mode = ADC_ULP_MODE_DISABLE,    // ULP 模式
};
adc_oneshot_new_unit(&init_config, &adc_handle);

// ADC 通道配置
adc_oneshot_chan_cfg_t config = {
    .bitwidth = ADC_BITWIDTH_DEFAULT,    // 采样位宽
    .atten = ADC_ATTEN_DB_11,           // 衰减设置
};
adc_oneshot_config_channel(adc_handle, adc_channel, &config);
```

### 2.2 连续采样 ADC 配置

```c
// ADC 连续采样配置
adc_continuous_handle_cfg_t adc_config = {
    .max_store_buf_size = 1024,
    .conv_frame_size = 100,
};
adc_continuous_new_handle(&adc_config, &handle);

// ADC 模式配置
adc_continuous_config_t dig_cfg = {
    .sample_freq_hz = 20 * 1000, // 20kHz
    .conv_mode = ADC_CONV_SINGLE_UNIT_1,
    .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
};
adc_continuous_config(handle, &dig_cfg);
```

## 3. 串行接口

### 3.1 I2C 配置

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

### 3.2 SPI 配置

```c
spi_bus_config_t buscfg = {
    .miso_io_num = miso_gpio,
    .mosi_io_num = mosi_gpio,
    .sclk_io_num = sclk_gpio,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 4092,
};

spi_device_interface_config_t devcfg = {
    .clock_speed_hz = 1000000,    // 1 MHz
    .mode = 0,                    // SPI 模式 0
    .spics_io_num = cs_gpio,      // CS 引脚
    .queue_size = 7,
};

spi_bus_initialize(host, &buscfg, SPI_DMA_CH_AUTO);
spi_bus_add_device(host, &devcfg, &handle);
```

## 4. PWM (LEDC) 配置

```c
ledc_timer_config_t ledc_timer = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .timer_num = LEDC_TIMER_0,
    .duty_resolution = LEDC_TIMER_13_BIT,
    .freq_hz = 5000,                    // 5 KHz
    .clk_cfg = LEDC_AUTO_CLK
};
ledc_timer_config(&ledc_timer);

ledc_channel_config_t ledc_channel = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .channel = LEDC_CHANNEL_0,
    .timer_sel = LEDC_TIMER_0,
    .intr_type = LEDC_INTR_DISABLE,
    .gpio_num = gpio_num,
    .duty = 0,
    .hpoint = 0
};
ledc_channel_config(&ledc_channel);
```

## 5. 通用配置最佳实践

### 5.1 引脚选择注意事项

1. 启动时的特殊引脚：
   - GPIO 0：下载模式
   - GPIO 2：某些开发板上连接板载 LED
   - GPIO 5：某些开发板上用于 SPI flash
   - GPIO 12：某些模组的 MTDI 引脚，建议不要用作输入

2. 常用外设默认引脚：
   - UART0：GPIO 1 (TX) 和 GPIO 3 (RX)
   - SPI flash：GPIO 6-11（在某些模组上）
   - JTAG：GPIO 12-15（调试时）

### 5.2 性能优化建议

1. ADC 采样优化：
   - 使用 DMA 进行连续采样
   - 实施多次采样平均
   - 根据信号特性选择合适的采样率
   - 使用校准提高精度

2. 中断处理：
   - 中断处理函数应尽可能简短
   - 使用事件队列传递数据
   - 考虑中断优先级

3. 通信接口优化：
   - 选择合适的时钟频率
   - 使用 DMA 进行数据传输
   - 优化数据包大小

### 5.3 电源管理考虑

1. GPIO 功耗优化：
   - 未使用的 GPIO 设置为高阻态
   - 合理使用上下拉电阻
   - 考虑使用 GPIO 唤醒功能

2. ADC 功耗优化：
   - 使用适当的采样频率
   - 不使用时关闭 ADC
   - 考虑使用 ULP 协处理器

## 6. 故障排除

1. GPIO 问题：
   - 检查引脚配置模式
   - 验证中断配置
   - 确认上下拉电阻设置

2. ADC 问题：
   - 检查输入电压范围
   - 验证衰减设置
   - 确认采样配置

3. 通信接口问题：
   - 检查时序参数
   - 验证引脚连接
   - 确认协议配置

## 7. 安全注意事项

1. 电气安全：
   - 注意最大额定电流
   - 保护输入电压范围
   - 添加适当的保护电路

2. 代码安全：
   - 实施看门狗保护
   - 处理异常情况
   - 添加错误检查
