/**
  ******************************************************************************
  * @file    radio/radio_cc1101.h
  * @brief   Драйвер CC1101 (на основе RadioLib)
  * @note    Чистый STM32 HAL, без Arduino
  ******************************************************************************
  */

#ifndef RADIO_CC1101_H
#define RADIO_CC1101_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32h7xx_hal.h"
#include "main.h"

/* ======================================================================== */
/*  Конфигурация пинов CC1101                                               */
/*  (определены в main.h: CC1101_CS_Pin, CC1101_GDO0_Pin, CC1101_GDO2_Pin) */
/* ======================================================================== */

/* ======================================================================== */
/*  Регистры CC1101 (TI CC1101 Data Sheet)                                  */
/* ======================================================================== */

/* Статус-регистры */
#define CC1101_STATUS_RSSI    0x81  /* Чтение RSSI */
#define CC1101_STATUS_LQI     0x82  /* Чтение LQI */
#define CC1101_STATUS_MARC    0x83  /* Маршрульный статус */

/* Команды (strobe) */
#define CC1101_STX            0x30  /* Переключить в TX */
#define CC1101_SRX            0x34  /* Переключить в RX */
#define CC1101_SIDLE          0x38  /* Перейти в idle */
#define CC1101_SWOR           0x3C  /* Авто-пробуждение RX */
#define CC1101_SPWD           0x3E  /* Разбудить из powerdown */
#define CC1101_SFRX           0x40  /* Сбросить RX FIFO */
#define CC1101_STX_FIFO       0x44  /* Сбросить TX FIFO */
#define CC1101_SWORRST        0x50  /* Сброс реального времени */
#define CC1101_SRES           0x52  /* Soft reset */
#define CC1101_RX_FIFO        0x7F  /* Чтение RX FIFO (burst) */
#define CC1101_TX_FIFO        0x3F  /* Запись TX FIFO (burst) */
#define CC1101_SNOP           0x52  /* Нет операции */

/* Регистры конфигурации (write) */
#define CC1101_IOCFG2         0x00  /* GDO2 output pin config */
#define CC1101_IOCFG1         0x01  /* GDO1 output pin config */
#define CC1101_IOCFG0         0x02  /* GDO0 output pin config */
#define CC1101_FIFOTHR        0x03  /* RX/TX FIFO threshold */
#define CC1101_SYNC1          0x04  /* Sync word, high byte */
#define CC1101_SYNC0          0x05  /* Sync word, low byte */
#define CC1101_PKTLEN         0x06  /* Packet length */
#define CC1101_PKTCTRL1       0x07  /* Packet automation control */
#define CC1101_PKTCTRL0       0x08  /* Packet automation control */
#define CC1101_ADDR           0x09  /* Device address */
#define CC1101_CHANNR         0x0A  /* Channel number */
#define CC1101_FSCTRL1        0x0B  /* Frequency synthesizer control */
#define CC1101_FSCTRL0        0x0C  /* Frequency synthesizer control */
#define CC1101_FREQ2          0x0D  /* Frequency control word, high byte */
#define CC1101_FREQ1          0x0E  /* Frequency control word, middle byte */
#define CC1101_FREQ0          0x0F  /* Frequency control word, low byte */
#define CC1101_MDMCFG4        0x10  /* Modem configuration */
#define CC1101_MDMCFG3        0x11  /* Modem configuration */
#define CC1101_MDMCFG2        0x12  /* Modem configuration */
#define CC1101_MDMCFG1        0x13  /* Modem configuration */
#define CC1101_MDMCFG0        0x14  /* Modem configuration */
#define CC1101_DEVIATN        0x15  /* Modem deviation setting */
#define CC1101_MCSM2          0x16  /* Main Radio Control State Machine config */
#define CC1101_MCSM1          0x17  /* Main Radio Control State Machine config */
#define CC1101_MCSM0          0x18  /* Main Radio Control State Machine config */
#define CC1101_FOCCFG         0x19  /* Frequency Offset Compensation config */
#define CC1101_BSCFG          0x1A  /* Bit Synchronization configuration */
#define CC1101_AGCCTRL2       0x1B  /* AGC control */
#define CC1101_AGCCTRL1       0x1C  /* AGC control */
#define CC1101_AGCCTRL0       0x1D  /* AGC control */
#define CC1101_WOREVT1        0x1E  /* High byte Event 0 timeout */
#define CC1101_WOREVT0        0x1F  /* Low byte Event 0 timeout */
#define CC1101_WORCTRL        0x20  /* Wake On Radio control */
#define CC1101_FREND1         0x21  /* Front end RX configuration */
#define CC1101_FREND0         0x22  /* Front end TX configuration */
#define CC1101_FSCAL3         0x23  /* ADC trim */
#define CC1101_FSCAL2         0x24  /* LDO current trim */
#define CC1101_FSCAL1         0x25  /* Amp. current trim */
#define CC1101_FSCAL0         0x26  /* Amp. stage current trim */
#define CC1101_RCCTRL1        0x27  /* RC oscillator configuration */
#define CC1101_RCCTRL0        0x28  /* RC oscillator configuration */
#define CC1101_FSTEST         0x29  /* Frequency synthesizer cal. control */
#define CC1101_PTEST          0x2A  /* Production test */
#define CC1101_AGCTEST        0x2B  /* AGC test */
#define CC1101_TEST2          0x2C  /* Various test settings */
#define CC1101_TEST1          0x2D  /* Various test settings */
#define CC1101_TEST0          0x2E  /* Various test settings */

