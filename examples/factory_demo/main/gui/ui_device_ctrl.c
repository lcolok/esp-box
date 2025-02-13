/*
 * SPDX-FileCopyrightText: 2015-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_log.h"
#include "bsp_board.h"
#include "lvgl.h"
#include "app_led.h"
#include "app_fan.h"
#include "app_switch.h"
#include "ui_main.h"
#include "ui_device_ctrl.h"

static const char *TAG = "ui_dev_ctrl";

LV_FONT_DECLARE(font_en_16);

LV_IMG_DECLARE(icon_fan_on)
LV_IMG_DECLARE(icon_fan_off)
LV_IMG_DECLARE(icon_light_on)
LV_IMG_DECLARE(icon_light_off)
LV_IMG_DECLARE(icon_switch_on)
LV_IMG_DECLARE(icon_switch_off)
LV_IMG_DECLARE(icon_air_on)
LV_IMG_DECLARE(icon_air_off)

static ui_dev_type_t g_active_dev_type = UI_DEV_LIGHT;
static lv_obj_t *g_func_btn[4] = {NULL};
static void (*g_dev_ctrl_end_cb)(void) = NULL;
static lv_obj_t *g_fan_popup = NULL;
static lv_obj_t *g_fan_mask = NULL;  // 透明遮罩

typedef struct {
    ui_dev_type_t type;
    const char *name;
    lv_img_dsc_t const *img_on;
    lv_img_dsc_t const *img_off;
} btn_img_src_t;

static const btn_img_src_t img_src_list[] = {
    { .type = UI_DEV_LIGHT, .name = "Light", .img_on = &icon_light_on, .img_off = &icon_light_off },
    { .type = UI_DEV_SWITCH, .name = "Switch", .img_on = &icon_switch_on, .img_off = &icon_switch_off },
    { .type = UI_DEV_FAN, .name = "Fan", .img_on = &icon_fan_on, .img_off = &icon_fan_off },
    { .type = UI_DEV_AIR, .name = "Air", .img_on = &icon_air_on, .img_off = &icon_air_off },
};

void ui_dev_ctrl_set_state(ui_dev_type_t type, bool state)
{
    if (NULL == g_func_btn[type]) {
        return;
    }
    ui_acquire();
    lv_obj_t *img = (lv_obj_t *) g_func_btn[type]->user_data;
    if (state) {
        lv_obj_add_state(g_func_btn[type], LV_STATE_CHECKED);
        lv_img_set_src(img, img_src_list[type].img_on);
    } else {
        lv_obj_clear_state(g_func_btn[type], LV_STATE_CHECKED);
        lv_img_set_src(img, img_src_list[type].img_off);
    }
    ui_release();
}

static void close_fan_popup(void)
{
    if (g_fan_mask) {
        // 先删除遮罩（会自动删除其子对象，包括弹窗）
        lv_obj_del(g_fan_mask);
        g_fan_mask = NULL;
        g_fan_popup = NULL;
    }
}

static void fan_power_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *btn = lv_event_get_target(e);
    bool is_checked = lv_obj_has_state(btn, LV_STATE_CHECKED);
    app_fan_set_power(is_checked);
}

static void fan_slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    app_fan_set_speed((fan_speed_t)value);
}

static void show_fan_speed_popup(lv_obj_t *parent)
{
    if (g_fan_popup) {
        close_fan_popup();
        return;
    }

    // 创建一个透明的全屏遮罩作为弹窗的容器
    g_fan_mask = lv_obj_create(parent);
    lv_obj_remove_style_all(g_fan_mask);
    lv_obj_set_size(g_fan_mask, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(g_fan_mask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_fan_mask, LV_OBJ_FLAG_CLICKABLE); // 确保遮罩可以接收点击事件
    lv_obj_set_style_bg_opa(g_fan_mask, LV_OPA_0, 0);  // 完全透明
    
    // 创建弹出窗口
    g_fan_popup = lv_obj_create(g_fan_mask);
    lv_obj_set_size(g_fan_popup, 280, 220);
    lv_obj_set_style_radius(g_fan_popup, 10, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_fan_popup, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_fan_popup, 20, LV_PART_MAIN);
    lv_obj_set_style_shadow_opa(g_fan_popup, LV_OPA_30, LV_PART_MAIN);
    lv_obj_align(g_fan_popup, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(g_fan_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_fan_popup, LV_OBJ_FLAG_CLICKABLE);

    // 添加关闭按钮
    lv_obj_t *btn_close = lv_btn_create(g_fan_popup);
    lv_obj_set_size(btn_close, 30, 30);
    lv_obj_align(btn_close, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_t *label_close = lv_label_create(btn_close);
    lv_label_set_text(label_close, LV_SYMBOL_CLOSE);
    lv_obj_center(label_close);
    lv_obj_add_event_cb(btn_close, (lv_event_cb_t)close_fan_popup, LV_EVENT_CLICKED, NULL);

    // 添加标题
    lv_obj_t *label = lv_label_create(g_fan_popup);
    lv_label_set_text_static(label, "Fan Control");
    lv_obj_set_style_text_font(label, &font_en_16, LV_STATE_DEFAULT);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);

    // 添加电源开关按钮
    lv_obj_t *btn_power = lv_btn_create(g_fan_popup);
    lv_obj_add_flag(btn_power, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_set_size(btn_power, 160, 40);
    lv_obj_align(btn_power, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_t *label_power = lv_label_create(btn_power);
    lv_label_set_text_static(label_power, LV_SYMBOL_POWER" Power");
    lv_obj_center(label_power);
    lv_obj_add_event_cb(btn_power, fan_power_btn_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    if (app_fan_get_state()) {
        lv_obj_add_state(btn_power, LV_STATE_CHECKED);
    }

    // 添加滑块
    lv_obj_t *slider = lv_slider_create(g_fan_popup);
    lv_obj_set_size(slider, 240, 15);
    lv_obj_align(slider, LV_ALIGN_TOP_MID, 0, 130);
    lv_slider_set_range(slider, 0, 100);
    lv_slider_set_value(slider, app_fan_get_speed(), LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, fan_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // 添加最小/最大标签
    lv_obj_t *label_min = lv_label_create(g_fan_popup);
    lv_label_set_text_static(label_min, "0%");
    lv_obj_align_to(label_min, slider, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 5);
    
    lv_obj_t *label_max = lv_label_create(g_fan_popup);
    lv_label_set_text_static(label_max, "100%");
    lv_obj_align_to(label_max, slider, LV_ALIGN_OUT_BOTTOM_RIGHT, 0, 5);

    if (ui_get_btn_op_group()) {
        lv_group_add_obj(ui_get_btn_op_group(), btn_close);
        lv_group_add_obj(ui_get_btn_op_group(), btn_power);
        lv_group_add_obj(ui_get_btn_op_group(), slider);
    }

    // 最后才给遮罩添加点击事件，避免创建过程中触发
    lv_obj_add_event_cb(g_fan_mask, (lv_event_cb_t)close_fan_popup, LV_EVENT_CLICKED, NULL);
}

static void ui_dev_ctrl_page_func_click_cb(lv_event_t *e)
{
    uint32_t i = (uint32_t)lv_event_get_user_data(e);
    g_active_dev_type = i;

    if (UI_DEV_LIGHT == g_active_dev_type) {
        bool state = app_pwm_led_get_state();
        app_pwm_led_set_power(!state);
    } else if (UI_DEV_SWITCH == g_active_dev_type) {
        bool state = app_switch_get_state();
        app_switch_set_power(!state);
    } else if (UI_DEV_FAN == g_active_dev_type) {
        show_fan_speed_popup(lv_scr_act());
    } else {
        ui_dev_ctrl_set_state(g_active_dev_type, !lv_obj_has_state(g_func_btn[g_active_dev_type], LV_STATE_CHECKED));
    }
}

static void ui_dev_ctrl_page_return_click_cb(lv_event_t *e)
{
    close_fan_popup();
    lv_obj_t *obj = lv_event_get_user_data(e);
    if (ui_get_btn_op_group()) {
        lv_group_remove_all_objs(ui_get_btn_op_group());
    }
#if !CONFIG_BSP_BOARD_ESP32_S3_BOX_Lite
    bsp_btn_rm_all_callback(BSP_BUTTON_MAIN);
#endif
    lv_obj_del(obj);
    g_func_btn[0] = NULL;
    g_func_btn[1] = NULL;
    g_func_btn[2] = NULL;
    g_func_btn[3] = NULL;
    if (g_dev_ctrl_end_cb) {
        g_dev_ctrl_end_cb();
    }
}

#if !CONFIG_BSP_BOARD_ESP32_S3_BOX_Lite
static void btn_return_down_cb(void *handle, void *arg)
{
    lv_obj_t *obj = (lv_obj_t *) arg;
    ui_acquire();
    lv_event_send(obj, LV_EVENT_CLICKED, NULL);
    ui_release();
}
#endif

void ui_device_ctrl_start(void (*fn)(void))
{
    ESP_LOGI(TAG, "device control initialize");
    g_dev_ctrl_end_cb = fn;

    lv_obj_t *page = lv_obj_create(lv_scr_act());
    lv_obj_set_size(page, lv_obj_get_width(lv_obj_get_parent(page)), lv_obj_get_height(lv_obj_get_parent(page)) - lv_obj_get_height(ui_main_get_status_bar()));
    lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(page, lv_obj_get_style_bg_color(lv_scr_act(), LV_STATE_DEFAULT), LV_PART_MAIN);
    lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align_to(page, ui_main_get_status_bar(), LV_ALIGN_OUT_BOTTOM_LEFT, 0, 0);

    lv_obj_t *btn_return = lv_btn_create(page);
    lv_obj_set_size(btn_return, 24, 24);
    lv_obj_add_style(btn_return, &ui_button_styles()->style, 0);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_pr, LV_STATE_PRESSED);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUS_KEY);
    lv_obj_add_style(btn_return, &ui_button_styles()->style_focus, LV_STATE_FOCUSED);
    lv_obj_align(btn_return, LV_ALIGN_TOP_LEFT, 0, -8);
    lv_obj_t *lab_btn_text = lv_label_create(btn_return);
    lv_label_set_text_static(lab_btn_text, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(lab_btn_text, lv_color_make(158, 158, 158), LV_STATE_DEFAULT);
    lv_obj_center(lab_btn_text);
    lv_obj_add_event_cb(btn_return, ui_dev_ctrl_page_return_click_cb, LV_EVENT_CLICKED, page);
#if !CONFIG_BSP_BOARD_ESP32_S3_BOX_Lite
    bsp_btn_register_callback(BSP_BUTTON_MAIN, BUTTON_PRESS_UP, btn_return_down_cb, (void *)btn_return);
#endif

    for (size_t i = 0; i < 4; i++) {
        g_func_btn[i] = lv_btn_create(page);
        lv_obj_set_size(g_func_btn[i], 85, 85);
        lv_obj_add_style(g_func_btn[i], &ui_button_styles()->style_focus, LV_STATE_FOCUS_KEY);
        lv_obj_add_style(g_func_btn[i], &ui_button_styles()->style_focus, LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(g_func_btn[i], lv_color_white(), LV_STATE_DEFAULT);
        lv_obj_set_style_bg_color(g_func_btn[i], lv_color_white(), LV_STATE_CHECKED);
        lv_obj_set_style_shadow_color(g_func_btn[i], lv_color_make(0, 0, 0), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(g_func_btn[i], 10, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(g_func_btn[i], LV_OPA_40, LV_PART_MAIN);

        lv_obj_set_style_border_width(g_func_btn[i], 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(g_func_btn[i], lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);

        lv_obj_set_style_radius(g_func_btn[i], 10, LV_STATE_DEFAULT);
        lv_obj_align(g_func_btn[i], LV_ALIGN_CENTER, i % 2 ? 48 : -48, i < 2 ? -48 - 3 : 48 - 3);

        lv_obj_t *img = lv_img_create(g_func_btn[i]);
        lv_img_set_src(img, img_src_list[i].img_off);
        lv_obj_align(img, LV_ALIGN_CENTER, 0, -10);
        lv_obj_set_user_data(img, (void *) &img_src_list[i]);

        lv_obj_t *label = lv_label_create(g_func_btn[i]);
        lv_label_set_text_static(label, img_src_list[i].name);
        lv_obj_set_style_text_color(label, lv_color_make(40, 40, 40), LV_STATE_DEFAULT);
        lv_obj_set_style_text_font(label, &font_en_16, LV_STATE_DEFAULT);
        lv_obj_align(label, LV_ALIGN_CENTER, 0, 20);

        lv_obj_set_user_data(g_func_btn[i], (void *) img);
        if (UI_DEV_LIGHT == i) {
            ui_dev_ctrl_set_state(i, app_pwm_led_get_state());
        } else if (UI_DEV_SWITCH == i) {
            ui_dev_ctrl_set_state(i, app_switch_get_state());
        } else if (UI_DEV_FAN == i) {
            ui_dev_ctrl_set_state(i, app_fan_get_state());
        }
        lv_obj_add_event_cb(g_func_btn[i], ui_dev_ctrl_page_func_click_cb, LV_EVENT_CLICKED, (void *)i);
        if (ui_get_btn_op_group()) {
            lv_group_add_obj(ui_get_btn_op_group(), g_func_btn[i]);
        }
    }
    if (ui_get_btn_op_group()) {
        lv_group_add_obj(ui_get_btn_op_group(), btn_return);
    }
}
