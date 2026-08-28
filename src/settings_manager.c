/**
 * ========================================================================
 *  Settings Manager — Implementation
 *  STM32H7 + W25Q SPI Flash
 *  Бинарное хранение настроек (без JSON, быстро и компактно)
 * ========================================================================
 */

#include "settings_manager.h"
#include "settings_storage.h"

#include <string.h>
#include <stdio.h>

/* ========================================================================
 *  Глобальные переменные проекта (экстерны из menu.c / main.c)
 * ======================================================================== */
extern uint8_t  rs485BaudIndex;
extern uint8_t  oledBrightness;
extern int      bluetoothEnabled, wifiEnabled, ntpSyncEnabled, buzzerOnOff;

/* CC1101 */
extern uint16_t cc1101FreqFixed;
extern uint16_t cc1101BitRateFixed;
extern uint8_t  cc1101RxBwIndex;
extern uint8_t  cc1101Modulation;
extern uint8_t  cc1101PowerIndex;

/* LCD backlight (из menu.c) */
extern uint8_t lcd_backlight_level;

/* ========================================================================
 *  Внутреннее состояние
 * ======================================================================== */
static Settings_t g_settings;
static bool       g_initialized = false;

/* Глобальный флаг "грязных" настроек (как в xz) */
bool settingsDirty = false;

/* ========================================================================
 *  Layout в Flash (внутри сектора 4КБ):
 *  [0x0000] Settings_t (145 байт)
 *  [0x0100] WiFi credentials:
 *           [0x0100] valid key (1 байт)
 *           [0x0101] SSID (32 байта)
 *           [0x0201] password (64 байта)
 * ======================================================================== */
#define WIFI_VALID_OFFSET       0x0100U
#define WIFI_SSID_OFFSET        0x0101U
#define WIFI_PASS_OFFSET        0x0201U

/* ========================================================================
 *  Значения по умолчанию (как в xz/settingsManager.cpp)
 * ======================================================================== */
static void settings_set_defaults(Settings_t* s) {
    memset(s, 0, sizeof(*s));
    s->magic     = SETTINGS_MAGIC;
    s->version   = SETTINGS_SCHEMA_VERSION;

    /* Передатчик */
    s->sys       = 1;
    s->room      = 1;
    s->btn       = 1;

    /* Общие настройки */
    s->rs485_baud_index    = 4;   /* Дефолтная скорость RS485 */
    s->oled_brightness     = 5;   /* Половина яркости */
    s->bluetooth_enabled   = 0;
    s->wifi_enabled        = 0;
    s->ntp_sync_enabled    = 0;
    s->buzzer_enabled      = 1;

    /* CC1101 (fixed-point x100) */
    s->cc1101_freq_fixed   = 43390;  /* 433.90 МГц */
    s->cc1101_bitrate_fixed = 960;   /* 9.60 kbps */
    s->cc1101_rxbw_index   = 11;     /* 406 kHz */
    s->cc1101_modulation   = 1;      /* GFSK */
    s->cc1101_power_index  = 7;      /* max power */
}

/* ========================================================================
 *  Валидация настроек
 * ======================================================================== */
static bool settings_validate(const Settings_t* s) {
    if (s->magic != SETTINGS_MAGIC) return false;
    if (s->version != SETTINGS_SCHEMA_VERSION) return false;

    if (s->rs485_baud_index > RS485_BAUD_MAX) return false;
    if (s->oled_brightness < BACKLIGHT_MIN || s->oled_brightness > BACKLIGHT_MAX) return false;
    if (s->bluetooth_enabled > 1) return false;
    if (s->wifi_enabled > 1) return false;
    if (s->ntp_sync_enabled > 1) return false;
    if (s->buzzer_enabled > 1) return false;
    if (s->cc1101_freq_fixed < CC1101_FREQ_MIN || s->cc1101_freq_fixed > CC1101_FREQ_MAX) return false;
    if (s->cc1101_bitrate_fixed < CC1101_BITRATE_MIN || s->cc1101_bitrate_fixed > CC1101_BITRATE_MAX) return false;
    if (s->cc1101_rxbw_index > CC1101_RXBW_MAX) return false;
    if (s->cc1101_modulation > CC1101_MOD_MAX) return false;
    if (s->cc1101_power_index > CC1101_PWR_MAX) return false;

    return true;
}

/* ========================================================================
 *  Применение настроек к глобальным переменным проекта
 * ======================================================================== */
void SettingsManager_Apply(void) {
    rs485BaudIndex         = g_settings.rs485_baud_index;
    oledBrightness         = g_settings.oled_brightness;
    lcd_backlight_level    = g_settings.oled_brightness;
    bluetoothEnabled       = g_settings.bluetooth_enabled;
    wifiEnabled            = g_settings.wifi_enabled;
    ntpSyncEnabled         = g_settings.ntp_sync_enabled;
    buzzerOnOff            = g_settings.buzzer_enabled;

    cc1101FreqFixed        = (uint16_t)g_settings.cc1101_freq_fixed;
    cc1101BitRateFixed     = (uint16_t)g_settings.cc1101_bitrate_fixed;
    cc1101RxBwIndex        = g_settings.cc1101_rxbw_index;
    cc1101Modulation       = g_settings.cc1101_modulation;
    cc1101PowerIndex       = g_settings.cc1101_power_index;
}

