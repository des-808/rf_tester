#include "SettingsManager.h"
#include "I2CBusManager.h"
#include "globals.h"


bool settingsDirty = false;
// Функция помечает, что настройки изменились
void markSettingDirty() {
    settingsDirty = true;
}

// Сохраняет только если были изменения
void savePendingSettings() {
    if (!settingsDirty) return;

    // Сохраняем только изменённые — или просто все (проще)
    saveAllSettings();  // Упрощённый вариант: всё сразу
    settingsDirty = false;
    Serial0.println("💾 Pending settings saved");
}

float readFloatFromEEPROM(uint16_t addr) {
    uint8_t bytes[4];
    for (int i = 0; i < 4; i++) {
        bytes[i] = i2cReadEEPROM(addr + i);
    }
    float value;
    memcpy(&value, bytes, 4);
    return value;
}

void loadAllSettings() {
    uint8_t valid = i2cReadEEPROM(ADDR_EEPROM_VALID);
    if (valid != EEPROM_VALID_KEY) {
        Serial0.println("⚠️ No valid settings found. Using defaults.");
        // Установим значения по умолчанию
        sys = 1;
        room = 1;
        btn = 1;
        rs485BaudIndex = 1;
        oledBrightness = 1;
        bluetoothEnabled = 0;
        wifiEnabled = 0;
        ntpSyncEnabled = 0;
        buzzerOnOff = 1;

        // === Значения по умолчанию для CC1101 ===
        cc1101Freq = 433.96f;
        cc1101BitRate = 9.6f;
        cc1101Modulation = 1;         // OOK
        cc1101PowerIndex = 7;         // 10 dBm
        cc1101RxBwIndex = 11;         // 406 kHz

        // Сохраним их!
        //Serial0.println("💾 Saving default settings...");
        saveAllSettings();
        Serial0.println("✅ Default settings saved.");
        return; // Оставляем значения по умолчанию
    }

    sys = i2cReadEEPROM(ADDR_SYS);
    room = i2cReadEEPROM(ADDR_ROOM);
    btn = i2cReadEEPROM(ADDR_BTN);
    rs485BaudIndex = i2cReadEEPROM(ADDR_RS485_BAUD);
    oledBrightness = i2cReadEEPROM(ADDR_OLED_BRIGHTNESS);
    bluetoothEnabled = i2cReadEEPROM(ADDR_BLUETOOTH);
    wifiEnabled = i2cReadEEPROM(ADDR_WIFI_ENABLED);
    ntpSyncEnabled = i2cReadEEPROM(ADDR_NTP_ENABLED);
    buzzerOnOff = i2cReadEEPROM(ADDR_BUZZER);

    // === Загружаем настройки CC1101 ===
    // Читаем частоту
    uint32_t freqFixed = 0;
    freqFixed |= (uint32_t)i2cReadEEPROM(ADDR_CC1101_FREQ_FIXED + 0) << 0;
    freqFixed |= (uint32_t)i2cReadEEPROM(ADDR_CC1101_FREQ_FIXED + 1) << 8;
    freqFixed |= (uint32_t)i2cReadEEPROM(ADDR_CC1101_FREQ_FIXED + 2) << 16;
    freqFixed |= (uint32_t)i2cReadEEPROM(ADDR_CC1101_FREQ_FIXED + 3) << 24;
    cc1101FreqFixed = freqFixed;
    cc1101Freq = fixedToFloat(freqFixed);

    // Читаем скорость
    uint32_t brFixed = 0;
    brFixed |= (uint32_t)i2cReadEEPROM(ADDR_CC1101_BITRATE_FIXED + 0) << 0;
    brFixed |= (uint32_t)i2cReadEEPROM(ADDR_CC1101_BITRATE_FIXED + 1) << 8;
    brFixed |= (uint32_t)i2cReadEEPROM(ADDR_CC1101_BITRATE_FIXED + 2) << 16;
    brFixed |= (uint32_t)i2cReadEEPROM(ADDR_CC1101_BITRATE_FIXED + 3) << 24;
    cc1101BitRateFixed = brFixed;
    cc1101BitRate = fixedToFloat(brFixed);

    // Читаем остальное
    cc1101Modulation = i2cReadEEPROM(ADDR_CC1101_MOD);
    cc1101PowerIndex = i2cReadEEPROM(ADDR_CC1101_POWER_INDEX);
    cc1101RxBwIndex = i2cReadEEPROM(ADDR_CC1101_RFBW_INDEX);

    // ✅ Синхронизируем с основными настройками
    CC1101Settings& s = cc1101GetSettings();
    s.frequency = cc1101Freq;
    s.bitRate = cc1101BitRate;
    s.modulation = cc1101Modulation;
    s.power = power_signal[cc1101PowerIndex];
    s.bandwich = bandwich[cc1101RxBwIndex];
    //Serial0.printf("🔧 Settings updated: F=%.2f, BR=%.2f, Mod=%d, Pwr=%d, Bw=%d\n",s.frequency, s.bitRate, s.modulation, s.power, s.bandwich);

    // Коррекция значений
    if (sys < 1 || sys > 32) sys = 5;
    if (room < 1 || room > 32) room = 1;
    if (btn < 1 || btn > 9) btn = 1;
    if (rs485BaudIndex > 7) rs485BaudIndex = 1;
    if (oledBrightness < 1 || oledBrightness > 10) oledBrightness = 5;

    Serial0.println("✅ Settings loaded from external EEPROM");
    //Serial0.printf("sys %2d room %2d btn %2d rs485 %2d oled %2d bt %2d wifi %2d ntp %2d",sys,room,btn,rs485BaudIndex,oledBrightness,bluetoothEnabled,wifiEnabled,ntpSyncEnabled); // Debug
    //Serial0.printf("💾 Loaded: BT=%d, WiFi=%d, NTP=%d\n",bluetoothEnabled, wifiEnabled, ntpSyncEnabled);
    Serial0.printf("💾 Loaded CC1101: F=%.2f, BR=%.1f, Mod=%d, PwrIdx=%d, BwIdx=%d\n",cc1101Freq, cc1101BitRate, cc1101Modulation, cc1101PowerIndex, cc1101RxBwIndex);           
} 
#define WRITE_IF_CHANGED(addr, var) { \
    uint8_t old = i2cReadEEPROM(addr); \
    if (old != var) { \
        i2cWriteEEPROM(addr, var); \
        changed = true; \
    } \
}

