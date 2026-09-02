/**
  ******************************************************************************
  * @file    radio/radio_cc1101.c
  * @brief   Драйвер CC1101 (чистый STM32 HAL, без Arduino)
  ******************************************************************************
  */

#include "stm32h7xx_hal.h"
#include "radio_cc1101.h"
#include "main.h"
#include <string.h>

/* ======================================================================== */
/*  Внутреннее состояние                                                     */
/* ======================================================================== */

/* Глобальный handle SPI (берём из spi.c) */
extern SPI_HandleTypeDef hspi6;

/* Внутреннее состояние */
static cc1101_state_t   cc1101_state = CC1101_STATE_IDLE;
static CC1101_Config_t  cc1101_config;
static int8_t           cc1101_rssi_cached = -128;
static uint8_t          cc1101_lqi_cached = 0;

/* Флаг: пакет принят (устанавливается в GDO0 EXTI) */
static volatile uint8_t cc1101_packet_received = 0;

/* ======================================================================== */
/*  Вспомогательные функции                                                  */
/* ======================================================================== */

/**
 * @brief Выбрать CC1101 (CS = LOW)
 */
static __INLINE void CC1101_Select(void)
{
    HAL_GPIO_WritePin(CC1101_CS_GPIO_Port, CC1101_CS_Pin, GPIO_PIN_RESET);
}

/**
 * @brief снять выборку (CS = HIGH)
 */
static __INLINE void CC1101_Deselect(void)
{
    HAL_GPIO_WritePin(CC1101_CS_GPIO_Port, CC1101_CS_Pin, GPIO_PIN_SET);
}

/**
 * @brief Обмен байтом по SPI (внутренний, с выборкой CS)
 */
static uint8_t CC1101_SpiTransfer(uint8_t data)
{
    uint8_t rx;
    HAL_SPI_TransmitReceive(&hspi6, &data, &rx, 1, HAL_MAX_DELAY);
    return rx;
}

/**
 * @brief Задержка (мкс) — простая, для инициализации
 */
static void CC1101_DelayUs(uint32_t us)
{
    /* D3PCLK1 = ~200 MHz на H7 => ~200 циклов/мкс */
    volatile uint32_t i;
    while (us--) {
        i = 200;
        while (i--) { }
    }
}

/* ======================================================================== */
/*  Strobe-команды                                                           */
/* ======================================================================== */

/**
 * @brief Отправить strobe-команду CC1101
 */
int CC1101_SendCommand(uint8_t cmd)
{
    int ret = 0;

    CC1101_Select();
    CC1101_SpiTransfer(cmd);
    CC1101_Deselect();

    /* После некоторых команд нужен минимум 2 мкс до первой операции SPI */
    if (cmd == CC1101_SPWD || cmd == CC1101_SWOR) {
        CC1101_DelayUs(5);
    }

    return ret;
}

/* ======================================================================== */
/*  Чтение/запись регистров                                                  */
/* ======================================================================== */

/**
 * @brief Чтение регистра CC1101
 * @param reg Адрес регистра (с флагом чтения 0x80 для burst)
 * @param data Указатель на буфер для чтения
 * @return 0 при успехе
 */
int CC1101_ReadReg(uint8_t reg, uint8_t* data)
{
    CC1101_Select();

    /* Чтение одиночного регистра */
    CC1101_SpiTransfer(reg & 0x7F);
    *data = CC1101_SpiTransfer(0x00);

    CC1101_Deselect();
    return 0;
}

/**
 * @brief Запись в регистр CC1101
 * @param reg Адрес регистра
 * @param data Данные для записи
 * @return 0 при успехе
 */
int CC1101_WriteReg(uint8_t reg, uint8_t data)
{
    CC1101_Select();

    CC1101_SpiTransfer(reg);
    CC1101_SpiTransfer(data);

    CC1101_Deselect();
    return 0;
}

/**
 * @brief Чтение многобайтового регистра (burst)
 * @param reg Адрес первого регистра (с флагом burst 0x40)
 * @param data Указатель на буфер
 * @param len Количество байт для чтения
 * @return 0 при успехе
 */
int CC1101_ReadMultiReg(uint8_t reg, uint8_t* data, uint8_t len)
{
    uint8_t i;

    CC1101_Select();

    /* Чтение burst */
    CC1101_SpiTransfer(reg & 0xBF); /* устанавливаем burst-флаг */
    for (i = 0; i < len; i++) {
        data[i] = CC1101_SpiTransfer(0x00);
    }

    CC1101_Deselect();
    return 0;
}

/**
 * @brief Запись многобайтового регистра (burst)
 * @param reg Адрес первого регистра (с флагом burst 0x40)
 * @param data Указатель на данные
 * @param len Длина данных
 * @return 0 при успехе
 */
