#include "buzzer.h"

static uint16_t current_period = 415; // по умолчанию для 2400 Hz

/* Неблокирующий buzzer — управляется TIM3 прерыванием */
static volatile uint32_t buzzer_start_tick = 0;
static volatile uint16_t buzzer_duration   = 0;
static volatile bool     buzzer_active     = false;

void Buzzer_SetFrequency(uint16_t frequency) {
    if (frequency < 100 || frequency > 10000) return;

    uint32_t period = 120000000 / 120 / frequency - 1;
    if (period > 65535) period = 65535;
    current_period = (uint16_t)period;

    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
    htim2.Instance->ARR = period;
    htim2.Instance->EGR = TIM_EGR_UG;
    htim2.Instance->CCR3 = period / 16;
}

void Buzzer_On(uint16_t frequency) {
    Buzzer_SetFrequency(frequency);
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
}

void Buzzer_Off(void) {
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
    __disable_irq();
    buzzer_active = false;
    buzzer_start_tick = 0;
    __enable_irq();
}

void Buzzer_SetDuty(uint16_t duty_percent) {
    uint16_t pulse = (current_period * duty_percent) / 100;
    htim2.Instance->CCR3 = pulse;
}

/* Неблокирующий запуск звука */
void Buzzer_PlayTone(uint16_t frequency, uint16_t duration_ms) {
    /* Останавливаем предыдущий звук */
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
    
    /* Защищённая запись — прерывание не должно видеть частично обновлённые данные */
    __disable_irq();
    buzzer_start_tick = 0;
    buzzer_duration   = duration_ms;
    buzzer_active     = true;
    __enable_irq();
    
    Buzzer_On(frequency);
}

/* Вызывается из TIM3 прерывания каждый 1мс */
void Buzzer_Ticker(void) {
    if (!buzzer_active) return;
    
    if (buzzer_start_tick == 0) {
        buzzer_start_tick = 1;  // Первый тик — фиксируем старт
        return;
    }
    
    buzzer_start_tick++;
    if (buzzer_start_tick >= buzzer_duration) {
        Buzzer_Off();
    }
}

/* Мгновенная остановка (для выключения из меню) */
void Buzzer_Stop(void) {
    if (buzzer_active) {
        HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);
        __disable_irq();
        buzzer_active = false;
        buzzer_start_tick = 0;
        __enable_irq();
    }
}

bool Buzzer_IsActive(void) {
    return buzzer_active;
}

void Buzzer_Short(void) {
    Buzzer_PlayTone(2400, 10);
}

void Buzzer_Long(void) {
    Buzzer_PlayTone(2400, 200);
}

void Buzzer_Beep2(void) {
    Buzzer_PlayTone(2400, 10);
    HAL_Delay(100);
    Buzzer_PlayTone(2400, 10);
}

void Buzzer_Beep3(void) {
    Buzzer_PlayTone(2400, 10);
    HAL_Delay(100);
    Buzzer_PlayTone(2400, 10);
    HAL_Delay(100);
    Buzzer_PlayTone(2400, 10);
}

void Buzzer_Alarm(void) {
    for (int i = 0; i < 6; i++) {
        Buzzer_PlayTone(2400, 100);
        HAL_Delay(50);
    }
}

void Buzzer_Confirm(void) {
    Buzzer_PlayTone(2400, 10);
    HAL_Delay(70);
    Buzzer_PlayTone(2400, 10);
}

void Buzzer_Error(void) {
    Buzzer_PlayTone(2400, 150);
    HAL_Delay(100);
    Buzzer_PlayTone(2400, 150);
}
