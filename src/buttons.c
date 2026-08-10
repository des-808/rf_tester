#include "buttons.h"

extern uint32_t HAL_GetTick(void); // Замените на ваш геттер миллисекунд, если нужно

Buttons_HandleTypeDef btn_s;


/**
 * @brief Maps a raw PCF8574 value to a logical MenuKey.
 * 
 * PCF8574 is Active Low. 
 * 0xFE (1111 1110) -> Bit 0 is Low -> Pin 0. 
 * 0xFD (1111 1101) -> Bit 1 is Low -> Pin 1.
 * 0xFB (1111 1011) -> Bit 2 is Low -> Pin 2.
 * 0xF7 (1111 0111) -> Bit 3 is Low -> Pin 3.
 */
static MenuKey RawToKey(uint8_t raw_value) {
    // Check which bit is 0 (Active Low)
    if ((raw_value & 0x01) == 0) return KEY_CANCEL; // Pin 0
    if ((raw_value & 0x02) == 0) return KEY_UP;     // Pin 1
    if ((raw_value & 0x04) == 0) return KEY_DOWN;   // Pin 2
    if ((raw_value & 0x08) == 0) return KEY_ENTER;  // Pin 3
    return KEY_NONE;
}

/**
 * @brief Maps a physical Pin index (0-7) to a logical MenuKey.
 */
static MenuKey PinToKey(uint8_t pin_index) {
    switch (pin_index) {
        case 0: return KEY_CANCEL; // Pin 0 -> 0xFE
        case 1: return KEY_UP;     // Pin 1 -> 0xFD
        case 2: return KEY_DOWN;   // Pin 2 -> 0xFB
        case 3: return KEY_ENTER;  // Pin 3 -> 0xF7
        default: return KEY_NONE;
    }
}

void Buttons_Init(Buttons_HandleTypeDef *btn, PCF8574_HandleTypeDef *pcf) {
    btn->pcf = pcf;
    for (int i = 0; i < 8; i++) {
        btn->state[i] = false;
        btn->was_pressed[i] = false;
        btn->is_held[i] = false;
        btn->hold_triggered[i] = false;
        btn->press_start_time[i] = 0;
    }
    // Read initial state
    uint8_t data = PCF8574_Read8(pcf);
    for (int i = 0; i < 8; i++) {
        btn->state[i] = ((data >> i) & 0x01) == 0; // True if pressed (Active Low)
    }
}

void Buttons_Update(Buttons_HandleTypeDef *btn) {
    if (btn->pcf == NULL) return;

    uint32_t now = HAL_GetTick(); 
    uint8_t raw_data = PCF8574_Read8(btn->pcf);

    for (int i = 0; i < 8; i++) {
        // Current physical state (Active Low: 0 = Pressed)
        bool current_active = ((raw_data >> i) & 0x01) == 0; 
        bool previous_active = btn->state[i];
        
        // 1. Detect Edge: Press (0 -> 1 logic state, i.e., Released -> Pressed)
        bool edge_pressed = (current_active && !previous_active);
        
        // 2. Detect Edge: Release
        bool edge_released = (!current_active && previous_active);

        if (edge_pressed) {
            // Button just pressed
            btn->was_pressed[i] = true;
            btn->press_start_time[i] = now;
            btn->is_held[i] = false;
            btn->hold_triggered[i] = false;
        } 
        else if (edge_released) {
            // Button just released
            btn->was_pressed[i] = false;
            btn->is_held[i] = false;
            btn->hold_triggered[i] = false;
        } 
        else if (current_active && previous_active) {
            // Button is currently held down
            if (!btn->is_held[i]) {
                // Check if hold timeout has been reached
                uint32_t duration = now - btn->press_start_time[i];
                if (duration >= BUTTON_HOLD_TIMEOUT_MS) {
                    btn->is_held[i] = true;
                    btn->hold_triggered[i] = true; // Flag for "Hold Just Triggered"
                }
            }
        }

        // Update state for next frame
        btn->state[i] = current_active;
    }
}

MenuKey Buttons_GetKeyShortPress(Buttons_HandleTypeDef *btn) {
    for (int i = 0; i < 8; i++) {
        if (btn->was_pressed[i]) {
            // Clear the flag
            btn->was_pressed[i] = false;
            return PinToKey(i);
        }
    }
    return KEY_NONE;
}

MenuKey Buttons_GetKeyHold(Buttons_HandleTypeDef *btn) {
    for (int i = 0; i < 8; i++) {
        if (btn->hold_triggered[i]) {
            // Clear the flag
            btn->hold_triggered[i] = false;
            return PinToKey(i);
        }
    }
    return KEY_NONE;
}

MenuKey Buttons_GetKeyCurrentlyHeld(Buttons_HandleTypeDef *btn) {
    for (int i = 0; i < 8; i++) {
        if (btn->is_held[i]) {
            return PinToKey(i);
        }
    }
    return KEY_NONE;
}