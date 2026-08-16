#ifndef MENU_H
#define MENU_H

#include <stdint.h>
#include "gui.h" // Для UIElement_t и типов

#include "buttons.h"

// === ТИПЫ ПУНКТОВ МЕНЮ ===
typedef enum {
    ITEM_TYPE_INFO,        // Просто текст (например "Transmitter")
    ITEM_TYPE_ACTION,      // Действие (кнопка "Send", "Settings")
    ITEM_TYPE_VALUE,       // Значение, которое можно менять (Sys, Room, Freq)
    ITEM_TYPE_SUBMENU      // Переход в подменю
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
    
    // Данные в зависимости от типа
    union {
        void* ptr_value;       // Для ITEM_TYPE_VALUE: указатель на int/uint
        void (*action_func)(void); // Для ITEM_TYPE_ACTION
        struct MenuItem* submenu_items; // Для ITEM_TYPE_SUBMENU: массив подменю
        uint8_t submenu_count;         // Для ITEM_TYPE_SUBMENU: кол-во элементов
        struct {
            int min_val;
            int max_val;
            uint8_t step;
        } value_limits;
    } data;
} MenuItem_t;


/* enum MenuKey {
    KEY_NONE,
    KEY_UP,
    KEY_DOWN,
    KEY_ENTER,
    KEY_CANCEL
}; */

// Глобальные переменные состояния
extern UIElement_t* current_menu_listbox;
extern MenuItem_t* current_menu_items;
extern uint8_t current_menu_count;
extern uint8_t main_menu_count;


// Указатель на текущий ListBox, в котором рисуется меню
extern UIElement_t* g_menu_listbox;

// Указатели на массивы меню (объявляем здесь как extern, определяем в menu.c)
extern const MenuItem_t menu_main[];
extern const MenuItem_t menu_settings[];
extern const MenuItem_t menu_tx_buttons[];
extern const MenuItem_t menu_tx_pager[];
extern const MenuItem_t menu_tx_transmitter[];
extern const MenuItem_t menu_cc1101[];
extern const MenuItem_t menu_get_call[];


// API
void Menu_Init(void);
void Menu_Draw(UIElement_t* listbox_container, MenuItem_t* items, uint8_t count);
void Menu_ProcessInput(uint8_t key);
void Menu_ProcessTouch(uint16_t tx, uint16_t ty);

#endif
