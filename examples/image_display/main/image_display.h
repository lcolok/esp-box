/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include "esp_adc/adc_oneshot.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

/* ADC Configuration */
#define ADC_UNIT ADC_UNIT_2
#define ADC_CHANNEL ADC_CHANNEL_3  // ADC2_CH3 on GPIO14
#define NO_OF_SAMPLES 32   // Average over 32 samples

/* Display Update Configuration */
#define DISPLAY_UPDATE_INTERVAL_MS 100  // Display update interval in milliseconds

/* ADC Conversion Parameters */
#define ADC_REFERENCE_VOLTAGE 3.3f  // Reference voltage in volts
#define ADC_MAX_VALUE 4095         // 12-bit ADC maximum value
#define ADC_ATTEN ADC_ATTEN_DB_12  // 12dB attenuation for 0-3.6V range

/* I2C Extension Board Configuration */
// 使用G9和G10作为新的I2C接口
#define I2C_EXT_SCL_IO           9        // GPIO9 for SCL
#define I2C_EXT_SDA_IO           10       // GPIO10 for SDA
#define I2C_EXT_FREQ_HZ          400000   // 400kHz
#define I2C_EXT_PORT             I2C_NUM_1 // 使用I2C1，因为I2C0已被BSP使用
#define I2C_EXT_TIMEOUT_MS       1000     // 1 second timeout

/* I2C Device Addresses */
#define I2C_ADDR_ADC_EXPANDER    0x18    // 8路ADC GPIO扩展板地址
#define I2C_ADDR_TPL0401A        0x40    // TPL0401A数字电位器地址

/* TPL0401A Commands */
#define TPL0401A_CMD_WRITE       0x00    // 写入电阻值命令
#define TPL0401A_MAX_STEPS       128     // 128档位可调

/* ADC Expander Registers */
#define ADC_EXP_REG_ADC0         0x00    // ADC通道0数据
#define ADC_EXP_REG_ADC1         0x01    // ADC通道1数据
#define ADC_EXP_REG_ADC2         0x02    // ADC通道2数据
#define ADC_EXP_REG_ADC3         0x03    // ADC通道3数据
#define ADC_EXP_REG_ADC4         0x04    // ADC通道4数据
#define ADC_EXP_REG_ADC5         0x05    // ADC通道5数据
#define ADC_EXP_REG_ADC6         0x06    // ADC通道6数据
#define ADC_EXP_REG_ADC7         0x07    // ADC通道7数据
#define ADC_EXP_REG_GPIO         0x08    // GPIO数据寄存器
#define ADC_EXP_REG_CONFIG       0x09    // 配置寄存器

/* ADC Expander Configuration */
#define ADC_EXP_CONFIG_SINGLE    0x00    // 单次转换模式
#define ADC_EXP_CONFIG_CONT      0x04    // 连续转换模式
#define ADC_EXP_CONFIG_CH_ALL    0x00    // 使能所有通道
#define ADC_EXP_CONFIG_RANGE_5V  0x00    // 0-5V量程
#define ADC_EXP_CONFIG_RANGE_3V3 0x01    // 0-3.3V量程
#define ADC_EXP_CONFIG_DEFAULT   (ADC_EXP_CONFIG_CONT | ADC_EXP_CONFIG_CH_ALL | ADC_EXP_CONFIG_RANGE_3V3)

/* Function Declarations */
// ADC Functions
void init_adc(void);
int read_adc_value(void);
float convert_adc_to_voltage(int adc_value);

// I2C Extension Functions
esp_err_t init_i2c_extension(void);
esp_err_t i2c_extension_scan(void);  // 扫描I2C总线上的设备
esp_err_t i2c_extension_read(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);
esp_err_t i2c_extension_write(uint8_t addr, uint8_t reg, uint8_t *data, size_t len);

// TPL0401A Digital Potentiometer Functions
esp_err_t tpl0401a_set_value(uint8_t value);  // 设置电位器值 (0-127)
uint16_t tpl0401a_value_to_resistance(uint8_t value);  // 返回类型改为uint16_t，因为可能超过255欧姆

// ADC Expander Functions
esp_err_t adc_expander_read_channel(uint8_t channel, uint16_t *value);  // 读取指定ADC通道
esp_err_t adc_expander_read_gpio(uint8_t *value);  // 读取GPIO状态
esp_err_t adc_expander_write_gpio(uint8_t value);  // 写入GPIO状态
esp_err_t adc_expander_config(uint8_t config);  // 配置扩展板
