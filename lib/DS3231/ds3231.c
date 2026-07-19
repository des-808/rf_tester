#include "ds3231.h"

// Вспомогательные функции преобразования BCD ↔ DEC
static uint8_t DS3231_BCD2DEC(uint8_t val) {
    return ((val >> 4) * 10) + (val & 0x0F);
}

static uint8_t DS3231_DEC2BCD(uint8_t val) {
    return ((val / 10) << 4) | (val % 10);
}

// Базовая запись регистра
static DS3231_Status_t DS3231_WriteReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    if (HAL_I2C_Master_Transmit(hi2c, DS3231_ADDRESS, buf, 2, 100) != HAL_OK) {
        return DS3231_ERROR;
    }
    return DS3231_OK;
}

// Базовое чтение регистра
static DS3231_Status_t DS3231_ReadReg(I2C_HandleTypeDef *hi2c, uint8_t reg, uint8_t *data) {
    if (HAL_I2C_Master_Transmit(hi2c, DS3231_ADDRESS, &reg, 1, 100) != HAL_OK) {
        return DS3231_ERROR;
    }
    if (HAL_I2C_Master_Receive(hi2c, DS3231_ADDRESS | 1, data, 1, 100) != HAL_OK) {
        return DS3231_ERROR;
    }
    return DS3231_OK;
}

// Инициализация (включает осциллятор, если был остановлен)
DS3231_Status_t DS3231_Init(I2C_HandleTypeDef *hi2c) {
    uint8_t status;
    DS3231_Status_t ret;

    ret = DS3231_ReadReg(hi2c, DS3231_REG_STATUS, &status);
    if (ret != DS3231_OK) return ret;

    if (status & DS3231_OSF) {
        // Осциллятор остановился — сбросим флаг (но это не восстановит время!)
        status &= ~DS3231_OSF;
        ret = DS3231_WriteReg(hi2c, DS3231_REG_STATUS, status);
        if (ret != DS3231_OK) return ret;
    }

    // Включаем осциллятор (если был выключен)
    uint8_t control;
    ret = DS3231_ReadReg(hi2c, DS3231_REG_CONTROL, &control);
    if (ret != DS3231_OK) return ret;
    control &= ~DS3231_EOSC;  // clear EOSC
    return DS3231_WriteReg(hi2c, DS3231_REG_CONTROL, control);
}

// Установка времени (24-часовой формат)
DS3231_Status_t DS3231_SetTime(I2C_HandleTypeDef *hi2c, DS3231_Time_t *time) {
    uint8_t data[7];
    data[0] = DS3231_DEC2BCD(time->Second);
    data[1] = DS3231_DEC2BCD(time->Minute);
    data[2] = DS3231_DEC2BCD(time->Hour);  // Предполагаем 24-часовой формат
    data[3] = DS3231_DEC2BCD(time->Day);
    data[4] = DS3231_DEC2BCD(time->Date);
    data[5] = DS3231_DEC2BCD(time->Month);
    data[6] = DS3231_DEC2BCD(time->Year);

    uint8_t buf[8] = {DS3231_REG_SECONDS};
    for (int i = 0; i < 7; i++) buf[i + 1] = data[i];

    if (HAL_I2C_Master_Transmit(hi2c, DS3231_ADDRESS, buf, 8, 100) != HAL_OK) {
        return DS3231_ERROR;
    }
    return DS3231_OK;
}

// Получение времени (24-часовой формат)
DS3231_Status_t DS3231_GetTime(I2C_HandleTypeDef *hi2c, DS3231_Time_t *time) {
    uint8_t data[7];
    if (HAL_I2C_Master_Transmit(hi2c, DS3231_ADDRESS, (uint8_t[]){DS3231_REG_SECONDS}, 1, 100) != HAL_OK) {
        return DS3231_ERROR;
    }
    if (HAL_I2C_Master_Receive(hi2c, DS3231_ADDRESS | 1, data, 7, 100) != HAL_OK) {
        return DS3231_ERROR;
    }

    time->Second = DS3231_BCD2DEC(data[0] & 0x7F);
    time->Minute = DS3231_BCD2DEC(data[1] & 0x7F);
    time->Hour   = DS3231_BCD2DEC(data[2] & 0x3F);  // 24-часовой формат
    time->Day    = DS3231_BCD2DEC(data[3] & 0x07);
    time->Date   = DS3231_BCD2DEC(data[4] & 0x3F);
    time->Month  = DS3231_BCD2DEC(data[5] & 0x1F);
    time->Year   = DS3231_BCD2DEC(data[6]);

    return DS3231_OK;
}

