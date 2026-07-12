#include "st7796.h"
#include <string.h>

#include "stdint.h"
#include <stdbool.h>
#include <stdio.h>
#include "font.h"

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

// Вспомогательные статические переменные (если нужны — лучше передавать через Sprite_t)
// static Sprite_t main_screen; // → теперь создаются динамически в init_ui()

// ✅ DMA-совместимый allocator для STM32 (аналог heap_caps_malloc)
/* void* heap_caps_malloc(size_t size, uint32_t caps) {
    (void)caps; // игнорируем caps — STM32 не поддерживает MALLOC_CAP_DMA
    void* ptr = malloc(size);
    if (!ptr) return NULL;
    return ptr;
} */
// Создаем один большой массив памяти строго в AXI SRAM. 
// Максимальный размер: статус-бар (480*30) + экран (480*290) = 153 600 слов (307 200 байт)
// Выравниваем сам массив по границе 32 байт для D-Cache
#define MAX_HEAP_POOL 160000
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

static HAL_StatusTypeDef ST7796_TransmitDMA(uint8_t *data, size_t len) {
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


// ✅ Создание спрайта — выделяем буфер
bool Sprite_create_XY(Sprite_t* s, uint16_t w, uint16_t h, uint16_t x, uint16_t y, SpriteAnchor_t anchor) {
    if (!s || w == 0 || h == 0) return false;
    
    s->x = x;
    s->y = y;
    s->w = w; 
    s->h = h;
    s->is_allocated = true;
    s->anchor = anchor;
    
    // Выделяем память: ширина * высота * 2 байта (RGB565)
    s->data = (uint16_t*)heap_caps_malloc(w * h * 2, 0);
    
    if (s->data != NULL) {
        s->is_allocated = true;
        return true;
    }
    
    s->is_allocated = false;
    return false;
}

// ✅ Уничтожение спрайта
void Sprite_destroy(Sprite_t* s) {
    if (!s || !s->is_allocated || !s->data) return;
    heap_caps_free(s->data);
    s->data = NULL;
    s->is_allocated = false;
}

// ✅ Очистка спрайта
void Sprite_fill(Sprite_t* s, uint16_t color) {
    if (!s || !s->data) return;
    uint32_t count = s->w * s->h;
    for (uint32_t i = 0; i < count; i++) {
        s->data[i] = color;
    }
}

void ST7796_PushSprite(Sprite_t* s) {
    if ((s->x + s->w) > Display_Width || (s->y + s->h) > Display_Height) {
        return; 
    }

    ST7796_SetAddressWindow(s->x, s->y, s->x + s->w - 1, s->y + s->h - 1);

    uint32_t total_bytes = (uint32_t)s->w * s->h * 2; // Полный размер буфера в байтах
    
    LCD_CS_LOW;
    LCD_DC_DATA;

    // 1. Очищаем D-Cache один раз для всего массива спрайта целиком
    SCB_CleanDCache_by_Addr((uint32_t*)s->data, (total_bytes + 31) & ~31);
    __DSB();

    // 2. Отправляем ВЕСЬ массив в DMA одной транзакцией без деления на кусочки по 320
    // В STM32H7 счетчик данных DMA поддерживает передачу до 65535 или даже больше (в зависимости от регистра),
    // но если HAL_SPI_Transmit_DMA принимает размер в байтах/словах, то H7 может отправить до 65535 элементов за раз.
    // Если размер спрайта больше 65535, HAL разобьет его, либо отправляем частями:
    
    uint32_t sent_bytes = 0;
    while (sent_bytes < total_bytes) {
        // HAL_SPI_Transmit_DMA обычно принимает размер в штуках элементов (uint16_t в режиме 16-бит)
        // или в байтах (в режиме 8-бит). Проверьте настройку вашего SPI!
        // Предположим, передача идет порциями по 32768 байт максимум за вызов:
        uint32_t chunk_bytes = (total_bytes - sent_bytes > 60000) ? 60000 : (total_bytes - sent_bytes);
        
        if (ST7796_TransmitDMA((uint8_t*)s->data + sent_bytes, chunk_bytes) != HAL_OK) break;
        
        // Ожидаем окончания отправки блока
        while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY);
        
        sent_bytes += chunk_bytes;
    }

    LCD_CS_HIGH;
}


void ST7796_DrawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= Display_Width || y < 0 || y >= Display_Height) return;
    ST7796_SetAddressWindow(x, y, x, y);
    uint8_t data[] = {color >> 8, color & 0xFF};
    ST7796_WriteData(data, 2);
}

void ST7796_FillScreen(uint16_t color) {
    Sprite_t s;
    if (!Sprite_create_XY(&s, 320, 480,0,0,ANCHOR_FILL_REMAINING)) return;
    Sprite_fill(&s, color);
    //Sprite_push(&s, 0, 0);
    ST7796_PushSprite(&s);
    Sprite_destroy(&s);
}