/* ======================================================================== */
/*  Типы модуляции CC1101                                                   */
/* ======================================================================== */

typedef enum {
    CC1101_MOD_ASK    = 0x00,
    CC1101_MOD_FSK    = 0x04,
    CC1101_MOD_2FSK   = 0x06,
    CC1101_MOD_GFSK   = 0x07,
    CC1101_MOD_OOK    = 0x08,
    CC1101_MOD_4FSK  = 0x0C,
    CC1101_MOD_MSK   = 0x0E
} CC1101_Modulation_t;

/* ======================================================================== */
/*  Структура конфигурации CC1101                                           */
/* ======================================================================== */

typedef struct {
    uint32_t frequency_hz;      /* Частота (300-348 МГц, 387-464 МГц, 779-928 МГц) */
    float bitrate;              /* Битрейт (1.2-500 kbps) */
    float freq_dev;             /* Частотное отклонение (Hz) */
    int8_t tx_power;            /* Мощность TX (-30 до +10 dBm) */
    uint8_t rx_bw;              /* Полоса RX (0-15, индекс из таблицы) */
    CC1101_Modulation_t modulation;
    uint8_t sync_word[2];       /* Синхрослово (0 = отключено) */
    uint8_t pkt_len;            /* Длина пакета (0 = переменная) */
    uint8_t addr;               /* Адрес устройства (0 = broadcast) */
    uint8_t chan;               /* Номер канала (0 = channel off) */
} CC1101_Config_t;

/* ======================================================================== */
/*  Структура принятого пакета                                              */
/* ======================================================================== */

typedef struct {
    uint8_t data[256];          /* Данные */
    uint8_t len;                /* Длина данных */
    int8_t rssi;                /* RSSI (dBm) */
    uint8_t lqi;                /* LQI (CRC ok = 0x80 bit) */
    uint8_t crc_ok;             /* 1 если CRC OK */
} CC1101_Packet_t;

/* ======================================================================== */
/*  API драйвера CC1101                                                     */
/* ======================================================================== */

/**
 * @brief Инициализация CC1101
 * @param hspi Указатель на SPI handle
 * @param config Указатель на конфигурацию
 * @return 0 при успехе, -1 при ошибке
 */
int CC1101_Init(SPI_HandleTypeDef* hspi, const CC1101_Config_t* config);

/**
 * @brief Деинициализация CC1101
 */
void CC1101_DeInit(void);

/**
 * @brief Получить handle SPI
 * @return Указатель на SPI handle
 */
SPI_HandleTypeDef* CC1101_GetSpiHandle(void);

/**
 * @brief Установка конфигурации
 * @param config Указатель на конфигурацию
 * @return 0 при успехе
 */
int CC1101_SetConfig(const CC1101_Config_t* config);

/**
 * @brief Установка частоты
 * @param frequency_hz Частота в Гц
 * @return 0 при успехе
 */
int CC1101_SetFrequency(uint32_t frequency_hz);

/**
 * @brief Установка мощности передатчика
 * @param power dBm (-30 до +10)
 * @return 0 при успехе
 */
int CC1101_SetTxPower(int8_t power);

/**
 * @brief Установка полосы пропускания RX
 * @param bw_index Индекс (0-15)
 * @return 0 при успехе
 */
int CC1101_SetRxBw(uint8_t bw_index);

/**
 * @brief Установка модуляции
 * @param mod Тип модуляции
 * @return 0 при успехе
 */
