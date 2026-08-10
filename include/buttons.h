#ifndef BUTTONS_H
#define BUTTONS_H

#include "main.h"
#include "pcf8574.h"
#include <stdint.h>
#include <stdbool.h>

// Конфигурация таймаутов (можно вынести в настройки)
#define BUTTON_HOLD_TIMEOUT_MS 500 // Время для срабатывания "зажато"
#define BUTTON_REPEAT_TIMEOUT_MS 200 // Повторный срабатывание при удержании

typedef enum {
    KEY_NONE = 0,
    KEY_CANCEL = 1, // 0xFE -> Bit 1 (Pin 1)
    KEY_UP = 2,     // 0xFD -> Bit 2 (Pin 2) -- Note: 0xFD is 11111101, so Bit 2 is low. Wait, 0xFD = 253. 253 & 1 = 1 (Pin 0 high). 253 & 2 = 2 (Pin 1 high). 253 & 4 = 4 (Pin 2 low?). 
                    // Let's rely on the mapping logic in .c.
    KEY_DOWN = 3,
    KEY_ENTER = 4
} MenuKey;

// Raw PCF8574 scan codes for mapping
#define RAW_CANCEL 0xFE
#define RAW_UP     0xFD
#define RAW_DOWN   0xFB
#define RAW_ENTER  0xF7

typedef struct {
    PCF8574_HandleTypeDef *pcf;
    bool state[8];         // текущее состояние кнопки: true = нажата
    bool was_pressed[8];   // флаг нажатия *с прошлого опроса*

    // --- Новые поля для автоматизации ---
    // Таймер простого нажатия (используется для определения "только что")
    uint16_t press_start_time[8]; 
    bool is_pressed_now[8];    // Двойная буферизация для корректного edge detection
    
    // Для_long_press/hold_
    bool is_held[8];           // Флаг: кнопка удерживается
    bool hold_triggered[8];    // Флаг: событие зажатия произошло в этот вызов
    uint32_t hold_start_time[8];
    
    // Общий контекст (опционально, если нужно глобальное время)
    // Предположим, что есть геттер времени или мы передаем ms_tick глобально
} Buttons_HandleTypeDef;

static MenuKey RawToKey(uint8_t raw_value);
void Buttons_Init(Buttons_HandleTypeDef *btn, PCF8574_HandleTypeDef *pcf);
void Buttons_Update(Buttons_HandleTypeDef *btn);
MenuKey Buttons_GetKeyShortPress(Buttons_HandleTypeDef *btn);
MenuKey Buttons_GetKeyHold(Buttons_HandleTypeDef *btn);
MenuKey Buttons_GetKeyCurrentlyHeld(Buttons_HandleTypeDef *btn);


#endif // BUTTONS_H