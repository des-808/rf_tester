#include "ft6336u.h"

// Регистры FT6336U
#define FT6336U_TD_STATUS   0x02  // Количество касаний (bits 3..0) — в оригинале это 0x02!
#define FT6336U_P1_XH       0x03  // X H 11..8
#define FT6336U_P1_XL       0x04  // X L 7..0
#define FT6336U_P1_YH       0x05  // Y H 11..8
#define FT6336U_P1_YL       0x06  // Y L 7..0
#define FT6336U_P1_ID       0x07  // ID




void FT6336U_Init(FT6336U_HandleTypeDef *ts, I2C_HandleTypeDef *hi2c, uint8_t address) {
    ts->hi2c = hi2c;
    ts->address = address << 1;  // 0x38 << 1 = 0x70
    ts->last_irq_state = 1;     // по умолчанию — INT = 1 (нет касаний)
    ts->has_touch = false;
    for (int i = 0; i < 5; i++) {
        ts->x[i] = 0;
        ts->y[i] = 0;
        ts->touch_id[i] = 0;
    }
    ts->touch_num = 0;

    // Проверка ID (опционально)
    uint8_t id;
    if (HAL_I2C_Master_Receive(ts->hi2c, ts->address | 1, &id, 1, 100) == HAL_OK) {
        // прочитали ID (но FT6336U не поддерживает прямой read ID без регистра)
        // можно считать через Write-then-Read, но пока просто инициализация
    }
}

// Вспомогательная функция: читать 1 байт из регистра
uint8_t FT6336U_ReadReg(FT6336U_HandleTypeDef *ts, uint8_t reg) {
    uint8_t data;
    HAL_I2C_Master_Transmit(ts->hi2c, ts->address, &reg, 1, 100);
    HAL_I2C_Master_Receive(ts->hi2c, ts->address | 1, &data, 1, 100);
    return data;
}

uint8_t FT6336U_ReadReg(FT6336U_HandleTypeDef *ts, uint8_t reg);
bool FT6336U_ReadRegs(FT6336U_HandleTypeDef *ts, uint8_t reg, uint8_t *data, uint8_t len);
// Читать n байт из регистра (Write-then-Read)
bool FT6336U_ReadRegs(FT6336U_HandleTypeDef *ts, uint8_t reg, uint8_t *data, uint8_t len) {
    HAL_I2C_Master_Transmit(ts->hi2c, ts->address, &reg, 1, 100);
    return HAL_I2C_Master_Receive(ts->hi2c, ts->address | 1, data, len, 100) == HAL_OK;
}

bool FT6336U_ReadData(FT6336U_HandleTypeDef *ts) {
    // Читаем TD_STATUS (количество касаний)
    uint8_t touch_num_reg = FT6336U_ReadReg(ts, FT6336U_TD_STATUS);
    uint8_t touch_num = touch_num_reg & 0x0F;

    // ВАЛИДАЦИЯ: FT6336U поддерживает только 1 касание
    // Если touch_num > 1 — это мусор от I2C, игнорируем
    if (touch_num > 1) {
        ts->has_touch = false;
        ts->touch_num = 0;
        for (int i = 0; i < 5; i++) {
            ts->x[i] = 0;
            ts->y[i] = 0;
        }
        return false;
    }

    if (touch_num == 0) {
        ts->has_touch = false;
        ts->touch_num = 0;
        for (int i = 0; i < 5; i++) {
            ts->x[i] = 0;
            ts->y[i] = 0;
        }
        return true;
    }

    // Читаем координаты первой точки
    uint8_t data[5];
    uint8_t base = 0x03; // P1_XH = 0x03

    if (!FT6336U_ReadRegs(ts, base, data, 5)) {
        // Ошибка I2C — сбрасываем данные
        ts->has_touch = false;
        ts->touch_num = 0;
        return false;
    }

    uint16_t x = ((data[0] & 0x0F) << 8) | data[1];
    uint16_t y = ((data[2] & 0x0F) << 8) | data[3];
    uint8_t id = data[4] & 0x0F;

    // ВАЛИДАЦИЯ: координаты должны быть в пределах экрана
    // Для 320x480 экрана допустимые значения: 0-319 по X, 0-479 по Y
    if (x > 320 || y > 480) {
        // Неверные координаты — это мусор, игнорируем
        ts->has_touch = false;
        ts->touch_num = 0;
        return false;
    }

    ts->has_touch = true;
    ts->touch_num = 1;
    ts->x[0] = x;
    ts->y[0] = y;
    ts->touch_id[0] = id;

    // Очищаем остальные точки
    for (int i = 1; i < 5; i++) {
        ts->x[i] = 0;
        ts->y[i] = 0;
        ts->touch_id[i] = 0;
    }

    return true;
}