int CC1101_SetModulation(CC1101_Modulation_t mod);

/**
 * @brief Установка канала
 * @param chan Номер канала (0 = off)
 * @return 0 при успехе
 */
int CC1101_SetChannel(uint8_t chan);

/**
 * @brief Переключение в idle режим
 * @return 0 при успехе
 */
int CC1101_Idle(void);

/**
 * @brief Переключение в режим TX
 * @return 0 при успехе
 */
int CC1101_TxStart(void);

/**
 * @brief Переключение в режим RX
 * @return 0 при успехе
 */
int CC1101_RxStart(void);

/**
 * @brief Передача данных
 * @param data Указатель на данные
 * @param len Длина данных
 * @return 0 при успехе
 */
int CC1101_Transmit(const uint8_t* data, uint8_t len);

/**
 * @brief Приём данных (блокирующий)
 * @param packet Указатель на структуру пакета
 * @param timeout_ms Таймаут в мс
 * @return 0 при успехе, -1 при таймауте
 */
int CC1101_Receive(CC1101_Packet_t* packet, uint32_t timeout_ms);

/**
 * @brief Проверка GDO0 (пакет принят)
 * @return true если GDO0 HIGH
 */
bool CC1101_IsGdo0High(void);

/**
 * @brief Сбросить FIFO RX
 * @return 0 при успехе
 */
int CC1101_ResetRxFifo(void);

/**
 * @brief Сбросить FIFO TX
 * @return 0 при успехе
 */
int CC1101_ResetTxFifo(void);

/**
 * @brief Получить RSSI
 * @return RSSI в dBm
 */
int8_t CC1101_GetRssi(void);

/* ======================================================================== */
/*  RadioLib-совместимое API                                                */
/* ======================================================================== */

typedef enum {
    CC1101_OK = 0,
    CC1101_ERROR = -1,
    CC1101_ERR_TIMEOUT = -2,
    CC1101_ERR_STATE = -3,
    CC1101_ERR_CRC = -4,
    CC1101_ERR_BUFFER = -5,
    CC1101_ERR_TX = -6
} cc1101_status_t;

typedef enum {
    CC1101_STATE_IDLE = 0,
    CC1101_STATE_RX,
    CC1101_STATE_TX,
    CC1101_STATE_SLEEP,
    CC1101_STATE_CALIBRATE
} cc1101_state_t;

/**
 * @brief Начальная инициализация (аналог RadioLib::begin)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_begin(void);

/**
 * @brief Перевести в sleep режим (аналог RadioLib::sleep)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_sleep(void);

/**
 * @brief Перевести в idle режим (аналог RadioLib::idle)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_idle(void);

/**
 * @brief Переключить в RX режим (аналог RadioLib::rx)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_rx(void);

/**
 * @brief Переключить в TX режим (аналог RadioLib::tx)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_tx(void);

/**
 * @brief Передать пакет данных (аналог RadioLib::transmit/send)
 * @param data Указатель на данные
 * @param len Длина данных (макс 255)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_send(const uint8_t* data, uint8_t len);

/**
 * @brief Принять пакет данных (аналог RadioLib::receive/read)
 * @param data Буфер для данных
 * @param len Максимальная длина буфера
 * @param timeout_ms Таймаут в мс
 * @param rssi Указатель на RSSI (можно NULL)
 * @param lqi Указатель на LQI (можно NULL)
 * @return кол-во принятых байт, или отрицательное значение ошибки
 */
int CC1101_receive(uint8_t* data, uint8_t len, uint32_t timeout_ms,
                   int8_t* rssi, uint8_t* lqi);

/**
 * @brief Проверить, принят ли пакет (аналог RadioLib::packetReceived)
 * @return 1 если пакет принят, 0 если нет
 */
int CC1101_packetReceived(void);

/**
 * @brief Получить количество принятых байт в FIFO
 * @return кол-во байт в RX FIFO
 */
uint8_t CC1101_bytesReceived(void);