// Установка будильника (alarm_id = 1 или 2)
DS3231_Status_t DS3231_SetAlarm(I2C_HandleTypeDef *hi2c, uint8_t alarm_id, DS3231_Alarm_t *alarm, bool enable) {
    uint8_t reg_start = (alarm_id == 1) ? DS3231_REG_AL1_SECONDS : DS3231_REG_AL2_MINUTES;
    uint8_t control_reg = (alarm_id == 1) ? DS3231_REG_CONTROL : DS3231_REG_CONTROL;
    uint8_t control_bit = (alarm_id == 1) ? DS3231_AEN1 : DS3231_AEN2;

    uint8_t data[4];
    data[0] = (alarm->Second == 0x80) ? 0x80 : DS3231_DEC2BCD(alarm->Second);
    data[1] = (alarm->Minute == 0x80) ? 0x80 : DS3231_DEC2BCD(alarm->Minute);
    data[2] = (alarm->Hour   == 0x80) ? 0x80 : DS3231_DEC2BCD(alarm->Hour);
    data[3] = (alarm->DayOrDate == 0x80) ? 0x80 : DS3231_DEC2BCD(alarm->DayOrDate);

    // Запись буфера: [reg_start, data0, data1, data2, data3]
    uint8_t buf[5];
    buf[0] = reg_start;
    for (int i = 0; i < 4; i++) buf[i + 1] = data[i];

    if (HAL_I2C_Master_Transmit(hi2c, DS3231_ADDRESS, buf, 5, 100) != HAL_OK) {
        return DS3231_ERROR;
    }

    // Управление разрешением будильника
    uint8_t control;
    if (DS3231_ReadReg(hi2c, control_reg, &control) != DS3231_OK) {
        return DS3231_ERROR;
    }
    if (enable) {
        control |= control_bit;
    } else {
        control &= ~control_bit;
    }
    return DS3231_WriteReg(hi2c, control_reg, control);
}

// Получение настроек будильника
DS3231_Status_t DS3231_GetAlarm(I2C_HandleTypeDef *hi2c, uint8_t alarm_id, DS3231_Alarm_t *alarm) {
    uint8_t reg_start = (alarm_id == 1) ? DS3231_REG_AL1_SECONDS : DS3231_REG_AL2_MINUTES;

    if (HAL_I2C_Master_Transmit(hi2c, DS3231_ADDRESS, (uint8_t[]){reg_start}, 1, 100) != HAL_OK) {
        return DS3231_ERROR;
    }

    uint8_t data[4];
    if (HAL_I2C_Master_Receive(hi2c, DS3231_ADDRESS | 1, data, 4, 100) != HAL_OK) {
        return DS3231_ERROR;
    }

    alarm->Second    = (data[0] & 0x80) ? 0x80 : DS3231_BCD2DEC(data[0] & 0x7F);
    alarm->Minute    = (data[1] & 0x80) ? 0x80 : DS3231_BCD2DEC(data[1] & 0x7F);
    alarm->Hour      = (data[2] & 0x80) ? 0x80 : DS3231_BCD2DEC(data[2] & 0x3F);
    alarm->DayOrDate = (data[3] & 0x80) ? 0x80 : DS3231_BCD2DEC(data[3] & 0x3F);

    return DS3231_OK;
}

