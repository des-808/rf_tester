#ifndef I2C_SCANNER_H
#define I2C_SCANNER_H

#include "main.h"
#include "pcf8574.h"
#include "st7796.h"

typedef struct {
    I2C_HandleTypeDef *hi2c;
    bool found[0x80];  // true, если устройство отвечает на этот адрес
    uint16_t count;    // количество найденных устройств
} I2C_Scanner_HandleTypeDef;

void I2C_Scanner_Init(I2C_Scanner_HandleTypeDef *scanner, I2C_HandleTypeDef *hi2c);
void I2C_Scanner_Run(I2C_Scanner_HandleTypeDef *scanner);
const char* I2C_Scanner_GetDeviceName(uint8_t address);
void I2C_Scanner_PrintOnTFT(I2C_Scanner_HandleTypeDef *scanner, uint16_t x, uint16_t y, uint16_t color, uint16_t bg_color, Sprite_t *sprite);

#endif