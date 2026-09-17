#ifndef LVGL_UI_H
#define LVGL_UI_H

#include <stdint.h>

/* Инициализация дисплея, тача и LVGL (вызывать ДО ui_init) */
void lvgl_driver_init(void);

/* Обновление данных тачскрина — вызывать перед LVGL_Tick() */
void lvgl_touch_update(void);

/* Вызывать в основном цикле (lv_tick_inc + lv_timer_handler) */
void LVGL_Tick(void);

#endif
