#ifndef SETTINGS_TYPES_H
#define SETTINGS_TYPES_H

#include <stdint.h>

/* ========================================================================
 *  Settings Types — общие типы для настроек
 *  Вынесен в отдельный файл чтобы разорвать circular dependency
 * ======================================================================== */

#define MAX_SSID_LEN              32
#define MAX_PASS_LEN              64

typedef struct {
    /* --- Магия и версия --- */
    uint32_t magic;
    uint32_t version;

    /* --- Передатчик --- */
    uint8_t  sys;
    uint8_t  room;
    uint8_t  btn;

    /* --- Общие настройки --- */
    uint8_t  rs485_baud_index;
    uint8_t  oled_brightness;
    uint8_t  bluetooth_enabled;
    uint8_t  wifi_enabled;
    uint8_t  ntp_sync_enabled;
    uint8_t  buzzer_enabled;
    uint8_t  vibro_enabled;

    /* --- CC1101 --- */
    uint32_t cc1101_freq_fixed;
    uint32_t cc1101_bitrate_fixed;
    uint8_t  cc1101_modulation;
    uint8_t  cc1101_power_index;
    uint8_t  cc1101_rxbw_index;

    /* --- WiFi credentials --- */
    uint8_t  wifi_valid;
    char     wifi_ssid[MAX_SSID_LEN + 1];
    char     wifi_pass[MAX_PASS_LEN + 1];

} Settings_t;

#endif /* SETTINGS_TYPES_H */
