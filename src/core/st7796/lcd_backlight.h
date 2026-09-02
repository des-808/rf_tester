#ifndef LCD_BACKLIGHT_H
#define LCD_BACKLIGHT_H

#include "stdint.h"
#include <stdbool.h>
#include "tim.h"

// 10 градаций яркости: 0 = выкл, 1-10 = тускло -> ярко
#define LCD_BACKLIGHT_LEVELS 10

// Инициализация PWM подсветки
void LCD_Backlight_Init(void);

// Установка уровня яркости (0 = выкл, 1-10 = тускло -> ярко)
void LCD_Backlight_SetLevel(uint8_t level);

// Получить текущий уровень (0-10)
uint8_t LCD_Backlight_GetLevel(void);

// Преобразовать уровень меню (0-10) в значение PWM (0-255)
// 0 -> 255 (выкл), 10 -> 0 (макс яркость)
uint8_t LCD_Backlight_LevelToPWM(uint8_t level);

// Преобразовать PWM (0-255) в уровень меню (0-10)
// 0 -> 10 (макс), 255 -> 0 (выкл)
//uint8_t LCD_Backlight_PWMToLevel(uint8_t pwm);
uint8_t LCD_Backlight_PWMToLevel(uint32_t ccr2);
// Плавное затухание подсветки до нуля
// duration_ms — длительность в миллисекундах
// step_ms — шаг обновления (меньше = плавнее, но больше нагрузка)
void LCD_Backlight_SmoothOff(uint32_t duration_ms, uint32_t step_ms);

// Плавное разгорание от нуля до заданного уровня
// duration_ms — длительность, target_level — конечный уровень (0-10)
// step_ms — шаг обновления
bool LCD_Backlight_SmoothOn(uint32_t duration_ms, uint8_t target_level, uint32_t step_ms);

// Неблокирующий вариант — вызывается из главного цикла
// Должен вызываться регулярно (каждые step_ms мс)
void LCD_Backlight_SmoothUpdate(void);


//static uint32_t LCD_Backlight_LevelToCCR2(uint8_t level);

uint8_t LCD_Backlight_PWMToLevel(uint32_t ccr2);












#endif
