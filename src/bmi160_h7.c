#include "bmi160_h7.h"
#include "stm32h7xx_hal.h"

// Константы для перевода сырых данных в физ. величины 
// (По умолчанию: Акселерометр = ±2g, Гироскоп = ±2000 dps)
#define ACCEL_SCALE_2G    (2.0f / 32768.0f)
#define GYRO_SCALE_2000   (2000.0f / 32768.0f)
#define I2C_MEMSIZE_8BIT   0x00000001U

// Внутренние функции для работы с шиной I2C
static HAL_StatusTypeDef BMI160_WriteReg(I2C_HandleTypeDef *hi2c, uint16_t devAddress, uint8_t reg, uint8_t value) {
    return HAL_I2C_Mem_Write(hi2c, devAddress, reg, I2C_MEMSIZE_8BIT, &value, 1, HAL_MAX_DELAY);
}

static HAL_StatusTypeDef BMI160_ReadRegs(I2C_HandleTypeDef *hi2c, uint16_t devAddress, uint8_t reg, uint8_t *data, uint16_t len) {
    return HAL_I2C_Mem_Read(hi2c, devAddress, reg, I2C_MEMSIZE_8BIT, data, len, HAL_MAX_DELAY);
}



/**
 * @brief Настройка аппаратного прерывания Any-Motion на пин INT1 датчика
 */
uint8_t BMI160_SetupHardwareInterrupt(I2C_HandleTypeDef *hi2c, uint16_t devAddress) {
    // 1. Включаем прерывание Any-Motion для осей X, Y, Z (Регистр INT_EN[0])
    // Бит 0, 1, 2 — активация осей. Бит 2:0 = 111b (0x07)
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_INT_EN_0, 0x07) != HAL_OK) return 0;

    // 2. Настраиваем физический пин INT1 самого датчика (Регистр INT_OUT_CTRL)
    // Бит 3 = 1 (включить выход INT1), Бит 2 = 0 (активный уровень - LOW, либо 1 - HIGH)
    // Бит 1 = 1 (режим Push-Pull). Запишем 0x0A (активный HIGH, push-pull)
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_INT_OUT_CTRL, 0x0A) != HAL_OK) return 0;

    // 3. Направляем (маппим) прерывание Any-Motion именно на пин INT1 (Регистр INT_MAP[0])
    // Бит 2 отвечает за Any-Motion -> INT1. Записываем 0x04
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_INT_MAP_0, 0x04) != HAL_OK) return 0;

    // 4. Настраиваем параметры фильтра движения
    // INT_MOTION[0]: Задает количество последовательных выборок. Оставим 0 (сложение из 1 выборки)
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_INT_MOTION_0, 0x00) != HAL_OK) return 0;
    
    // INT_MOTION[1]: Порог чувствительности (Threshold). 
    // 1 условый попугай = 3.91 mg (при диапазоне измерителя ±2g). 
    // Запишем значение 20 (20 * 3.91мг = ~78мг). Это уберет ложные срабатывания от мелкого тремора рук.
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_INT_MOTION_1, 20) != HAL_OK) return 0;

    return 1;
}

// Инициализация датчика
uint8_t BMI160_Init(I2C_HandleTypeDef *hi2c, uint16_t devAddress) {
    uint8_t chip_id = 0;
    
    // 1. Проверка CHIP_ID (Должен быть 0xD1)
    if (BMI160_ReadRegs(hi2c, devAddress, BMI160_REG_CHIP_ID, &chip_id, 1) != HAL_OK) {
        return 0; // Ошибка шины
    }
    if (chip_id != 0xD1) {
        return 0; // Неверный ID устройства
    }
    
    // 2. Мягкий программный сброс
    BMI160_WriteReg(hi2c, devAddress, BMI160_REG_COMMAND, BMI160_CMD_SOFT_RESET);
    HAL_Delay(50); // Пауза на перезагрузку чипа
    
    // 3. Включение акселерометра в Normal Mode
    BMI160_WriteReg(hi2c, devAddress, BMI160_REG_COMMAND, BMI160_CMD_ACCEL_NORMAL);
    HAL_Delay(20);
    
    // 4. Включение гироскопа в Normal Mode
    BMI160_WriteReg(hi2c, devAddress, BMI160_REG_COMMAND, BMI160_CMD_GYRO_NORMAL);
    HAL_Delay(60); // Гироскопу требуется больше времени на стабилизацию

    BMI160_SetupHardwareInterrupt(hi2c, BMI160_I2C_ADDR_VCC);
    return 1; // Успешно инициализировано
}


