#ifndef BUTTONS_H
#define BUTTONS_H

#include "main.h"
#include "pcf8574.h"
#include <stdint.h>
#include <stdbool.h>

typedef struct {
    PCF8574_HandleTypeDef *pcf;
    bool state[8];         // текущее состояние кнопки: true = нажата
    bool was_pressed[8];   // флаг нажатия *с прошлого опроса*
} Buttons_HandleTypeDef;

void Buttons_Init(Buttons_HandleTypeDef *btn, PCF8574_HandleTypeDef *pcf);
void Buttons_Update(Buttons_HandleTypeDef *btn);
bool Buttons_IsPressed(Buttons_HandleTypeDef *btn, uint8_t pin);
bool Buttons_IsJustPressed(Buttons_HandleTypeDef *btn, uint8_t pin);
uint8_t Buttons_GetJustPressed(Buttons_HandleTypeDef *btn);

#endif // BUTTONS_H