/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <dirent.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "image_display.h"

static const char *TAG = "main";
static lv_obj_t *g_label_adc = NULL;
static adc_oneshot_unit_handle_t adc2_handle;
static uint8_t g_current_pot_value = 64;  // 默认设置为中间值

float convert_adc_to_voltage(int adc_value) {
    return (float)adc_value * ADC_REFERENCE_VOLTAGE / ADC_MAX_VALUE;
}

int read_adc_value(void) {
    int adc_reading = 0;
    int adc_raw = 0;
    
    // Temporarily disable WiFi if it's active (ADC2 limitation)
    wifi_mode_t wifi_mode;
    bool wifi_was_active = false;
    if (esp_wifi_get_mode(&wifi_mode) == ESP_OK) {
        wifi_was_active = (wifi_mode != WIFI_MODE_NULL);
        if (wifi_was_active) {
            esp_wifi_stop();
        }
    }
    
    // Read and average multiple samples
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc2_handle, ADC_CHANNEL, &adc_raw));
        adc_reading += adc_raw;
    }
    
    // Restore WiFi if it was active
    if (wifi_was_active) {
        esp_wifi_start();
    }
    
    adc_reading /= NO_OF_SAMPLES;
    return adc_reading;
}

static void update_display_timer_cb(void *arg) {
    int adc_value = read_adc_value();
    float voltage = convert_adc_to_voltage(adc_value);
    
    // 将ADC值（0-4095）映射到数字电位器值（0-127）
    uint8_t pot_value = (uint8_t)((adc_value * (TPL0401A_MAX_STEPS - 1)) / ADC_MAX_VALUE);
    
    // 设置数字电位器的值
    esp_err_t ret = tpl0401a_set_value(pot_value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set potentiometer value: %s", esp_err_to_name(ret));
    }
    
    uint16_t resistance = tpl0401a_value_to_resistance(g_current_pot_value);
    
    char buf[64];
    snprintf(buf, sizeof(buf), "ADC: %d\nVoltage: %.2fV\nPot: %d ohms", 
             adc_value, voltage, resistance);
    ESP_LOGI(TAG, "%s", buf);
    lv_label_set_text(g_label_adc, buf);
}

void init_adc(void) {
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc2_handle));

    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc2_handle, ADC_CHANNEL, &config));
}

/* I2C Extension Implementation */
esp_err_t init_i2c_extension(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_EXT_SDA_IO,
        .scl_io_num = I2C_EXT_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,  // 启用内部上拉
        .scl_pullup_en = GPIO_PULLUP_ENABLE,  // 启用内部上拉
        .master.clk_speed = I2C_EXT_FREQ_HZ,
    };

    esp_err_t err = i2c_param_config(I2C_EXT_PORT, &conf);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C parameter configuration failed: %s", esp_err_to_name(err));
        return err;
    }

    err = i2c_driver_install(I2C_EXT_PORT, conf.mode, 0, 0, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "I2C driver installation failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "I2C extension initialized on port %d (SDA: %d, SCL: %d)", 
             I2C_EXT_PORT, I2C_EXT_SDA_IO, I2C_EXT_SCL_IO);
    
    return ESP_OK;
}

esp_err_t i2c_extension_scan(void)
{
    ESP_LOGI(TAG, "Scanning I2C bus for devices...");
    
    for (uint8_t i = 1; i < 127; i++) {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (i << 1) | I2C_MASTER_WRITE, true);
        i2c_master_stop(cmd);
        
        esp_err_t ret = i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(1000));
        i2c_cmd_link_delete(cmd);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Found I2C device at address 0x%02X", i);
        }
    }
    
    return ESP_OK;
}

