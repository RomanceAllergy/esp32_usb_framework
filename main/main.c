#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "tinyusb.h"
#include "class/hid/hid_device.h"
#include "class/cdc/cdc_device.h"

// 接口索引定义
enum { ITF_NUM_CDC = 0, ITF_NUM_CDC_DATA, ITF_NUM_HID, ITF_NUM_TOTAL };
// Report ID 定义
enum { RID_KEYBOARD = 1, RID_TOUCH = 2, RID_CONSUMER = 3 };

static const char *TAG = "NIUNIU";

// 🛡️ 严格遵循 Android 原生协议的描述符
const uint8_t hid_report_desc[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(RID_KEYBOARD)),
    
    // Consumer 描述符：支持系统按键（返回、桌面、搜索）
    0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, RID_CONSUMER,
    0x15, 0x00, 0x26, 0xFF, 0x03, 0x19, 0x00, 0x29, 0xFF, 0x03,
    0x75, 0x10, 0x95, 0x01, 0x81, 0x00, 0xC0,
    
    // 真实触摸屏 (Digitizer) 描述符：彻底抹除鼠标光标
    0x05, 0x0D, 0x09, 0x04, 0xA1, 0x01, 0x85, RID_TOUCH, 0x09, 0x22, 0xA1, 0x02,
    0x09, 0x42, 0x09, 0x32, 0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x02, 0x81, 0x02,
    0x09, 0x51, 0x25, 0x3F, 0x75, 0x06, 0x95, 0x01, 0x81, 0x02,
    0x05, 0x01, 0x09, 0x30, 0x09, 0x31, 0x15, 0x00, 0x26, 0xFF, 0x7F, 0x75, 0x10, 0x95, 0x02, 0x81, 0x02,
    0xC0, 0xC0
};

static const uint8_t composite_config_desc[] = {
    TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN + TUD_HID_DESC_LEN), 0, 100),
    TUD_CDC_DESCRIPTOR(ITF_NUM_CDC, 4, 0x81, 8, 0x02, 0x82, 64),
    TUD_HID_DESCRIPTOR(ITF_NUM_HID, 5, false, sizeof(hid_report_desc), 0x83, 16, 10)
};

const char *string_desc_arr[] = {(char[]){0x09, 0x04}, "Espressif", "NiuNiu Core", "1.0", "CDC", "HID"};

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance) { return hid_report_desc; }
uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t id, hid_report_type_t type, uint8_t* buf, uint16_t req) { return 0; }
void tud_hid_set_report_cb(uint8_t itf, uint8_t id, hid_report_type_t type, uint8_t const* buf, uint16_t size) {}

// 触摸结构体 (5字节)
typedef struct TU_ATTR_PACKED {
    uint8_t status; 
    uint16_t x;
    uint16_t y;
} hid_touch_report_t;

void send_touch(uint8_t is_down, uint16_t x, uint16_t y) {
    if(!tud_hid_ready()) return;
    hid_touch_report_t r = { .status = is_down ? 0x03 : 0x00, .x = x, .y = y };
    tud_hid_report(RID_TOUCH, &r, sizeof(r));
}