void ST7796_Init(void) {
    // Инициализация дисплея (без frame_buffer)
    LCD_RESET_LOW;
    HAL_Delay(100);
    LCD_RESET_HIGH;
    HAL_Delay(150);

    ST7796_WriteCmd(ST7796_SWRESET);
    HAL_Delay(150);

    ST7796_WriteCmd(ST7796_SLPOUT);
    HAL_Delay(150);

    ST7796_WriteCmd(ST7796_COLMOD);
    ST7796_WriteDataByte(0x55);
    HAL_Delay(10);

    ST7796_WriteCmd(ST7796_MADCTL);
    ST7796_WriteDataByte(MADCTL_MX | MADCTL_BGR);

    ST7796_WriteCmd(ST7796_INVON);
    ST7796_WriteCmd(ST7796_NORON);
    HAL_Delay(10);

    ST7796_WriteCmd(ST7796_DISPON);
    HAL_Delay(10);

    ST7796_FillScreen(RGB565_CYAN);
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

// функция зеркалирования иконок
// Временный буфер для обработанной иконки (хватит для 16x16)
static uint8_t temp_icon_buffer[256]; 

// Функция горизонтального отзеркаливания (слева-направо)
const uint8_t* iconMirrorHorizontal(const unsigned char* bitmap, int w, int h) {
    int bytesPerRow = (w + 7) / 8;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < bytesPerRow; i++) {
            uint8_t b = bitmap[j * bytesPerRow + i];
            // Реверс бит в байте (для XBM-иконок)
            b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
            b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
            b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
            temp_icon_buffer[j * bytesPerRow + (bytesPerRow - 1 - i)] = b;
        }
    }
    return temp_icon_buffer;
}

// Функция вертикального отзеркаливания (верх-низ)
const uint8_t* iconMirrorVertical(const unsigned char* bitmap, int w, int h) {
    int bytesPerRow = (w + 7) / 8;
    for (int j = 0; j < h; j++) {
        for (int i = 0; i < bytesPerRow; i++) {
            temp_icon_buffer[(h - 1 - j) * bytesPerRow + i] = bitmap[j * bytesPerRow + i];
        }
    }
    return temp_icon_buffer;
}

// Глобальные переменные (как у тебя в ESP32)
/*extern int batteryLevel;
extern bool bluetoothEnabled, wifiEnabled, ntpSyncEnabled, buzzerOnOff, bluetoothMode;
extern uint8_t currentHour, currentMinute;

// Внешние иконки (примеры — см. ниже)
extern const uint8_t icon_battery_bits[];
extern const uint8_t icon_bluetooth_bits[];
extern const uint8_t icon_wifi_bits[];
extern const uint8_t icon_not_wifi_bits[];
extern const uint8_t icon_ntp_bits[];
extern const uint8_t icon_buzzer_on_bits[];
extern const uint8_t icon_buzzer_off_bits[];
extern const uint8_t icon_rs485ToBt_bits[];

uint8_t currentHour = 22;
uint8_t currentMinute = 43;
uint8_t battery_Level = 17;

 void drawStatusBar(Sprite_t *sprite) {
    lcd_set_font(&font_segoe_struct);
    // 1. Очистка буфера статус-бара — ИСПРАВЛЕНО: ST7796_FillBufferRect → Sprite_fill
    //Sprite_fill(sprite, RGB565_BLACK);
    Sprite_fill(sprite, RGB565_DARK_GRAY);
    // --- Время ---
    char timeBuf[6];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", currentHour, currentMinute);
    lcd_print_to_buffer(0, 4, 0xFFFF, timeBuf, 0x0000, sprite);
    lcd_set_font(&font_arial_9_struct);
    // --- Батарея ---
    char batBuf[8]; 
    snprintf(batBuf, sizeof(batBuf), "%d%%", battery_Level);
    int batWidth = lcd_get_str_width(batBuf);
    int curX = sprite->w - batWidth;
    // Текст батареи — ИСПРАВЛЕНО: удалена несуществующая функция
    // lcd_print_to_buffer_ex(curX, 4, 0xFFFF, batBuf, 0x0000, sprite, false);
    lcd_print_to_buffer(curX, 4, 0xFFFF, batBuf, 0x0000, sprite);
    // --- Иконка батареи ---
    ST7796_DrawBitmap(curX - 10, 4, icon_battery_16_16_bits, 16, 16, 0xFFFF, 0x0000, sprite->data);
    // --- Bluetooth ---
    if (bluetoothEnabled) {
        const uint8_t* icon = iconMirrorHorizontal(icon_bluetooth_bits, 10, 8);
        ST7796_DrawBitmap(curX - 32, 4, icon, 10, 8, 0x001F, 0x0000, sprite->data);
    }
    // --- Wi-Fi ---
    bool wifiConnected = wifiEnabled;
    if (wifiConnected) {
        const uint8_t* icon = iconMirrorHorizontal(icon_wifi_bits, 10, 8);
        ST7796_DrawBitmap(curX - 44, 4, icon, 10, 8, 0x07E0, 0x0000, sprite->data);
    }
    // --- Auto NTP ---
    if (ntpSyncEnabled) {
        const uint8_t* icon = iconMirrorHorizontal(icon_ntp_bits, 8, 8);
        ST7796_DrawBitmap(curX - 58, 4, icon, 8, 8, 0xFFFF, 0x0000, sprite->data);
    }
    // --- Buzzer ---
    const uint8_t* icon_buzz = buzzerOnOff ? icon_buzzer_on_bits : icon_buzzer_off_bits;
    ST7796_DrawBitmap(curX - 72, 4, iconMirrorHorizontal(icon_buzz, 8, 8), 8, 8, 0xFFFF, 0x0000, sprite->data);
    // --- Bluetooth Mode (RS485→BT) ---
    const uint8_t* icon_bt_mode = iconMirrorHorizontal(icon_rs485ToBt_bits, 10, 8);
    ST7796_DrawBitmap(curX - 86, 4, icon_bt_mode, 10, 8, bluetoothMode ? 0x07E0 : 0x0000, 0x0000, sprite->data);
    // 2. Отправить буфер статус-бара на экран — ИСПРАВЛЕНО: ST7796_UpdateSprite → Sprite_push
    //Sprite_push(sprite, sprite->x, sprite->y);
    ST7796_PushSprite(sprite);
} */

