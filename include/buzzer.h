#ifndef BUZZER_H
#define BUZZER_H

#include "main.h"
#include "stdint.h"
#include "stdbool.h"
#include "tim.h"

void Buzzer_SetFrequency(uint16_t frequency); // Hz
void Buzzer_On(uint16_t frequency);
void Buzzer_Off(void);
void Buzzer_SetDuty(uint16_t duty_percent); // 0..100

/* Неблокирующий запуск */
void Buzzer_PlayTone(uint16_t frequency, uint16_t duration_ms);

/* Вызывается из TIM3 прерывания — не вызывать вручную */
void Buzzer_Ticker(void);

/* Мгновенная остановка (для выключения из меню) */
void Buzzer_Stop(void);

/* Проверить, активен ли buzzer */
bool Buzzer_IsActive(void);

/* Быстрые звуки (неблокирующие) */
void Buzzer_Short(void);
void Buzzer_Long(void);
void Buzzer_Beep2(void);
void Buzzer_Beep3(void);
void Buzzer_Alarm(void);
void Buzzer_Confirm(void);
void Buzzer_Error(void);

#endif