int CC1101_WriteMultiReg(uint8_t reg, const uint8_t* data, uint8_t len)
{
    uint8_t i;

    CC1101_Select();

    /* Запись burst */
    CC1101_SpiTransfer(reg); /* устанавливаем burst-флаг */
    for (i = 0; i < len; i++) {
        CC1101_SpiTransfer(data[i]);
    }

    CC1101_Deselect();
    return 0;
}

/* ======================================================================== */
/*  FIFO операции                                                            */
/* ======================================================================== */

/**
 * @brief Запись в TX FIFO
 * @param data Указатель на данные
 * @param len Длина данных (макс 61 байт для burst)
 * @return 0 при успехе
 */
int CC1101_WriteTxFifo(const uint8_t* data, uint8_t len)
{
    uint8_t i;

    /* Для burst > 61 байт нужен цикл — но по умолчанию PKTLEN = 0 (variable) */
    CC1101_Select();
    CC1101_SpiTransfer(CC1101_TX_FIFO);

    for (i = 0; i < len; i++) {
        CC1101_SpiTransfer(data[i]);
    }

    CC1101_Deselect();
    return 0;
}

/**
 * @brief Чтение из RX FIFO
 * @param data Указатель на буфер
 * @param len Количество байт для чтения (макс 61 байт для burst)
 * @return 0 при успехе
 */
int CC1101_ReadRxFifo(uint8_t* data, uint8_t len)
{
    uint8_t i;

    CC1101_Select();
    CC1101_SpiTransfer(CC1101_RX_FIFO);

    for (i = 0; i < len; i++) {
        data[i] = CC1101_SpiTransfer(0x00);
    }

    CC1101_Deselect();
    return 0;
}

/* ======================================================================== */
/*  GDO0 interrupt                                                           */
/* ======================================================================== */

/**
 * @brief Вызывается из EXTI2 ISR при возникновении GDO0
 * @note Вызывает HAL_GPIO_EXTI_Callback или обрабатывает пакет
 */
void CC1101_Gdo0Interrupt(void)
{
    cc1101_packet_received = 1;
}

/* ======================================================================== */
/*  Базовые операции                                                         */
/* ======================================================================== */

/**
 * @brief Сброс радиомодуля к дефолтным значениям
 * @return 0 при успехе
 */
int CC1101_Reset(void)
{
    int ret = 0;

    /* Strobe SRES */
    CC1101_SendCommand(CC1101_SNOP);
    CC1101_SendCommand(CC1101_SNOP);
    CC1101_SendCommand(CC1101_SRES);
    CC1101_SendCommand(CC1101_SRES);

    /* Ждём сброс маркера (ожидание MARCSTATE != 0xFF) */
    uint8_t status;
    uint32_t tick = HAL_GetTick();
    do {
        CC1101_ReadReg(0x04, &status); /* IOCFG0 — но используем как status read */
        if (HAL_GetTick() - tick > 100) {
            ret = -1; /* таймаут */
            break;
        }
    } while (status == 0xFF);

    cc1101_state = CC1101_STATE_IDLE;
    return ret;
}

/**
 * @brief Перейти в idle режим
 * @return 0 при успехе
 */
int CC1101_Idle(void)
{
    int ret = 0;

    CC1101_SendCommand(CC1101_SIDLE);

    /* Ждём MARCSTATE == 0x11 (IDLE) */
    uint8_t marc;
    uint32_t tick = HAL_GetTick();
    do {
        CC1101_ReadReg(CC1101_STATUS_MARC, &marc);
        if (HAL_GetTick() - tick > 100) {
            ret = -1;
            break;
        }
    } while (marc != 0x11);

    cc1101_state = CC1101_STATE_IDLE;
    return ret;
}

/**
 * @brief Переключить в режим TX
 * @return 0 при успехе
 */
int CC1101_TxStart(void)
{
    CC1101_SendCommand(CC1101_STX);
    cc1101_state = CC1101_STATE_TX;
    return 0;
}

/**
 * @brief Переключить в режим RX
 * @return 0 при успехе
 */
int CC1101_RxStart(void)
{
    CC1101_SendCommand(CC1101_SRX);
    cc1101_state = CC1101_STATE_RX;
    return 0;
}

/* ======================================================================== */
/*  STEP 4: RadioLib-совместимое API                                         */
/* ======================================================================== */

