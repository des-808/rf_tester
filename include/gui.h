#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include "st7796.h"


typedef enum {
    UI_TYPE_SPRITE,
    UI_TYPE_STACK_PANEL,
    UI_TYPE_GRID
} UIType_t;

typedef enum {
    ORIENTATION_VERTICAL,
    ORIENTATION_HORIZONTAL
} Orientation_t;

// Структура для Grid (задание строк и колонок в процентах или пропорциях)
typedef struct {
    uint8_t rows_count;
    uint8_t cols_count;
    uint8_t row_definitions[8]; // Доли/проценты для строк (например: 10, 70, 20)
    uint8_t col_definitions[8]; // Доли/проценты для колонок (например: 60, 40)
} GridDefinition_t;

// Структура для StackPanel (последовательное расположение)
typedef struct {
    Orientation_t orientation;
    uint16_t spacing;           // Зазор между элементами в пикселях
} StackPanelDefinition_t;

// Универсальная сущность UI (Элемент)
typedef struct UIElement {
    UIType_t type;
    int16_t x;                  // Абсолютная координата на экране (рассчитывается)
    int16_t y;                  // Абсолютная координата на экране (рассчитывается)
    uint16_t w;                 // Рассчитанная или заданная ширина
    uint16_t h;                 // Рассчитанная или заданная высота
    
    // Если это простой спрайт, привязываем вашу структуру
    Sprite_t* sprite;           

    // Указатели на детей (для контейнеров Grid и StackPanel)
    struct UIElement* children[8];
    uint8_t children_count;

    // Специфичные настройки контейнеров
    union {
        GridDefinition_t grid;
        StackPanelDefinition_t stack;
    } layout;

    // Свойства привязки элемента ВНУТРИ родительского Grid
    uint8_t grid_row;
    uint8_t grid_col;
} UIElement_t;


void GUI_ShowAdvancedMeasurementScreen(uint8_t rotation);
void UI_MeasureAndArrange(UIElement_t* element, int16_t parent_x, int16_t parent_y, uint16_t available_w, uint16_t available_h);
void UI_DrawTree(UIElement_t* element);

#endif // GUI_H