#ifndef __DS3231_H
#define __DS3231_H

#include "main.h"      // Для HAL, I2C_HandleTypeDef
#include <stdint.h>
#include <stdbool.h>

// Адрес DS3231 (7-битный)
#define DS3231_ADDRESS      (0x68 << 1)  // 0xD0 при записи, 0xD1 при чтении

// Регистры DS3231
#define DS3231_REG_SECONDS  0x00
#define DS3231_REG_MINUTES  0x01
#define DS3231_REG_HOURS    0x02
#define DS3231_REG_DAY      0x03
#define DS3231_REG_DATE     0x04
#define DS3231_REG_MONTH    0x05
#define DS3231_REG_YEAR     0x06
#define DS3231_REG_AL1_SECONDS  0x07
#define DS3231_REG_AL1_MINUTES  0x08
#define DS3231_REG_AL1_HOURS    0x09
#define DS3231_REG_AL1_DAY_DATE 0x0A
#define DS3231_REG_AL2_MINUTES  0x0B
#define DS3231_REG_AL2_HOURS    0x0C
#define DS3231_REG_AL2_DAY_DATE 0x0D
#define DS3231_REG_CONTROL    0x0E
#define DS3231_REG_STATUS     0x0F
#define DS3231_REG_AGING      0x10
#define DS3231_REG_TEMP       0x11  // Старший байт температуры (0x11) и младший (0x12)

// Биты в регистре CONTROL (0x0E)
#define DS3231_AEN1   (1U << 0)  //Alarm 1 enable
#define DS3231_AEN2   (1U << 1)  //Alarm 2 enable
#define DS3231_INTCN  (1U << 2)  //Interrupt control (0 = alarm interrupt output enabled, 1 = square wave output enabled)
#define DS3231_RS1    (1U << 3)  //Rate select 1
#define DS3231_RS0    (1U << 4)  //Rate select 0
#define DS3231_EOSC   (1U << 7)  //Enable Oscillator (0 = enabled, 1 = disabled)

// Биты в регистре STATUS (0x0F)
#define DS3231_OSF    (1U << 7)  //Oscillator Stop Flag
#define DS3231_BSY    (1U << 2)  //Busy
#define DS3231_A2F    (1U << 1)  //Alarm 2 Flag
#define DS3231_A1F    (1U << 0)  //Alarm 1 Flag

// Дни недели
typedef enum {
    DS3231_SUNDAY    = 1,
    DS3231_MONDAY    = 2,
    DS3231_TUESDAY   = 3,
    DS3231_WEDNESDAY = 4,
    DS3231_THURSDAY  = 5,
    DS3231_FRIDAY    = 6,
    DS3231_SATURDAY  = 7
} DS3231_Day_t;

// Тип данных для времени
typedef struct {
    uint8_t Second;   // 0–59
    uint8_t Minute;   // 0–59
    uint8_t Hour;     // 0–23 (24-hour) или 1–12 (12-hour)
    uint8_t AM_PM;    // 0 = AM, 1 = PM (только для 12-часового режима)
    uint8_t Day;      // 1–7 (см. DS3231_Day_t)
    uint8_t Date;     // 1–31
    uint8_t Month;    // 1–12
    uint8_t Year;     // 0–99 (последние 2 цифры года, напр. 25 = 2025)
} DS3231_Time_t;

// Настройки будильника
typedef struct {
    uint8_t Second;   // 0–59 или DONTCARE (0x80)
    uint8_t Minute;   // 0–59 или DONTCARE (0x80)
    uint8_t Hour;     // 0–23 или 1–12 или DONTCARE (0x80)
    uint8_t DayOrDate;// День недели (1–7) или число (1–31) или DONTCARE (0x80)
    uint8_t Mode;     // 0 = каждый сек, 1 = мин/сек, 2 = час/мин/сек, 3 = дата/час/мин/сек
} DS3231_Alarm_t;

// Ошибки
typedef enum {
    DS3231_OK      = 0,
    DS3231_ERROR   = -1,
    DS3231_TIMEOUT = -2
} DS3231_Status_t;

// Прототипы функций
DS3231_Status_t DS3231_Init(I2C_HandleTypeDef *hi2c);
DS3231_Status_t DS3231_SetTime(I2C_HandleTypeDef *hi2c, DS3231_Time_t *time);
DS3231_Status_t DS3231_GetTime(I2C_HandleTypeDef *hi2c, DS3231_Time_t *time);
DS3231_Status_t DS3231_SetAlarm(I2C_HandleTypeDef *hi2c, uint8_t alarm_id, DS3231_Alarm_t *alarm, bool enable);
DS3231_Status_t DS3231_GetAlarm(I2C_HandleTypeDef *hi2c, uint8_t alarm_id, DS3231_Alarm_t *alarm);
DS3231_Status_t DS3231_ClearAlarmFlag(I2C_HandleTypeDef *hi2c, uint8_t alarm_id);
bool DS3231_IsOscillatorStopped(I2C_HandleTypeDef *hi2c);
float DS3231_GetTemperature(I2C_HandleTypeDef *hi2c);
DS3231_Status_t DS3231_EnableSquareWave(I2C_HandleTypeDef *hi2c, uint8_t frequency);
DS3231_Status_t DS3231_DisableSquareWave(I2C_HandleTypeDef *hi2c);
DS3231_Status_t DS3231_Enable32kHzOutput(I2C_HandleTypeDef *hi2c, bool enable);

#endif // __DS3231_H