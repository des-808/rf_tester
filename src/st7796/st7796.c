#include "st7796.h"
#include <string.h>

#include "stdint.h"
#include <stdbool.h>
#include <stdio.h>


extern SPI_HandleTypeDef hspi4;
uint8_t screen_rotation = 0;

bool bluetoothEnabled = true;
bool wifiEnabled = true;
bool ntpSyncEnabled = true;
bool buzzerOnOff = true;
bool bluetoothMode = true;

uint16_t Display_Width = ST7796_WIDTH;   // Изначально 320
uint16_t Display_Height = ST7796_HEIGHT; // Изначально 480

// Буфер для DMA (остаётся — нужен для передачи)
uint8_t dma_buffer[320 * 2] __attribute__((section(".ram_d1"), aligned(32)));

// Создаем один большой массив памяти строго в AXI SRAM. 
// Максимальный размер: статус-бар (480*30) + экран (480*290) = 153 600 слов (307 200 байт)
// Выравниваем сам массив по границе 32 байт для D-Cache
#define MAX_HEAP_POOL 160// 160000
__attribute__((aligned(32))) uint16_t global_sprite_pool[MAX_HEAP_POOL];
uint32_t pool_offset = 0;

void* heap_caps_malloc(size_t size, uint32_t caps) {
    (void)caps;
    size_t size_in_words = (size + 1) / 2;
    size_t aligned_words = (size_in_words + 15) & ~15;

    // Проверяем по новому лимиту
    if (pool_offset + aligned_words > MAX_HEAP_POOL) {
        return NULL; 
    }

    void* ptr = (void*)&global_sprite_pool[pool_offset];
    pool_offset += aligned_words;
    return ptr;
}

void heap_caps_free(void* ptr) {
    // При статическом пуле нам не нужно освобождать отдельные куски, 
    // так как мы перевыделим весь пул заново при смене ориентации!
    (void)ptr;
}

/**
 * @brief Полный сброс памяти пула (Вызывается при повороте экрана перед пересчетом)
 */
void heap_caps_reset_pool(void) {
    pool_offset = 0; 
}

static void ST7796_WriteCmd(uint8_t cmd);
static void ST7796_WriteData(const uint8_t *data, size_t len);

static void ST7796_WriteCmd(uint8_t cmd) {
    LCD_CS_LOW;
    LCD_DC_CMD;
    HAL_SPI_Transmit(&hspi4, &cmd, 1, HAL_MAX_DELAY);
    LCD_CS_HIGH;
}

static void ST7796_WriteData(const uint8_t *data, size_t len) {
    LCD_CS_LOW;
    LCD_DC_DATA;
    HAL_SPI_Transmit_DMA(&hspi4, (uint8_t*)data, len);
    while (hspi4.State != HAL_SPI_STATE_READY);
    LCD_CS_HIGH;
}

static void ST7796_WriteDataByte(uint8_t data) {
    LCD_CS_LOW;
    LCD_DC_DATA;
    HAL_SPI_Transmit(&hspi4, &data, 1, HAL_MAX_DELAY);
    LCD_CS_HIGH;
}

HAL_StatusTypeDef ST7796_TransmitDMA(uint8_t *data, size_t len) {
    uint32_t size = (len + 31) & ~31;
    SCB_CleanDCache_by_Addr((uint32_t*)data, size);
    __DSB();
    HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(&hspi4, data, len);
    if (status != HAL_OK) return status;
    while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) {}
    return HAL_OK;
}

/**
 * @brief Установка окна адресации дисплея
 * @note  Параметры x2 и y2 ДОЛЖНЫ быть конечными координатами (Старт + Размер - 1), 
 *        а не шириной и высотой.
 */
void ST7796_SetAddressWindow(uint16_t x, uint16_t y, uint16_t x2, uint16_t y2) {
    // Защита от выхода за физические границы текущего режима экрана
    if (x2 >= Display_Width)  x2 = Display_Width - 1;
    if (y2 >= Display_Height) y2 = Display_Height - 1;

    ST7796_WriteCmd(ST7796_CASET); // Column Address Set (0x2A)
    ST7796_WriteDataByte(x >> 8);
    ST7796_WriteDataByte(x & 0xFF);
    ST7796_WriteDataByte(x2 >> 8);
    ST7796_WriteDataByte(x2 & 0xFF);

    ST7796_WriteCmd(ST7796_PASET); // Row Address Set (0x2B)
    ST7796_WriteDataByte(y >> 8);
    ST7796_WriteDataByte(y & 0xFF);
    ST7796_WriteDataByte(y2 >> 8);
    ST7796_WriteDataByte(y2 & 0xFF);

    ST7796_WriteCmd(ST7796_RAMWR); // Memory Write (0x2C)
}




void ST7796_DrawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= Display_Width || y < 0 || y >= Display_Height) return;
    ST7796_SetAddressWindow(x, y, x, y);
    uint8_t data[] = {color >> 8, color & 0xFF};
    ST7796_WriteData(data, 2);
}

