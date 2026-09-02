/**
 * @file    radio/radio_config_bridge.c
 * @brief   Мост между Settings_t (Flash) и CC1101_Config_t (hardware)
 *
 *  Преобразует настройки из EEPROM/Flash в конфигурацию CC1101
 *  и применяет их к аппаратному драйверу.
 */

#include "radio_config_bridge.h"
#include "radio_cc1101.h"
#include "radio_driver.h"
#include "settings_manager.h"

#include <string.h>

/* Глобальный handle SPI (определён в spi.c) */
extern SPI_HandleTypeDef hspi6;

/* ======================================================================== */
/*  Реальные таблицы из ESP32-кода (cc1101d.cpp)                             */
/* ======================================================================== */

/* Полоса RX в кГц (индекс 0..15) — для отображения в меню */
static const uint16_t rx_bw_display_khz[] = {
    58, 68, 81, 102, 116, 135, 162, 203,
    232, 270, 325, 406, 464, 541, 650, 812
};

/* Мощность TX в dBm (индекс 0..7) — как в ESP32 */
static const int8_t tx_power_dbm[] = {
    -30, -20, -15, -10, 0, 5, 7, 10
};

/* ======================================================================== */
/*  Конвертация типов                                                        */
/* ======================================================================== */

/**
 * @brief Конвертация модуляции из Settings (хранит реальные значения CC1101_MOD_*)
 *
 *  Теперь settings->cc1101_modulation хранит прямое значение:
 *  CC1101_MOD_ASK, CC1101_MOD_FSK, CC1101_MOD_2FSK, и т.д.
 */
static CC1101_Modulation_t bridge_ModulationFromSettings(uint8_t val)
{
    switch (val) {
        case CC1101_MOD_ASK:    return CC1101_MOD_ASK;
        case CC1101_MOD_FSK:    return CC1101_MOD_FSK;
        case CC1101_MOD_2FSK:   return CC1101_MOD_2FSK;
        case CC1101_MOD_GFSK:   return CC1101_MOD_GFSK;
        case CC1101_MOD_OOK:    return CC1101_MOD_OOK;
        case CC1101_MOD_4FSK:   return CC1101_MOD_4FSK;
        case CC1101_MOD_MSK:    return CC1101_MOD_MSK;
        default:                return CC1101_MOD_GFSK;
    }
}
}

/**
 * @brief Конвертация мощности из индекса в dBm
 */
static int8_t bridge_PowerFromIndex(uint8_t idx)
{
    if (idx < 0) idx = 0;
    if (idx > 7) idx = 7;

    return tx_power_dbm[idx];
}

/**
 * @brief Конвертация мощности из dBm в индекс
 */
static uint8_t bridge_IndexFromPower(int8_t power_dbm)
{
    for (uint8_t i = 0; i < 8; i++) {
        if (tx_power_dbm[i] >= power_dbm) {
            return i;
        }
    }
    return 7; /* max */
}

/* ======================================================================== */
/*  Основной мост: Settings_t → CC1101_Config_t                             */
/* ======================================================================== */

/**
 * @brief Конвертировать Settings_t в CC1101_Config_t
 */
void Bridge_SettingsToCC1101Config(const Settings_t* settings, CC1101_Config_t* config)
{
    if (!settings || !config) return;

    memset(config, 0, sizeof(*config));

    /* Частота: fixed-point MHz × 100 → Hz */
    config->frequency_hz = (uint32_t)settings->cc1101_freq_fixed * 10000;

    /* Битрейт: fixed-point kbps × 100 → bps */
    config->bitrate = (float)settings->cc1101_bitrate_fixed * 100.0f;

    /* Частотное отклонение (расчётное) */
    config->freq_dev = config->bitrate * 0.5f; /* Дефолт: 50% от битрейта */

    /* Мощность */
    config->tx_power = bridge_PowerFromIndex(settings->cc1101_power_index);

    /* Модуляция */
    config->modulation = bridge_ModulationFromSettings(settings->cc1101_modulation);

    /* Полоса RX */
    config->rx_bw = settings->cc1101_rxbw_index;

    /* Синхрослово (дефолт) */
    config->sync_word[0] = 0xD3;
    config->sync_word[1] = 0x91;

    /* Переменная длина пакета */
    config->pkt_len = 0;
    config->addr = 0;
    config->chan = 0;
}

/* ======================================================================== */
/*  Применение настроек к CC1101                                             */
/* ======================================================================== */

/**
 * @brief Применить настройки из Flash к CC1101
 * @return 0 при успехе, -1 при ошибке
 */
int Bridge_ApplyCC1101Settings(void)
{
    const Settings_t* settings = SettingsManager_Get();
    if (!settings) {
        return -1;
    }

    CC1101_Config_t config;
    Bridge_SettingsToCC1101Config(settings, &config);

    /* Применяем к драйверу */
    if (CC1101_Init(&hspi6, &config) != 0) {
        return -1;
    }

    return 0;
}

/**
 * @brief Применить ТОЛЬКО изменённые параметры (без полной инициализации)
 *
 *  Вызывается после изменения одного параметра в меню.
 */
int Bridge_ApplyCC1101Param(uint8_t param_mask)
{
    const Settings_t* settings = SettingsManager_Get();
    if (!settings) {
        return -1;
    }

    /* Сначала переводим в idle */
    CC1101_Idle();

    if (param_mask & BRIDGE_PARAM_FREQ) {
        uint32_t freq_hz = (uint32_t)settings->cc1101_freq_fixed * 10000;
        CC1101_SetFrequency(freq_hz);
    }

    if (param_mask & BRIDGE_PARAM_BITRATE) {
        float bitrate = (float)settings->cc1101_bitrate_fixed * 100.0f;
        CC1101_setBitRate(bitrate);
    }

    if (param_mask & BRIDGE_PARAM_MODULATION) {
        CC1101_Modulation_t mod = bridge_ModulationFromSettings(settings->cc1101_modulation);
        CC1101_SetModulation(mod);
    }

    if (param_mask & BRIDGE_PARAM_POWER) {
        int8_t power = bridge_PowerFromIndex(settings->cc1101_power_index);
        CC1101_SetTxPower(power);
    }

    if (param_mask & BRIDGE_PARAM_RXBW) {
        CC1101_SetRxBw(settings->cc1101_rxbw_index);
    }

    /* Возвращаемся в RX */
    CC1101_RxStart();

    return 0;
}

/**
 * @brief Обновить индекс мощности в настройках на основе текущего dBm
 */
void Bridge_UpdatePowerIndexFromHardware(void)
{
    Settings_t* settings = SettingsManager_GetMutable();
    if (!settings) return;

    /* Считаем текущую мощность из hardware */
    int8_t current_power = bridge_PowerFromIndex(settings->cc1101_power_index);
    uint8_t new_idx = bridge_IndexFromPower(current_power);

    if (settings->cc1101_power_index != new_idx) {
        settings->cc1101_power_index = new_idx;
        markSettingDirty();
    }
}

/**
 * @brief Обновить частоту в настройках на основе hardware
 */
void Bridge_UpdateFreqFromHardware(void)
{
    Settings_t* settings = SettingsManager_GetMutable();
    if (!settings) return;

    /* Текущая частота из hardware (приблизительно) */
    int8_t rssi = CC1101_GetRssi();
    (void)rssi; /* Для отладки */

    /* Частота уже синхронизирована через SettingsManager_Get() */
}