// Опрос всех 6 осей за одну транзакцию
uint8_t BMI160_ReadData(I2C_HandleTypeDef *hi2c, uint16_t devAddress, BMI160_Data_t *data) {
    uint8_t raw_buffer[12]; // 6 осей по 2 байта каждая
    
    // Читаем пакет данных начиная с регистра данных гироскопа (0x0C по 0x17)
    if (BMI160_ReadRegs(hi2c, devAddress, BMI160_REG_DATA_GYRO_X, raw_buffer, 12) != HAL_OK) {
        return 0; // Ошибка чтения
    }
    
    // Сборка сырых 16-битных знаковых данных (int16_t)
    int16_t raw_gx = (int16_t)((raw_buffer[1] << 8) | raw_buffer[0]);
    int16_t raw_gy = (int16_t)((raw_buffer[3] << 8) | raw_buffer[2]);
    int16_t raw_gz = (int16_t)((raw_buffer[5] << 8) | raw_buffer[4]);
    
    int16_t raw_ax = (int16_t)((raw_buffer[7] << 8) | raw_buffer[6]);
    int16_t raw_ay = (int16_t)((raw_buffer[9] << 8) | raw_buffer[8]);
    int16_t raw_az = (int16_t)((raw_buffer[11] << 8) | raw_buffer[10]);
    
    // Перевод в физические величины
    data->gx = (float)raw_gx * GYRO_SCALE_2000;
    data->gy = (float)raw_gy * GYRO_SCALE_2000;
    data->gz = (float)raw_gz * GYRO_SCALE_2000;
    
    data->ax = (float)raw_ax * ACCEL_SCALE_2G;
    data->ay = (float)raw_ay * ACCEL_SCALE_2G;
    data->az = (float)raw_az * ACCEL_SCALE_2G;
    
    return 1;
}

extern uint8_t bmi160_irq_received;
void BMI160_IRQHandler(uint16_t GPIO_Pin)
{
  // Проверяем, что прерывание пришло именно от ножки датчика BMI160
  if (GPIO_Pin == BMI160_INT_Pin) 
  {
    // Внутри ISR только взводим флаг. Никакого тяжелого кода и I2C!
    bmi160_irq_received = 1; 
  }
}

uint8_t BMI160_CheckOrientationTask(I2C_HandleTypeDef *hi2c, uint16_t devAddress, uint8_t current_orientation) {
    BMI160_Data_t sensor_data;

    // Считываем актуальные данные осей
    if (BMI160_ReadData(hi2c, devAddress, &sensor_data)) {
        
        // Проверяем наклон по оси X (переход в Портретный режим)
        if (fabsf(sensor_data.ax) > BMI160_THRESHOLD_45_DEG) {
            if (current_orientation != 2) {
                return 2; // Требуется портретная ориентация
            }
        } 
        // Проверяем наклон по оси Y (возврат в Альбомный режим)
        else if (fabsf(sensor_data.ay) > BMI160_THRESHOLD_45_DEG) {
            if (current_orientation != 1) {
                return 1; // Требуется альбомная ориентация
            }
        }
    }
    
    return 0; // Изменений нет
}


uint8_t BMI160_InitLowPowerAnyMotion(I2C_HandleTypeDef *hi2c, uint16_t devAddress) {
    uint8_t chip_id = 0;
    
    // 1. Проверяем CHIP_ID
    if (BMI160_ReadRegs(hi2c, devAddress, BMI160_REG_CHIP_ID, &chip_id, 1) != HAL_OK) return 0;
    if (chip_id != 0xD1) return 0;
    
    // 2. Мягкий программный сброс в исходное состояние
    BMI160_WriteReg(hi2c, devAddress, BMI160_REG_COMMAND, BMI160_CMD_SOFT_RESET);
    HAL_Delay(50);
    
    // 3. Отключаем гироскоп (переводим в Suspend режим для экономии 900 мкА)
    BMI160_WriteReg(hi2c, devAddress, BMI160_REG_COMMAND, BMI160_CMD_GYRO_SUSPEND);
    HAL_Delay(5);
    
    // 4. Настраиваем параметры энергосбережения акселерометра (Регистр ACC_CONF)
    // Включаем бит ACC_US (under-sampling), фильтр усреднения и частоту 25 Гц
    uint8_t acc_conf = BMI160_ACC_US | BMI160_ACC_BWP_AVG4 | BMI160_ACC_ODR_25HZ;
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_ACC_CONF, acc_conf) != HAL_OK) return 0;
    HAL_Delay(5);

    // 5. Включаем акселерометр в режим Low-Power
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_COMMAND, BMI160_CMD_ACCEL_LPW) != HAL_OK) return 0;
    HAL_Delay(20); // Время на стабилизацию питания режима LP

    // 6. Включаем аппаратный Any-Motion детектор для осей X, Y, Z (Регистр INT_EN_0)
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_INT_EN_0, 0x07) != HAL_OK) return 0;

    // 7. Настраиваем физический пин INT1 датчика (Активный HIGH, режим Push-Pull)
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_INT_OUT_CTRL, 0x0A) != HAL_OK) return 0;

    // 8. Направляем (маппим) прерывание Any-Motion на пин INT1
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_INT_MAP_0, 0x04) != HAL_OK) return 0;

    // 9. Настройка фильтра длительности движения (0 - срабатывание по первой выборке)
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_INT_MOTION_0, 0x00) != HAL_OK) return 0;
    
    // 10. Порог чувствительности движения (Threshold)
    // Поставим значение 25 (~97 mg), чтобы датчик просыпался только от уверенного движения руками
    if (BMI160_WriteReg(hi2c, devAddress, BMI160_REG_INT_MOTION_1, 25) != HAL_OK) return 0;

    return 1; // Успешно настроено в Low-Power режиме
}


