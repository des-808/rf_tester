/**
  ******************************************************************************
  * @file    src/radio/radio_driver.c
  * @brief   Унифицированный радио-драйвер (обёртка над CC1101/NRF24/SX1262)
  ******************************************************************************
  */

#include "radio_driver.h"
#include "radio_cc1101.h"
#include "spi.h"
#include "main.h"
#include <string.h>

/* Глобальный handle SPI (определён в spi.c) */
extern SPI_HandleTypeDef hspi6;

/* ======================================================================== */
/*  Внутреннее состояние                                                     */
/* ======================================================================== */

static RadioModule_t current_module = RADIO_MODULE_COUNT; /* Не выбран */

/* ======================================================================== */
/*  Конвертация типов                                                        */
/* ======================================================================== */

static CC1101_Modulation_t RadioModToCC1101(RadioModulation_t mod)
{
    switch (mod) {
        case RADIO_MOD_FSK:     return CC1101_MOD_FSK;
        case RADIO_MOD_GFSK:    return CC1101_MOD_GFSK;
        case RADIO_MOD_OOK:     return CC1101_MOD_OOK;
        case RADIO_MOD_ASK:     return CC1101_MOD_ASK;
        default:                return CC1101_MOD_GFSK;
    }
}

static RadioPacket_t CC1101ToRadioPacket(const CC1101_Packet_t* pkt)
{
    RadioPacket_t radio;
    memset(&radio, 0, sizeof(RadioPacket_t));
    memcpy(radio.data, pkt->data, pkt->len);
    radio.len = pkt->len;
    radio.rssi = pkt->rssi;
    radio.snr = 0; /* CC1101 не предоставляет SNR */
    radio.address = 0;
    return radio;
}

static CC1101_Config_t RadioTxToCC1101(const RadioTxConfig_t* config)
{
    CC1101_Config_t cc;
    memset(&cc, 0, sizeof(cc));
    cc.frequency_hz = config->frequency_hz;
    cc.bitrate = config->bitrate;
    cc.freq_dev = config->freq_dev;
    cc.tx_power = config->tx_power_dbm;
    cc.modulation = RadioModToCC1101(config->modulation);
    cc.rx_bw = config->cc1101_rx_bw;
    cc.sync_word[0] = 0xD3;
    cc.sync_word[1] = 0x91;
    cc.pkt_len = 0; /* variable length */
    cc.addr = 0;
    cc.chan = 0;
    return cc;
}

/* ======================================================================== */
/*  Базовые функции                                                          */
/* ======================================================================== */

/**
 * @brief Инициализация радио-драйвера и выбранного модуля
 */
RadioStatus_t Radio_Init(RadioModule_t module)
{
    if (module >= RADIO_MODULE_COUNT) {
        return RADIO_ERR_PARAM;
    }

    current_module = module;

    switch (module) {
        case RADIO_MODULE_CC1101: {
            CC1101_Config_t config = {
                .frequency_hz = 433000000,
                .bitrate = 9600.0f,
                .freq_dev = 47600.0f,
                .tx_power = 5,
                .rx_bw = 5,
                .modulation = CC1101_MOD_GFSK,
                .sync_word = {0xD3, 0x91},
                .pkt_len = 0,
                .addr = 0,
                .chan = 0
            };
            if (CC1101_Init(&hspi6, &config) != 0) {
                return RADIO_ERR_INIT;
            }
            break;
        }
        case RADIO_MODULE_NRF24L01:
        case RADIO_MODULE_SX1262:
            return RADIO_ERR_NOT_SUPPORTED;
        default:
            return RADIO_ERR_PARAM;
    }

    return RADIO_OK;
}

/**
 * @brief Переключение на другой модуль
 */
RadioStatus_t Radio_SwitchModule(RadioModule_t module)
{
    if (module >= RADIO_MODULE_COUNT) {
        return RADIO_ERR_PARAM;
    }

    /* Деинициализируем текущий */
    if (current_module != RADIO_MODULE_COUNT) {
        Radio_DeInit();
    }

    /* Инициализируем новый */
    return Radio_Init(module);
}

/**
 * @brief Получение текущего модуля
 */
RadioModule_t Radio_GetCurrentModule(void)
{
    return current_module;
}

/**
 * @brief Деинициализация радио
 */
void Radio_DeInit(void)
{
    switch (current_module) {
        case RADIO_MODULE_CC1101:
            CC1101_DeInit();
            break;
        default:
            break;
    }
    current_module = RADIO_MODULE_COUNT;
}

/* ======================================================================== */
/*  Настройки передатчика                                                    */
/* ======================================================================== */

/**
 * @brief Установка настроек передатчика
 */
