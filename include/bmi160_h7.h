#ifndef BMI160_H7_H
#define BMI160_H7_H

#include "stm32h7xx_hal.h"
#include "main.h"
#include <math.h> // Обязательно для fabsf()

// При наклоне > 45 градусов проекция силы тяжести на ось X или Y превышает g * sin(45°) ≈ 0.707g
#define BMI160_THRESHOLD_45_DEG  0.707f 

// Адреса I2C (HAL требует сдвинутый влево адрес: 0x69 << 1 = 0xD2)
#define BMI160_I2C_ADDR_GND      (0x68 << 1)
#define BMI160_I2C_ADDR_VCC      (0x69 << 1)

// Основные регистры BMI160
#define BMI160_REG_CHIP_ID       0x00
#define BMI160_REG_DATA_GYRO_X   0x0C
#define BMI160_REG_DATA_ACCEL_X  0x12
#define BMI160_REG_COMMAND       0x7E

// Команды управления питанием
#define BMI160_CMD_ACCEL_NORMAL  0x11
#define BMI160_CMD_GYRO_NORMAL   0x15
#define BMI160_CMD_SOFT_RESET    0xB6

// Дополнительные регистры прерываний BMI160
#define BMI160_REG_INT_EN_0      0x50
#define BMI160_REG_INT_OUT_CTRL  0x53
#define BMI160_REG_INT_MAP_0     0x55
#define BMI160_REG_INT_MOTION_0  0x5F // Регистр длительности (Duration)
#define BMI160_REG_INT_MOTION_1  0x60 // Регистр порога (Threshold)

#define BMI160_REG_ACC_CONF      0x40 // Конфигурация частоты и фильтров акселерометра

// Команда перевода акселерометра в Low-Power режим
#define BMI160_CMD_ACCEL_LPW     0x12 
// Команда полного отключения гироскопа (Suspend)
#define BMI160_CMD_GYRO_SUSPEND  0x14 

// Битовые маски для регистра ACC_CONF (0x40)
#define BMI160_ACC_US            (1 << 7) // Включить андерсемплинг для Low-Power
#define BMI160_ACC_BWP_AVG4      (0 << 4) // Аппаратное усреднение по 4 выборкам (снижает шумы)
#define BMI160_ACC_ODR_25HZ      0x06     // Частота опроса 25 Гц (этого достаточно для детекции поворота)



// Структура для хранения обработанных данных датчика
typedef struct {
    float ax, ay, az; // Ускорение в g
    float gx, gy, gz; // Угловая скорость в dps (градусы в секунду)
} BMI160_Data_t;

// Прототипы функций
uint8_t BMI160_Init(I2C_HandleTypeDef *hi2c, uint16_t devAddress);
uint8_t BMI160_SetupHardwareInterrupt(I2C_HandleTypeDef *hi2c, uint16_t devAddress);
uint8_t BMI160_ReadData(I2C_HandleTypeDef *hi2c, uint16_t devAddress, BMI160_Data_t *data);

// Прототип функции для периодической проверки ориентации в main loop
uint8_t BMI160_CheckOrientationTask(I2C_HandleTypeDef *hi2c, uint16_t devAddress, uint8_t current_orientation);
void BMI160_IRQHandler(uint16_t GPIO_Pin);

// Прототип новой функции
uint8_t BMI160_InitLowPowerAnyMotion(I2C_HandleTypeDef *hi2c, uint16_t devAddress);


#endif // BMI160_H7_H