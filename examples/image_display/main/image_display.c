/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

#include <dirent.h>
#include "bsp/esp-bsp.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"

static const char *TAG = "main";
static lv_obj_t *g_label_adc = NULL;
static adc_oneshot_unit_handle_t adc2_handle;
#define ADC_UNIT ADC_UNIT_2
#define ADC_CHANNEL ADC_CHANNEL_0  // GPIO11 maps to ADC2_CHANNEL_0
#define NO_OF_SAMPLES 32   // Average over 32 samples

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

static void update_display_timer_cb(void *arg) {
    int adc_value = read_adc_value();
    float voltage = (float)adc_value * 3.3 / 4095.0;  // Convert to voltage (3.3V reference)
    
    // Update label with ADC value and voltage
    char buf[32];
    snprintf(buf, sizeof(buf), "ADC: %d\nVoltage: %.2fV", adc_value, voltage);
    ESP_LOGI(TAG, "%s", buf);  // Add logging to use TAG
    lv_label_set_text(g_label_adc, buf);
}

static void init_adc(void) {
    // Initialize ADC
    adc_oneshot_unit_init_cfg_t init_config = {
        .unit_id = ADC_UNIT,
        .ulp_mode = ADC_ULP_MODE_DISABLE,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config, &adc2_handle));

    // Configure ADC channel
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,           // Use 12dB attenuation for 0-3.3V range
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

    /* Set display brightness to 100% */
    bsp_display_backlight_on();

    /* Initialize ADC */
    init_adc();

    /* Create a label for ADC display */
    g_label_adc = lv_label_create(lv_scr_act());
    lv_obj_align(g_label_adc, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(g_label_adc, &lv_font_montserrat_14, 0);  // Use available font
    lv_label_set_text(g_label_adc, "ADC: ---\nVoltage: ---V");

    /* Create a timer to update the display */
    esp_timer_handle_t timer_handle;
    esp_timer_create_args_t timer_args = {
        .callback = update_display_timer_cb,
        .name = "display_update"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer_handle));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer_handle, 100000)); // Update every 100ms
}
