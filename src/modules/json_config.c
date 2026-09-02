/**
 * ========================================================================
 *  JSON Configuration Module — Implementation
 *  Сериализация/десериализация настроек в JSON формат
 *  Использует cJSON для парсинга
 * ========================================================================
 */

#include "json_config.h"
#include "settings_types.h"
#include "cJSON.h"

#include <string.h>
#include <stdio.h>

/* ========================================================================
 *  Внутренние функции для работы с JSON
 * ======================================================================== */

/**
 * @brief Создать JSON объект с настройками по умолчанию
 */
static cJSON* create_default_settings_json(void) {
    cJSON* root = cJSON_CreateObject();
    if (!root) return NULL;
    
    /* Передатчик */
    cJSON_AddNumberToObject(root, "sys", 1);
    cJSON_AddNumberToObject(root, "room", 1);
    cJSON_AddNumberToObject(root, "btn", 1);
    
    /* Общие настройки */
    cJSON_AddNumberToObject(root, "rs485_baud_index", 4);
    cJSON_AddNumberToObject(root, "oled_brightness", 5);
    cJSON_AddBoolToObject(root, "bluetooth_enabled", 0);
    cJSON_AddBoolToObject(root, "wifi_enabled", 0);
    cJSON_AddBoolToObject(root, "ntp_sync_enabled", 0);
    cJSON_AddBoolToObject(root, "buzzer_enabled", 1);
    cJSON_AddBoolToObject(root, "vibro_enabled", 1);
    
    /* CC1101 */
    cJSON_AddNumberToObject(root, "cc1101_freq_fixed", 43396);
    cJSON_AddNumberToObject(root, "cc1101_bitrate_fixed", 960);
    cJSON_AddNumberToObject(root, "cc1101_rxbw_index", 11);
    cJSON_AddNumberToObject(root, "cc1101_modulation", 1);
    cJSON_AddNumberToObject(root, "cc1101_power_index", 3);
    
    /* WiFi credentials */
    cJSON_AddStringToObject(root, "wifi_ssid", "");
    cJSON_AddStringToObject(root, "wifi_pass", "");
    
    return root;
}

/**
 * @brief Получить числовое значение из JSON с дефолтом
 */
static int get_json_number(cJSON* obj, const char* key, int default_val) {
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item || !cJSON_IsNumber(item)) return default_val;
    return (int)item->valueint;
}

/**
 * @brief Получить булево значение из JSON с дефолтом
 */
static int get_json_bool(cJSON* obj, const char* key, int default_val) {
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item) return default_val;
    if (cJSON_IsBool(item)) return cJSON_IsTrue(item) ? 1 : 0;
    if (cJSON_IsNumber(item)) return (item->valueint != 0) ? 1 : 0;
    return default_val;
}

/**
 * @brief Получить строковое значение из JSON
 */
static const char* get_json_string(cJSON* obj, const char* key, const char* default_val) {
    cJSON* item = cJSON_GetObjectItem(obj, key);
    if (!item || !cJSON_IsString(item)) return default_val;
    return item->valuestring;
}

/* ========================================================================
 *  Публичные API — ЗАГЛУШКИ
 * ========================================================================
 *  Эти функции требуют конкретных структур Settings_t.
 *  Реализация будет в settings_manager.c после подключения json_config.
 *  
 *  Для компиляции сейчас — заглушки, возвращающие пустой JSON.
 */

