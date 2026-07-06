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
void* heap_caps_malloc(size_t size, uint32_t caps) {
    (void)caps; // игнорируем caps — STM32 не поддерживает MALLOC_CAP_DMA
    void* ptr = malloc(size);
    if (!ptr) return NULL;
    return ptr;
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

/* void ST7796_SetAddressWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t xe = x + w - 1, ye = y + h - 1;
    ST7796_WriteCmd(ST7796_CASET);
    uint8_t data[] = {x >> 8, x & 0xFF, xe >> 8, xe & 0xFF};
    ST7796_WriteData(data, 4);
    ST7796_WriteCmd(ST7796_PASET);
    data[0] = y >> 8; 
    data[1] = y & 0xFF;
    data[2] = ye >> 8; 
    data[3] = ye & 0xFF;
    ST7796_WriteData(data, 4);
    ST7796_WriteCmd(ST7796_RAMWR);
} */

/**
 * @brief Установка окна адресации дисплея
 * @note  Параметры x2 и y2 ДОЛЖНЫ быть конечными координатами (Старт + Размер - 1), 
 *        а не шириной и высотой.
 */
void ST7796_SetAddressWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // Защита от выхода за физические границы текущего режима экрана
    if (w >= Display_Width)  w = Display_Width - 1;
    if (h >= Display_Height) h = Display_Height - 1;

    ST7796_WriteCmd(ST7796_CASET); // Column Address Set (0x2A)
    ST7796_WriteDataByte(x >> 8);
    ST7796_WriteDataByte(x & 0xFF);
    ST7796_WriteDataByte(w >> 8);
    ST7796_WriteDataByte(w & 0xFF);

    ST7796_WriteCmd(ST7796_PASET); // Row Address Set (0x2B)
    ST7796_WriteDataByte(y >> 8);
    ST7796_WriteDataByte(y & 0xFF);
    ST7796_WriteDataByte(h >> 8);
    ST7796_WriteDataByte(h & 0xFF);

    ST7796_WriteCmd(ST7796_RAMWR); // Memory Write (0x2C)
}


// ✅ Создание спрайта — выделяем буфер
bool Sprite_create_XY(Sprite_t* s, uint16_t w, uint16_t h,uint16_t x, uint16_t y, SpriteAnchor_t anchor) {
    if (!s || w == 0 || h == 0) return false;
    s->x=x;
    s->y=y;
    s->w = w; 
    s->h = h;
    s->is_allocated = true;
    s->data = (uint16_t*)malloc(w * h * 2);
    s->anchor = anchor;
    return s->data != NULL;
}

// ✅ Уничтожение спрайта
void Sprite_destroy(Sprite_t* s) {
    if (!s || !s->is_allocated || !s->data) return;
    free(s->data); // ← ИСПРАВЛЕНО
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

// ✅ Отправка спрайта на экран — здесь учитываем поворот!
/* void Sprite_push(const Sprite_t* s, int16_t x, int16_t y) {
    if (!s || !s->data || s->w == 0 || s->h == 0) return;

    // Просто окно (0, 0, s->w, s->h)
    ST7796_SetAddressWindow(s->x, s->y, s->w, s->h);
    LCD_CS_LOW;
    LCD_DC_DATA;

    uint32_t total = s->w * s->h;
    uint32_t sent = 0;
    while (sent < total) {
        uint32_t chunk = (total - sent > 320) ? 320 : (total - sent);
        memcpy(dma_buffer, &s->data[sent], chunk * 2);
        SCB_CleanDCache_by_Addr((uint32_t*)dma_buffer, (chunk * 2 + 31) & ~31);
        __DSB();
        if (ST7796_TransmitDMA(dma_buffer, chunk * 2) != HAL_OK) break;
        sent += chunk;
    }

    LCD_CS_HIGH;
} */

/**
 * @brief Вывод готового спрайта на экран
 */
void ST7796_PushSprite(Sprite_t* s) {
    // Проверка на выход за границы текущего разрешения экрана
    if ((s->x + s->w) > Display_Width || (s->y + s->h) > Display_Height) {
        return; // Защита от разрушения памяти дисплея
    }

    // Передаем правильные конечные координаты: (Старт + Размер - 1)
    ST7796_SetAddressWindow(s->x, s->y, s->x + s->w - 1, s->y + s->h - 1);

    uint32_t total_pixels = (uint32_t)s->w * s->h;
    
    LCD_CS_LOW;
    LCD_DC_DATA;

    uint32_t total = s->w * s->h;
    uint32_t sent = 0;
    while (sent < total) {
        uint32_t chunk = (total - sent > 320) ? 320 : (total - sent);
        memcpy(dma_buffer, &s->data[sent], chunk * 2);
        SCB_CleanDCache_by_Addr((uint32_t*)dma_buffer, (chunk * 2 + 31) & ~31);
        __DSB();
        if (ST7796_TransmitDMA(dma_buffer, chunk * 2) != HAL_OK) break;
        sent += chunk;
    }

    LCD_CS_HIGH;
}


void ST7796_DrawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= 320 || y < 0 || y >= 480) return;
    ST7796_SetAddressWindowRotated(x, y, 1, 1);
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

    ST7796_FillScreen(RGB565_BLACK);
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

    int16_t px = x, py = y;
    uint16_t pw = w, ph = h;

    switch (screen_rotation) {
        case 1: { int tmp = px; px = py; py = 319 - tmp - pw + 1; int tmp2 = pw; pw = ph; ph = tmp2; } break;
        case 2: px = 319 - px - pw + 1; py = 479 - py - ph + 1; break;
        case 3: { int tmp = px; px = py; py = 319 - tmp - pw + 1; int tmp2 = pw; pw = ph; ph = tmp2; } break;
        default: break;
    }

    //ST7796_SetAddressWindow(px, py, pw, ph);
    ST7796_SetAddressWindow(x, y, w, h);
    LCD_CS_LOW;
    LCD_DC_DATA;

    uint32_t total = w * h, sent = 0;
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
extern int batteryLevel;
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
}