/**
 * @brief Начальная инициализация (аналог RadioLib::begin)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_begin(void)
{
    CC1101_Config_t default_config = {
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

    if (CC1101_Init(&hspi6, &default_config) != 0) {
        return CC1101_ERROR;
    }

    return CC1101_OK;
}

/**
 * @brief Перевести в sleep режим (аналог RadioLib::sleep)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_sleep(void)
{
    CC1101_SendCommand(CC1101_SIDLE);
    CC1101_SendCommand(CC1101_SPWD);
    cc1101_state = CC1101_STATE_SLEEP;
    return CC1101_OK;
}

/**
 * @brief Перевести в idle режим (аналог RadioLib::idle)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_idle(void)
{
    if (CC1101_Idle() == 0) {
        return CC1101_OK;
    }
    return CC1101_ERR_STATE;
}

/**
 * @brief Переключить в RX режим (аналог RadioLib::rx)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_rx(void)
{
    CC1101_RxStart();
    return CC1101_OK;
}

/**
 * @brief Переключить в TX режим (аналог RadioLib::tx)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_tx(void)
{
    CC1101_TxStart();
    return CC1101_OK;
}

/**
 * @brief Передать пакет данных (аналог RadioLib::transmit/send)
 * @param data Указатель на данные
 * @param len Длина данных (макс 255)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_send(const uint8_t* data, uint8_t len)
{
    if (data == NULL || len == 0 || len > 255) {
        return CC1101_ERR_BUFFER;
    }

    if (CC1101_Transmit(data, len) == 0) {
        return CC1101_OK;
    }
    return CC1101_ERR_TX;
}

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
                   int8_t* rssi, uint8_t* lqi)
{
    if (data == NULL) {
        return -CC1101_ERR_BUFFER;
    }

    CC1101_Packet_t pkt;
    memset(&pkt, 0, sizeof(pkt));

    if (CC1101_Receive(&pkt, timeout_ms) != 0) {
        return -CC1101_ERR_TIMEOUT;
    }

    /* Копируем данные */
    uint8_t copy_len = (pkt.len < len) ? pkt.len : len;
    memcpy(data, pkt.data, copy_len);

    if (rssi != NULL) {
        *rssi = pkt.rssi;
    }
    if (lqi != NULL) {
        *lqi = pkt.lqi;
    }

    return copy_len;
}

/**
 * @brief Проверить, принят ли пакет (аналог RadioLib::packetReceived)
 * @return 1 если пакет принят, 0 если нет
 */
int CC1101_packetReceived(void)
{
    return cc1101_packet_received ? 1 : 0;
}

/**
 * @brief Получить количество принятых байт в RX FIFO
 * @return кол-во байт в RX FIFO (approx)
 */
uint8_t CC1101_bytesReceived(void)
{
    /* Считываем длину из первого байта RX FIFO */
    uint8_t len = 0;
    CC1101_Select();
    CC1101_SpiTransfer(CC1101_RX_FIFO);
    CC1101_SpiTransfer(0x00);
    CC1101_Deselect();
    return len;
}

/**
 * @brief Установить частоту (аналог RadioLib::setFrequency)
 * @param frequency_hz Частота в Гц
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setFrequency(uint32_t frequency_hz)
{
    if (CC1101_SetFrequency(frequency_hz) == 0) {
        return CC1101_OK;
    }
    return CC1101_ERROR;
}

/**
 * @brief Установить мощность TX (аналог RadioLib::setTxPower)
 * @param power dBm (-30 .. +10)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setTxPower(int8_t power)
{
    if (CC1101_SetTxPower(power) == 0) {
        return CC1101_OK;
    }
    return CC1101_ERROR;
}

/**
 * @brief Установить модуляцию (аналог RadioLib::setModulation)
 * @param mod Тип модуляции
 * @param rxBw_index Индекс полосы RX
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setModulation(CC1101_Modulation_t mod, uint8_t rxBw_index)
{
    CC1101_SetModulation(mod);
    CC1101_SetRxBw(rxBw_index);
    return CC1101_OK;
}

/**
 * @brief Установить битрейт (аналог RadioLib::setBitRate)
 * @param bitrate Битрейт в bps (1.2k .. 500k)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setBitRate(float bitrate)
{
    /* MDMCFG3 = mantissa, MDMCFG4 = exponent */
    /* Relating to MDMCFG4: symbol_rate = clk_freq / 256 / (X + E/256) */
    /* where X = MDMCFG4[7:4], E = MDMCFG3 */
    /* For 26 MHz clk: symbol_rate = 26000000 / 256 / (X + E/256) */

    float symbol_rate = bitrate; /* Assuming 1 bit per symbol for GFSK */
    float clk_freq = 26000000.0f;

    uint16_t temp = (uint16_t)((clk_freq / symbol_rate) * 256.0f);
    uint8_t mantissa = temp & 0xFF;
    uint8_t exponent = (temp >> 8) & 0xFF;

    /* MDMCFG3 = mantissa */
    CC1101_WriteReg(CC1101_MDMCFG3, mantissa);
    /* MDMCFG4 = exponent | (RX_BW << 4) */
    uint8_t mdmcfg4 = exponent;
    CC1101_WriteReg(CC1101_MDMCFG4, mdmcfg4);

    cc1101_config.bitrate = bitrate;
    return CC1101_OK;
}