bool SettingsToJson(const void* settings, char* json_buf, size_t buf_size) {
    const Settings_t* s = (const Settings_t*)settings;
    if (!s) return false;
    
    cJSON* root = cJSON_CreateObject();
    if (!root) return false;
    
    cJSON_AddNumberToObject(root, "sys", s->sys);
    cJSON_AddNumberToObject(root, "room", s->room);
    cJSON_AddNumberToObject(root, "btn", s->btn);
    cJSON_AddNumberToObject(root, "rs485_baud_index", s->rs485_baud_index);
    cJSON_AddNumberToObject(root, "oled_brightness", s->oled_brightness);
    cJSON_AddBoolToObject(root, "bluetooth_enabled", s->bluetooth_enabled);
    cJSON_AddBoolToObject(root, "wifi_enabled", s->wifi_enabled);
    cJSON_AddBoolToObject(root, "ntp_sync_enabled", s->ntp_sync_enabled);
    cJSON_AddBoolToObject(root, "buzzer_enabled", s->buzzer_enabled);
    cJSON_AddBoolToObject(root, "vibro_enabled", s->vibro_enabled);
    cJSON_AddNumberToObject(root, "cc1101_freq_fixed", s->cc1101_freq_fixed);
    cJSON_AddNumberToObject(root, "cc1101_bitrate_fixed", s->cc1101_bitrate_fixed);
    cJSON_AddNumberToObject(root, "cc1101_rxbw_index", s->cc1101_rxbw_index);
    cJSON_AddNumberToObject(root, "cc1101_modulation", s->cc1101_modulation);
    cJSON_AddNumberToObject(root, "cc1101_power_index", s->cc1101_power_index);
    cJSON_AddStringToObject(root, "wifi_ssid", s->wifi_ssid);
    cJSON_AddStringToObject(root, "wifi_pass", s->wifi_pass);
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json) return false;
    
    size_t len = strlen(json);
    if (len >= buf_size) {
        free(json);
        return false;
    }
    
    memcpy(json_buf, json, len + 1);
    free(json);
    return true;
}

bool JsonToSettings(const char* json, void* settings) {
    /* Парсим JSON */
    cJSON* root = cJSON_Parse(json);
    if (!root) return false;
    
    /* Получаем значения с дефолтами */
    Settings_t* s = (Settings_t*)settings;
    if (!s) { cJSON_Delete(root); return false; }
    
    s->sys            = get_json_number(root, "sys", 1);
    s->room           = get_json_number(root, "room", 1);
    s->btn            = get_json_number(root, "btn", 1);
    s->rs485_baud_index   = get_json_number(root, "rs485_baud_index", 4);
    s->oled_brightness    = get_json_number(root, "oled_brightness", 5);
    s->bluetooth_enabled  = get_json_bool(root, "bluetooth_enabled", 0);
    s->wifi_enabled       = get_json_bool(root, "wifi_enabled", 0);
    s->ntp_sync_enabled   = get_json_bool(root, "ntp_sync_enabled", 0);
    s->buzzer_enabled     = get_json_bool(root, "buzzer_enabled", 1);
    s->vibro_enabled      = get_json_bool(root, "vibro_enabled", 1);
    s->cc1101_freq_fixed    = get_json_number(root, "cc1101_freq_fixed", 43396);
    s->cc1101_bitrate_fixed = get_json_number(root, "cc1101_bitrate_fixed", 960);
    s->cc1101_rxbw_index    = get_json_number(root, "cc1101_rxbw_index", 11);
    s->cc1101_modulation    = get_json_number(root, "cc1101_modulation", 1);
    s->cc1101_power_index   = get_json_number(root, "cc1101_power_index", 3);
    
    /* WiFi credentials */
    const char* ssid  = get_json_string(root, "wifi_ssid", "");
    const char* pass  = get_json_string(root, "wifi_pass", "");
    strncpy(s->wifi_ssid, ssid, MAX_SSID_LEN);
    s->wifi_ssid[MAX_SSID_LEN] = '\0';
    strncpy(s->wifi_pass, pass, MAX_PASS_LEN);
    s->wifi_pass[MAX_PASS_LEN] = '\0';
    
    printf("JSON loaded: sys=%d room=%d btn=%d vibro=%d\n",
           s->sys, s->room, s->btn, s->vibro_enabled);
    
    cJSON_Delete(root);
    return true;
}

bool GetDefaultSettingsJson(char* json_buf, size_t buf_size) {
    cJSON* root = create_default_settings_json();
    if (!root) return false;
    
    char* json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    if (!json) return false;
    
    size_t len = strlen(json);
    if (len >= buf_size) {
        free(json);
        return false;
    }
    
    memcpy(json_buf, json, len + 1);
    free(json);
    return true;
}
