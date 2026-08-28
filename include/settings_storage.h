/**
 * ========================================================================
 *  Settings Storage — W25Q SPI Flash backend
 *  STM32H7 + W25Q32 (QSPI)
 *
 *  Персистентное хранение настроек во внешней SPI Flash.
 *  Адрес в Flash: SETTINGS_FLASH_ADDR (начало сектора 4КБ)
 * ========================================================================
 */

#ifndef SETTINGS_STORAGE_H
#define SETTINGS_STORAGE_H

#include <stdint.h>
#include <stdbool.h>

/* ----------------------------------------------------------------
 *  Адрес в W25Q Flash для хранения настроек (1 сектор = 4КБ)
 *  После шрифта (0x000000 + 14КБ) — безопасно с 16КБ выравн.
 * ---------------------------------------------------------------- */
#define SETTINGS_FLASH_ADDR         0x004000U
#define SETTINGS_FLASH_SIZE         0x1000U     /* 4КБ — один сектор */

/* ----------------------------------------------------------------
 *  Layout внутри сектора (4КБ):
 *  [0x0000] Settings data (~145 bytes)
 *  [0x0100] WiFi credentials area (256 bytes)
 *  [0x0200] Reserved / future use
 * ---------------------------------------------------------------- */
#define SETTINGS_DATA_OFFSET        0x0000U
#define SETTINGS_WIFI_OFFSET        0x0100U
#define SETTINGS_MAGIC_OFFSET       0x0000U     /* magic внутри данных */

/* --- Magic & version --- */
#define SETTINGS_STORAGE_MAGIC      0x53544F53U /* "STOS" */
#define SETTINGS_STORAGE_VERSION    1U

/* --- Valid key (как в xz: EEPROM_VALID_KEY = 0x57) --- */
#define SETTINGS_VALID_KEY          0x57U

/* --- API --- */

/**
 *  @brief  Инициализация storage (проверка W25Q).
 *  @retval true если OK
 */
bool SettingsStorage_Init(void);

/**
 *  @brief  Прочитать N байт настроек из Flash.
 *  @param  buf    — буфер для данных
 *  @param  len    — количество байт
 *  @retval true если успешно
 */
bool SettingsStorage_Read(void* buf, uint32_t len);

/**
 *  @brief  Записать N байт настроек во Flash.
 *          Предварительно стирает сектор.
 *  @param  buf    — данные для записи
 *  @param  len    — количество байт
 *  @retval true если успешно
 */
bool SettingsStorage_Write(const void* buf, uint32_t len);

/**
 *  @brief  Стереть сектор настроек (сброс к 0xFF).
 *  @retval true если успешно
 */
bool SettingsStorage_Erase(void);

/**
 *  @brief  Прочитать байт из произвольного смещения в секторе.
 */
uint8_t SettingsStorage_ReadByte(uint32_t offset);

/**
 *  @brief  Записать байт в произвольное смещение сектора.
 */
bool SettingsStorage_WriteByte(uint32_t offset, uint8_t data);

#endif /* SETTINGS_STORAGE_H */
