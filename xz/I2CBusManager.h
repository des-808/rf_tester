#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "globals.h"

//extern TwoWire I2C_PERIPH;
//extern TwoWire& I2C_PERIPH;
// Адреса устройств
#define I2C_ADDR_PCF8574    0x27
#define I2C_ADDR_DS3231     0x68
#define I2C_ADDR_EEPROM     0x57
#define I2C_ADDR_OLED       0x78  // или 0x7A

// Инициализация шин
void i2cInit();

// Проверка наличия устройства на шине
bool i2cDeviceExists(uint8_t addr, TwoWire &wire);

// === Функции для периферии (I2C_PERIPH) ===
uint8_t i2cReadPCF();  // Чтение с расширителя
bool i2cWriteEEPROM(uint16_t memAddr, uint8_t data);
uint8_t i2cReadEEPROM(uint16_t memAddr);
bool i2cReadDS3231Time(uint8_t *sec, uint8_t *min, uint8_t *hour);

// Вспомогательные
uint8_t bcdToDec(uint8_t val);
uint8_t decToBcd(uint8_t val);

// Сканирование шины (для отладки)
void i2cScan(TwoWire &wire, Stream &output = Serial0);

bool i2cReadDS3231Date(uint8_t* day, uint8_t* month, uint8_t* year);