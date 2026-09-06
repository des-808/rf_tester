#ifndef VIBRATOR_H
#define VIBRATOR_H

#include "main.h"
#include "stdint.h"
#include "stdbool.h"

#define VIBRATOR_Pin GPIO_PIN_10
#define VIBRATOR_GPIO_Port GPIOE

void Vibrator_On(void);
void Vibrator_Off(void);
void Vibrator_Pulse(uint16_t duration_ms);

/* Вызывается из TIM3 прерывания — не вызывать вручную */
void Vibrator_Ticker(void);

/* Проверить, активен ли вибратор */
bool Vibrator_IsActive(void);

#endif