void ST7796_FillScreen(uint16_t color) {
    uint16_t x, y;
    uint8_t data[] = {color >> 8, color & 0xFF};

    for (y = 0; y < Display_Height; y++) {
        ST7796_SetAddressWindow(0, y, Display_Width - 1, y);

        LCD_CS_LOW;
        LCD_DC_DATA;

        for (x = 0; x < Display_Width; x++) {
            HAL_SPI_Transmit(&hspi4, data, 2, HAL_MAX_DELAY);
        }

        LCD_CS_HIGH;
    }
}

void ST7796_Init(void) {
    // Инициализация дисплея (без frame_buffer)
    LCD_RESET_LOW;
    HAL_Delay(100);
    LCD_RESET_HIGH;
    HAL_Delay(200);  // Увеличено с 150 до 200 мс для стабильности

    ST7796_WriteCmd(ST7796_SWRESET);
    HAL_Delay(150);  // DS требует min 5ms, но 120-200ms надёжнее

    ST7796_WriteCmd(ST7796_SLPOUT);
    HAL_Delay(120);  // DS требует min 120ms

    ST7796_WriteCmd(ST7796_COLMOD);
    ST7796_WriteDataByte(0x55);
    HAL_Delay(20);

    ST7796_WriteCmd(ST7796_MADCTL);
    ST7796_WriteDataByte(MADCTL_MX | MADCTL_BGR);

    ST7796_WriteCmd(ST7796_INVON);
    ST7796_WriteCmd(ST7796_NORON);
    HAL_Delay(10);

    ST7796_WriteCmd(ST7796_DISPON);
    HAL_Delay(100);  // Увеличено с 10 до 100 мс

    // Тестовый FillScreen через HAL_SPI_Transmit (без DMA, без спрайтов)
    //ST7796_FillScreen(RGB565_CYAN);
}



uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    // ✅ SWAP байтов для SPI
    return ((color & 0xFF) << 8) | ((color >> 8) & 0xFF);
}

uint16_t BGR565(uint8_t r, uint8_t g, uint8_t b) {
    uint16_t color = ((b & 0xF8) << 8) | ((g & 0xFC) << 3) | (r >> 3);
    // ✅ SWAP байтов для SPI
    return ((color & 0xFF) << 8) | ((color >> 8) & 0xFF);
}
 

void ST7796_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* data) {
    if (!data || w == 0 || h == 0) return;
    if (x >= Display_Width || y >= Display_Height || x + w > Display_Width || y + h > Display_Height) return;

    ST7796_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    LCD_CS_LOW;
    LCD_DC_DATA;

    uint32_t total = (uint32_t)w * h;
    uint32_t sent = 0;
    while (sent < total) {
        uint32_t chunk = (total - sent > 320) ? 320 : (total - sent);
        memcpy(dma_buffer, &data[sent], chunk * 2);
        SCB_CleanDCache_by_Addr((uint32_t*)dma_buffer, (chunk * 2 + 31) & ~31);
        __DSB();
        if (ST7796_TransmitDMA(dma_buffer, chunk * 2) != HAL_OK) break;
        sent += chunk;
    }
    LCD_CS_HIGH;
}








void ST7796_SetRotation(uint8_t rotation) {
    uint8_t madctl_param = 0;
    screen_rotation = rotation % 4;

    switch (screen_rotation) {
        case 0: // Книжный (Стандартный)
            madctl_param = 0x48; // MX=0, MY=1, MV=0, ML=0, BGR=1
            Display_Width  = ST7796_WIDTH;
            Display_Height = ST7796_HEIGHT;
            break;
            
        case 1: // Альбомный (Разворот по часовой)
            madctl_param = 0x28; // MX=0, MY=0, MV=1, ML=0, BGR=1
            Display_Width  = ST7796_HEIGHT;
            Display_Height = ST7796_WIDTH;
            break;
            
        case 2: // Книжный (Перевернутый на 180)
            madctl_param = 0x88; // MX=1, MY=0, MV=0, ML=0, BGR=1
            Display_Width  = ST7796_WIDTH;
            Display_Height = ST7796_HEIGHT;
            break;
            
        case 3: // Альбомный (Разворот против часовой)
            madctl_param = 0xE8; // MX=1, MY=1, MV=1, ML=0, BGR=1
            Display_Width  = ST7796_HEIGHT;
            Display_Height = ST7796_WIDTH;
            break;
    }

    // 1. Предписываем контроллеру новый порядок обхода памяти
    ST7796_WriteCmd(ST7796_MADCTL);
    ST7796_WriteDataByte(madctl_param);

    // 2. Сбрасываем внутреннее окно адресации чипа на полные новые габариты
    ST7796_SetAddressWindow(0, 0, Display_Width - 1, Display_Height - 1);
}