/**
 * @brief Установить частотное отклонение (аналог RadioLib::setFrequencyDeviation)
 * @param freq_dev_hz Частотное отклонение в Гц
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setFrequencyDeviation(float freq_dev_hz)
{
    /* DEVIATN: MDMCFG1[7:5] = exponent, MDMCFG1[4:0] = mantissa */
    /* freq_dev = (26e6 / 2^19) * (M + E/32) */
    /* freq_dev = 49.8657 * (M + E/32) */

    float temp = freq_dev_hz / 49.8657f;
    uint8_t mantissa = (uint8_t)temp & 0x1F;
    uint8_t exponent = (uint8_t)(temp / 32.0f) & 0x07;

    uint8_t mdmcfg1 = (exponent << 5) | mantissa;
    CC1101_WriteReg(CC1101_DEVIATN, mdmcfg1);

    cc1101_config.freq_dev = freq_dev_hz;
    return CC1101_OK;
}

/**
 * @brief Установить полосу RX (аналог RadioLib::setRxBw)
 * @param bw_khz Полоса в кГц (индекс из таблицы CC1101)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setRxBw(uint8_t bw_index)
{
    if (CC1101_SetRxBw(bw_index) == 0) {
        return CC1101_OK;
    }
    return CC1101_ERROR;
}

/**
 * @brief Установить синхрослово (аналог RadioLib::setSyncWord)
 * @param sync_word Указатель на синхрослово
 * @param len Длина синхрослова (1 или 2 байта, 0 = отключено)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setSyncWord(const uint8_t* sync_word, uint8_t len)
{
    if (sync_word == NULL) {
        return CC1101_ERROR;
    }

    if (len == 0) {
        /* Отключаем синхрослово */
        CC1101_WriteReg(CC1101_SYNC1, 0x00);
        CC1101_WriteReg(CC1101_SYNC0, 0x00);
    } else if (len == 1) {
        CC1101_WriteReg(CC1101_SYNC1, sync_word[0]);
        CC1101_WriteReg(CC1101_SYNC0, sync_word[0]);
    } else {
        CC1101_WriteReg(CC1101_SYNC1, sync_word[0]);
        CC1101_WriteReg(CC1101_SYNC0, sync_word[1]);
    }

    return CC1101_OK;
}

/**
 * @brief Настроить фильтрацию адресов (аналог RadioLib::setAddressFiltering)
 * @param enable 1 = включить, 0 = отключить (broadcast)
 * @param my_addr Адрес устройства
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setAddressFiltering(uint8_t enable, uint8_t my_addr)
{
    uint8_t pktctrl0;
    CC1101_ReadReg(CC1101_PKTCTRL0, &pktctrl0);

    if (enable) {
        pktctrl0 |= (1 << 6); /* Address checking enabled */
        CC1101_WriteReg(CC1101_ADDR, my_addr);
    } else {
        pktctrl0 &= ~(1 << 6); /* Address checking disabled */
    }

    CC1101_WriteReg(CC1101_PKTCTRL0, pktctrl0);
    return CC1101_OK;
}

/**
 * @brief Настроить формат пакета (аналог RadioLib::setPacketFormat)
 * @param fixed_len 1 = фиксированная длина, 0 = переменная
 * @param pkt_len Длина пакета (если fixed_len != 0)
 * @param crc_enable 1 = включить CRC, 0 = отключить
 * @param whitening 1 = включить whitening, 0 = отключить
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_setPacketFormat(uint8_t fixed_len, uint8_t pkt_len,
                                        uint8_t crc_enable, uint8_t whitening)
{
    /* PKTCTRL1 */
    uint8_t pktctrl1;
    CC1101_ReadReg(CC1101_PKTCTRL1, &pktctrl1);

    if (fixed_len) {
        pktctrl1 &= ~(1 << 5); /* Fixed length */
        CC1101_WriteReg(CC1101_PKTLEN, pkt_len);
    } else {
        pktctrl1 |= (1 << 5); /* Variable length */
    }

    CC1101_WriteReg(CC1101_PKTCTRL1, pktctrl1);

    /* PKTCTRL0 */
    uint8_t pktctrl0;
    CC1101_ReadReg(CC1101_PKTCTRL0, &pktctrl0);

    if (!crc_enable) {
        pktctrl0 |= (1 << 1); /* Auto CRC off */
    } else {
        pktctrl0 &= ~(1 << 1); /* Auto CRC on */
    }

    CC1101_WriteReg(CC1101_PKTCTRL0, pktctrl0);
    return CC1101_OK;
}

