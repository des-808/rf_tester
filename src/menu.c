#include "menu.h"
#include "st7796.h"
#include <string.h>

// === ВНЕШНИЕ ПЕРЕМЕННЫЕ (Ваш код) ===
// Предполагаем, что эти переменные объявлены где-то в main.c или globals.h
extern uint16_t sys, room, btn;
extern void transmit(uint16_t sys, uint16_t room, uint16_t btn, uint8_t type);
extern void initRfTransmitter(int is_pager);
extern void enterReceiverMode();
extern void showNC();
extern void showAbout();
extern void rs485ToggleBluetoothMode();

// Для настроек
extern uint8_t rs485BaudIndex;
extern uint8_t oledBrightness;
extern int bluetoothEnabled, wifiEnabled, ntpSyncEnabled, buzzerOnOff;
extern void toggleWiFi();
extern void manualSyncTimeWithNTP();
extern void clearWiFiCredentials();

// Для CC1101
extern uint16_t cc1101FreqFixed;
extern uint16_t cc1101BitRateFixed;
extern uint8_t cc1101RxBwIndex;
extern uint8_t cc1101Modulation;
extern uint8_t cc1101PowerIndex;
extern void cc1101ApplySettingsFromMenu();

// === ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ МЕНЮ ===
UIElement_t* current_menu_listbox = NULL;
MenuItem_t* current_menu_items = NULL;
uint8_t current_menu_count = 0;

// Временный буфер для пунктов (чтобы UI_ListBox_AddItem работал корректно)
// ВАЖНО: Нужно следить за panel_rows_count в gui.c
// Для простоты считаем, что Menu_Draw вызывается редко, и мы можем временно "забрать" строки из пула gui.c
// Но лучше использовать собственный пул или гарантировать, что digits_node не используется другим образом.
// В вашей реализации digits_node используется постоянно.
// Поэтому мы будем использовать статический массив элементов для меню, если digits_node занят,
// ИЛИ мы очищаем digits_node, рисуем меню, и восстанавливаем?
// Нет, лучше всего: Меню занимает ВЕСЬ список или заменяет его временно.
// Давайте предположим, что меню рисуется в тот же ListBox (ui_bands_listbox или новый).
// Для совместимости с вашим gui.c, я буду использовать глобальный пул panel_rows для элементов меню,
// но сначала очистим ListBox, а затем добавим элементы.

#define MAX_MENU_ENTRIES 20
static MenuItem_t menu_entries_buffer[MAX_MENU_ENTRIES];

////////////////////////////////////////////////////////////////////////////////////////////////////////
// Пример простой иконки 16x16 (синий квадрат для примера)
// Лучше заменить эти данные на реальные иконки
static const uint16_t icon_tx[] = {
    // ... ваши пиксели 16x16 ...
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
};

static const uint16_t icon_settings[] = {
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
};

static const uint16_t icon_nc[] = {
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
    0x001F, 0x001F, 0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,
};

// Массив всех доступных иконок, чтобы обращаться по ID
static const Icon_t all_icons[] = {
    { .pixels = (uint16_t*)icon_tx, .width = 16, .height = 16 },
    { .pixels = (uint16_t*)icon_settings, .width = 16, .height = 16 },
    { .pixels = (uint16_t*)icon_nc, .width = 16, .height = 16 },
    // Добавьте другие иконки сюда
};

#define ICON_COUNT (sizeof(all_icons) / sizeof(all_icons[0]))
////////////////////////////////////////////////////////////////////////////////////////////////////////
// === ОПРЕДЕЛЕНИЕ МЕНЮ (Перенос из вашего C++ кода) ===

// --- Вспомогательные колбэки ---
static void Action_Send_Key() { transmit(sys, room, btn, 0); /* DEVICE_TYPE_KEY */ }
static void Action_Send_Pager() { transmit(sys, room, btn, 1); /* DEVICE_TYPE_PAGER */ }
static void Action_GoBack() { /* Логика возврата к главному меню */ }