/* ========================================================================
 *  WiFi credentials — Flash layout
 * ======================================================================== */
bool hasSavedWiFiCredentials(void) {
    uint8_t valid = SettingsStorage_ReadByte(WIFI_VALID_OFFSET);
    return (valid == WIFI_VALID_KEY);
}

bool readWiFiCredentials(char* ssid, char* pass) {
    if (!hasSavedWiFiCredentials()) return false;

    /* Читаем SSID по байтово */
    for (int i = 0; i <= MAX_SSID_LEN; i++) {
        ssid[i] = (char)SettingsStorage_ReadByte(WIFI_SSID_OFFSET + i);
        if (ssid[i] == 0 || ssid[i] == (char)0xFF) {
            ssid[i] = '\0';
            break;
        }
    }

    /* Читаем пароль по байтово */
    for (int i = 0; i <= MAX_PASS_LEN; i++) {
        pass[i] = (char)SettingsStorage_ReadByte(WIFI_PASS_OFFSET + i);
        if (pass[i] == 0 || pass[i] == (char)0xFF) {
            pass[i] = '\0';
            break;
        }
    }

    return true;
}

void saveWiFiCredentials(const char* ssid, const char* pass) {
    /* Очищаем старые данные по байтово */
    for (int i = 0; i <= MAX_SSID_LEN; i++) {
        SettingsStorage_WriteByte(WIFI_SSID_OFFSET + i, 0);
    }
    for (int i = 0; i <= MAX_PASS_LEN; i++) {
        SettingsStorage_WriteByte(WIFI_PASS_OFFSET + i, 0);
    }

    /* Записываем новые */
    for (size_t i = 0; i < strlen(ssid) && i <= MAX_SSID_LEN; i++) {
        SettingsStorage_WriteByte(WIFI_SSID_OFFSET + i, (uint8_t)ssid[i]);
    }
    SettingsStorage_WriteByte(WIFI_SSID_OFFSET + strlen(ssid), 0);
    
    for (size_t i = 0; i < strlen(pass) && i <= MAX_PASS_LEN; i++) {
        SettingsStorage_WriteByte(WIFI_PASS_OFFSET + i, (uint8_t)pass[i]);
    }
    SettingsStorage_WriteByte(WIFI_PASS_OFFSET + strlen(pass), 0);

    /* Ставим флаг валидности */
    SettingsStorage_WriteByte(WIFI_VALID_OFFSET, WIFI_VALID_KEY);
}

void clearWiFiCredentials(void) {
    for (int i = 0; i <= MAX_SSID_LEN; i++) {
        SettingsStorage_WriteByte(WIFI_SSID_OFFSET + i, 0);
    }
    for (int i = 0; i <= MAX_PASS_LEN; i++) {
        SettingsStorage_WriteByte(WIFI_PASS_OFFSET + i, 0);
    }
    SettingsStorage_WriteByte(WIFI_VALID_OFFSET, 0);
}

/* ========================================================================
 *  Fixed-point helpers (как в xz)
 * ======================================================================== */
uint32_t floatToFixed(float f) {
    return (uint32_t)(f * 100.0f + 0.5f);
}

float fixedToFloat(uint32_t x) {
    return x / 100.0f;
}

/* ========================================================================
 *  Публичные API (как в xz)
 * ======================================================================== */

void markSettingDirty(void) {
    settingsDirty = true;
}

void savePendingSettings(void) {
    if (!settingsDirty) return;
    saveAllSettings();
    settingsDirty = false;
}

void saveAllSettings(void) {
    /* Записываем структуру настроек в Flash (offset 0x0000) */
    SettingsStorage_Write(&g_settings, sizeof(g_settings));
}

void loadAllSettings(void) {
    /* Читаем структуру настроек из Flash */
    bool loaded = SettingsStorage_Read(&g_settings, sizeof(g_settings));

    if (loaded && settings_validate(&g_settings)) {
        /* Валидные данные — применяем */
        SettingsManager_Apply();
    } else {
        /* Нет валидных данных — устанавливаем defaults и сохраняем */
        settings_set_defaults(&g_settings);
        saveAllSettings();
    }
}

void clearAllSettings(void) {
    settings_set_defaults(&g_settings);
    SettingsManager_Apply();
    saveAllSettings();
}

bool SettingsManager_Init(void) {
    /* Инициализируем storage */
    if (!SettingsStorage_Init()) {
        return false;
    }

    /* Устанавливаем defaults на всякий случай */
    settings_set_defaults(&g_settings);

    /* Загружаем настройки */
    loadAllSettings();

    g_initialized = true;
    return true;
}

const Settings_t* SettingsManager_Get(void) {
    return &g_settings;
}

Settings_t* SettingsManager_GetMutable(void) {
    return &g_settings;
}
