#include "pcf8574.h"
#include "st7796.h"
#include <stdio.h>

 /* static uint8_t PCF8574_Read8Cached(PCF8574_HandleTypeDef *pcf) {
    uint8_t data = 0;
    if (HAL_I2C_Master_Receive(pcf->hi2c, pcf->address, &data, 1, 100) == HAL_OK) {
        return data;
    }
        // ❗ ОБЯЗАТЕЛЬНО: сообщите об ошибке — добавьте LED или UART
    //  for (int i = 0; i < 4; i++) {
    //    HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    //    HAL_Delay(1000);
    //}
    return 0xFF;
} */



void PCF8574_Init(PCF8574_HandleTypeDef *pcf, I2C_HandleTypeDef *hi2c, uint8_t address) {
    pcf->hi2c = hi2c;
    pcf->address = address << 1;
    pcf->output_cache = 0xFF;
    pcf->last_input = 0xFF;
    pcf->changed = false;
}

uint8_t PCF8574_Read8(PCF8574_HandleTypeDef *pcf) {
    uint8_t data = 0;
    HAL_StatusTypeDef status = HAL_I2C_Master_Receive(pcf->hi2c, pcf->address, &data, 1, 100);
    if (status == HAL_OK) {
        return data;
    }
    return 0xFF; // ошибка — возвращаем 0xFF (все 1)
}

void PCF8574_Write8(PCF8574_HandleTypeDef *pcf, uint8_t value) {
    HAL_I2C_Master_Transmit(pcf->hi2c, pcf->address, &value, 1, 100);
    pcf->output_cache = value;
}

uint8_t PCF8574_ReadPin(PCF8574_HandleTypeDef *pcf, uint8_t pin) {
    if (pin > 7) return 0;
    uint8_t data = PCF8574_Read8(pcf);
    return (data >> pin) & 0x01;
}

void PCF8574_WritePin(PCF8574_HandleTypeDef *pcf, uint8_t pin, bool value) {
    if (pin > 7) return;

    if (value) {
        pcf->output_cache |= (1 << pin);
    } else {
        pcf->output_cache &= ~(1 << pin);
    }
    PCF8574_Write8(pcf, pcf->output_cache);
}

void PCF8574_SetPinMode(PCF8574_HandleTypeDef *pcf, uint8_t pin, bool input) {
    // PCF8574: pins are always input when read; output when written low/high.
    // Но если вы хотите, чтобы пин был выходом — просто пишите в него.
    // Если хотите настроить как вход (HIGH-Z), нужно писать в него '1' (и не трогать, чтобы не переконфигурировать).
    if (pin > 7) return;

    if (input) {
        pcf->output_cache |= (1 << pin);  // "input" = high-impedance (всегда можно читать), но для read8 нужна "1"
    } else {
        pcf->output_cache &= ~(1 << pin);
    }
    PCF8574_Write8(pcf, pcf->output_cache);
}



bool PCF8574_HasChanges(PCF8574_HandleTypeDef *pcf) {
    return pcf->changed;
}

void PCF8574_AcknowledgeChanges(PCF8574_HandleTypeDef *pcf) {
    pcf->changed = false;
}

/* void PCF8574_IRQHandler(PCF8574_HandleTypeDef *pcf) {
    // Читаем текущее состояние
    uint8_t current = PCF8574_Read8Cached(pcf);
    if (current != pcf->last_input) {
        pcf->last_input = current;
        pcf->changed = true;

        HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin); //Отладка: мигнуть LED
    }
    // Здесь флаг уже очищен HAL_GPIO_EXTI_IRQHandler — не нужно больше clear
} */

//extern Sprite_t main_screen_sprite;
void PCF8574_IRQHandler(PCF8574_HandleTypeDef *pcf) {
    uint8_t current = PCF8574_Read8(pcf);
    // Логика
    if (current != pcf->last_input) {
        pcf->last_input = current;
        pcf->changed = true;
    } 
}