/**
 * @brief Установить частоту (аналог RadioLib::setFrequency)
 * @param frequency_hz Частота в Гц
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setFrequency(uint32_t frequency_hz);

/**
 * @brief Установить мощность TX (аналог RadioLib::setTxPower)
 * @param power dBm (-30 .. +10)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setTxPower(int8_t power);

/**
 * @brief Установить модуляцию (аналог RadioLib::setModulation)
 * @param mod Тип модуляции
 * @param rxBw_index Индекс полосы RX
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setModulation(CC1101_Modulation_t mod, uint8_t rxBw_index);

/**
 * @brief Установить битрейт (аналог RadioLib::setBitRate)
 * @param bitrate Битрейт в bps (1.2k .. 500k)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setBitRate(float bitrate);

/**
 * @brief Установить частотное отклонение (аналог RadioLib::setFrequencyDeviation)
 * @param freq_dev_hz Частотное отклонение в Гц
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setFrequencyDeviation(float freq_dev_hz);

/**
 * @brief Установить полосу RX (аналог RadioLib::setRxBw)
 * @param bw_khz Полоса в кГц (индекс из таблицы CC1101)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setRxBw(uint8_t bw_index);

/**
 * @brief Установить синхрослово (аналог RadioLib::setSyncWord)
 * @param sync_word Указатель на синхрослово
 * @param len Длина синхрослова (1 или 2 байта, 0 = отключено)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setSyncWord(const uint8_t* sync_word, uint8_t len);

/**
 * @brief Настроить фильтрацию адресов (аналог RadioLib::setAddressFiltering)
 * @param enable 1 = включить, 0 = отключить (broadcast)
 * @param my_addr Адрес устройства
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setAddressFiltering(uint8_t enable, uint8_t my_addr);

/**
 * @brief Настроить формат пакета (аналог RadioLib::setPacketFormat)
 * @param fixed_len 1 = фиксированная длина, 0 = переменная
 * @param pkt_len Длина пакета (если fixed_len != 0)
 * @param crc_enable 1 = включить CRC, 0 = отключить
 * @param whitening 1 = включить whitening, 0 = отключить
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setPacketFormat(uint8_t fixed_len, uint8_t pkt_len,
                                        uint8_t crc_enable, uint8_t whitening);

/**
 * @brief Получить текущее состояние радиомодуля
 * @return cc1101_state_t
 */
cc1101_state_t CC1101_getState(void);

/**
 * @brief Получить RSSI текущего сигнала / принятого пакета
 * @param update 1 = обновить кэш
 * @return RSSI в dBm
 */
int16_t CC1101_getRssiCached(uint8_t update);

/**
 * @brief Получить LQI последнего пакета
 * @return LQI (0x80 bit = CRC OK)
 */
uint8_t CC1101_getLqi(void);

/**
 * @brief Измерить RSSI канала (carrier sense)
 * @return RSSI канала в dBm
 */
int16_t CC1101_channelRssi(void);

/**
 * @brief Сбросить радиомодуль к дефолтным значениям (аналог RadioLib::reset)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_reset(void);

/**
 * @brief Сбросить указатель FIFO (аналог RadioLib::resetFifoPtr)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_resetFifoPtr(void);

/* ======================================================================== */
/*  Внутренние функции (SPI обмен)                                          */
/* ======================================================================== */

/**
 * @brief Запись в регистр CC1101
 * @param reg Адрес регистра
 * @param data Данные для записи
 * @return 0 при успехе
 */
int CC1101_WriteReg(uint8_t reg, uint8_t data);

/**
 * @brief Чтение регистра CC1101
 * @param reg Адрес регистра
 * @param data Указатель на буфер для чтения
 * @return 0 при успехе
 */
int CC1101_ReadReg(uint8_t reg, uint8_t* data);

/**
 * @brief Запись в TX FIFO
 * @param data Указатель на данные
 * @param len Длина данных
 * @return 0 при успехе
 */
int CC1101_WriteTxFifo(const uint8_t* data, uint8_t len);

/**
 * @brief Чтение из RX FIFO
 * @param data Указатель на буфер
 * @param len Количество байт для чтения
 * @return 0 при успехе
 */
int CC1101_ReadRxFifo(uint8_t* data, uint8_t len);

/**
 * @brief Запись многобайтового регистра
 * @param reg Адрес первого регистра (с флагом burst)
 * @param data Указатель на данные
 * @param len Длина данных
 * @return 0 при успехе
 */
int CC1101_WriteMultiReg(uint8_t reg, const uint8_t* data, uint8_t len);

/**
 * @brief Чтение многобайтового регистра
 * @param reg Адрес первого регистра (с флагом burst)
 * @param data Указатель на буфер
 * @param len Количество байт для чтения
 * @return 0 при успехе
 */
int CC1101_ReadMultiReg(uint8_t reg, uint8_t* data, uint8_t len);

/**
 * @brief Отправка команды CC1101
 * @param cmd Команда
 * @return 0 при успехе
 */
int CC1101_SendCommand(uint8_t cmd);

#endif /* RADIO_CC1101_H */