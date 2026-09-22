#ifndef MENU_TOUCH_H
#define MENU_TOUCH_H

#include <stdint.h>
#include "touch_gesture.h"

/**
 * @brief Обработка жеста из touch_gesture системы
 * @param event указатель на событие жеста
 * 
 * Вызывается из main.c после распознавания жеста.
 * Физические кнопки обрабатываются отдельно через Menu_ProcessInput().
 */
void Menu_ProcessGesture(TouchGesture_Event_t* event);

#endif