// --- Подменю передачи ---
static MenuItem_t buttonSubMenu[] = {
    { "Sys:", ITEM_TYPE_VALUE, .data.ptr_value = &sys, .data.value_limits = {1, 32, 1} },
    { "Room:", ITEM_TYPE_VALUE, .data.ptr_value = &room, .data.value_limits = {1, 32, 1} },
    { "Btn:", ITEM_TYPE_VALUE, .data.ptr_value = &btn, .data.value_limits = {1, 9, 1} },
    { "Send", ITEM_TYPE_ACTION, .data.action_func = Action_Send_Key },
};

static MenuItem_t pagerSubMenu[] = {
    { "Sys:", ITEM_TYPE_VALUE, .data.ptr_value = &sys, .data.value_limits = {1, 32, 1} },
    { "Room:", ITEM_TYPE_VALUE, .data.ptr_value = &room, .data.value_limits = {1, 32, 1} },
    { "Btn:", ITEM_TYPE_VALUE, .data.ptr_value = &btn, .data.value_limits = {1, 9, 1} },
    { "Send", ITEM_TYPE_ACTION, .data.action_func = Action_Send_Pager },
};

static MenuItem_t transmitterSubMenu[] = {
    { "Buttons", ITEM_TYPE_SUBMENU, .data.submenu_items = buttonSubMenu, .data.submenu_count = sizeof(buttonSubMenu)/sizeof(buttonSubMenu[0]) },
    { "Pager", ITEM_TYPE_SUBMENU, .data.submenu_items = pagerSubMenu, .data.submenu_count = sizeof(pagerSubMenu)/sizeof(pagerSubMenu[0]) },
};

// --- Подменю CC1101 ---
static MenuItem_t cc1101SubMenu[] = {
    { "Freq MHz", ITEM_TYPE_VALUE, .data.ptr_value = &cc1101FreqFixed, .data.value_limits = {30000, 92800, 10} }, // Шаг большой для частоты
    { "BitRate", ITEM_TYPE_VALUE, .data.ptr_value = &cc1101BitRateFixed, .data.value_limits = {100, 6000, 10} },
    { "RxBw", ITEM_TYPE_VALUE, .data.ptr_value = &cc1101RxBwIndex, .data.value_limits = {0, 15, 1} },
    { "Mod", ITEM_TYPE_VALUE, .data.ptr_value = &cc1101Modulation, .data.value_limits = {0, 1, 1} },
    { "Power", ITEM_TYPE_VALUE, .data.ptr_value = &cc1101PowerIndex, .data.value_limits = {0, 7, 1} },
    { "Apply", ITEM_TYPE_ACTION, .data.action_func = cc1101ApplySettingsFromMenu },
};

// --- Подменю настроек ---
static MenuItem_t settingsSubMenu[] = {
    { "RS485", ITEM_TYPE_VALUE, .data.ptr_value = &rs485BaudIndex, .data.value_limits = {0, 7, 1} },
    { "Light", ITEM_TYPE_VALUE, .data.ptr_value = &oledBrightness, .data.value_limits = {1, 10, 1} },
    { "BT", ITEM_TYPE_VALUE, .data.ptr_value = &bluetoothEnabled, .data.value_limits = {0, 1, 1} },
    { "WiFi", ITEM_TYPE_VALUE, .data.action_func = toggleWiFi, .data.value_limits = {0, 1, 1} }, // Логика toggle внутри
    { "NTP Auto", ITEM_TYPE_VALUE, .data.ptr_value = &ntpSyncEnabled, .data.value_limits = {0, 1, 1} },
    { "Sync Now", ITEM_TYPE_ACTION, .data.action_func = manualSyncTimeWithNTP },
    { "Buzzer", ITEM_TYPE_VALUE, .data.ptr_value = &buzzerOnOff, .data.value_limits = {0, 1, 1} },
    { "CC1101", ITEM_TYPE_SUBMENU, .data.submenu_items = cc1101SubMenu, .data.submenu_count = sizeof(cc1101SubMenu)/sizeof(cc1101SubMenu[0]) },
    { "Forget WiFi", ITEM_TYPE_ACTION, .data.action_func = NULL }, // Реализовать отдельно
};

// --- Главное меню ---
static MenuItem_t getCallMenu[] = {
    { "1.Transmitter", ITEM_TYPE_SUBMENU, .data.submenu_items = transmitterSubMenu, .data.submenu_count = sizeof(transmitterSubMenu)/sizeof(transmitterSubMenu[0]) },
    { "2.Receiver", ITEM_TYPE_ACTION, .data.action_func = enterReceiverMode },
};

