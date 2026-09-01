#ifndef RADIO_DRIVER_H
#define RADIO_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

/* ======================================================================== */
/*  Конфигурация модулей (пины)                                             */
/* ======================================================================== */

/* --- Общий SPI для всех модулей --- */
/* SPI6: PA5=SCK, PA6=MISO, PA7=MOSI (определено в spi.c) */

/* --- CC1101 пины --- */
#define RADIO_CC1101_CS_Port    GPIOB
#define RADIO_CC1101_CS_Pin     GPIO_PIN_11
#define RADIO_CC1101_GDO0_Port  GPIOA
#define RADIO_CC1101_GDO0_Pin   GPIO_PIN_2

/* --- NRF24L01 пины --- */
#define RADIO_NRF24_CE_Port     GPIOC
#define RADIO_NRF24_CE_Pin      GPIO_PIN_2
#define RADIO_NRF24_IRQ_Port    GPIOC
#define RADIO_NRF24_IRQ_Pin     GPIO_PIN_3
#define RADIO_NRF24_CS_Port     GPIOE
#define RADIO_NRF24_CS_Pin      GPIO_PIN_15

/* --- SX1262 пины (абстрактные, заменить на реальные) --- */
#define RADIO_SX1262_CS_Port    GPIOB  /* TODO: заменить на реальный порт */
#define RADIO_SX1262_CS_Pin     GPIO_PIN_1  /* TODO: заменить на реальный пин */
#define RADIO_SX1262_DIO1_Port  GPIOB  /* TODO: заменить на реальный порт */
#define RADIO_SX1262_DIO1_Pin   GPIO_PIN_1  /* TODO: заменить на реальный пин */
#define RADIO_SX1262_RST_Port   GPIOB  /* TODO: заменить на реальный порт */
#define RADIO_SX1262_RST_Pin    GPIO_PIN_1  /* TODO: заменить на реальный пин */

/* ======================================================================== */
/*  Перечисления                                                            */
/* ======================================================================== */

typedef enum {
    RADIO_MODULE_CC1101 = 0,
    RADIO_MODULE_NRF24L01,
    RADIO_MODULE_SX1262,
    RADIO_MODULE_COUNT
} RadioModule_t;

typedef enum {
    RADIO_OK = 0,
    RADIO_ERR_INIT,
    RADIO_ERR_TX,
    RADIO_ERR_RX,
    RADIO_ERR_PARAM,
    RADIO_ERR_NOT_SUPPORTED
} RadioStatus_t;

typedef enum {
    RADIO_MOD_FSK = 0,
    RADIO_MOD_GFSK,
    RADIO_MOD_OOK,
    RADIO_MOD_ASK
} RadioModulation_t;

/* ======================================================================== */
/*  Структуры данных                                                         */
/* ======================================================================== */

/* Настройки передатчика */
typedef struct {
    uint32_t frequency_hz;      /* Частота в Гц */
    float bitrate;              /* Битрейт (bps) */
    float freq_dev;             /* Частотное отклонение (Hz), для FSK/GFSK */
    int8_t tx_power_dbm;        /* Мощность передатчика (dBm) */
    uint8_t pa_gain;            /* Выбор PA вывода */
    RadioModulation_t modulation;
    
    /* CC1101-specific */
    uint8_t cc1101_rx_bw;       /* Индекс полосы RX (0-15) */
    uint8_t cc1101_channrsp;    /* Channel spacing */
    
    /* NRF24L01-specific */
    uint8_t nrf24_data_rate;    /* 0=1Mbps, 1=2Mbps, 2=250kbps */
    uint8_t nrf24_payload_width;/* Ширина.payload (0-32) */
    
    /* SX1262-specific */
    uint8_t sx1262_bw;          /* Полоса (0-6: 7.8,10.4,15.6,20.8,31.2,41.7,62.5,125,250,500 kHz) */
    uint8_t sx1262_sf;          /* Spreading factor (6-12) */
    uint8_t sx1262_cr;          /* Coding rate (1-4: 4/5,4/6,4/7,4/8) */
    uint8_t sx1262_preamble_len;/* Длина преамбулы */
} RadioTxConfig_t;