/**
 * @brief Сбросить указатель FIFO (аналог RadioLib::resetFifoPtr)
 * @return cc1101_status_t
 */
cc1101_status_t CC1101_resetFifoPtr(void)
{
    CC1101_SendCommand(CC1101_SFRX);
    CC1101_SendCommand(CC1101_STX_FIFO);
    return CC1101_OK;
}

/**
 * @brief Получить RSSI
 * @return RSSI в dBm
 */
int8_t CC1101_GetRssi(void)
{
    uint8_t rssi_raw;
    CC1101_ReadReg(CC1101_STATUS_RSSI, &rssi_raw);
    /* RSSI = (int8_t)(rssi_raw / 2) - 148 */
    return (int8_t)(rssi_raw >> 1) - 148;
}

/**
 * @brief Получить RSSI с кэшированием
 */
int16_t CC1101_getRssiCached(uint8_t update)
{
    if (update) {
        uint8_t rssi_raw;
        CC1101_ReadReg(CC1101_STATUS_RSSI, &rssi_raw);
        cc1101_rssi_cached = (int8_t)(rssi_raw >> 1) - 148;
    }
    return (int16_t)cc1101_rssi_cached;
}

/**
 * @brief Получить LQI последнего пакета
 * @return LQI (0x80 bit = CRC OK)
 */
uint8_t CC1101_getLqi(void)
{
    if (cc1101_packet_received) {
        CC1101_ReadReg(CC1101_STATUS_LQI, &cc1101_lqi_cached);
        cc1101_packet_received = 0;
    }
    return cc1101_lqi_cached;
}

/**
 * @brief Измерить RSSI канала (carrier sense)
 * @return RSSI канала в dBm
 */
int16_t CC1101_channelRssi(void)
{
    int16_t rssi;

    /* Сохраняем текущее состояние */
    CC1101_SendCommand(CC1101_SIDLE);

    /* Входим в RX (для измерения carrier) */
    CC1101_SendCommand(CC1101_SRX);

    /* Ждём ~10 мс пока RX стабилизируется */
    CC1101_DelayUs(10000);

    rssi = (int16_t)CC1101_GetRssi();

    /* Восстанавливаем состояние */
    if (cc1101_state == CC1101_STATE_TX) {
        CC1101_TxStart();
    } else {
        CC1101_RxStart();
    }

    return rssi;
}

/* ======================================================================== */
/*  Получение handle                                                         */
/* ======================================================================== */

SPI_HandleTypeDef* CC1101_GetSpiHandle(void)
{
    return &hspi6;
}

/* ======================================================================== */
/*  STEP 2: Инициализация и конфигурация                                     */
/* ======================================================================== */
/*  END — STEP 1: базовые SPI-операции                                       */
/* ======================================================================== */

/* ======================================================================== */
/*  STEP 2: Инициализация и конфигурация                                     */
/* ======================================================================== */

/**
 * @brief Инициализация CC1101
 * @param hspi Указатель на SPI handle
 * @param config Указатель на конфигурацию
 * @return 0 при успехе, -1 при ошибке
 */
