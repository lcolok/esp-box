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
    
    char buf[32];
    snprintf(buf, sizeof(buf), "ADC: %d\nVoltage: %.2fV", adc_value, voltage);
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

void app_main(void)
{
    /* Initialize I2C (for touch and audio) */
    bsp_i2c_init();

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
