#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int adc_raw;        // Raw ADC value (0-4095)
    float voltage;      // Voltage in V (0-3.3V)
    float percentage;   // Position percentage (0-100%)
} potentiometer_data_t;

esp_err_t app_potentiometer_init(void);
potentiometer_data_t app_potentiometer_read_value(void);

#ifdef __cplusplus
}
#endif