int CC1101_Init(SPI_HandleTypeDef* hspi, const CC1101_Config_t* config)
{
    if (hspi == NULL || config == NULL) {
        return -1;
    }

    /* Сохраняем конфигурацию */
    cc1101_config = *config;

    /* Инициализируем состояние */
    cc1101_state = CC1101_STATE_IDLE;
    cc1101_rssi_cached = -128;
    cc1101_lqi_cached = 0;
    cc1101_packet_received = 0;

    /* Сброс модуля */
    CC1101_Reset();

    /* Отключаем FIFO — для пакетного режима */
    CC1101_SendCommand(CC1101_SFRX);
    CC1101_SendCommand(CC1101_STX_FIFO);

    /* --- Настройка регистров по умолчанию --- */

    /* IOCFG2 — GDO2: выход при синхрослове */
    CC1101_WriteReg(CC1101_IOCFG2, 0x0B);

    /* IOCFG0 — GDO0: выход при совпадении адреса / конце пакета */
    CC1101_WriteReg(CC1101_IOCFG0, 0x06);

    /* FIFOTHR — TX FIFO threshold */
    CC1101_WriteReg(CC1101_FIFOTHR, 0x07);

    /* SYNC1/SYNC0 — синхрослово */
    if (config->sync_word[0] != 0 && config->sync_word[1] != 0) {
        CC1101_WriteReg(CC1101_SYNC1, config->sync_word[0]);
        CC1101_WriteReg(CC1101_SYNC0, config->sync_word[1]);
    } else {
        /* Дефолт: 0xD3 0x91 (2-sync-word mode) */
        CC1101_WriteReg(CC1101_SYNC1, 0xD3);
        CC1101_WriteReg(CC1101_SYNC0, 0x91);
    }

    /* PKTLEN — длина пакета (0 = переменная) */
    CC1101_WriteReg(CC1101_PKTLEN, config->pkt_len);

    /* PKTCTRL1 — формат пакета */
    uint8_t pktctrl1 = 0x00;
    if (config->pkt_len == 0) {
        pktctrl1 |= (1 << 5); /* Variable length */
    }
    CC1101_WriteReg(CC1101_PKTCTRL1, pktctrl1);

    /* PKTCTRL0 — автоматическое CRC / адрес */
    uint8_t pktctrl0 = 0x00;
    if (config->addr != 0) {
        pktctrl0 |= (1 << 6); /* Address checking enabled */
        CC1101_WriteReg(CC1101_ADDR, config->addr);
    }
    CC1101_WriteReg(CC1101_PKTCTRL0, pktctrl0);

    /* --- Модуляция --- */
    CC1101_SetModulation(config->modulation);
    CC1101_SetRxBw(config->rx_bw);

    /* --- Частота --- */
    CC1101_SetFrequency(config->frequency_hz);

    /* --- Мощность TX --- */
    CC1101_SetTxPower(config->tx_power);

    /* --- Канал --- */
    if (config->chan != 0) {
        CC1101_SetChannel(config->chan);
    }

    /* --- FSCTRL1 — настройка синтезатора частоты --- */
    /* PLL-Configuration — enhancement mode */
    CC1101_WriteReg(CC1101_FSCTRL1, 0x06);

    /* --- MDMCFG4/3/2/1/0 — modem configuration --- */
    /* Устанавливаются через SetBitRate/SetFrequencyDeviation */

    /* --- MCSM0 — Main Radio Control State Machine --- */
    /* Auto-calibrate on idle-to-rx / rx-to-tx transition */
    CC1101_WriteReg(CC1101_MCSM0, 0x18);

    /* --- FOCCFG — Frequency Offset Compensation --- */
    CC1101_WriteReg(CC1101_FOCCFG, 0x1C);

    /* --- BSCFG — Bit Synchronization --- */
    CC1101_WriteReg(CC1101_BSCFG, 0x1C);

    /* --- AGCCTRL2/1/0 — AGC Control --- */
    CC1101_WriteReg(CC1101_AGCCTRL2, 0xC7);
    CC1101_WriteReg(CC1101_AGCCTRL1, 0x00);
    CC1101_WriteReg(CC1101_AGCCTRL0, 0xB0);

    /* --- FREND1 — Front End RX Configuration --- */
    CC1101_WriteReg(CC1101_FREND1, 0xB6);

    /* --- FREND0 — Front End TX Configuration --- */
    CC1101_WriteReg(CC1101_FREND0, 0x10);

    /* --- FSCAL3/2/1/0 — Frequency Synthesizer Calibration --- */
    CC1101_WriteReg(CC1101_FSCAL3, 0xEA);
    CC1101_WriteReg(CC1101_FSCAL2, 0x2A);
    CC1101_WriteReg(CC1101_FSCAL1, 0x00);
    CC1101_WriteReg(CC1101_FSCAL0, 0x1F);

    /* --- TEST2 — Test Configuration --- */
    CC1101_WriteReg(CC1101_TEST2, 0x81);
    CC1101_WriteReg(CC1101_TEST1, 0x35);
    CC1101_WriteReg(CC1101_TEST0, 0x09);

    /* Переходим в IDLE */
    CC1101_Idle();

    return 0;
}

/**
 * @brief Деинициализация CC1101
 */
void CC1101_DeInit(void)
{
    /* Переходим в sleep */
    CC1101_SendCommand(CC1101_SIDLE);
    CC1101_SendCommand(CC1101_SPWD);

    cc1101_state = CC1101_STATE_SLEEP;
}

/**
 * @brief Установка конфигурации
 * @param config Указатель на конфигурацию
 * @return 0 при успехе
 */
int CC1101_SetConfig(const CC1101_Config_t* config)
{
    if (config == NULL) {
        return -1;
    }

    cc1101_config = *config;

    /* Применяем изменения */
    CC1101_SetFrequency(config->frequency_hz);
    CC1101_SetTxPower(config->tx_power);
    CC1101_SetModulation(config->modulation);
    CC1101_SetRxBw(config->rx_bw);

    return 0;
}

