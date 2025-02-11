/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t fan_speed_t;  // 0-100 for speed percentage

esp_err_t app_fan_change_io(gpio_num_t gpio, bool act_level);
esp_err_t app_fan_set_power(bool power);
bool app_fan_get_state(void);
esp_err_t app_fan_set_speed(fan_speed_t speed);  // speed: 0-100
fan_speed_t app_fan_get_speed(void);

#ifdef __cplusplus
}
#endif
