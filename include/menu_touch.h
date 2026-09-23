#ifndef MENU_TOUCH_H
#define MENU_TOUCH_H

#include <stdint.h>
#include "touch_gesture.h"

/* ========================================================================
 *  TOUCH STATE — состояние касания для каждого пункта меню
 * ======================================================================== */

/**
 * @brief Состояние касания для одного пункта меню
 * Отслеживает фазы касания: press, hold, release
 */
typedef struct {
    uint8_t is_active : 1;       // Палец на этом пункте
    uint8_t is_pressed : 1;      // Палец нажат (но ещё не отпущен)
    uint8_t hold_triggered : 1;  // Удержание сработало
    uint8_t tap_confirmed : 1;   // Тап подтверждён (палец отпущен)
    uint32_t press_tick;         // Время начала нажатия
    int8_t item_index;           // Индекс пункта меню
} MenuItemTouchState_t;

/* ========================================================================
 *  API
 * ======================================================================== */

/**
 * @brief Обработка жеста из touch_gesture системы
 * @param event указатель на событие жеста
 * 
 * Вызывается из main.c после распознавания жеста.
 * Физические кнопки обрабатываются отдельно через Menu_ProcessInput().
 */
void Menu_ProcessGesture(TouchGesture_Event_t* event);

/**
 * @brief Инициализация системы касаний меню
 */
void MenuTouch_Init(void);

/**
 * @brief Получить состояние касания пункта меню
 * @param index индекс пункта
 * @return указатель на состояние или NULL
 */
MenuItemTouchState_t* MenuTouch_GetItemState(int8_t index);

/**
 * @brief Обновить состояние касания при движении пальца
 * @param index индекс пункта
 * @param x, y координаты
 */
void MenuTouch_UpdatePosition(int8_t index, uint16_t x, uint16_t y);

/* Текущее активное касание (для main.c) */
extern int8_t g_active_touch_index;

#endif
