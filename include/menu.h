#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "gui.h" // Для UIElement_t и типов

#include "buttons.h"
#include "page.h"

/* Шаги редактирования CC1101 хранятся в сотых долях единицы:
 * частота: 1 = 10 kHz, битрейт: 1 = 100 baud. */
#define CC1101_FREQ_FINE_STEP       1U
#define CC1101_FREQ_COARSE_STEP    10U
#define CC1101_FREQ_ACCEL_STEP    100U
#define CC1101_BITRATE_FINE_STEP   1U
#define CC1101_BITRATE_COARSE_STEP 10U
#define CC1101_BITRATE_ACCEL_STEP 100U

#define CC1101_FREQ_BAND1_MIN_FIXED 30000UL
#define CC1101_FREQ_BAND1_MAX_FIXED 34800UL
#define CC1101_FREQ_BAND2_MIN_FIXED 38700UL
#define CC1101_FREQ_BAND2_MAX_FIXED 46400UL
#define CC1101_FREQ_BAND3_MIN_FIXED 77900UL
#define CC1101_FREQ_BAND3_MAX_FIXED 92800UL

// === ТИПЫ ПУНКТОВ МЕНЮ ===
typedef enum {
    ITEM_TYPE_INFO,        // Просто текст (например "Transmitter")
    ITEM_TYPE_ACTION,      // Действие (кнопка "Send", "Settings")
    ITEM_TYPE_VALUE,       // Значение, которое можно менять (Sys, Room, Freq)
    ITEM_TYPE_SUBMENU,     // Переход в подменю
    ITEM_TYPE_EDIT_MODE,   // Режим редактирования значения (временный)
    ITEM_TYPE_PAGE         // Переход на страницу (UIElement-контейнер)
} MenuItemType_t;

// Структура для иконок (если вы используете их как пиксели/спрайты)
typedef struct {
    uint16_t* pixels;           // Указатель на массив цветов (RGB565)
    uint8_t  width;
    uint8_t  height;
} Icon_t;

// Структура пункта меню
typedef struct MenuItem {
    const char* text;
    uint16_t icon_id;           // Индекс иконки (0 = нет иконки)
    MenuItemType_t type;
    uint8_t data_count;         // Количество элементов для SUBMENU
    void (*on_value_changed)(void);  // Колбэк при изменении значения (например для обновления статус-бара)
    
    // Данные в зависимости от типа
    union {
        void* ptr_value;       // Для ITEM_TYPE_VALUE: указатель на int/uint
        void (*action_func)(void); // Для ITEM_TYPE_ACTION
        struct MenuItem* submenu_items; // Для ITEM_TYPE_SUBMENU: массив подменю
        PageDef_t* page_def;   // Для ITEM_TYPE_PAGE: указатель на описание страницы
    } data;
    
    // Лимиты значений (для ITEM_TYPE_VALUE) — вне union, всегда доступны
    struct {
        int min_val;
        int max_val;
        uint8_t step;
    } value_limits;
    
    // Размер значения в байтах (1 = uint8_t, 2 = uint16_t) — 0 = auto-detect по типу памяти
    uint8_t value_size;
    
} MenuItem_t;


/* enum MenuKey {
    KEY_NONE,
    KEY_UP,
    KEY_DOWN,
    KEY_ENTER,
    KEY_CANCEL
}; */

// === СТЕК НАВИГАЦИИ МЕНЮ ===
// Позволяет сохранять состояние меню при переходах в подменю
// и возвращаться на предыдущий уровень (включая вложенные подменю)

#define MAX_MENU_DEPTH 5          // Максимальная глубина вложенности

typedef struct {
    MenuItem_t* items;          // Массив пунктов меню
    uint8_t count;              // Количество пунктов
    uint8_t selected_index;     // Выбранный элемент (для восстановления скролла)
    uint8_t scroll_offset;      // Смещение прокрутки (для восстановления)
    int8_t last_leaf_selected;  // Выделение последнего листового элемента
} MenuState_t;

// Глобальные переменные состояния
extern UIElement_t* current_menu_listbox;
extern MenuItem_t* current_menu_items;
extern uint8_t current_menu_count;
extern uint8_t main_menu_count;

/* Флаг длинного нажатия (1 = удерживали Enter 5+ циклов) */
extern uint8_t menu_long_press_active;
extern uint8_t menu_hold_multiplier;

/* Блокировка тача после перехода между меню */
extern uint32_t touch_lock_tick;

// Стек навигации
extern MenuState_t menu_stack[MAX_MENU_DEPTH];
extern int menu_stack_top;        // -1 = стек пуст, 0 = один уровень на стеке


// === API СТЕКА НАВИГАЦИИ ===
void Menu_PushMenu(MenuItem_t* items, uint8_t count);   // Войти в подменю (сохраняет текущее)
void Menu_PopMenu(UIElement_t* listbox);                // Выход на уровень выше

// === API СВОРАЧИВАНИЯ/РАЗВОРАЧИВАНИЯ МЕНЮ ===
// Для экономии RAM при запуске полноэкранных режимов (RSSI Plotter, Spectrum и др.)
bool Menu_Collapse(void);   // Скрыть ListBox, освободить память детей
void Menu_Expand(void);     // Восстановить ListBox и перерисовать

// API
void Menu_Init(void);
void Menu_Draw(UIElement_t* listbox_container, MenuItem_t* items, uint8_t count);
void Menu_ProcessInput(uint8_t key);

void Update_MenuItem_Text(UIElement_t* ui_item, MenuItem_t* menu_item);

/* Хелперы для menu_touch.c */
void Menu_ExecuteSelected(UIElement_t* listbox, uint8_t selected_index);
void Menu_EditMode_Enter(MenuItem_t* item, int selected_index);
void Menu_EditMode_Exit(void);
void Menu_Update_EditDisplay(void);
void Menu_EditMode_PreviewValue(void);

/* Переменные режима редактирования (для menu_touch.c) */
extern uint8_t  edit_mode_active;
extern uint32_t edit_temp_value;
extern uint8_t  edit_value_size;
extern uint32_t edit_original_value;
extern uint8_t  edit_step;
extern MenuItem_t* edit_source_item;
extern int8_t   edit_selected_index;
extern char     edit_original_text[];

uint32_t Menu_NormalizeFrequency(uint32_t value, int8_t direction);

#endif