/* Настройки приёмника */
typedef struct {
    uint32_t frequency_hz;
    float bitrate;
    
    /* CC1101-specific */
    uint8_t cc1101_rx_bw;
    
    /* NRF24L01-specific */
    uint8_t nrf24_data_rate;
    uint8_t nrf24_payload_width;
    
    /* SX1262-specific */
    uint8_t sx1262_bw;
    uint8_t sx1262_sf;
    uint8_t sx1262_cr;
} RadioRxConfig_t;

/* Структура пакета */
typedef struct {
    uint8_t data[256];          /* Данные (макс 256 байт) */
    uint8_t len;                /* Длина данных */
    int8_t rssi;                /* RSSI (дБм), -128 если не доступно */
    uint8_t snr;                /* SNR (0.1 dB), 0 если не доступно */
    uint8_t address;            /* Адрес (для NRF24L01) */
} RadioPacket_t;

/* ======================================================================== */
/*  API радио-драйвера                                                       */
/* ======================================================================== */

/* --- Базовые функции --- */

/**
 * @brief Инициализация радио-драйвера и выбранного модуля
 * @param module Выбранный модуль
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_Init(RadioModule_t module);

/**
 * @brief Переключение на другой модуль
 * @param module Новый модуль
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_SwitchModule(RadioModule_t module);

/**
 * @brief Получение текущего модуля
 * @return Текущий модуль
 */
RadioModule_t Radio_GetCurrentModule(void);

/**
 * @brief Деинициализация радио
 */
void Radio_DeInit(void);

/* --- Настройки передатчика --- */

/**
 * @brief Установка настроек передатчика
 * @param config Указатель на конфигурацию
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_SetTxConfig(const RadioTxConfig_t* config);

/**
 * @brief Установка мощности передатчика
 * @param power_dbm Мощность в dBm
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_SetTxPower(int8_t power_dbm);

/**
 * @brief Установка частоты
 * @param frequency_hz Частота в Гц
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_SetFrequency(uint32_t frequency_hz);

/* --- Настройки приёмника --- */

/**
 * @brief Установка настроек приёмника
 * @param config Указатель на конфигурацию
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_SetRxConfig(const RadioRxConfig_t* config);

/**
 * @brief Включение режима приёма
 * @param continuous true = непрерывный приём, false = одиночный пакет
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_StartReceive(bool continuous);

/**
 * @brief Остановка приёма
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_StopReceive(void);

/* --- Передача/приём --- */

/**
 * @brief Передача пакета
 * @param packet Указатель на пакет
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_Transmit(const RadioPacket_t* packet);

/**
 * @brief Приём пакета (блокирующий, с таймаутом)
 * @param packet Указатель на пакет для приёма
 * @param timeout_ms Таймаут в мс (0 = без таймаута)
 * @return RADIO_OK при успехе, RADIO_ERR_RX при таймауте
 */
RadioStatus_t Radio_Receive(RadioPacket_t* packet, uint32_t timeout_ms);

/**
 * @brief Проверка наличия принятого пакета (неблокирующая)
 * @return true если пакет доступен
 */
bool Radio_IsPacketReceived(void);

/**
 * @brief Чтение принятого пакета без блокировки
 * @param packet Указатель на пакет для приёма
 * @return RADIO_OK при успехе, RADIO_ERR_RX если нет пакета
 */
RadioStatus_t Radio_ReadPacket(RadioPacket_t* packet);

/* --- Вспомогательные функции --- */

/**
 * @brief Получение RSSI текущего сигнала
 * @return RSSI в dBm, -128 если не доступно
 */
int8_t Radio_GetRssi(void);

/**
 * @brief Установка канала фильтрации (для NRF24L01)
 * @param channel Канал (0-127)
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_SetChannel(uint8_t channel);

/**
 * @brief Установка адреса (для NRF24L01)
 * @param address Адрес (1-5 байт)
 * @param length Длина адреса
 * @return RADIO_OK при успехе
 */
RadioStatus_t Radio_SetAddress(const uint8_t* address, uint8_t length);

/**
 * @brief Проверка доступности модуля по SPI
 * @param module Модуль для проверки
 * @return true если модуль отвечает
 */
bool Radio_CheckModulePresence(RadioModule_t module);

#endif /* RADIO_DRIVER_H */
