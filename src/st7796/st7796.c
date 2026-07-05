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

void ST7796_SetAddressWindow(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    uint16_t xe = x + w - 1, ye = y + h - 1;
    ST7796_WriteCmd(ST7796_CASET);
    uint8_t data[] = {x >> 8, x & 0xFF, xe >> 8, xe & 0xFF};
    ST7796_WriteData(data, 4);
    ST7796_WriteCmd(ST7796_PASET);
    data[0] = y >> 8; data[1] = y & 0xFF;
    data[2] = ye >> 8; data[3] = ye & 0xFF;
    ST7796_WriteData(data, 4);
    ST7796_WriteCmd(ST7796_RAMWR);
}

// ✅ Спрайтовая функция — поворот координат здесь!
void ST7796_SetAddressWindowRotated(int16_t x, int16_t y, uint16_t w, uint16_t h) {
    if (screen_rotation == 0) {
        ST7796_SetAddressWindow(x, y, w, h);
        return;
    }

    int16_t px = x, py = y;
    uint16_t pw = w, ph = h;

    switch (screen_rotation) {
        case 1: {
            int tmp = px; px = py; py = 319 - tmp - pw + 1;
            int tmp2 = pw; pw = ph; ph = tmp2;
            break;
        }
        case 2:
            px = 319 - px - pw + 1;
            py = 479 - py - ph + 1;
            break;
        case 3: {
            int tmp = px; px = 479 - py - ph + 1; py = tmp;
            int tmp2 = pw; pw = ph; ph = tmp2;
            break;
        }
        default: break;
    }

    ST7796_SetAddressWindow(px, py, pw, ph);
}

// ✅ Создание спрайта — выделяем буфер
bool Sprite_create_XY(Sprite_t* s, uint16_t w, uint16_t h,uint16_t x, uint16_t y) {
    if (!s || w == 0 || h == 0) return false;
    s->x=x;
    s->y=y;
    s->w = w; s->h = h;
    s->is_allocated = true;
    s->data = (uint16_t*)malloc(w * h * 2);
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
void Sprite_push(const Sprite_t* s, int16_t x, int16_t y) {
    //(void)x; (void)y; // игнорируем — дисплей не знает про поворот
    if (!s || !s->data || s->w == 0 || s->h == 0) return;

    // Просто окно (0, 0, s->w, s->h)
    ST7796_SetAddressWindow(0, 0, s->w, s->h);
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


// ✅ Тест поворота — теперь используем спрайты
void ST7796_TestRotation(Sprite_t test_sprite) {
    //Sprite_t test_sprite;
    //if (!Sprite_create(&test_sprite, 320, 480)) return;

    Sprite_fill(&test_sprite, RGB565_BLACK);

    uint16_t cx = 160, cy = 240, r = 50;
    uint16_t colors[4] = {0xF800, 0x07E0, 0x001F, 0xFFFF};

    for (int dy = -r; dy <= r; dy++) {
        for (int dx = -r; dx <= r; dx++) {
            if (dx*dx + dy*dy <= r*r) {
                int16_t rx = dx, ry = dy;
                switch (screen_rotation) {
                    case 1: rx = dy; ry = -dx; break;
                    case 2: rx = -dx; ry = -dy; break;
                    case 3: rx = -dy; ry = dx; break;
                }
                int quadrant = (ry < 0) ? 0 : 2;
                if (rx >= 0) quadrant += 1;

                int16_t px = cx + dx, py = cy + dy;
                if (px >= 0 && px < 320 && py >= 0 && py < 480) {
                    test_sprite.data[py * 320 + px] = colors[quadrant];
                }
            }
        }
    }

    char text[8];
    snprintf(text, sizeof(text), "ROT: %d", screen_rotation);
    int16_t text_width = lcd_get_str_width(text);
    int16_t text_x = (320 - text_width) / 2;

    lcd_set_font(&font_segoe_struct);
    lcd_print_to_buffer(text_x, 310, RGB565_GREEN, text, RGB565_BLACK, &test_sprite);
    Sprite_push(&test_sprite, 0, 0);
    Sprite_destroy(&test_sprite);
}

void ST7796_DrawPixel(int16_t x, int16_t y, uint16_t color) {
    if (x < 0 || x >= 320 || y < 0 || y >= 480) return;
    ST7796_SetAddressWindowRotated(x, y, 1, 1);
    uint8_t data[] = {color >> 8, color & 0xFF};
    ST7796_WriteData(data, 2);
}

void ST7796_FillScreen(uint16_t color) {
    Sprite_t s;
    if (!Sprite_create_XY(&s, 320, 480,0,0)) return;
    Sprite_fill(&s, color);
    Sprite_push(&s, 0, 0);
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
    Sprite_push(sprite, sprite->x, sprite->y);
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

void ST7796_SetRotation(int r){
    switch(r){
        case(0):   
            // Обычный портрет (0°):
            screen_rotation = 0;
            break;
        case(1): 
            // Поворот на 90° (пейзаж):
            screen_rotation = 1;
            break;
        case(2):
            // Поворот на 180°:
            screen_rotation = 2;
            break;
        case(3):
            // Поворот на 270°:
            screen_rotation = 3;
            break;
        default:screen_rotation = 0;
    }
}

// ✅ Поворот буфера спрайта (создаёт новый буфер, старый освобождает)
void Sprite_rotate(Sprite_t* s) {
    uint8_t r = screen_rotation;
    if (!s || !s->data || s->w == 0 || s->h == 0) return;
    if (r == 0) return;

    uint16_t w = s->w;
    uint16_t h = s->h;
    uint16_t* new_data = NULL;

    if (r == 2) { // 180°
        new_data = malloc(w * h * 2);
        if (!new_data) return;
        for (uint32_t i = 0; i < w * h; ++i) {
            new_data[i] = s->data[w * h - 1 - i];
        }
    } else if (r == 1 || r == 3) { // 90° / 270°
        uint16_t new_w = h;
        uint16_t new_h = w;
        new_data = malloc(new_w * new_h * 2);
        if (!new_data) return;

        for (uint16_t y = 0; y < h; ++y) {
            for (uint16_t x = 0; x < w; ++x) {
                uint16_t px, py;
                if (r == 1) { // 90°
                    px = y;
                    py = h - 1 - x;
                } else { // r == 3 → 270°
                    px = w - 1 - y;
                    py = x;
                }
                new_data[py * new_w + px] = s->data[y * w + x];
            }
        }

        s->w = new_w;
        s->h = new_h;
    } else {
        return; // Ошибка
    }

    free(s->data);
    s->data = new_data;
}