// 🪄 拟人化二阶贝塞尔算法
void do_drag(int x1, int y1, int x2, int y2, int press_dur, int slide_dur) {
    if (press_dur < 50) press_dur = 50; 
    send_touch(1, x1, y1); 
    vTaskDelay(pdMS_TO_TICKS(press_dur));
    
    if (slide_dur > 0) {
        int steps = slide_dur / 10;
        if (steps < 1) steps = 1;
        float dx = (float)(x2 - x1), dy = (float)(y2 - y1);
        float offset = ((rand() % 40) - 20) / 100.0f; 
        float cx = x1 + dx/2.0f - dy * offset, cy = y1 + dy/2.0f + dx * offset; 

        for (int i = 1; i <= steps; i++) {
            float t = (float)i / steps, u = 1.0f - t;
            int bx = (int)(u * u * x1 + 2 * u * t * cx + t * t * x2);
            int by = (int)(u * u * y1 + 2 * u * t * cy + t * t * y2);
            if (bx < 0) bx = 0; else if (bx > 32767) bx = 32767;
            if (by < 0) by = 0; else if (by > 32767) by = 32767;
            send_touch(1, (uint16_t)bx, (uint16_t)by);
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
    send_touch(0, x2, y2); 
}

void do_sys_action(char type) {
    if (!tud_hid_ready()) return;
    uint16_t c_key = 0;
    
    if (type == 'H') c_key = 0x0223;       // Home
    else if (type == 'B') c_key = 0x0224;  // Back
    else if (type == 'R') c_key = 0x01A2;  // Recents
    else if (type == 'S' || type == 'W') { 
        // 🚀 针对华为/安卓优化的‘召唤’逻辑
        // 首先尝试发送 Consumer 协议的 Search 键 (0x0221)
        c_key = 0x0221; 
        tud_hid_report(RID_CONSUMER, &c_key, 2);
        vTaskDelay(pdMS_TO_TICKS(50));
        uint16_t empty = 0;
        tud_hid_report(RID_CONSUMER, &empty, 2);
        
        // 备选逻辑：发送键盘物理 Win 键 (HID_KEY_GUI_LEFT)
        uint8_t keycode[6] = {0}; 
        tud_hid_keyboard_report(RID_KEYBOARD, KEYBOARD_MODIFIER_LEFTGUI, keycode);
        vTaskDelay(pdMS_TO_TICKS(50));
        tud_hid_keyboard_report(RID_KEYBOARD, 0, NULL);
        return; 
    }
    
    if (c_key != 0) {
        tud_hid_report(RID_CONSUMER, &c_key, 2);
        vTaskDelay(pdMS_TO_TICKS(50));
        uint16_t empty = 0;
        tud_hid_report(RID_CONSUMER, &empty, 2);
    }
}

// ⚔️ 核心指令监听与分发
void cdc_task(void *pv) {
    char buf[128];
    while (1) {
        if (tud_cdc_available()) {
            uint32_t len = tud_cdc_read(buf, sizeof(buf)-1);
            buf[len] = 0; 
            
            if (buf[0] == 'C') { // 点击
                int x, y, d; 
                if (sscanf(buf, "C,%d,%d,%d", &x, &y, &d) >= 2) do_drag(x, y, x, y, d, 0); 
            }
            else if (buf[0] == 'D') { // 滑动
                int x1, y1, x2, y2, pd, sd; 
                if (sscanf(buf, "D,%d,%d,%d,%d,%d,%d", &x1, &y1, &x2, &y2, &pd, &sd) == 6) do_drag(x1, y1, x2, y2, pd, sd); 
            }
            else if (buf[0] == 'N') { // 系统导航
                do_sys_action(buf[2]); 
            }
            else if (buf[0] == 'K') { // 普通按键
                int k; 
                if (sscanf(buf, "K,%d", &k) == 1) { 
                    uint8_t kc[6] = { (uint8_t)k }; 
                    tud_hid_keyboard_report(RID_KEYBOARD, 0, kc); 
                    vTaskDelay(pdMS_TO_TICKS(40)); 
                    tud_hid_keyboard_report(RID_KEYBOARD, 0, NULL); 
                } 
            }
            else if (buf[0] == 'V') { // 粘贴
                uint8_t key[6] = {HID_KEY_V, 0, 0, 0, 0, 0};
                tud_hid_keyboard_report(RID_KEYBOARD, KEYBOARD_MODIFIER_LEFTCTRL, key);
                vTaskDelay(pdMS_TO_TICKS(50));
                tud_hid_keyboard_report(RID_KEYBOARD, 0, NULL);
            }
            else if (buf[0] == 'A') { // 全选
                uint8_t key[6] = {HID_KEY_A, 0, 0, 0, 0, 0};
                tud_hid_keyboard_report(RID_KEYBOARD, KEYBOARD_MODIFIER_LEFTCTRL, key);
                vTaskDelay(pdMS_TO_TICKS(50));
                tud_hid_keyboard_report(RID_KEYBOARD, 0, NULL);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}

void app_main(void) {
    const tinyusb_config_t tusb_cfg = {
        .external_phy = false, 
        .configuration_descriptor = composite_config_desc, 
        .string_descriptor = string_desc_arr, 
        .string_descriptor_count = 6 
    };
    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));
    ESP_LOGI(TAG, "USB HID/CDC 系统已启动");
    xTaskCreatePinnedToCore(cdc_task, "cdc", 4096, NULL, 5, NULL, 1);
}