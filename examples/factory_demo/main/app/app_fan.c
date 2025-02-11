/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_check.h"
#include "app_led.h"
#include "bsp_board.h"
#include "driver/gpio.h"
#include "ui_main.h"
#include "ui_device_ctrl.h"
#include "app_fan.h"

static const char *TAG = "app_fan";

static bool g_fan_on = 0;
static fan_speed_t g_fan_speed = 0;

esp_err_t app_fan_change_io(gpio_num_t gpio, bool act_level)
{
    ESP_LOGW(TAG, "change io not be supported");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t app_fan_set_power(bool power)
{
    esp_err_t ret_val = ESP_OK;

    if (power) {
        g_fan_on = true;
        if (g_fan_speed == 0) {
            g_fan_speed = 50;  // Default to 50% speed when turning on
        }
    } else {
        g_fan_on = false;
    }
    ui_dev_ctrl_set_state(UI_DEV_FAN, g_fan_on);

    return ret_val;
}

bool app_fan_get_state(void)
{
    return g_fan_on;
}

esp_err_t app_fan_set_speed(fan_speed_t speed)
{
    esp_err_t ret_val = ESP_OK;
    
    if (speed > 100) {
        speed = 100;
    }
    
    g_fan_speed = speed;
    
    if (speed > 0) {
        app_fan_set_power(true);
    }
    
    // TODO: 根据百分比设置实际的PWM占空比
    ESP_LOGI(TAG, "Set fan speed to %d%%", speed);

    return ret_val;
}

fan_speed_t app_fan_get_speed(void)
{
    return g_fan_speed;
}
