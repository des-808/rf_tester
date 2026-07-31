#ifndef GUI_H
#define GUI_H

#include <stdint.h>
#include "st7796.h"
#include <stdbool.h>


#define MAX_PANEL_ROWS 18
#define MAX_GRID_CHILDREN  8   // Для сеток и панелей этого более чем достаточно
#define MAX_LISTBOX_ITEMS  24  // Столько пунктов теперь может быть в меню/списках

// Выберите бóльшее значение для размера базового массива
#define MAX_ELEMENT_CHILDREN MAX_LISTBOX_ITEMS 

#define SHRIFT_HEIGHT 18
#define SHRIFT_OTSTUP_TOP_BOTTOM 4

#define UI_GRID_DIMENSION_PERCENT 100  // обычный процент
#define UI_GRID_DIMENSION_PIXEL   0xFFFF  // специальный код: пиксельный размер

typedef enum {
    UI_TYPE_GRID,
    UI_TYPE_STACK_PANEL,
    UI_TYPE_TEXT_BLOCK,   // Просто текст (неизменяемый или изменяемый)
    UI_TYPE_TEXT_BOX,     // Поле ввода с курсором
    UI_TYPE_BUTTON,       // Кнопка (текст + рамка + состояние нажатия)
    UI_TYPE_BORDER,       // Рамка вокруг вложенного элемента
    UI_TYPE_CHECK_BOX,    // Флажок [X]
    UI_TYPE_RADIO_BUTTON, // Переключатель (О)
    UI_TYPE_LIST_BOX      // Список элементов с прокруткой
} UIType_t;

// --- Свойства для TextBlock ---
typedef struct {
    uint16_t foreground_color;
    uint16_t background_color;
} TextBlockProps_t;

// --- Свойства для TextBox ---
typedef struct {
    bool is_focused;       // Активно ли поле для ввода прямо сейчас
    uint16_t cursor_pos;   // Позиция курсора в строке
    uint16_t border_color;
} TextBoxProps_t;

// --- Свойства для Button ---
typedef struct {
    bool is_pressed;       // Состояние кнопки (нажата/отжата)
    uint16_t normal_color; // Цвет в обычном состоянии
    uint16_t press_color;  // Цвет при нажатии
} ButtonProps_t;

// --- Свойства для Border ---
typedef struct {
    uint16_t border_color;
    uint8_t thickness;     // Толщина рамки в пикселях
} BorderProps_t;

// --- Свойства для CheckBox и RadioButton ---
typedef struct {
    bool is_checked;       // Включен/выключен
    uint8_t group_id;      // Только для RadioButton (объединение в группу)
} ToggleProps_t;

// --- Свойства для ListBox ---
typedef struct {
    int8_t selected_index; // Какой элемент выбран (-1 если никто)
    uint8_t scroll_offset; // Для прокрутки, если элементов много

    uint8_t height_mode;         // 0: по строкам, 1: по пикселям, 2: fill remaining
    uint8_t visible_row_count;   // используется, если height_mode == 0
    uint16_t pixel_height;       // используется, если height_mode == 1
} ListBoxProps_t;

typedef enum {
    ORIENTATION_VERTICAL,
    ORIENTATION_HORIZONTAL
} Orientation_t;

// Перечисление для типов горизонтального выравнивания текста
typedef enum {
    HORIZONTAL_ALIGN_LEFT,
    HORIZONTAL_ALIGN_CENTER,
    HORIZONTAL_ALIGN_RIGHT
} HorizontalAlignment_t;

// Перечисление для типов горизонтального выравнивания текста
typedef enum {
    VERTICAL_ALIGN_TOP,
    VERTICAL_ALIGN_CENTER,
    VERTICAL_ALIGN_BOTTOM
} VerticalAlignment_t;

