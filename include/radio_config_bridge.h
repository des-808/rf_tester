/**
 * @file    radio/radio_config_bridge.h
 * @brief   Мост между Settings_t (Flash) и CC1101_Config_t (hardware)
 */

#ifndef RADIO_CONFIG_BRIDGE_H
#define RADIO_CONFIG_BRIDGE_H

#include <stdint.h>
#include "settings_manager.h"
#include "radio_cc1101.h"

/* Маски параметров для частичного применения */
#define BRIDGE_PARAM_FREQ       (1 << 0)
#define BRIDGE_PARAM_BITRATE    (1 << 1)
#define BRIDGE_PARAM_MODULATION (1 << 2)
#define BRIDGE_PARAM_POWER      (1 << 3)
#define BRIDGE_PARAM_RXBW       (1 << 4)

/* ======================================================================== */
/*  API моста                                                                */
/* ======================================================================== */

/**
 * @brief Конвертировать Settings_t в CC1101_Config_t
 */
void Bridge_SettingsToCC1101Config(const Settings_t* settings, CC1101_Config_t* config);

/**
 * @brief Применить ВСЕ настройки из Flash к CC1101
 * @return 0 при успехе, -1 при ошибке
 */
int Bridge_ApplyCC1101Settings(void);

/**
 * @brief Применить изменённые параметры (без полной инициализации)
 * @param param_mask Маска BRIDGE_PARAM_*
 * @return 0 при успехе
 */
int Bridge_ApplyCC1101Param(uint8_t param_mask);

/**
 * @brief Обновить индекс мощности в настройках
 */
void Bridge_UpdatePowerIndexFromHardware(void);

/**
 * @brief Обновить частоту в настройках
 */
void Bridge_UpdateFreqFromHardware(void);

#endif /* RADIO_CONFIG_BRIDGE_H */