RadioStatus_t Radio_SetTxConfig(const RadioTxConfig_t* config)
{
    if (config == NULL) {
        return RADIO_ERR_PARAM;
    }

    switch (current_module) {
        case RADIO_MODULE_CC1101: {
            CC1101_Config_t cc = RadioTxToCC1101(config);
            if (CC1101_SetConfig(&cc) != 0) {
                return RADIO_ERR_INIT;
            }
            break;
        }
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
    return RADIO_OK;
}

/**
 * @brief Установка мощности передатчика
 */
RadioStatus_t Radio_SetTxPower(int8_t power_dbm)
{
    switch (current_module) {
        case RADIO_MODULE_CC1101:
            return (CC1101_SetTxPower(power_dbm) == 0) ? RADIO_OK : RADIO_ERR_INIT;
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
}

/**
 * @brief Установка частоты
 */
RadioStatus_t Radio_SetFrequency(uint32_t frequency_hz)
{
    switch (current_module) {
        case RADIO_MODULE_CC1101:
            return (CC1101_SetFrequency(frequency_hz) == 0) ? RADIO_OK : RADIO_ERR_INIT;
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
}

/* ======================================================================== */
/*  Настройки приёмника                                                      */
/* ======================================================================== */

/**
 * @brief Установка настроек приёмника
 */
RadioStatus_t Radio_SetRxConfig(const RadioRxConfig_t* config)
{
    if (config == NULL) {
        return RADIO_ERR_PARAM;
    }

    switch (current_module) {
        case RADIO_MODULE_CC1101: {
            CC1101_Config_t cc;
            memset(&cc, 0, sizeof(cc));
            cc.frequency_hz = config->frequency_hz;
            cc.rx_bw = config->cc1101_rx_bw;
            if (CC1101_SetConfig(&cc) != 0) {
                return RADIO_ERR_INIT;
            }
            break;
        }
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
    return RADIO_OK;
}

/**
 * @brief Включение режима приёма
 */
RadioStatus_t Radio_StartReceive(bool continuous)
{
    switch (current_module) {
        case RADIO_MODULE_CC1101:
            CC1101_RxStart();
            break;
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
    return RADIO_OK;
}

/**
 * @brief Остановка приёма
 */
RadioStatus_t Radio_StopReceive(void)
{
    switch (current_module) {
        case RADIO_MODULE_CC1101:
            CC1101_Idle();
            break;
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
    return RADIO_OK;
}

/* ======================================================================== */
/*  Передача/приём                                                           */
/* ======================================================================== */

/**
 * @brief Передача пакета
 */
RadioStatus_t Radio_Transmit(const RadioPacket_t* packet)
{
    if (packet == NULL || packet->len == 0) {
        return RADIO_ERR_PARAM;
    }

    switch (current_module) {
        case RADIO_MODULE_CC1101: {
            if (CC1101_Transmit(packet->data, packet->len) == 0) {
                return RADIO_OK;
            }
            return RADIO_ERR_TX;
        }
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
}

/**
 * @brief Приём пакета (блокирующий, с таймаутом)
 */
RadioStatus_t Radio_Receive(RadioPacket_t* packet, uint32_t timeout_ms)
{
    if (packet == NULL) {
        return RADIO_ERR_PARAM;
    }

    switch (current_module) {
        case RADIO_MODULE_CC1101: {
            CC1101_Packet_t cc_pkt;
            memset(&cc_pkt, 0, sizeof(cc_pkt));

            if (CC1101_Receive(&cc_pkt, timeout_ms) != 0) {
                return RADIO_ERR_RX; /* таймаут */
            }

            *packet = CC1101ToRadioPacket(&cc_pkt);
            return RADIO_OK;
        }
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
}

/**
 * @brief Проверка наличия принятого пакета (неблокирующая)
 */
bool Radio_IsPacketReceived(void)
{
    switch (current_module) {
        case RADIO_MODULE_CC1101:
            return CC1101_IsGdo0High();
        default:
            return false;
    }
}

/**
 * @brief Чтение принятого пакета без блокировки
 */
RadioStatus_t Radio_ReadPacket(RadioPacket_t* packet)
{
    if (packet == NULL) {
        return RADIO_ERR_PARAM;
    }

    switch (current_module) {
        case RADIO_MODULE_CC1101: {
            if (!CC1101_IsGdo0High()) {
                return RADIO_ERR_RX; /* нет пакета */
            }

            CC1101_Packet_t cc_pkt;
            memset(&cc_pkt, 0, sizeof(cc_pkt));

            if (CC1101_Receive(&cc_pkt, 100) != 0) {
                return RADIO_ERR_RX;
            }

            *packet = CC1101ToRadioPacket(&cc_pkt);
            return RADIO_OK;
        }
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
}

/* ======================================================================== */
/*  Вспомогательные функции                                                  */
/* ======================================================================== */

/**
 * @brief Получение RSSI текущего сигнала
 */
int8_t Radio_GetRssi(void)
{
    switch (current_module) {
        case RADIO_MODULE_CC1101:
            return CC1101_GetRssi();
        default:
            return -128;
    }
}

/**
 * @brief Установка канала фильтрации
 */
RadioStatus_t Radio_SetChannel(uint8_t channel)
{
    switch (current_module) {
        case RADIO_MODULE_CC1101:
            return (CC1101_SetChannel(channel) == 0) ? RADIO_OK : RADIO_ERR_INIT;
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
}

/**
 * @brief Установка адреса
 */
RadioStatus_t Radio_SetAddress(const uint8_t* address, uint8_t length)
{
    if (address == NULL || length == 0 || length > 5) {
        return RADIO_ERR_PARAM;
    }

    switch (current_module) {
        case RADIO_MODULE_CC1101:
            /* CC1101 поддерживает только 1-байтовый адрес */
            if (length == 1) {
                return (CC1101_SetChannel(*address) == 0) ? RADIO_OK : RADIO_ERR_INIT;
            }
            return RADIO_ERR_PARAM;
        default:
            return RADIO_ERR_NOT_SUPPORTED;
    }
}

/**
 * @brief Проверка доступности модуля по SPI
 */
bool Radio_CheckModulePresence(RadioModule_t module)
{
    if (module >= RADIO_MODULE_COUNT) {
        return false;
    }

    switch (module) {
        case RADIO_MODULE_CC1101: {
            /* Пробуем прочитать статус-регистр */
            uint8_t val;
            CC1101_ReadReg(CC1101_STATUS_MARC, &val);
            /* Если получили не 0xFF — модуль отвечает */
            return (val != 0xFF);
        }
        default:
            return false;
    }
}
