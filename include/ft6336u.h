#ifndef FT6336U_H
#define FT6336U_H

#include "main.h"
#include <stdint.h>
#include <stdbool.h>

#define FT6336U_ADDR    (0x38 << 1)  // 7-bit address 0x38 → 8-bit 0x70

// Регистры (из даташита и исходника)
#define FT6336U_DEV_ID      0xA8
#define FT6336U_VENDOR_ID   0xA3
#define FT6336U_GLIB_VERSION 0xAF
#define FT6336U_FIRM_ID       0xA6
#define FT6336U_POINT_MODE    0x02
#define FT6336U_TOUCH_NUM     0x02
#define FT6336U_TOUCH_XH      0x03
#define FT6336U_TOUCH_XL      0x04
#define FT6336U_TOUCH_YH      0x05
#define FT6336U_TOUCH_YL      0x06

// Режимы
#define FT6336U_MODE_REGISTER   0x00
#define FT6336U_GESTURE_MODE    0x01
#define FT6336U_INT_MODE        0x01
#define FT6336U_THRESHOLD       0x80

// Режимы работы
#define FT6336U_MODE_NORMAL     0x00
#define FT6336U_MODE_FACTORY    0x04
#define FT6336U_MODE_MONITOR    0x01
#define FT6336U_MODE_SLEEP      0x07

// Жесты
#define FT6336U_GESTURE_NONE        0x00
#define FT6336U_GESTURE_UP          0x10
#define FT6336U_GESTURE_DOWN        0x20
#define FT6336U_GESTURE_LEFT        0x30
#define FT6336U_GESTURE_RIGHT       0x40
#define FT6336U_GESTURE_ZOOM_IN     0x50
#define FT6336U_GESTURE_ZOOM_OUT    0x60

typedef struct {
    I2C_HandleTypeDef *hi2c;
    uint8_t address;
    uint8_t last_irq_state;
    bool has_touch;
    uint16_t x[5];
    uint16_t y[5];
    uint8_t touch_id[5];
    uint8_t touch_num;
    uint8_t last_gesture;
    bool has_gesture;
} FT6336U_HandleTypeDef;

void FT6336U_Init(FT6336U_HandleTypeDef *ts, I2C_HandleTypeDef *hi2c, uint8_t address);
uint8_t FT6336U_ReadReg(FT6336U_HandleTypeDef *ts, uint8_t reg);
bool FT6336U_ReadRegs(FT6336U_HandleTypeDef *ts, uint8_t reg, uint8_t *data, uint8_t len);
bool FT6336U_ReadData(FT6336U_HandleTypeDef *ts);
uint8_t FT6336U_GetTouchNum(FT6336U_HandleTypeDef *ts);
bool FT6336U_GetTouchPoint(FT6336U_HandleTypeDef *ts, uint8_t index, uint16_t *x, uint16_t *y);

uint8_t FT6336U_GetChipID(FT6336U_HandleTypeDef *ts);
uint8_t FT6336U_GetFirmwareVersion(FT6336U_HandleTypeDef *ts);
uint8_t FT6336U_GetThreshold(FT6336U_HandleTypeDef *ts);
void FT6336U_SetThreshold(FT6336U_HandleTypeDef *ts, uint8_t threshold);
uint8_t FT6336U_GetMode(FT6336U_HandleTypeDef *ts);
void FT6336U_SetMode(FT6336U_HandleTypeDef *ts, uint8_t mode);
uint8_t FT6336U_GetGesture(FT6336U_HandleTypeDef *ts);
bool FT6336U_IsGestureAvailable(FT6336U_HandleTypeDef *ts);
void FT6336U_EnableInterrupt(FT6336U_HandleTypeDef *ts);
void FT6336U_DisableInterrupt(FT6336U_HandleTypeDef *ts);
void FT6336U_ClearInterruptFlag(FT6336U_HandleTypeDef *ts);

#endif