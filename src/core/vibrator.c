#include "vibrator.h"

/* Неблокирующий вибратор */
static uint32_t vibro_start_tick = 0;
static uint16_t vibro_duration   = 0;
static bool     vibro_active     = false;

void Vibrator_On(void) {
    HAL_GPIO_WritePin(VIBRATOR_GPIO_Port, VIBRATOR_Pin, GPIO_PIN_SET);
}

void Vibrator_Off(void) {
    HAL_GPIO_WritePin(VIBRATOR_GPIO_Port, VIBRATOR_Pin, GPIO_PIN_RESET);
    vibro_active = false;
}

void Vibrator_Pulse(uint16_t duration_ms) {
    /* Если вибратор уже активен — перезапускаем */
    vibro_start_tick = HAL_GetTick();
    vibro_duration   = duration_ms;
    vibro_active     = true;
    Vibrator_On();
}

/* Вызывать из main loop */
void Vibrator_Update(void) {
    if (!vibro_active) return;
    
    if (HAL_GetTick() - vibro_start_tick >= vibro_duration) {
        Vibrator_Off();
    }
}

bool Vibrator_IsActive(void) {
    return vibro_active;
}
