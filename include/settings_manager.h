#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "settings_types.h"

/* ========================================================================
 *  Settings Manager — STM32H7 + W25Q SPI Flash
 *
 *  Перенос xz/settingsManager.cpp для STM32.
 *  Хранит все настройки устройства в одной структуре.
 *  Сохраняет/загружает данные во внешнюю SPI Flash (W25Q).
 *  Применяет настройки к глобальным переменным проекта.
 *
 *  Отличия от оригинала (xz):
 *    - EEPROM заменён на W25Q SPI Flash (посекторное хранение)
 *    - Serial заменён на UART/логирование проекта
 * ======================================================================== */

/* --- Версия схемы настроек (для миграций) --- */
#define SETTINGS_SCHEMA_VERSION   1U

/* --- Magic key (для валидации) --- */
#define SETTINGS_MAGIC            0x534E4554U  /* "SNET" */

/* --- Valid key (как в xz: EEPROM_VALID_KEY = 0x57) --- */
#define EEPROM_VALID_KEY          0x57U
#define WIFI_VALID_KEY            0x57U

/* --- Индексы скоростей RS485 --- */
#define RS485_BAUD_MIN            0U
#define RS485_BAUD_MAX            7U

/* --- Индексы полосы пропускания RX CC1101 --- */
#define CC1101_RXBW_MIN           0U
#define CC1101_RXBW_MAX           15U

/* --- Индексы модуляции CC1101 --- */
#define CC1101_MOD_MIN            0U
#define CC1101_MOD_MAX            1U

/* --- Индексы мощности CC1101 --- */
#define CC1101_PWR_MIN            0U
#define CC1101_PWR_MAX            7U

/* --- Диапазоны частот CC1101 (fixed-point x100) --- */
#define CC1101_FREQ_MIN           300U     /* 3.00 МГц */
#define CC1101_FREQ_MAX           92800U   /* 928.00 МГц */

/* --- Диапазоны битрейта CC1101 (fixed-point x100) --- */
#define CC1101_BITRATE_MIN        1U       /* 0.01 kbps */
#define CC1101_BITRATE_MAX        10000U   /* 100.00 kbps */

/* --- Подсветка --- */
#define BACKLIGHT_MIN             1U
#define BACKLIGHT_MAX             10U

/* --- sys, room, btn --- */
#define SYS_MIN                   1U
#define SYS_MAX                   32U
#define ROOM_MIN                  1U
#define ROOM_MAX                  32U
#define BTN_MIN                   1U
#define BTN_MAX                   9U

/* ========================================================================
 *  Глобальный флаг: что-то изменилось (как в xz/settingsManager.cpp)
 * ======================================================================== */
extern bool settingsDirty;

/* ========================================================================
 *  API
 * ======================================================================== */

/**
 *  @brief  Полное чтение настроек при старте.
 *          Если нет валидных данных — устанавливает и сохраняет значения по умолчанию.
 */
void loadAllSettings(void);

/**
 *  @brief  Полное сохранение всех настроек.
 */
void saveAllSettings(void);

/**
 *  @brief  Сохранить, если были изменения (проверяет settingsDirty).
 */
void savePendingSettings(void);

/**
 *  @brief  Пометить настройки как изменённые.
 */
void markSettingDirty(void);

/**
 *  @brief  Сбросить все настройки к значениям по умолчанию.
 */
void clearAllSettings(void);

/* --- Применение настроек к глобальным переменным проекта --- */
void SettingsManager_Apply(void);

/* --- Инициализация (загрузка настроек) --- */
bool SettingsManager_Init(void);

/* --- Получить текущую структуру настроек --- */
const Settings_t* SettingsManager_Get(void);
Settings_t* SettingsManager_GetMutable(void);

/* --- WiFi credentials --- */
bool hasSavedWiFiCredentials(void);
bool readWiFiCredentials(char* ssid, char* pass);
void saveWiFiCredentials(const char* ssid, const char* pass);
void clearWiFiCredentials(void);

/* --- Fixed-point helpers (как в xz) --- */
uint32_t floatToFixed(float f);
float    fixedToFloat(uint32_t x);

/* ========================================================================
 *  JSON Configuration API (опционально, требует json_config.h)
 * ======================================================================== */

/**
 * @brief Сохранить настройки в JSON файл на SD
 * @param filepath Путь к файлу (например, "/settings.json")
 * @return true если успешно
 */
bool SettingsManager_SaveToJson(const char* filepath);

/**
 * @brief Загрузить настройки из JSON файла на SD
 * @param filepath Путь к файлу
 * @return true если успешно
 */
bool SettingsManager_LoadFromJson(const char* filepath);

#endif /* SETTINGS_MANAGER_H */
