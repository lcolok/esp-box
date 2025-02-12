# ESP32-S3 湿度检测实现原理

## 概述

在这个项目中，我们使用电位器模拟湿度传感器的输入。电位器通过ADC（模数转换器）连接到ESP32-S3，实现了0-100%的湿度模拟显示。

## 硬件连接

- 电位器连接到 ESP32-S3 的 GPIO11（ADC1_CH0）
- 电位器的三个引脚连接：
  - VCC: 3.3V
  - GND: GND
  - 信号端: GPIO11

## 软件实现

### 1. ADC初始化
```c
esp_err_t app_humidity_drive_init(app_humidity_t *ref)
{
    // 配置ADC单次采样模式
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    
    // 配置ADC通道
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,  // 默认ADC位宽
        .atten = ADC_ATTEN_DB_11,         // 11dB衰减，适用于0-3.3V输入范围
    };
}
```

### 2. 数值读取和转换
```c
int app_humidity_drive_read_value(app_humidity_t *ref)
{
    // 读取ADC原始值（0-4095）
    adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &adc_raw);
    
    // 转换为百分比（0-100%）
    percentage = ((float)adc_raw / 4095.0) * 100.0;
    
    return percentage;
}
```

### 3. 定时更新机制

系统使用FreeRTOS任务来定期读取湿度值：

```c
void humidity_task(void *pvParam)
{
    for (;;) {
        // 读取新值
        int value = app_humidity_drive_read_value(ref);
        
        // 防抖处理
        if (value != cur_humidity) {
            cur_humidity = value;
            debounce_cnt = 0;
        } else {
            debounce_cnt++;
            if (debounce_cnt > APP_HUMIDITY_MAX_DEBOUNCE) {
                // 数值稳定，更新显示
                ref->humidity = cur_humidity;
                // 通知所有观察者
                notify_watchers();
            }
        }
        
        // 每100ms更新一次
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
```

### 4. UI显示

使用LVGL库实现UI显示：

```c
// 创建显示标签
lv_obj_t *label_rh_value = lv_label_create(page);
lv_obj_set_style_text_font(label_rh_value, &lv_font_montserrat_28, 0);
lv_label_set_text_fmt(label_rh_value, "   %2d%%", humidity_value);

// 注册回调更新显示
app_humidity_add_watcher(label_humidity_event_send, label_rh_value);
```

## 工作原理

1. **ADC采样**：
   - ESP32-S3的ADC以12位分辨率采样电位器输入
   - 输入范围0-3.3V被映射到0-4095的数字值

2. **数值转换**：
   - ADC原始值（0-4095）被线性映射到湿度百分比（0-100%）
   - 转换公式：humidity = (adc_raw / 4095) * 100

3. **防抖机制**：
   - 连续读取到相同值2次后才更新显示
   - 避免由于ADC噪声导致的数值抖动

4. **更新频率**：
   - ADC采样频率：100ms一次
   - 实际显示更新：需要至少200ms（防抖时间）

5. **观察者模式**：
   - 使用观察者模式通知UI更新
   - 支持多个观察者同时监听湿度变化

## 注意事项

1. ADC输入电压不能超过3.3V
2. 电位器应该使用线性电位器以获得线性的百分比显示
3. 防抖计数可以根据实际需求调整
4. 更新频率可以通过修改任务延时来调整

## 可能的改进

1. 添加湿度值平滑处理
2. 实现非线性映射曲线
3. 添加校准功能
4. 增加异常值检测和处理
5. 添加数据记录和趋势显示功能
