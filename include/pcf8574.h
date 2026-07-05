#ifndef PCF8574_H
#define PCF8574_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define PCF8574_DEFAULT_ADDRESS   (0x20 << 1)

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t address;
    uint8_t output_cache;
    uint8_t last_input;  // для сравнения с предыдущим состоянием
    bool changed;        // флаг, что состояние изменилось (используется в прерывании)
} PCF8574_HandleTypeDef;

void PCF8574_Init(PCF8574_HandleTypeDef *pcf, I2C_HandleTypeDef *hi2c, uint8_t address);
uint8_t PCF8574_Read8(PCF8574_HandleTypeDef *pcf);
void PCF8574_Write8(PCF8574_HandleTypeDef *pcf, uint8_t value);
uint8_t PCF8574_ReadPin(PCF8574_HandleTypeDef *pcf, uint8_t pin);
void PCF8574_WritePin(PCF8574_HandleTypeDef *pcf, uint8_t pin, bool value);
void PCF8574_SetPinMode(PCF8574_HandleTypeDef *pcf, uint8_t pin, bool input);
bool PCF8574_HasChanges(PCF8574_HandleTypeDef *pcf);
void PCF8574_AcknowledgeChanges(PCF8574_HandleTypeDef *pcf);

// Обработчик прерывания — вызывается из EXTI9_5_IRQHandler
void PCF8574_IRQHandler(PCF8574_HandleTypeDef *pcf);

#endif