/**
 * @brief Установка частоты
 * @param frequency_hz Частота в Гц
 * @return 0 при успехе
 */
int CC1101_SetFrequency(uint32_t frequency_hz)
{
    CC1101_Idle();

    /* Расчёт по формуле из datasheet: FSCHZ = FREQ / (26 MHz / 2^19) */
    uint32_t fschz = (frequency_hz * (1 << 19)) / 26000000;

    CC1101_WriteReg(CC1101_FREQ2, (uint8_t)((fschz >> 16) & 0xFF));
    CC1101_WriteReg(CC1101_FREQ1, (uint8_t)((fschz >> 8) & 0xFF));
    CC1101_WriteReg(CC1101_FREQ0, (uint8_t)(fschz & 0xFF));

    /* Пересчёт частоты */
    cc1101_config.frequency_hz = frequency_hz;

    return 0;
}

/**
 * @brief Установка мощности TX
 * @param power dBm (-30 до +10)
 * @return 0 при успехе
 */
int CC1101_SetTxPower(int8_t power)
{
    CC1101_Idle();

    /* Для PA_LIST (PA0, PA1, PA2) — используем PATABLE */
    /* Для PA_BOOST — используем FREND0 */

    uint8_t pa = 0x00; /* PA0 + PA1 для 433 MHz */

    if (power >= 10) {
        pa = 0x00; /* +10 dBm */
    } else if (power >= 7) {
        pa = 0x01;
    } else if (power >= 5) {
        pa = 0x02;
    } else if (power >= 3) {
        pa = 0x03;
    } else if (power >= 0) {
        pa = 0x04;
    } else if (power >= -3) {
        pa = 0x05;
    } else if (power >= -6) {
        pa = 0x06;
    } else if (power >= -10) {
        pa = 0x07;
    } else if (power >= -15) {
        pa = 0x08;
    } else if (power >= -20) {
        pa = 0x09;
    } else if (power >= -25) {
        pa = 0x0A;
    } else {
        pa = 0x0B; /* -30 dBm */
    }

    /* Запись в PATABLE */
    CC1101_WriteReg(0x3E, pa); /* PATABLE address */

    cc1101_config.tx_power = power;
    return 0;
}

/**
 * @brief Установка полосы RX
 * @param bw_index Индекс (0-15)
 * @return 0 при успехе
 */
int CC1101_SetRxBw(uint8_t bw_index)
{
    CC1101_Idle();

    /* MDMCFG2[7:4] = RX_BW index (0-15)
     * Стандартные значения CC1101 (при SYMBOL_RATE=49.8657kbps):
     * 0=54k, 1=58k, 2=65k, 3=78k, 4=81k, 5=102k, 6=108k, 7=135k,
     * 8=162k, 9=203k, 10=205k, 11=270k, 12=325k, 13=406k, 14=410k, 15=541k
     *
     * ESP32-библиотека использует свою таблицу (58,68,81...),
     * но это фактические kHz при конкретном SYMBOL_RATE.
     * Здесь используем стандартные регистры CC1101.
     */
    uint8_t mdmcfg2;
    CC1101_ReadReg(CC1101_MDMCFG2, &mdmcfg2);
    mdmcfg2 = (mdmcfg2 & 0x0F) | ((bw_index & 0x0F) << 4);

    CC1101_WriteReg(CC1101_MDMCFG2, mdmcfg2);

    cc1101_config.rx_bw = bw_index;
    return 0;
}

/**
 * @brief Установка модуляции
 * @param mod Тип модуляции
 * @return 0 при успехе
 */
int CC1101_SetModulation(CC1101_Modulation_t mod)
{
    CC1101_Idle();

    /* MDMCFG2: bits [3:0] = modulation format */
    uint8_t mdmcfg2;
    CC1101_ReadReg(CC1101_MDMCFG2, &mdmcfg2);
    mdmcfg2 = (mdmcfg2 & 0xF0) | (mod & 0x0F);

    CC1101_WriteReg(CC1101_MDMCFG2, mdmcfg2);

    cc1101_config.modulation = mod;
    return 0;
}

/**
 * @brief Установка канала
 * @param chan Номер канала (0 = off)
 * @return 0 при успехе
 */
int CC1101_SetChannel(uint8_t chan)
{
    CC1101_Idle();
    CC1101_WriteReg(CC1101_CHANNR, chan);

    cc1101_config.chan = chan;
    return 0;
}

/**
 * @brief Получить текущее состояние радиомодуля
 * @return cc1101_state_t
 */
