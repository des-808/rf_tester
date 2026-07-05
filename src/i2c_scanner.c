#include "i2c_scanner.h"
#include <stdio.h>
#include <string.h>

void I2C_Scanner_Init(I2C_Scanner_HandleTypeDef *scanner, I2C_HandleTypeDef *hi2c) {
    scanner->hi2c = hi2c;
    memset(scanner->found, 0, sizeof(scanner->found));
    scanner->count = 0;
}

void I2C_Scanner_Run(I2C_Scanner_HandleTypeDef *scanner) {
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        // Попытка передачи данных (один байт — не важно какой)
        HAL_StatusTypeDef status = HAL_I2C_Master_Transmit(scanner->hi2c, addr << 1, NULL, 0, 100);
        if (status == HAL_OK) {
            scanner->found[addr] = true;
            scanner->count++;
        }
    }
}

const char* I2C_Scanner_GetDeviceName(uint8_t address) {
    switch (address) {
        case 0x20: return "PCF8574";  // I/O expander
        case 0x27: return "PCF8574 (ALT)";
        case 0x38: return "PCF8574A (ALT2)";
        case 0x57: return "EEPROM 24C02";
        case 0x5D: return "LC709203F (battery)";
        case 0x68: return "DS3231";
        case 0x69: return "MPU6050 (ALT)";
        case 0x3C: return "SSD1306 OLED";
        case 0x3D: return "SSD1306 OLED (ALT)";
        case 0x60: return "RTC DS3231";
        case 0x1D: return " accel/gyro ADXL345";
        case 0x53: return " accel ADXL345 (ALT)";
        default:   return "Unknown device";
    }
}
extern 

void I2C_Scanner_PrintOnTFT(I2C_Scanner_HandleTypeDef *scanner, uint16_t x, uint16_t y, uint16_t color, uint16_t bg_color, Sprite_t *sprite) {
    char buffer[64];

    // Заголовок
    lcd_print_to_buffer_ex(x, y, color, "I2C Scanner v1.0", bg_color, sprite, false);
    y += 16;

    if (scanner->count == 0) {
        lcd_print_to_buffer_ex(x, y, color, "No devices found", bg_color, sprite, false);
        return;
    }

    // Список устройств
    for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
        if (scanner->found[addr]) {
            const char *devName = I2C_Scanner_GetDeviceName(addr);
            snprintf(buffer, sizeof(buffer), "0x%02X: %s", addr, devName);
            lcd_print_to_buffer_ex(x, y, color, buffer, bg_color, sprite, true);
            y += 16;
            if (y > 220) break;  // защита от выхода за экран
        }
    }
}