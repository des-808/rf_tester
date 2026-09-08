#ifndef MENU_TOUCH_H
#define MENU_TOUCH_H

#include <stdint.h>

/**
 * @brief Обработка касания экрана в меню
 * @param tx  преобразованная координата X
 * @param ty  преобразованная координата Y
 * 
 * Вызывается из main.c после чтения координат с FT6336U.
 */
void Menu_ProcessTouch(uint16_t tx, uint16_t ty);

/**
 * @brief Обработка отпускания пальца с экрана
 * 
 * Вызывается из main.c когда has_touch становится false.
 */
void Menu_ProcessTouchRelease(void);

#endif