static MenuItem_t mainMenu[] = {
    { "1.GetCall", 0 ,ITEM_TYPE_SUBMENU, .data.submenu_items = getCallMenu, .data.submenu_count = sizeof(getCallMenu)/sizeof(getCallMenu[0]) },
    { "2.RS485_To_Bt",0, ITEM_TYPE_ACTION, .data.action_func = rs485ToggleBluetoothMode },
    { "3.NC",2, ITEM_TYPE_INFO },
    { "4.Settings",1, ITEM_TYPE_SUBMENU, .data.submenu_items = settingsSubMenu, .data.submenu_count = sizeof(settingsSubMenu)/sizeof(settingsSubMenu[0]) },
    { "5.About",0, ITEM_TYPE_INFO },
    { "6.RSSI Plotter",0, ITEM_TYPE_ACTION, .data.action_func = NULL },
};

// Глобальная переменная для размера
uint8_t main_menu_count = sizeof(mainMenu) / sizeof(mainMenu[0]);

// === РЕАЛИЗАЦИЯ ФУНКЦИЙ ===

void Menu_SetMainMenu(void) {
    // mainMenu определен в этом файле (пусть будет static или без static, доступ только внутри)
    // Но так как мы здесь, мы видим mainMenu
    current_menu_items = mainMenu; 
    current_menu_count = sizeof(mainMenu) / sizeof(mainMenu[0]);
}

void Menu_Init(void) {
    current_menu_listbox = NULL;
    Menu_SetMainMenu(); // Инициализируем указатели на начало
}

void Menu_Draw(UIElement_t* listbox_container, MenuItem_t* items, uint8_t count) {
    if (!listbox_container || !items) return;

    // Очищаем старые элементы (здесь должен быть цикл удаления старых children,
    // чтобы избежать утечек памяти/ссылок!)
    for (uint8_t i = 0; i < listbox_container->children_count; i++) {
        free(listbox_container->children[i]); // Или возврат в пул объектов
    }
    listbox_container->children_count = 0;

    listbox_container->props.list_box.scroll_offset = 0;
    listbox_container->props.list_box.selected_index = 0;

    // Заполняем новыми данными
    for (uint8_t i = 0; i < count; i++) {
        //UI_ListBox_AddItem(listbox_container, items[i].text, items[i].icon_id);
        UI_ListBox_AddItem(listbox_container, items[i].text);
    }

    current_menu_items = items;
    current_menu_count = count;

    GUI_InvalidateSprite(listbox_container->sprite); // Просим GUI перерисовать всё с учетом новых иконок
}

// Хелпер для изменения значения
static void AdjustValue(void* ptr, int min, int max, int step, int direction) {
    int val = *(int*)ptr;
    val += direction * step;
    if (val < min) val = max;
    if (val > max) val = min;
    *(int*)ptr = val;
}