/* #define WRITE_FLOAT_IF_CHANGED(addr, f) { \
    uint32_t newValue = *(uint32_t*)&f; \
    uint32_t oldValue = 0; \
    oldValue |= (uint32_t)i2cReadEEPROM(addr + 0) << 0; \
    oldValue |= (uint32_t)i2cReadEEPROM(addr + 1) << 8; \
    oldValue |= (uint32_t)i2cReadEEPROM(addr + 2) << 16; \
    oldValue |= (uint32_t)i2cReadEEPROM(addr + 3) << 24; \
    if (oldValue != newValue) { \
        i2cWriteEEPROM(addr + 0, (newValue >> 0) & 0xFF); \
        i2cWriteEEPROM(addr + 1, (newValue >> 8) & 0xFF); \
        i2cWriteEEPROM(addr + 2, (newValue >> 16) & 0xFF); \
        i2cWriteEEPROM(addr + 3, (newValue >> 24) & 0xFF); \
        changed = true; \
        Serial0.printf("💾 Updated %s at %d\n", #f, addr); \
    } \
} */

void saveAllSettings() {
    bool changed = false;
    WRITE_IF_CHANGED(ADDR_SYS, sys);
    WRITE_IF_CHANGED(ADDR_ROOM, room);
    WRITE_IF_CHANGED(ADDR_BTN, btn);
    WRITE_IF_CHANGED(ADDR_RS485_BAUD, rs485BaudIndex);
    WRITE_IF_CHANGED(ADDR_OLED_BRIGHTNESS, oledBrightness);
    WRITE_IF_CHANGED(ADDR_BLUETOOTH, bluetoothEnabled);
    WRITE_IF_CHANGED(ADDR_WIFI_ENABLED, wifiEnabled);
    WRITE_IF_CHANGED(ADDR_NTP_ENABLED, ntpSyncEnabled);
    WRITE_IF_CHANGED(ADDR_BUZZER, buzzerOnOff);

    // === Сохраняем CC1101 настройки ===
    // Сохраняем частоту
    uint32_t newFreqFixed = floatToFixed(cc1101Freq);
    WRITE_IF_CHANGED(ADDR_CC1101_FREQ_FIXED + 0, (newFreqFixed >> 0) & 0xFF);
    WRITE_IF_CHANGED(ADDR_CC1101_FREQ_FIXED + 1, (newFreqFixed >> 8) & 0xFF);
    WRITE_IF_CHANGED(ADDR_CC1101_FREQ_FIXED + 2, (newFreqFixed >> 16) & 0xFF);
    WRITE_IF_CHANGED(ADDR_CC1101_FREQ_FIXED + 3, (newFreqFixed >> 24) & 0xFF);

    // Сохраняем скорость
    uint32_t newBrFixed = floatToFixed(cc1101BitRate);
    WRITE_IF_CHANGED(ADDR_CC1101_BITRATE_FIXED + 0, (newBrFixed >> 0) & 0xFF);
    WRITE_IF_CHANGED(ADDR_CC1101_BITRATE_FIXED + 1, (newBrFixed >> 8) & 0xFF);
    WRITE_IF_CHANGED(ADDR_CC1101_BITRATE_FIXED + 2, (newBrFixed >> 16) & 0xFF);
    WRITE_IF_CHANGED(ADDR_CC1101_BITRATE_FIXED + 3, (newBrFixed >> 24) & 0xFF);
    // Сохраняем остальное
    WRITE_IF_CHANGED(ADDR_CC1101_MOD, cc1101Modulation);
    WRITE_IF_CHANGED(ADDR_CC1101_POWER_INDEX, cc1101PowerIndex);
    WRITE_IF_CHANGED(ADDR_CC1101_RFBW_INDEX, cc1101RxBwIndex);

    if (changed) {
        i2cWriteEEPROM(ADDR_EEPROM_VALID, EEPROM_VALID_KEY);
        Serial0.println("✅ Settings saved (only changed)");
    }
    Serial0.printf("💾 Saved CC1101: F=%.2f, BwIdx=%d\n", cc1101Freq, cc1101RxBwIndex);
}