// Сброс флага будильника (чтобы разрешить срабатывание повторно)
DS3231_Status_t DS3231_ClearAlarmFlag(I2C_HandleTypeDef *hi2c, uint8_t alarm_id) {
    uint8_t status;
    uint8_t flag_bit = (alarm_id == 1) ? DS3231_A1F : DS3231_A2F;

    DS3231_Status_t ret = DS3231_ReadReg(hi2c, DS3231_REG_STATUS, &status);
    if (ret != DS3231_OK) return ret;

    status &= ~flag_bit;  // сброс флага
    return DS3231_WriteReg(hi2c, DS3231_REG_STATUS, status);
}

// Проверка, остановился ли осциллятор (OSF в STATUS)
bool DS3231_IsOscillatorStopped(I2C_HandleTypeDef *hi2c) {
    uint8_t status;
    if (DS3231_ReadReg(hi2c, DS3231_REG_STATUS, &status) != DS3231_OK) {
        return false;
    }
    return (status & DS3231_OSF) != 0;
}

// Считывание температуры (старший байт — целая часть, младший — дробная в 2-х битах)
float DS3231_GetTemperature(I2C_HandleTypeDef *hi2c) {
    uint8_t msb, lsb;
    DS3231_Status_t ret;

    ret = DS3231_ReadReg(hi2c, DS3231_REG_TEMP, &msb);
    if (ret != DS3231_OK) return -1000.0f; // Ошибка

    ret = DS3231_ReadReg(hi2c, DS3231_REG_TEMP + 1, &lsb);
    if (ret != DS3231_OK) return -1000.0f;

    // Температура в формате signed integer + 2 бита дробной части
    // Значение — (msb << 2 | (lsb >> 6)) * 0.25, но проще:
    int16_t temp_raw = (int16_t)msb;
    if (msb & 0x80) {
        temp_raw |= 0xFF00; // sign extension для отрицательных
    }
    float temp = (float)temp_raw + ((lsb >> 6) * 0.25f);
    return temp;
}

// Включение квадратного генератора (частота: 1, 1024, 4096, 8192 Гц)
DS3231_Status_t DS3231_EnableSquareWave(I2C_HandleTypeDef *hi2c, uint8_t frequency) {
    uint8_t control;
    DS3231_Status_t ret = DS3231_ReadReg(hi2c, DS3231_REG_CONTROL, &control);
    if (ret != DS3231_OK) return ret;

    control &= ~(DS3231_RS1 | DS3231_RS0); // сброс RS[1:0]
    control &= ~DS3231_INTCN;              // 0 — выключаем прерывания, включаем SQW

    switch (frequency) {
        case 1:      control |= DS3231_RS1 | DS3231_RS0; break; // 1 Hz
        case 1024:   control |= DS3231_RS0;              break; // 1024 Hz
        case 4096:   control |= DS3231_RS1;              break; // 4096 Hz
        case 8192:   /* nothing */                        break; // 8192 Hz
        default:     return DS3231_ERROR;
    }

    return DS3231_WriteReg(hi2c, DS3231_REG_CONTROL, control);
}

// Отключение квадратного генератора (возврат к режиму прерываний)
DS3231_Status_t DS3231_DisableSquareWave(I2C_HandleTypeDef *hi2c) {
    uint8_t control;
    DS3231_Status_t ret = DS3231_ReadReg(hi2c, DS3231_REG_CONTROL, &control);
    if (ret != DS3231_OK) return ret;

    control |= DS3231_INTCN;   // 1 — включаем INT (Square Wave disabled)
    control &= ~(DS3231_RS1 | DS3231_RS0); // сброс RS[1:0]
    return DS3231_WriteReg(hi2c, DS3231_REG_CONTROL, control);
}

// Управление выходом 32.768 kHz (контролируется битом EOSC в Control)
DS3231_Status_t DS3231_Enable32kHzOutput(I2C_HandleTypeDef *hi2c, bool enable) {
    uint8_t control;
    DS3231_Status_t ret = DS3231_ReadReg(hi2c, DS3231_REG_CONTROL, &control);
    if (ret != DS3231_OK) return ret;

    if (enable) {
        control &= ~DS3231_EOSC;
    } else {
        control |= DS3231_EOSC; // disable oscillator — 32kHz тоже выключится!
    }
    return DS3231_WriteReg(hi2c, DS3231_REG_CONTROL, control);
}