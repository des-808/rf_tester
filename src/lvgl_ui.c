#include "lv_conf.h"
#include "lvgl.h"
#include "st7796.h"
#include "ft6336u.h"
#include "buzzer.h"
#include <stdio.h>
#include <stdbool.h>

extern SPI_HandleTypeDef hspi4;
extern FT6336U_HandleTypeDef ft6336u;

static lv_obj_t* screen;
static lv_obj_t* label_swr;
static lv_obj_t* label_status;
static lv_obj_t* label_btn;
static lv_obj_t* label_touch;

/* LVGL touch input device */
static lv_indev_t* lvgl_touch_indev = NULL;

/* Флаг для защиты от повторного срабатывания buzzer */
static bool touch_buzzer_triggered = false;

/* Вспомогательный буфер для byte-swap строки — размещён в D2 SRAM с выравниванием 32B для D-Cache */
static uint16_t row_swap_buf[320] __attribute__((aligned(32)));

static void lvgl_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    uint16_t x1 = (uint16_t)area->x1;
    uint16_t y1 = (uint16_t)area->y1;
    uint16_t x2 = (uint16_t)area->x2;
    uint16_t y2 = (uint16_t)area->y2;

    uint16_t width = (uint16_t)(x2 - x1 + 1);
    uint16_t height = (uint16_t)(y2 - y1 + 1);

    uint16_t* framebuffer = (uint16_t*)px_map;

    ST7796_SetAddressWindow(x1, y1, x2, y2);
    LCD_CS_LOW;
    LCD_DC_DATA;

    for (uint16_t row = 0; row < height; ++row) {
        /* Копируем строку во временный буфер и сразу делаем byte-swap */
        uint16_t* src = framebuffer + (row * width);
        for (uint16_t i = 0; i < width; ++i) {
            row_swap_buf[i] = ((src[i] & 0x00FF) << 8) | ((src[i] & 0xFF00) >> 8);
        }

        ST7796_TransmitDMA((uint8_t*)row_swap_buf, width * 2);
        while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) {}
    }
    LCD_CS_HIGH;

    /* Ждём финального завершения DMA SPI перед сигналом LVGL */
    while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) {}

    lv_display_flush_ready(disp);
}

/* ============================================
   LVGL Touch Driver — нативный callback
   ============================================ */
static void lvgl_touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    (void)indev;

    /* Сбрасываем: нет касания */
    data->state = LV_INDEV_STATE_RELEASED;
    data->continue_reading = false;

    if (!ft6336u.has_touch) {
        /* Касание отпущено — сбрасываем флаг buzzer */
        touch_buzzer_triggered = false;
        return;
    }

    uint16_t raw_x, raw_y;
    if (!FT6336U_GetTouchPoint(&ft6336u, 0, &raw_x, &raw_y)) return;

    /* FT6336U: 12-bit (0-4095). Конвертируем в разрешение экрана 320x480 */
    data->state = LV_INDEV_STATE_PRESSED;
    data->point.x = (raw_x * LV_HOR_RES_MAX) / 4095;
    data->point.y = (raw_y * LV_VER_RES_MAX) / 4095;

    /* === ТЕСТ: координаты на экране + звук === */
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "T:%u,%u", data->point.x, data->point.y);
    lv_label_set_text(label_touch, tmp);
    lv_obj_invalidate(label_touch);

    /* Buzzer при первом касании (защита от повторов) */
    if (!touch_buzzer_triggered) {
        Buzzer_Short();
        touch_buzzer_triggered = true;
    }

    /* Очищаем флаг касания — LVGL сам будет опрашивать */
    ft6336u.has_touch = false;
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

    label_btn = lv_label_create(screen);
    lv_obj_align(label_btn, LV_ALIGN_TOP_LEFT, 8, 60);
    lv_label_set_text(label_btn, "Btn: --");
    lv_obj_set_style_text_color(label_btn, lv_color_hex(0x87CEFA), LV_PART_MAIN);

    label_touch = lv_label_create(screen);
    lv_obj_align(label_touch, LV_ALIGN_TOP_LEFT, 8, 86);
    lv_label_set_text(label_touch, "T:--,--");
    lv_obj_set_style_text_color(label_touch, lv_color_hex(0xFFD500), LV_PART_MAIN);

    lv_scr_load(screen);

    /* ============================================
       Регистрируем нативный touch driver LVGL
       ============================================ */
    lvgl_touch_indev = lv_indev_create();
    lv_indev_set_type(lvgl_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lvgl_touch_indev, lvgl_touch_read_cb);
}

void LVGL_SetSWR(float swr) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "SWR: %.2f", swr);
    lv_label_set_text(label_swr, tmp);
    lv_obj_invalidate(label_swr);
}

void LVGL_SetStatus(const char* text) {
    lv_label_set_text(label_status, text);
    lv_obj_invalidate(label_status);
}

void LVGL_SetButton(const char* text) {
    lv_label_set_text(label_btn, text);
    lv_obj_invalidate(label_btn);
}

void LVGL_Tick(void) {
    /* Обновляем тик LVGL (совпадает с HAL_Delay(10) в main) */
    lv_tick_inc(10);
    /* Обрабатываем таймеры с задержкой 10 мс */
    lv_timer_handler();
}