cc1101_state_t CC1101_getState(void)
{
    return cc1101_state;
}

/* ======================================================================== */
/*  STEP 3: TX/RX — передача и приём пакетов                                 */
/* ======================================================================== */

/**
 * @brief Сбросить FIFO RX
 * @return 0 при успехе
 */
int CC1101_ResetRxFifo(void)
{
    CC1101_SendCommand(CC1101_SFRX);
    return 0;
}

/**
 * @brief Сбросить FIFO TX
 * @return 0 при успехе
 */
int CC1101_ResetTxFifo(void)
{
    CC1101_SendCommand(CC1101_STX_FIFO);
    return 0;
}

/**
 * @brief Передача данных
 * @param data Указатель на данные
 * @param len Длина данных
 * @return 0 при успехе
 */
int CC1101_Transmit(const uint8_t* data, uint8_t len)
{
    if (data == NULL || len == 0) {
        return -1;
    }

    CC1101_Idle();

    /* Сбрасываем TX FIFO */
    CC1101_ResetTxFifo();

    /* Записываем данные в TX FIFO */
    /* Для burst > 61 байт нужно разбить на несколько операций */
    uint8_t remaining = len;
    uint8_t* p = (uint8_t*)data;

    CC1101_Select();
    CC1101_SpiTransfer(CC1101_TX_FIFO);

    while (remaining > 0) {
        uint8_t chunk = (remaining > 61) ? 61 : remaining;
        for (uint8_t i = 0; i < chunk; i++) {
            CC1101_SpiTransfer(*p++);
        }
        remaining -= chunk;
    }

    CC1101_Deselect();

    /* Переходим в TX */
    CC1101_TxStart();

    /* Ждём окончания передачи (GDO0 HIGH) */
    uint32_t tick = HAL_GetTick();
    while (!CC1101_IsGdo0High()) {
        if (HAL_GetTick() - tick > 1000) {
            /* Таймаут — выходим в idle */
            CC1101_Idle();
            return -1;
        }
    }

    /* Ждём окончания передачи (GDO0 LOW) */
    tick = HAL_GetTick();
    while (CC1101_IsGdo0High()) {
        if (HAL_GetTick() - tick > 1000) {
            CC1101_Idle();
            return -1;
        }
    }

    /* Возвращаемся в RX */
    CC1101_RxStart();

    return 0;
}

/**
 * @brief Приём данных (блокирующий)
 * @param packet Указатель на структуру пакета
 * @param timeout_ms Таймаут в мс
 * @return 0 при успехе, -1 при таймауте
 */
int CC1101_Receive(CC1101_Packet_t* packet, uint32_t timeout_ms)
{
    if (packet == NULL) {
        return -1;
    }

    memset(packet, 0, sizeof(CC1101_Packet_t));

    /* Запускаем RX */
    CC1101_RxStart();

    /* Ждём пакет (GDO0 HIGH = начало приёма) */
    uint32_t tick = HAL_GetTick();
    while (!CC1101_IsGdo0High()) {
        if (HAL_GetTick() - tick > timeout_ms) {
            return -1; /* таймаут */
        }
    }

    /* GDO0 HIGH = пакет принят (при настроенном IOCFG0 = 0x06) */
    /* Читаем данные из RX FIFO */

    /* Сначала читаем длину пакета (первый байт) */
    uint8_t pkt_len = 0;
    CC1101_ReadRxFifo(&pkt_len, 1);

    if (pkt_len > 255) {
        pkt_len = 255;
    }

    /* Читаем данные */
    CC1101_ReadRxFifo(packet->data, pkt_len);

    /* Читаем LQI и CRC */
    uint8_t lqi;
    CC1101_ReadReg(CC1101_STATUS_LQI, &lqi);

    packet->len = pkt_len;
    packet->lqi = lqi;
    packet->crc_ok = (lqi & 0x80) ? 1 : 0;

    /* Получаем RSSI */
    uint8_t rssi_raw;
    CC1101_ReadReg(CC1101_STATUS_RSSI, &rssi_raw);
    packet->rssi = (int8_t)(rssi_raw >> 1) - 148;

    /* Сбрасываем RX FIFO */
    CC1101_ResetRxFifo();

    /* Возвращаемся в RX */
    CC1101_RxStart();

    return 0;
}

/**
 * @brief Проверка GDO0 (пакет принят)
 * @return true если GDO0 HIGH
 */
bool CC1101_IsGdo0High(void)
{
    return (HAL_GPIO_ReadPin(CC1101_GDO0_GPIO_Port, CC1101_GDO0_Pin) == GPIO_PIN_SET);
}