// ✅ Реализация ST7796_DrawBitmap — отрисовка битовой маски (XBM)
void ST7796_DrawBitmap(int16_t x, int16_t y, const uint8_t *bitmap, uint16_t w, uint16_t h, uint16_t fgColor, uint16_t bgColor, uint16_t *buffer) {
    if (x < 0 || y < 0 || x + w > 320 || y + h > 480) return;

    for (uint16_t row = 0; row < h; row++) {
        uint16_t byte_idx = row * ((w + 7) / 8);
        for (uint16_t col = 0; col < w; col++) {
            uint8_t mask = 0x80 >> (col % 8);
            uint8_t bit = (bitmap[byte_idx + col / 8] & mask);

            uint32_t idx = (y + row) * 320 + (x + col);
            if (bit) {
                buffer[idx] = fgColor;
            } else if (bgColor != 0xFFFF) {
                buffer[idx] = bgColor;
            }
        }
    }

    // Отрисовка на экране (только область) — ИСПРАВЛЕНО: удалена несуществующая ST7796_UpdateScreenArea
    // Вызвать ST7796_SetAddressWindowRotated и отправить по DMA
    ST7796_SetAddressWindow(x, y, w, h);
    LCD_CS_LOW;
    LCD_DC_DATA;

    uint32_t total = w * h, sent = 0;
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


// Глобальный флаг текущего режима (0 - Portrait, 1 - Landscape)
uint8_t current_layout_mode = 0; 

/**
 * @brief Универсальный автоматический перевод спрайта из Portrait в Landscape
 * @note  Вызывается ДЛЯ КАЖДОГО спрайта при смене режима устройства
 */
void Sprite_ChangeOrientation(Sprite_t* sprite, uint8_t target_rotation) {
    // Если мы уже в этом режиме, ничего не делаем
    if ((target_rotation == 1 && current_layout_mode == 1) || 
        (target_rotation == 0 && current_layout_mode == 0)) {
        return;
    }

    if (target_rotation == 1) { // ПЕРЕХОД ИЗ PORTRAIT В LANDSCAPE (320x480 -> 480x320)
        // 1. Автоматический пересчет координат X и Y
        sprite->x = (int16_t)((float)sprite->x * 1.5f);
        sprite->y = (int16_t)((float)sprite->y * 0.666f);

        // 2. Автоматический пересчет размеров W и H
        sprite->w = (uint16_t)((float)sprite->w * 1.5f);
        sprite->h = (uint16_t)((float)sprite->h * 0.666f);
    } 
    else if (target_rotation == 0) { // ПЕРЕХОД ИЗ LANDSCAPE В PORTRAIT (480x320 -> 320x480)
        // Обратный пересчет
        sprite->x = (int16_t)((float)sprite->x / 1.5f);
        sprite->y = (int16_t)((float)sprite->y / 0.666f);

        sprite->w = (uint16_t)((float)sprite->w / 1.5f);
        sprite->h = (uint16_t)((float)sprite->h / 0.666f);
    }
}


void Sprite_UpdatePosition(Sprite_t* sprite) {
    switch (sprite->anchor) {
        case ANCHOR_TOP_LEFT:
            // Координаты остаются фиксированными (0,0), размеры не меняются.
            // Если статус-бар всегда должен быть во всю ширину экрана, раскомментируйте строку ниже:
            sprite->w = Display_Width; 
            break;

        case ANCHOR_BOTTOM_LEFT:
            // Прижимаем к самому низу экрана с сохранением его фиксированной высоты
            sprite->x = 0;
            sprite->y = Display_Height - sprite->h;
            sprite->w = Display_Width; // растягиваем по ширине
            break;

        case ANCHOR_CENTER:
            // Центрируем спрайт на экране (размеры остаются оригинальными)
            sprite->x = (Display_Width - sprite->w) / 2;
            sprite->y = (Display_Height - sprite->h) / 2;
            break;

        case ANCHOR_FILL_REMAINING:
            // Спрайт занимает всё оставшееся пространство от своей начальной точки Y до низа экрана
            sprite->x = 0;
            sprite->w = Display_Width;
            sprite->h = Display_Height - sprite->y; // Высота динамически подстроится под экран
            break;
    }
}


