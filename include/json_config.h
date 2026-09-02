#ifndef __JSON_CONFIG_H__
#define __JSON_CONFIG_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief JSON Configuration Module
 * 
 * Сериализация/десериализация настроек в JSON формат.
 * Позволяет сохранять/загружать настройки в читаемом формате.
 * Использует cJSON для парсинга.
 * 
 * Формат JSON:
 * {
 *   "sys": 1,
 *   "room": 1,
 *   "btn": 1,
 *   "rs485_baud_index": 4,
 *   "oled_brightness": 5,
 *   "bluetooth_enabled": 0,
 *   "wifi_enabled": 0,
 *   "ntp_sync_enabled": 0,
 *   "buzzer_enabled": 1,
 *   "cc1101_freq_fixed": 43396,
 *   "cc1101_bitrate_fixed": 960,
 *   "cc1101_rxbw_index": 11,
 *   "cc1101_modulation": 1,
 *   "cc1101_power_index": 3,
 *   "wifi_ssid": "",
 *   "wifi_pass": ""
 * }
 */

/* Размер буфера для JSON (достаточно для всех настроек) */
#define JSON_CONFIG_BUF_SIZE  512

/**
 * @brief Сериализовать настройки в JSON строку
 * @param settings Указатель на структуру настроек
 * @param json_buf Буфер для JSON строки
 * @param buf_size Размер буфера
 * @return true если успешно, false если ошибка или буфер мал
 */
bool SettingsToJson(const void* settings, char* json_buf, size_t buf_size);

/**
 * @brief Десериализовать JSON строку в настройки
 * @param json JSON строка
 * @param settings Указатель на структуру настроек (будет заполнена)
 * @return true если успешно, false если ошибка парсинга
 */
bool JsonToSettings(const char* json, void* settings);

/**
 * @brief Получить JSON строку настроек по умолчанию
 * @param json_buf Буфер для JSON строки
 * @param buf_size Размер буфера
 * @return true если успешно
 */
bool GetDefaultSettingsJson(char* json_buf, size_t buf_size);

#endif /* __JSON_CONFIG_H__ */