esp_err_t i2c_extension_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_READ, true);
    if (len > 1) {
        i2c_master_read(cmd, data, len - 1, I2C_MASTER_ACK);
    }
    i2c_master_read_byte(cmd, data + len - 1, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

esp_err_t i2c_extension_write(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    if (data == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg, true);
    i2c_master_write(cmd, data, len, true);
    i2c_master_stop(cmd);
    
    esp_err_t ret = i2c_master_cmd_begin(BSP_I2C_NUM, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);
    
    return ret;
}

/* TPL0401A Digital Potentiometer Implementation */
esp_err_t tpl0401a_set_value(uint8_t value)
{
    if (value >= TPL0401A_MAX_STEPS) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data = value;
    esp_err_t ret = i2c_extension_write(I2C_ADDR_TPL0401A, TPL0401A_CMD_WRITE, &data, 1);
    if (ret == ESP_OK) {
        g_current_pot_value = value;  // 更新当前值
    }
    return ret;
}

uint16_t tpl0401a_value_to_resistance(uint8_t value)
{
    // TPL0401A是10K电阻，128档位
    // 当value=0时，电阻为0 ohms
    // 当value=127时，电阻为10K ohms
    return (uint16_t)((uint32_t)value * 10000 / (TPL0401A_MAX_STEPS - 1));
}

/* ADC Expander Implementation */
esp_err_t adc_expander_read_channel(uint8_t channel, uint16_t *value)
{
    if (channel >= 8 || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2];
    esp_err_t ret = i2c_extension_read(I2C_ADDR_ADC_EXPANDER, ADC_EXP_REG_ADC0 + channel, data, 2);
    if (ret != ESP_OK) {
        return ret;
    }

    // 只使用低字节，因为高字节总是0xFF
    *value = data[0];
    
    // 将8位值转换为12位范围
    *value = (*value * 4095) / 255;

    // 计算电压值（mV）
    uint32_t voltage_mv = (*value * 3300) / 4095;
    
    ESP_LOGI(TAG, "ADC Channel %d: raw=0x%02X, value=%d, voltage=%dmV", 
             channel, data[0], *value, voltage_mv);

    // 如果电压小于50mV，认为是接地或未连接
    if (voltage_mv < 50) {
        ESP_LOGW(TAG, "Channel %d appears to be grounded or not connected", channel);
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

esp_err_t adc_expander_read_gpio(uint8_t *value)
{
    if (value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    return i2c_extension_read(I2C_ADDR_ADC_EXPANDER, ADC_EXP_REG_GPIO, value, 1);
}

esp_err_t adc_expander_write_gpio(uint8_t value)
{
    return i2c_extension_write(I2C_ADDR_ADC_EXPANDER, ADC_EXP_REG_GPIO, &value, 1);
}

esp_err_t adc_expander_config(uint8_t config)
{
    return i2c_extension_write(I2C_ADDR_ADC_EXPANDER, ADC_EXP_REG_CONFIG, &config, 1);
}

/* Test function to demonstrate device usage */
void test_devices(void)
{
    // 配置ADC扩展板
    ESP_LOGI(TAG, "Configuring ADC expander...");
    esp_err_t ret = adc_expander_config(ADC_EXP_CONFIG_DEFAULT);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure ADC expander: %s", esp_err_to_name(ret));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // 等待配置生效

    // 测试数字电位器
    ESP_LOGI(TAG, "Testing TPL0401A digital potentiometer...");
    
    // 测试最小值（0 ohms）
    ret = tpl0401a_set_value(0);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "TPL0401A set to minimum: %d ohms", tpl0401a_value_to_resistance(0));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 测试中间值（约5K ohms）
    ret = tpl0401a_set_value(64);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "TPL0401A set to middle: %d ohms", tpl0401a_value_to_resistance(64));
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 测试最大值（10K ohms）
    ret = tpl0401a_set_value(127);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "TPL0401A set to maximum: %d ohms", tpl0401a_value_to_resistance(127));
    }
    vTaskDelay(pdMS_TO_TICKS(100));

    // 测试ADC扩展板的所有通道
    ESP_LOGI(TAG, "Reading all ADC channels (checking for grounded/unconnected inputs):");
    for (int i = 0; i < 8; i++) {
        uint16_t adc_value;
        ret = adc_expander_read_channel(i, &adc_value);
        if (ret == ESP_OK) {
            float voltage = (adc_value * 3.3f) / 4095.0f;
            ESP_LOGI(TAG, "ADC Channel %d: %d (%.2fV)", i, adc_value, voltage);
        } else if (ret == ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "ADC Channel %d: GROUNDED OR NOT CONNECTED", i);
        } else {
            ESP_LOGE(TAG, "Failed to read ADC channel %d: %s", i, esp_err_to_name(ret));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void app_main(void)
{
    /* Initialize I2C (for touch and audio) */
    bsp_i2c_init();

    /* Scan for I2C devices */
    i2c_extension_scan();

    /* Test the I2C devices */
    test_devices();

    /* Initialize display and LVGL */
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size = BSP_LCD_H_RES * CONFIG_BSP_LCD_DRAW_BUF_HEIGHT,
        .double_buffer = 0,
        .flags = {
            .buff_dma = true,
        }
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_backlight_on();

    /* Initialize ADC */
    init_adc();

    /* Create a label for ADC display */
    g_label_adc = lv_label_create(lv_scr_act());
    lv_obj_align(g_label_adc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(g_label_adc, &lv_font_montserrat_14, 0);
    lv_label_set_text(g_label_adc, "ADC: ---\nVoltage: ---V");

    /* Create a timer to update the display */
    esp_timer_handle_t timer_handle;
    esp_timer_create_args_t timer_args = {
        .callback = update_display_timer_cb,
        .name = "display_update"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, DISPLAY_UPDATE_INTERVAL_MS * 1000));
}