void Menu_ProcessInput(uint8_t key) {
    if (!current_menu_listbox || !current_menu_items) return;

    UIElement_t* lb = current_menu_listbox;
    uint8_t idx = lb->props.list_box.selected_index;
    
    if (idx >= current_menu_count) return;
    
    MenuItem_t* item = &current_menu_items[idx];

    switch (key) {
        case KEY_UP:
            if (lb->props.list_box.selected_index > 0) {
                lb->props.list_box.selected_index--;
                GUI_InvalidateSprite(lb->sprite);
            }
            break;

        case KEY_DOWN:
            if (lb->props.list_box.selected_index < lb->children_count - 1) {
                lb->props.list_box.selected_index++;
                GUI_InvalidateSprite(lb->sprite);
            }
            break;

        case KEY_ENTER: { // Это действие "выбора" (Enter или Tap)
            switch (item->type) {
                case ITEM_TYPE_ACTION:
                    if (item->data.action_func) {
                        item->data.action_func();
                    }
                    break;

                case ITEM_TYPE_VALUE:
                    // При нажатии на значение — меняем его (или открываем редактор)
                    // Для простоты: +/- при повторном нажатии или на разных кнопках?
                    // Давайте сделаем так: UP/DOWN меняют значение, ENTER - фиксирует/следующий шаг?
                    // Или: UP/DOWN меняют выделение, ENTER/SELECT - действие.
                    // Если это VALUE, то Enter меняет значение на +step?
                    // Или добавим KEY_INC/DEC.
                    // Пока сделаем: Enter меняет значение на +step.
                    // А UP/DOWN в контексте VALUE?
                    
                    // Альтернатива: 
                    // UP/Down - навигация.
                    // Enter - если ACTION, то вызов. Если VALUE, то шаг +1.
                    // shift+Enter (или другая кнопка) шаг -1.
                    
                    // Давайте добавим обработку INC/DEC в вашем main.c к этой функции.
                    // Но здесь, для отклика на SELECT:
                    AdjustValue(item->data.ptr_value, 
                                item->data.value_limits.min_val, 
                                item->data.value_limits.max_val, 
                                item->data.value_limits.step, 
                                1);
                    // Обновляем текст элемента в ListBox?
                    // UI_ListBox_AddItem создал текстовый блок. Нужно его обновить.
                    UIElement_t* ui_item = (UIElement_t*)lb->children[idx];
                    if (ui_item) {
                        // Форматируем значение обратно в текст
                        char buf[32];
                        // Упрощенная отрисовка: просто перерисуем ListBox, а UI_RenderListBox возьмет текст из ui_item?
                        // Нет, UI_ListBox_AddItem скопировал строку в ui_item->text_content.
                        // Нам нужно обновить ui_item->text_content.
                        // Но мы не знаем формат! 
                        // Решение: При каждом рендере или обновлении значения вызывать UI_SetText?
                        // Но UI_SetText обновляет только текст.
                        
                        // Лучшее решение: При изменении значения вызывать функцию перерисовки конкретного элемента
                        // Но у нас нет прямого доступа к формату.
                        // Поэтому в ITEM_TYPE_VALUE лучше хранить указатель на функцию обновления текста?
                        // Или просто сохранять старый текст и паттерн замены?
                        
                        // Для простоты примера: оставим как есть. В реальном коде нужно обновлять строку в ui_item.
                        // Например:
                        snprintf(buf, sizeof(buf), "%s: %d", item->text, *(int*)item->data.ptr_value);
                        strncpy(ui_item->text_content, buf, 31);
                        // Но это сложно универсализировать.
                        // Давайте просто отметим, что нужно перерисовать.
                    }
                    GUI_InvalidateSprite(lb->sprite);
                    break;

                case ITEM_TYPE_SUBMENU:
                    if (item->data.submenu_items) {
                        Menu_Draw(lb, item->data.submenu_items, item->data.submenu_count);
                    }
                    break;
                    
                case ITEM_TYPE_INFO:
                    break;
            }
            break;
        }
        
        case KEY_CANCEL:
            // Реализовать возврат к предыдущему меню.
            // Нужно хранить стек меню. Сейчас упростим: выход в mainMenu
            Menu_Draw(lb, mainMenu, sizeof(mainMenu)/sizeof(mainMenu[0]));
            break;
    }
}

/**
 * @brief Обновляет текстовое представление пункта меню в ListBox
 *        Так как UI_ListBox_AddItem копирует строку в ui_item->text_content,
 *        нам нужно переформатировать её, если это тип ITEM_TYPE_VALUE.
 */
static void Update_MenuItem_Text(UIElement_t* ui_item, MenuItem_t* menu_item) {
    if (!ui_item || !menu_item || menu_item->type != ITEM_TYPE_VALUE) return;

    // Для типов, зависящих от значений, нужно пересобрать строку.
    // Поскольку у нас разные типы данных (uint16_t, int, enum), 
    // мы делаем простую логику: берем базовый текст и добавляем значение.
    // Это упрощение. В продакшене лучше использовать форматирование.
    
    // Базовая строка (например "Sys:")
    const char* label = menu_item->text;
    
    // Читаем значение
    int val = 0;
    if (menu_item->data.ptr_value) {
        val = *(int*)menu_item->data.ptr_value;
    }

    // Формируем новую строку
    // ВАЖНО: text_content в UIElement_t обычно имеет фиксированный размер (например 32 или 64 байта).
    // Убедитесь, что ваш UIElement_t позволяет такое обновление.
    // В gui.h: char text_content[64]; (или больше)
    
    char new_text[64];
    snprintf(new_text, sizeof(new_text), "%s %d", label, val);
    
    strncpy(ui_item->text_content, new_text, sizeof(ui_item->text_content) - 1);
    ui_item->text_content[sizeof(ui_item->text_content) - 1] = '\0';
}

