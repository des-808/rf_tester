#ifndef _FONT_H_
#define _FONT_H_

#include <stdint.h>
#include "st7796.h"


// --- Обобщённая структура шрифта ---
typedef struct {
    uint16_t start_char;
    uint16_t length;
    uint16_t char_width;
    uint16_t char_height;
    uint16_t spacing;
    uint8_t  bytes_per_column;
    const uint8_t *data;
} Font_t;

// Параметры шрифта


// --- Константы для шрифта Arial 9 (2 байта на столбец) ---
//#define FONT_ARIAL_9_START_CHAR     32
//#define FONT_ARIAL_9_LENGTH         224
//#define FONT_ARIAL_9_CHAR_WIDTH     15
//#define FONT_ARIAL_9_CHAR_HEIGHT    14
#define FONT_ARIAL_9_SPACING        1
extern const uint8_t font_arial_9[];

// --- Константы для шрифта Segoe Print 12 (3 байта на столбец) ---
//#define FONT_SEGOE_PRINT_12_START_CHAR      0x20
//#define FONT_SEGOE_PRINT_12_LENGTH          96
//#define FONT_SEGOE_PRINT_12_CHAR_WIDTH      21
//#define FONT_SEGOE_PRINT_12_CHAR_HEIGHT     24
#define FONT_SEGOE_PRINT_12_SPACING         2
extern const uint8_t font_segoe_print_12[];

// Фиксированная структура шрифта
static const Font_t font_arial_9_struct = {
    .start_char         = 32,
    .length             = 224,
    .char_width         = 15,
    .char_height        = 16,
    .bytes_per_column   = 2,
    .spacing            = 1,
    .data               = font_arial_9
};
// Фиксированная структура шрифта
static const Font_t font_segoe_struct = {
    .start_char         = 32,
    .length             = 224,
    .char_width         = 21,
    .char_height        = 24,
    .bytes_per_column   = 3,
    .spacing            = 2,
    .data               = font_segoe_print_12
};

// --- Глобальная переменная текущего шрифта ---
extern Font_t* current_font;
extern uint16_t frame_buffer[]; 

void lcd_set_font(const Font_t *font);
void lcd_draw_char(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg_color,int char_real_width,Sprite_t *sprite);
//void lcd_print(int16_t x, int16_t y, uint16_t color, const char *str);
int16_t lcd_get_str_width(const char *str);

void lcd_print(int16_t x, int16_t y, uint16_t color, const char *str, uint16_t bg_color,Sprite_t *sprite);//обёртка
// --- Рисование в frame_buffer ---

void lcd_draw_char_to_buffer(int16_t x, int16_t y, char c, uint16_t color, uint16_t bg_color,int char_real_width,Sprite_t *sprite);
void lcd_print_to_buffer(int16_t x, int16_t y, uint16_t color, const char *str, uint16_t bg_color,Sprite_t *sprite);
void lcd_print_to_buffer_ex(int16_t x, int16_t y, uint16_t color, const char *str, uint16_t bg_color, Sprite_t *sprite, bool update_after_print);
void lcd_print_int(int16_t x, int16_t y, uint16_t color, int value, uint16_t bg_color,Sprite_t *sprite);
void lcd_print_float(int16_t x, int16_t y, uint16_t color, float value, uint8_t decimals, uint16_t bg_color,Sprite_t *sprite);

void lcd_clear_line(uint16_t x,uint16_t y, uint16_t height, uint16_t bg, Sprite_t *sprite);
#endif