uint8_t FT6336U_GetTouchNum(FT6336U_HandleTypeDef *ts) {
    return ts->touch_num;
}

bool FT6336U_GetTouchPoint(FT6336U_HandleTypeDef *ts, uint8_t index, uint16_t *x, uint16_t *y) {
    if (index >= ts->touch_num) return false;
    *x = ts->x[index];
    *y = ts->y[index];
    return true;
}





uint8_t FT6336U_GetChipID(FT6336U_HandleTypeDef *ts) {
    return FT6336U_ReadReg(ts, FT6336U_DEV_ID);
}

uint8_t FT6336U_GetFirmwareVersion(FT6336U_HandleTypeDef *ts) {
    return FT6336U_ReadReg(ts, FT6336U_FIRM_ID);
}

uint8_t FT6336U_GetThreshold(FT6336U_HandleTypeDef *ts) {
    return FT6336U_ReadReg(ts, FT6336U_THRESHOLD);
}

void FT6336U_SetThreshold(FT6336U_HandleTypeDef *ts, uint8_t threshold) {
    // В даташите: регистр 0x80 (threshold)
    FT6336U_WriteReg(ts, FT6336U_THRESHOLD, threshold);
}

uint8_t FT6336U_GetMode(FT6336U_HandleTypeDef *ts) {
    return FT6336U_ReadReg(ts, FT6336U_MODE_REGISTER);
}

void FT6336U_SetMode(FT6336U_HandleTypeDef *ts, uint8_t mode) {
    FT6336U_WriteReg(ts, FT6336U_MODE_REGISTER, mode);
}

uint8_t FT6336U_GetGesture(FT6336U_HandleTypeDef *ts) {
    // Жест читается из регистра 0x01 (GEST_ID)
    return FT6336U_ReadReg(ts, FT6336U_GESTURE_MODE);
}

bool FT6336U_IsGestureAvailable(FT6336U_HandleTypeDef *ts) {
    uint8_t data = FT6336U_ReadReg(ts, FT6336U_GESTURE_MODE);
    return (data & 0x0F) != 0;
}

void FT6336U_EnableInterrupt(FT6336U_HandleTypeDef *ts) {
    // режим INT_ACTIVE_LOW или INT_PUSH_PULL — зависит от даташита
    uint8_t val = FT6336U_ReadReg(ts, FT6336U_INT_MODE);
    val &= ~(1 << 3); // сброс бита INT_MODE (если нужен)
    FT6336U_WriteReg(ts, FT6336U_INT_MODE, val);
}

void FT6336U_DisableInterrupt(FT6336U_HandleTypeDef *ts) {
    uint8_t val = FT6336U_ReadReg(ts, FT6336U_INT_MODE);
    val |= (1 << 3);
    FT6336U_WriteReg(ts, FT6336U_INT_MODE, val);
}

void FT6336U_ClearInterruptFlag(FT6336U_HandleTypeDef *ts) {
    // Один из способов: читать GEST_ID илиINT_MODE — зависит от чипа
    (void)FT6336U_ReadReg(ts, FT6336U_GESTURE_MODE);
}