#include "buttons.h"

Buttons_HandleTypeDef btn_s;

void Buttons_Init(Buttons_HandleTypeDef *btn, PCF8574_HandleTypeDef *pcf) {
    btn->pcf = pcf;
    for (int i = 0; i < 8; i++) {
        btn->state[i] = false;
        btn->was_pressed[i] = false;
    }
    // Сразу читаем стартовое состояние
    PCF8574_Read8(pcf);
}

void Buttons_Update(Buttons_HandleTypeDef *btn) {
    // Обновляем состояние кнопок через PCF8574
    uint8_t data = PCF8574_Read8(btn->pcf);

    for (int i = 0; i < 8; i++) {
        bool current = ((data >> i) & 0x01) == 0;  // LOW (0) = нажата (активный низ)
        btn->state[i] = current;
        btn->was_pressed[i] = current && !btn->state[i]; // false → true: только что нажата
    }
}

bool Buttons_IsPressed(Buttons_HandleTypeDef *btn, uint8_t pin) {
    if (pin > 7) return false;
    return btn->state[pin];
}

bool Buttons_IsJustPressed(Buttons_HandleTypeDef *btn, uint8_t pin) {
    if (pin > 7) return false;
    uint8_t data = PCF8574_Read8(btn->pcf);
    bool current = ((data >> pin) & 0x01) == 0;
    bool justPressed = current && !btn->state[pin];
    btn->state[pin] = current; // обновить только для этой кнопки (опционально)
    return justPressed;
}

uint8_t Buttons_GetJustPressed(Buttons_HandleTypeDef *btn) {
    // Сначала обновим все кнопки
    Buttons_Update(btn);

    // Теперь ищем только что нажатые
    for (int i = 0; i < 8; i++) {
        bool current = btn->state[i];
        //bool wasReleased = !btn->was_pressed[i]; // или !btn->state_prev[i] — зависит от реализации
        if (current && !btn->was_pressed[i]) {
            btn->was_pressed[i] = true; // помечаем, чтобы не повторяться
            return i;
        } else if (!current) {
            btn->was_pressed[i] = false; // сброс при отпускании
        }
    }
    return 0xFF;
}