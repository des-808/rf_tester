#include "buzzer.h"
#include "tim.h"

static uint16_t current_period = 415; // по умолчанию для 2400 Hz

void Buzzer_SetFrequency(uint16_t frequency) {
    if (frequency < 100 || frequency > 10000) return;

    uint32_t period = 120000000 / 120 / frequency - 1;
    if (period > 65535) period = 65535;
    current_period = (uint16_t)period;

    // Остановим канал
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);

    // Обновим ARR
    htim2.Instance->ARR = period;

    // UG для синхронного обновления
    htim2.Instance->EGR = TIM_EGR_UG;

    // Установим 100% duty (HIGH) — только для старта
    htim2.Instance->CCR3 = period/16;

    // Запустим — но это вызывает щелчок
    // HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
    // ❌ Убираем, чтобы не щёлкать при смене частоты
}

void Buzzer_On(uint16_t frequency) {
    Buzzer_SetFrequency(frequency);

    // Включаем только здесь — чтобы щёлчок был один раз
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_3);
}

void Buzzer_Off(void) {
    // Останавливаем PWM — это вызывает щелчок
    HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_3);

    // Принудительно LOW
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

void Buzzer_SetDuty(uint16_t duty_percent) {
    uint16_t pulse = (current_period * duty_percent) / 100;
    htim2.Instance->CCR3 = pulse;
}

void Buzzer_PlayTone(uint16_t frequency, uint16_t duration_ms) {
    Buzzer_On(frequency);
    HAL_Delay(duration_ms);
    Buzzer_Off();
}


void Buzzer_Short(void) {
    Buzzer_PlayTone(2400, 10);
}

void Buzzer_Long(void) {
    Buzzer_PlayTone(2400, 200);
}

void Buzzer_Beep2(void) {
    Buzzer_Short();
    HAL_Delay(100);
    Buzzer_Short();
}

void Buzzer_Beep3(void) {
    Buzzer_Short();
    HAL_Delay(100);
    Buzzer_Short();
    HAL_Delay(100);
    Buzzer_Short();
}

void Buzzer_Alarm(void) {
    for (int i = 0; i < 6; i++) {
        Buzzer_PlayTone(2400, 100);
        HAL_Delay(50);
    }
}

void Buzzer_Confirm(void) {
    Buzzer_Short();
    HAL_Delay(70);
    Buzzer_Short();
}

void Buzzer_Error(void) {
    Buzzer_PlayTone(2400, 150);
    HAL_Delay(100);
    Buzzer_PlayTone(2400, 150);
}