#include "lv_conf.h"
#include "lvgl.h"
#include "st7796.h"
#include <stdio.h>

extern SPI_HandleTypeDef hspi4;

static lv_obj_t* screen;
static lv_obj_t* label_swr;
static lv_obj_t* label_status;
static lv_obj_t* label_touch;
static lv_obj_t* label_btn;

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint16_t x1 = (uint16_t)area->x1;
    uint16_t y1 = (uint16_t)area->y1;
    uint16_t x2 = (uint16_t)area->x2;
    uint16_t y2 = (uint16_t)area->y2;

    uint16_t width = (uint16_t)(x2 - x1 + 1);
    uint16_t height = (uint16_t)(y2 - y1 + 1);

    uint16_t* framebuffer = (uint16_t*)px_map;
    uint32_t pixel_count = (uint32_t)width * height;

    for (uint32_t i = 0; i < pixel_count; ++i) {
        uint16_t c = framebuffer[i];
        framebuffer[i] = ((c & 0x00FF) << 8) | ((c & 0xFF00) >> 8);
    }

    ST7796_SetAddressWindow(x1, y1, x2, y2);
    LCD_CS_LOW;
    LCD_DC_DATA;
    for (uint16_t row = 0; row < height; ++row) {
        uint16_t* row_ptr = framebuffer + (row * width);
        ST7796_TransmitDMA((uint8_t*)row_ptr, width * 2);
        while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) {}
    }
    LCD_CS_HIGH;

    lv_display_flush_ready(disp);
}

void LVGL_InitScreen(void) {
    static lv_color_t buf[320 * 480 / 10];

    lv_init();

    lv_display_t* display = lv_display_create(LV_HOR_RES_MAX, LV_VER_RES_MAX);
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    lv_display_set_buffers(display, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    screen = lv_obj_create(NULL);
    lv_obj_set_size(screen, 320, 480);
    lv_obj_set_style_bg_color(screen, lv_color_hex(0x101820), LV_PART_MAIN);

    label_swr = lv_label_create(screen);
    lv_obj_align(label_swr, LV_ALIGN_TOP_LEFT, 8, 8);
    lv_label_set_text(label_swr, "SWR: --");
    lv_obj_set_style_text_color(label_swr, lv_color_hex(0x00FF00), LV_PART_MAIN);

    label_status = lv_label_create(screen);
    lv_obj_align(label_status, LV_ALIGN_TOP_LEFT, 8, 34);
    lv_label_set_text(label_status, "Status: boot");
    lv_obj_set_style_text_color(label_status, lv_color_hex(0xFFFFFF), LV_PART_MAIN);

    label_touch = lv_label_create(screen);
    lv_obj_align(label_touch, LV_ALIGN_TOP_LEFT, 8, 60);
    lv_label_set_text(label_touch, "Touch: --");
    lv_obj_set_style_text_color(label_touch, lv_color_hex(0xFFD500), LV_PART_MAIN);

    label_btn = lv_label_create(screen);
    lv_obj_align(label_btn, LV_ALIGN_TOP_LEFT, 8, 86);
    lv_label_set_text(label_btn, "Btn: --");
    lv_obj_set_style_text_color(label_btn, lv_color_hex(0x87CEFA), LV_PART_MAIN);

    lv_scr_load(screen);
}

void LVGL_SetSWR(float swr) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "SWR: %.2f", swr);
    lv_label_set_text(label_swr, tmp);
}

void LVGL_SetStatus(const char* text) {
    lv_label_set_text(label_status, text);
}

void LVGL_SetTouch(uint16_t x, uint16_t y) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "Touch: (%u,%u)", x, y);
    lv_label_set_text(label_touch, tmp);
}

void LVGL_SetButton(const char* text) {
    lv_label_set_text(label_btn, text);
}

void LVGL_Tick(void) {
    lv_timer_handler();
}