// === ФУНКЦИИ РАБОТЫ С Wi-Fi ===

bool hasSavedWiFiCredentials() {
    return i2cReadEEPROM(ADDR_WIFI_VALID) == WIFI_VALID_KEY;
}

bool readWiFiCredentials(char* ssid, char* pass) {
    if (!hasSavedWiFiCredentials()) return false;

    bool ssidFound = false, passFound = false;
    char tempSsid[MAX_SSID_LEN + 1] = {0};
    char tempPass[MAX_PASS_LEN + 1] = {0};

    // Читаем SSID
    for (int i = 0; i < MAX_SSID_LEN; i++) {
        uint8_t b = i2cReadEEPROM(ADDR_SSID + i);
        if (b == 0 || b == 0xFF) break;
        tempSsid[i] = b;
        ssidFound = true;
    }

    // Читаем пароль
    for (int i = 0; i < MAX_PASS_LEN; i++) {
        uint8_t b = i2cReadEEPROM(ADDR_PASSWORD + i);
        if (b == 0 || b == 0xFF) break;
        tempPass[i] = b;
        passFound = true;
    }

    if (ssidFound) strcpy(ssid, tempSsid);
    if (passFound) strcpy(pass, tempPass);

    return ssidFound; // если SSID есть — считаем, что данные валидны
}

void saveWiFiCredentials(const char* ssid, const char* pass) {
    // Очистка старых данных
    for (int i = 0; i < MAX_SSID_LEN; i++) {
        i2cWriteEEPROM(ADDR_SSID + i, 0);
    }
    for (int i = 0; i < MAX_PASS_LEN; i++) {
        i2cWriteEEPROM(ADDR_PASSWORD + i, 0);
    }

    // Запись новых
    for (size_t i = 0; i < strlen(ssid) && i < MAX_SSID_LEN; i++) {
        i2cWriteEEPROM(ADDR_SSID + i, ssid[i]);
    }
    for (size_t i = 0; i < strlen(pass) && i < MAX_PASS_LEN; i++) {
        i2cWriteEEPROM(ADDR_PASSWORD + i, pass[i]);
    }

    i2cWriteEEPROM(ADDR_WIFI_VALID, WIFI_VALID_KEY);
    Serial0.println("💾 Wi-Fi credentials saved via SettingsManager");
}

void clearWiFiCredentials() {
    for (int i = 0; i < MAX_SSID_LEN; i++) {
        i2cWriteEEPROM(ADDR_SSID + i, 0);
    }
    for (int i = 0; i < MAX_PASS_LEN; i++) {
        i2cWriteEEPROM(ADDR_PASSWORD + i, 0);
    }
    i2cWriteEEPROM(ADDR_WIFI_VALID, 0); // сбрасываем флаг
    Serial0.println("🗑️ Wi-Fi credentials cleared");
}


void clearAllEEPROM() {
    Serial0.println("⚠️ ERASING ALL EEPROM... (512 bytes)");

    for (int i = 0; i < 512; i++) {
        i2cWriteEEPROM(i, 0xFF);  // Обычно "пустая" ячейка — 0xFF
        delay(1);  // Некоторые EEPROM требуют паузу между записями
    }

    // Сбрасываем валидный ключ
    i2cWriteEEPROM(ADDR_EEPROM_VALID, 0x00);
    
    Serial0.println("✅ EEPROM erased. All bytes set to 0xFF");
}

// Конвертирует float → fixed (×100)
uint32_t floatToFixed(float f) {
    return (uint32_t)(f * 100.0f + 0.5f);  // +0.5 для округления
}

// Конвертирует fixed → float
float fixedToFloat(uint32_t x) {
    return x / 100.0f;
}