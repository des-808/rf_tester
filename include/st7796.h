#ifndef ST7796_H
#define ST7796_H

#include "main.h"
#include <stdbool.h> // 👈 добавить


// --- Константы дисплея ---
#define ST7796_WIDTH  320
#define ST7796_HEIGHT 480

// Команды
#define ST7796_NOP         0x00
#define ST7796_SWRESET     0x01
#define ST7796_RDDID       0x04
#define ST7796_RDDST       0x09
#define ST7796_SLPIN       0x10
#define ST7796_SLPOUT      0x11
#define ST7796_PTLON       0x12
#define ST7796_NORON       0x13
#define ST7796_INVOFF      0x20
#define ST7796_INVON       0x21
#define ST7796_DISPOFF     0x28
#define ST7796_DISPON      0x29
#define ST7796_CASET       0x2A
#define ST7796_PASET       0x2B
#define ST7796_RAMWR       0x2C
#define ST7796_RAMRD       0x2E
#define ST7796_MADCTL      0x36
#define ST7796_VSCSAD      0x37
#define ST7796_COLMOD      0x3A

// MADCTL bits
#define MADCTL_MY  0x80
#define MADCTL_MX  0x40
#define MADCTL_MV  0x20
#define MADCTL_ML  0x10
#define MADCTL_RGB 0x00
#define MADCTL_BGR 0x08

#define LCD_RESET_LOW HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_RESET)
#define LCD_RESET_HIGH HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET)
#define LCD_DC_CMD HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET)
#define LCD_DC_DATA HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET)
#define LCD_CS_LOW HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)
#define LCD_CS_HIGH HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)

// ✅ DMA-совместимый allocator для STM32 (аналог heap_caps_malloc)
void* heap_caps_malloc(size_t size, uint32_t caps);
void heap_caps_reset_pool(void);
void heap_caps_free(void* ptr);
HAL_StatusTypeDef ST7796_TransmitDMA(uint8_t *data, size_t len);
//static void ST7796_WriteCmd(uint8_t cmd);
//static void ST7796_WriteData(const uint8_t *data, size_t len);
//static void ST7796_WriteDataByte(uint8_t data);
//static HAL_StatusTypeDef ST7796_TransmitDMA(uint8_t *data, size_t len);
// x/y = start coordinate, x2/y2 = end coordinate inclusive
void ST7796_SetAddressWindow(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2);

void ST7796_DrawPixel(int16_t x, int16_t y, uint16_t color);
void ST7796_FillScreen(uint16_t color);
void ST7796_Init(void);
uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b);
uint16_t BGR565(uint8_t r, uint8_t g, uint8_t b);
void ST7796_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* data);

void ST7796_SetRotation(uint8_t r);

 #define RGB565_BLACK        0x0000
#define RGB565_WHITE        0xFFFF
#define RGB565_RED          0x00F8  // swapped from 0xF800
#define RGB565_GREEN        0xE007  // swapped from 0x07E0
#define RGB565_BLUE         0x1F00  // swapped from 0x001F
#define RGB565_YELLOW       0xE0FF  // Red + Green (swapped from 0xFFE0)
#define RGB565_CYAN         0xFF07  // Green + Blue (swapped from 0x07FF)
#define RGB565_MAGENTA      0x1FF8  // Red + Blue (swapped from 0xF81F)
#define RGB565_GRAY         0x1084  // ~50% gray (swapped from 0x8410)
#define RGB565_ORANGE       0xA8FD  // swapped from 0xFDA8
#define RGB565_PURPLE       0x0F78  // swapped from 0x780F
#define RGB565_PINK         0xF6F9  // swapped from 0xF9F6
#define RGB565_BROWN        0x689A  // swapped from 0x9A68
#define RGB565_LIGHT_GRAY   0x18C6  // swapped from 0xC618
#define RGB565_DARK_GRAY    0x0038  // swapped from 0x3800
#define RGB565_DARK_GREEN   0x0004  // swapped from 0x0400
#define RGB565_LIGHT_GREEN  0xF087  // swapped from 0x87F0
#define RGB565_LIGHT_BLUE   0x1F05  // swapped from 0x051F
#define RGB565_NAVY         0x0F00  // swapped from 0x000F
#define RGB565_DARK_RED     0x0080  // swapped from 0x8000

#endif







