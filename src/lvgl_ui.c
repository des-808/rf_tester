// LVGL Display & Touch Driver

#include "lv_conf.h"
#include "lvgl.h"
#include "st7796.h"
#include "ft6336u.h"
#include "buzzer.h"
#include <stdbool.h>

extern SPI_HandleTypeDef hspi4;
extern FT6336U_HandleTypeDef ft6336u;

/* LVGL touch input device */
lv_indev_t* lvgl_touch_indev = NULL;

/* Текущие координаты тачскрина */
static uint16_t touch_x = 0;
static uint16_t touch_y = 0;
static bool touch_pressed = false;

/* Флаг для защиты от повторного срабатывания buzzer */
static bool touch_buzzer_triggered = false;

/* Вспомогательный буфер для byte-swap строки */
static uint16_t row_swap_buf[320] __attribute__((aligned(32)));

/* ============================================
   LVGL Display Flush Callback
   ============================================ */
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
        uint16_t* src = framebuffer + (row * width);
        for (uint16_t i = 0; i < width; ++i) {
            row_swap_buf[i] = ((src[i] & 0x00FF) << 8) | ((src[i] & 0xFF00) >> 8);
        }

        ST7796_TransmitDMA((uint8_t*)row_swap_buf, width * 2);
        while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) {}
    }
    LCD_CS_HIGH;

    while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) {}

    lv_display_flush_ready(disp);
}

/* ============================================
   LVGL Touch Input Driver
   ============================================ */
static lv_obj_t* touch_label = NULL;

static void lvgl_touch_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    (void)indev;

    /* Возвращаем текущие координаты тача — LVGL сам различит клик и свайп */
    data->state = touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = touch_x;
    data->point.y = touch_y;
    data->continue_reading = false;
    
    /* Обновляем метку с координатами */
    if (touch_label && touch_pressed) {
        char buf[32];
        lv_snprintf(buf, sizeof(buf), "X:%d Y:%d", touch_x, touch_y);
        lv_label_set_text(touch_label, buf);
    }
}

/* ============================================
   Обновление данных тачскрина — вызывать из main()
   ============================================ */
void lvgl_touch_update(void)
{
    /* Читаем данные тачскрина (I2C — может быть долгим) */
    FT6336U_ReadData(&ft6336u);

    /* Проверяем, есть ли касание */
    if (ft6336u.touch_num > 0) {
        uint16_t raw_x, raw_y;
        if (FT6336U_GetTouchPoint(&ft6336u, 0, &raw_x, &raw_y)) {
            touch_x = raw_x;
            touch_y = raw_y;
            touch_pressed = true;

            /* Buzzer при первом касании */
            if (!touch_buzzer_triggered) {
                //Buzzer_Short();
                touch_buzzer_triggered = true;
            }
        }
    } else {
        /* Нет касания — палец убран */
        if (touch_pressed) {
            touch_pressed = false;
            touch_buzzer_triggered = false;
        }
    }
}

/* ============================================
   LVGL Init — вызывается из ui_init()
   ============================================ */
void lvgl_driver_init(void)
{
    /* LVGL init */
    lv_init();

    /* Display buffer */
    static lv_color_t buf[320 * 480 / 10];

    lv_display_t* display = lv_display_create(LV_HOR_RES_MAX, LV_VER_RES_MAX);
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    lv_display_set_buffers(display, buf, NULL, sizeof(buf), LV_DISPLAY_RENDER_MODE_PARTIAL);

    /* Touch input */
    lvgl_touch_indev = lv_indev_create();
    lv_indev_set_type(lvgl_touch_indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(lvgl_touch_indev, lvgl_touch_read_cb);
    
    /* Создаём метку для отладки координат тача */
    touch_label = lv_label_create(lv_scr_act());
    lv_label_set_text(touch_label, "Touch: ---");
    lv_obj_align(touch_label, LV_ALIGN_TOP_RIGHT, -10, 10);
}

/* ============================================
   LVGL Tick — вызывать в основном цикле
   ============================================ */
void LVGL_Tick(void) {
    lv_tick_inc(10);
    lv_timer_handler();
}
