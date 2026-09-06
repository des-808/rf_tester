#include "vibrator.h"

/* Неблокирующий вибратор — управляется TIM3 прерыванием */
static volatile uint32_t vibro_start_tick = 0;
static volatile uint16_t vibro_duration   = 0;
static volatile bool     vibro_active     = false;

void Vibrator_On(void) {
    HAL_GPIO_WritePin(VIBRATOR_GPIO_Port, VIBRATOR_Pin, GPIO_PIN_SET);
}

void Vibrator_Off(void) {
    HAL_GPIO_WritePin(VIBRATOR_GPIO_Port, VIBRATOR_Pin, GPIO_PIN_RESET);
    __disable_irq();
    vibro_active = false;
    __enable_irq();
}

void Vibrator_Pulse(uint16_t duration_ms) {
    __disable_irq();
    vibro_start_tick = 0;
    vibro_duration   = duration_ms;
    vibro_active     = true;
    __enable_irq();
    Vibrator_On();
}

/* Вызывается из TIM3 прерывания каждый 1мс */
void Vibrator_Ticker(void) {
    if (!vibro_active) return;
    
    if (vibro_start_tick == 0) {
        vibro_start_tick = 1;
        return;
    }
    
    vibro_start_tick++;
    if (vibro_start_tick >= vibro_duration) {
        Vibrator_Off();
    }
}

bool Vibrator_IsActive(void) {
    return vibro_active;
}