// Структура для Grid (задание строк и колонок в процентах или пропорциях)
typedef struct {
    uint8_t rows_count;
    uint8_t cols_count;
    uint8_t row_definitions[MAX_GRID_CHILDREN]; // Доли/проценты для строк (например: 10, 70, 20)
    uint8_t col_definitions[MAX_GRID_CHILDREN]; // Доли/проценты для колонок (например: 60, 40)
    // 🔥 НОВОЕ: массивы флагов — false = процент, true = пиксели
    bool row_is_pixel[MAX_GRID_CHILDREN];
    bool col_is_pixel[MAX_GRID_CHILDREN];
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
    struct UIElement* children[MAX_ELEMENT_CHILDREN];
    uint8_t children_count;

    // КРИТИЧЕСКИ ВАЖНО: Добавляем указатель на функцию отрисовки содержимого!
    //void (*render_callback)(Sprite_t* s); 
    void (*render_callback)(struct UIElement* element); // Оставляем для кастомных окон

    // КРИТИЧЕСКИ ВАЖНО: Собственный текстовый буфер элемента
    char text_content[32]; 
    
    uint16_t background_color;
    uint16_t foreground_color;
    const Font_t* font;
    // КРИТИЧЕСКИ ВАЖНО: Добавляем свойство выравнивания
    HorizontalAlignment_t horizontal_alignment; 
    VerticalAlignment_t vertical_alignment; 

    // СЮДА УПАКОВЫВАЕМ ВСЕ СВОЙСТВА НОВЫХ КОНТЕНЕРОВ И КОМПОНЕНТОВ
    union {
        GridDefinition_t grid;
        StackPanelDefinition_t stack;
        TextBlockProps_t text_block;
        TextBoxProps_t text_box;
        ButtonProps_t button;
        BorderProps_t border;
        ToggleProps_t toggle;
        ListBoxProps_t list_box;
    } props;

    // Свойства привязки элемента ВНУТРИ родительского Grid
    uint8_t grid_row;
    uint8_t grid_col;
} UIElement_t;

#define HEAP_CAP_DEFAULT 0

void GUI_ShowAdvancedMeasurementScreen(uint8_t rotation);

//Хелперы для работы с элементами интерфейса Grid
void UI_SetGridRowPixel(UIElement_t* grid_elem, uint8_t row_idx, uint8_t size_px);
void UI_SetGridColPixel(UIElement_t* grid_elem, uint8_t col_idx, uint8_t size_px);
void UI_SetGridRowPercent(UIElement_t* grid_elem, uint8_t row_idx, uint8_t percent);
void UI_SetGridColPercent(UIElement_t* grid_elem, uint8_t col_idx, uint8_t percent);

void UI_MeasureAndArrange(UIElement_t* element, int16_t parent_x, int16_t parent_y, uint16_t available_w, uint16_t available_h);
static void UI_RenderChildElement(void* child_ptr);
void UI_DrawTree(UIElement_t* element);



void Draw_StatusBar_Callback(UIElement_t* el);
void Draw_Graph_Content(UIElement_t* els);

void Convert_Touch_Coordinates(uint16_t raw_x, uint16_t raw_y, uint16_t* out_x, uint16_t* out_y);

void GUI_InvalidateSprite(Sprite_t* s);
void GUI_InvalidateAll(UIElement_t* element);
void GUI_InvalidateRect(Sprite_t* s, int16_t rx, int16_t ry, uint16_t rw, uint16_t rh);


void UI_SetText(UIElement_t* element, const char* format, ...);
UIElement_t* GUI_Panel_AddString(UIElement_t* parent, const char* initial_text);
void GUI_Panel_ClearStrings(UIElement_t* parent);
void Draw_GeneralText_Callback(UIElement_t* el);



void UI_RenderBorder(UIElement_t* el);
void UI_RenderButton(UIElement_t* el);
void UI_RenderToggle(UIElement_t* el);

void UI_InitListBox(UIElement_t* el, Sprite_t* target_sprite);
UIElement_t* UI_ListBox_AddItem(UIElement_t* listbox, const char* item_text);

int8_t UI_ListBox_ProcessTouch(UIElement_t* listbox, uint16_t tx, uint16_t ty);
UIElement_t* UI_FindElementAt(UIElement_t* root, uint16_t tx, uint16_t ty, int16_t* out_local_x, int16_t* out_local_y);
void UI_RenderListBox(UIElement_t* el);

// Включить/выключить отладочную отрисовку геометрии элементов и скроллбара
extern bool ui_debug_draw;

#endif // GUI_H