// ✅ Реализация ST7796_DrawBitmap — отрисовка битовой маски (XBM)
void ST7796_DrawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint16_t w, uint16_t h, uint16_t fgColor, uint16_t bgColor, uint16_t *buffer) {
    if (x < 0 || y < 0 || (uint16_t)(x + w) > Display_Width || (uint16_t)(y + h) > Display_Height) return;

    for (uint16_t row = 0; row < h; row++) {
        uint16_t byte_idx = row * ((w + 7) / 8);
        for (uint16_t col = 0; col < w; col++) {
            uint8_t mask = 0x80 >> (col % 8);
            uint8_t bit = (bitmap[byte_idx + col / 8] & mask);

            uint32_t idx = (uint32_t)(y + row) * Display_Width + (x + col);
            if (bit) {
                buffer[idx] = fgColor;
            } else if (bgColor != 0xFFFF) {
                buffer[idx] = bgColor;
            }
        }
    }

    ST7796_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    LCD_CS_LOW;
    LCD_DC_DATA;

    uint32_t total = (uint32_t)w * h;
    uint32_t sent = 0;
    while (sent < total) {
        uint32_t chunk = (total - sent > 320) ? 320 : (total - sent);
        memcpy(dma_buffer, &buffer[sent], chunk * 2);
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


/**
 * @brief Быстрое рисование линии внутри локального буфера спрайта (Алгоритм Брезенхема)
 * @param x0, y0 - стартовая точка (локальные координаты внутри спрайта)
 * @param x1, y1 - конечная точка (локальные координаты внутри спрайта)
 */
void Draw_Line_To_Sprite(Sprite_t* s, int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color) {
    if (!s || !s->data || !s->is_allocated) return;

    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy_sign = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;

    while (1) {
        // Проверка границ: рисуем пиксель только если он внутри локального массива спрайта
        if (x0 >= 0 && x0 < s->w && y0 >= 0 && y0 < s->h) {
            uint32_t idx = (uint32_t)y0 * s->w + x0;
            s->data[idx] = color;
        }

        if (x0 == x1 && y0 == y1) break;

        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += dy_sign;
        }
    }
}

/**
 * @brief Отправляет на экран только выделенную прямоугольную область внутри спрайта
 */
void ST7796_PushSpriteRect(Sprite_t* s, int16_t rx1, int16_t ry1, int16_t rx2, int16_t ry2) {
    // 1. Вычисляем абсолютные координаты на физическом экране дисплея
    uint16_t screen_x1 = s->x + rx1;
    uint16_t screen_y1 = s->y + ry1;
    uint16_t screen_x2 = s->x + rx2;
    uint16_t screen_y2 = s->y + ry2;

    // 2. Открываем аппаратное окно в контроллере ST7796 СТРОГО под размер грязной зоны
    ST7796_SetAddressWindow(screen_x1, screen_y1, screen_x2, screen_y2);

    LCD_CS_LOW;
    LCD_DC_DATA;

    // 3. Выгружаем пиксели строка за строкой (так как в памяти спрайта они лежат сплошным массивом)
    uint16_t rect_w = rx2 - rx1 + 1;
    
    // Очищаем кэш данных для всего региона спрайта один раз для безопасности DMA
    uint32_t total_bytes = (uint32_t)s->w * s->h * 2;
    SCB_CleanDCache_by_Addr((uint32_t*)s->data, (total_bytes + 31) & ~31);
    __DSB();

    for (int16_t y = ry1; y <= ry2; y++) {
        // Находим указатель на начало грязной строки в массиве спрайта
        uint16_t* row_start_ptr = &s->data[y * s->w + rx1];
        
        // Отправляем строку длиной rect_w по DMA
        // Внимание: так как отправка идет построчно, нужно использовать синхронный DMA 
        // или дожидаться окончания строки, чтобы не затереть SPI
        ST7796_TransmitDMA((uint8_t*)row_start_ptr, rect_w * 2);
        while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY); 
    }

    LCD_CS_HIGH;
}