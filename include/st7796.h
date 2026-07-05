#ifndef ST7796_H
#define ST7796_H

#include "main.h"
#include <stdbool.h> // 👈 добавить

typedef struct {
    uint16_t *data;   // Указатель на буфер спрайта (w * h uint16_t)
    uint16_t x, y;    // Координаты на экране (верхний левый угол)
    uint16_t w, h;    // Размеры спрайта
    bool is_allocated;
} Sprite_t;

#include "font.h"

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
//static void ST7796_WriteCmd(uint8_t cmd);
//static void ST7796_WriteData(const uint8_t *data, size_t len);
//static void ST7796_WriteDataByte(uint8_t data);
//static HAL_StatusTypeDef ST7796_TransmitDMA(uint8_t *data, size_t len);
void ST7796_SetAddressWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
// ✅ Спрайтовая функция — поворот координат здесь!
void ST7796_SetAddressWindowRotated(int16_t x, int16_t y, uint16_t w, uint16_t h);
// ✅ Создание спрайта — выделяем буфер
bool Sprite_create_XY(Sprite_t* s, uint16_t w, uint16_t h,uint16_t x, uint16_t y);
// ✅ Уничтожение спрайта
void Sprite_destroy(Sprite_t* s);
// ✅ Очистка спрайта
void Sprite_fill(Sprite_t* s, uint16_t color);
// ✅ Отправка спрайта на экран — здесь учитываем поворот!
void Sprite_push(const Sprite_t* s, int16_t x, int16_t y);
 // ✅ Рисуем текст в буфер — НЕ учитываем поворот!
//void lcd_print_to_buffer_sprite(int16_t x, int16_t y, uint16_t fg, const char* str, uint16_t bg, const Sprite_t* sprite);
// ✅ Тест поворота — теперь используем спрайты
void ST7796_TestRotation(Sprite_t test_sprite);
void ST7796_DrawPixel(int16_t x, int16_t y, uint16_t color);
void ST7796_FillScreen(uint16_t color);
void ST7796_Init(void);
uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b);
uint16_t BGR565(uint8_t r, uint8_t g, uint8_t b);
void ST7796_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* data);
// Функция горизонтального отзеркаливания (слева-направо)
const uint8_t* iconMirrorHorizontal(const unsigned char* bitmap, int w, int h);
// Функция вертикального отзеркаливания (верх-низ)
const uint8_t* iconMirrorVertical(const unsigned char* bitmap, int w, int h);
void drawStatusBar(Sprite_t *sprite);
// ✅ Реализация ST7796_DrawBitmap — отрисовка битовой маски (XBM)
void ST7796_DrawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint16_t w, uint16_t h, uint16_t fgColor, uint16_t bgColor, uint16_t *buffer);
void ST7796_SetRotation(uint8_t r);
//void Sprite_rotate(Sprite_t* s);

void SetMode_Portrait(Sprite_t * sprite);
void SetMode_Landscape(Sprite_t * sprite);
/*
#define RGB565_BLACK        0x0000
#define RGB565_WHITE        0xFFFF
#define RGB565_RED          0xF800
#define RGB565_GREEN        0x07E0
#define RGB565_BLUE         0x001F
#define RGB565_YELLOW       0xFFE0  // Red + Green
#define RGB565_CYAN         0x07FF  // Green + Blue
#define RGB565_MAGENTA      0xF81F  // Red + Blue
#define RGB565_GRAY         0x8410  // ~50% gray
#define RGB565_ORANGE       0xFDA8  // Red + a bit of green
#define RGB565_PURPLE       0x780F  // Red + blue, less green
#define RGB565_PINK         0xF9F6  // Red + mostly green, less blue
#define RGB565_BROWN        0x9A68  // Dark orange/brown
#define RGB565_LIGHT_GRAY   0xC618
#define RGB565_DARK_GRAY    0x3800
#define RGB565_DARK_GREEN   0x0400
#define RGB565_LIGHT_GREEN  0x87F0
#define RGB565_LIGHT_BLUE   0x051F
#define RGB565_NAVY         0x000F
#define RGB565_DARK_RED     0x8000
 */

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

// === КАСТОМНЫЕ ИКОНКИ (XBM) ===

// 🔋 Батарея 8x8
//static const unsigned char icon_battery_bits[] = { 0x38, 0x7C, 0x44, 0x44, 0x44, 0x44, 0x44, 0x7C };
//#define ICON_BAT_WIDTH  8
//#define ICON_BAT_HEIGHT 8

static const unsigned char icon_battery_16_16_bits[] = {0x03, 0xF8, 0x03, 0xF8, 0x1F, 0xFF, 0x18, 0x03, 0x18, 0x03, 0x18, 0x03, 0x18, 0x03, 0x18, 0x03, 0x18, 0x03, 0x18, 0x03, 0x18, 0x03, 0x18, 0x03, 0x18, 0x03, 0x18, 0x03, 0x18, 0x03, 0x1F, 0xFF};
#define ICON_BAT_WIDTH  16
#define ICON_BAT_HEIGHT 16
// 📶 Wi-Fi 8x8
static const unsigned char icon_wifi_bits[]  = { 0x00, 0x18, 0x24, 0x42, 0x99, 0x24, 0x00, 0x18 };
                                                        //0x1C, 0x00, 0x3E, 0x00, 0x63, 0x00, 0xC9, 0x01, 0x9C, 0x01, 0x36, 0x00, 0x63, 0x00, 0x08, 0x00 
#define ICON_WIFI_WIDTH  8
#define ICON_WIFI_HEIGHT 8

static const unsigned char icon_not_wifi_bits[] = {0x80, 0x58, 0x24, 0x5A, 0x99, 0x24, 0x02, 0x19 };
#define ICON_NOT_WIFI_WIDTH  8
#define ICON_NOT_WIFI_HEIGHT 8

// 🔵 Bluetooth 9x8
static const unsigned char icon_bluetooth_bits[] = { 0x0C, 0x15, 0x16, 0x0C, 0x16, 0x25, 0x14, 0x0C };
#define ICON_BT_WIDTH  8
#define ICON_BT_HEIGHT 8

// ⏰ NTP 8x8
static const unsigned char icon_ntp_bits[] = { 0x5A, 0x24, 0x46, 0x89, 0x91, 0xD2, 0x66, 0x3C };
#define ICON_NTP_WIDTH  8
#define ICON_NTP_HEIGHT 8

//buzzer
static const unsigned char icon_buzzer_on_bits[] = { 0x30, 0x28, 0x27, 0x23, 0x23, 0x27, 0x28, 0x30  };

#define ICON_BUZZER_WIDTH  8
#define ICON_BUZZER_HEIGHT 8

static const unsigned char icon_buzzer_off_bits[] = { 0xB0, 0x68, 0x27, 0x33, 0x2B, 0x27, 0x2A, 0x31  };

static const unsigned char icon_rs485ToBt_bits[] = { 0x20, 0x24, 0x2E, 0x35, 0xAC, 0x74, 0x24, 0x04 };
#define ICON_RS485TOBT_WIDTH  8
#define ICON_RS485TOBT_HEIGHT 8
static const unsigned char icon_not_rs485ToBt_bits[] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

#endif







