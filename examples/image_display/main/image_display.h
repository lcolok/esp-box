/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#include "esp_adc/adc_oneshot.h"

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

/* Function Declarations */
void init_adc(void);
int read_adc_value(void);
float convert_adc_to_voltage(int adc_value);
