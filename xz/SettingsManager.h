#pragma once
#include "globals.h"

extern bool settingsDirty;         // Общий флаг: что-то изменилось

float readFloatFromEEPROM(uint16_t addr);

void markSettingDirty();           // Пометить как "грязное"
void savePendingSettings();        // Сохранить, если есть изменения
void saveAllSettings();            // Полное сохранение (по запросу)
void loadAllSettings();            // Полное чтение (при старте)

bool hasSavedWiFiCredentials();
bool readWiFiCredentials(char* ssid, char* pass);
void saveWiFiCredentials(const char* ssid, const char* pass);
void clearWiFiCredentials();
void clearAllEEPROM();

//функции преобразования из float в fixed и обратно
uint32_t floatToFixed(float f);
float fixedToFloat(uint32_t x);