void Menu_ProcessTouch(uint16_t tx, uint16_t ty) {
    if (!current_menu_listbox || !current_menu_items) return;

    UIElement_t* lb = current_menu_listbox;
    
    // КРИТИЧЕСКИ ВАЖНО: Игнорируем выбор, если сейчас идет активное инерционное скроллирование.
    // Это предотвращает случайные клики при пролистывании.
    if (lb->touch_state.drag_active) {
        return;
    }

    int8_t selected = UI_ListBox_ProcessTouch(lb, tx, ty);

    // Индекс -1 означает промах, клик по скроллбару или начало драг-жеста.
    // Нас интересует только фактический выбор элемента (OnSelect / OnClick).
    if (selected < 0) {
        return;
    }

    // Защита от выхода за границы массива данных
    if ((uint8_t)selected >= current_menu_count) {
        return;
    }

    MenuItem_t* item = &current_menu_items[selected];
    
    // Безопасное получение UI-элемента (защита от рассинхрона data vs view)
    UIElement_t* ui_item = NULL;
    if ((uint8_t)selected < lb->children_count) {
        ui_item = (UIElement_t*)lb->children[selected];
    }

    // Актуализируем текст значений (ползунки, переключатели), чтобы они отображали текущее состояние системы
    if (item->type == ITEM_TYPE_VALUE && ui_item) {
        Update_MenuItem_Text(ui_item, item);
    }

    // Выполняем действие выбранного пункта
    switch (item->type) {
        case ITEM_TYPE_ACTION:
            if (item->data.action_func) {
                item->data.action_func();
            }
            break;

        case ITEM_TYPE_VALUE: {
            // Убираем дублирование кода, используя вспомогательную переменную-флаг
            bool needs_redraw = false;
            
            if (ui_item) {
                AdjustValue(item->data.ptr_value, 
                            item->data.value_limits.min_val, 
                            item->data.value_limits.max_val, 
                            item->data.value_limits.step, 
                            1);
                Update_MenuItem_Text(ui_item, item);
                needs_redraw = true; // Перерисуем только конкретный дочерний элемент
            } else {
                 AdjustValue(item->data.ptr_value, 
                            item->data.value_limits.min_val, 
                            item->data.value_limits.max_val, 
                            item->data.value_limits.step, 
                            1);
                 needs_redraw = true;
            }

            if (needs_redraw) {
                // Вместо перерисовки всего спрайта листа, эффективнее пометить конкретную строку
                uint16_t font_h = (lb->font != NULL) ? lb->font->char_height : font_arial_9_struct.char_height;
                uint16_t item_h = font_h + 6;
                
                GUI_InvalidateRect(lb->sprite, 
                                   lb->x, 
                                   lb->y + (selected - lb->props.list_box.scroll_offset) * item_h, 
                                   lb->w, 
                                   item_h);
            }
            break;
        }

        case ITEM_TYPE_SUBMENU:
            if (item->data.submenu_items) {
                // Перед передачей управления убеждаемся, что старое состояние сброшено
                lb->touch_state.drag_last_y = -1;
                lb->touch_state.drag_active = false;
                
                Menu_Draw(lb, item->data.submenu_items, item->data.submenu_count);
            }
            break;
            
        case ITEM_TYPE_INFO:
            // Можно добавить звуковую обратную связь (Tick) для информативности
            break;
    }
}



void transmit(uint16_t sys, uint16_t room, uint16_t btn, uint8_t type){}
void initRfTransmitter(int is_pager){}
void enterReceiverMode(){}
void showNC(){}
void showAbout(){}
void rs485ToggleBluetoothMode(){;}

// Для настроек
//extern uint8_t rs485BaudIndex;
//extern uint8_t oledBrightness;
//int bluetoothEnabled, wifiEnabled, ntpSyncEnabled, buzzerOnOff;
void toggleWiFi(){}
void manualSyncTimeWithNTP(){}
void clearWiFiCredentials(){}