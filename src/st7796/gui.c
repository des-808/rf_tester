#include "gui.h"
#include "menu.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
//#include "measurement/measurement.h"


#define MAX_GRID_ROWS 20
#define MAX_GRID_COLS 20
// Глобальные переменные сущностей интерфейса
UIElement_t root_grid;

UIElement_t main_work_grid;
UIElement_t graph_node;
UIElement_t digits_node;   // Наша правая панель (используется в MeasurementScreen)
UIElement_t digits_panel;  // Спрайт-контейнер панели (используется в GUI_BuildProInterface)
UIElement_t ui_bands_listbox; // Сам контейнер ListBox

// Флаг отладки: если true — рисуем границы и текстовые метки геометрии ListBox/scrollbar
bool ui_debug_draw = true;

bool bluetoothEnabled = true;
bool wifiEnabled = true;
bool ntpSyncEnabled = true;
bool buzzerOnOff = true;
bool bluetoothMode = true;
bool rs485toBt = true;

// Глобальные объекты для статус-бара
static UIElement_t status_bar_node;      // Контейнер Grid
static UIElement_t status_clock_node;    // Узел часов
static UIElement_t status_icons_node;    // Контейнер для иконок (Grid или StackPanel)

// Спрайты
static Sprite_t status_clock_sprite;
static Sprite_t status_icon_battery_sprite;
static Sprite_t status_icon_bt_sprite;
static Sprite_t status_icon_wifi_sprite;
static Sprite_t status_icon_ntp_sprite;
 Sprite_t status_icon_buzzer_sprite;
static Sprite_t status_icon_mode_sprite;
static Sprite_t status_spacer_sprite;

// Узлы иконок
static UIElement_t status_icon_battery_node;
static UIElement_t status_icon_bt_node;
static UIElement_t status_icon_wifi_node;
static UIElement_t status_icon_ntp_node;
 UIElement_t status_icon_buzzer_node;
static UIElement_t status_icon_mode_node;
static UIElement_t status_icon_spacer_node;

// Размеры
#define STATUS_BAR_HEIGHT 25
#define CLOCK_WIDTH 60

/* Глобальные объекты спрайтов для экранов интерфейса */
Sprite_t status_bar_sprite;
Sprite_t graph_sprite;
Sprite_t main_screen_sprite;

// Пул элементов для строк (выделяем память статически внутри gui.c, чтобы не плодить глобальные имена)
static UIElement_t panel_rows[MAX_PANEL_ROWS];
uint8_t panel_rows_count = 0;

// Нам нужны указатели только на динамические данные. 
// Переменные для динамических текстовых блоков (указатели на узлы дерева)
// ====================================================================
// 4. УКАЗАТЕЛИ НА ДИНАМИЧЕСКИЕ СТРОКИ ДЛЯ ОБНОВЛЕНИЯ ИЗ MAIN.C
// ====================================================================
UIElement_t* ui_swr_row   = NULL;
UIElement_t* ui_btn_row   = NULL;
UIElement_t* ui_touch_row = NULL; 


extern UIElement_t* current_menu_listbox;
extern MenuItem_t* current_menu_items;
extern uint8_t current_menu_count;



void Buzzer_On_Off_(void) {
    // Меняем состояние переменной
    //buzzerOnOff = !buzzerOnOff; 
    Draw_Icon_Buzzer_Callback(&status_icon_buzzer_node);
    // Говорим движку: "Узел иконки изменился, обнови его"
    GUI_InvalidateSprite(status_icon_buzzer_node.sprite); 
    
    // Больше ничего делать НЕ НУЖНО. 
    // UI_DrawTree(&status_icon_buzzer_node);// увидит флаг needs_render у этого узла и вызовет Draw_Icon_Buzzer_Callback автоматически.

    UI_DrawTree(&root_grid);
}

/* static void gui_measurement_callback(const MeasurementResults* r) {
    if (!r) return;
    if (ui_swr_row != NULL) {
        UI_SetText(ui_swr_row, "SWR: %.2f", r->swr);
    }
    // Пометим панели на перерисовку
    if (digits_node.sprite != NULL) GUI_InvalidateSprite(digits_node.sprite);
    if (graph_node.sprite != NULL) GUI_InvalidateSprite(graph_node.sprite);
} */

/* void GUI_ShowAdvancedMeasurementScreen(uint8_t rotation) {
    // ====================================================================
    // ЭТАП 1: АППАРАТНЫЙ СБРОС И ПОСТРОЕНИЕ "СКЕЛЕТА" СЕТОК
    // ====================================================================
    
    // 1. Поворачиваем железо дисплея
    ST7796_SetRotation(rotation);
    
    // Полностью очищаем прошлый пул памяти графики
    // ВАЖНО: Это освобождает память, но не обнуляет данные в старых буферах, если они останутся.
    // Однако, так как мы используем reset_pool, мы теряем доступ ко всем старым данным.
    heap_caps_reset_pool();
    
    // Принудительно зануляем указатели .data, чтобы движок понял, что память очищена!
    status_bar_sprite.data = NULL;
    status_bar_sprite.is_allocated = false;
    
    graph_sprite.data = NULL;
    graph_sprite.is_allocated = false;
    
    main_screen_sprite.data = NULL;
    main_screen_sprite.is_allocated = false;

    // !!! ДОБАВЬТЕ ЭТО: Сброс состояния иконок статус-бара !!!
    // Это заставит MeasureAndArrange пересоздать их память при повороте
    status_clock_sprite.data = NULL;
    status_clock_sprite.is_allocated = false;
    
    status_icon_battery_sprite.data = NULL;
    status_icon_battery_sprite.is_allocated = false;
    
    status_icon_bt_sprite.data = NULL;
    status_icon_bt_sprite.is_allocated = false;
    
    status_icon_wifi_sprite.data = NULL;
    status_icon_wifi_sprite.is_allocated = false;
    
    status_icon_ntp_sprite.data = NULL;
    status_icon_ntp_sprite.is_allocated = false;
    
    status_icon_buzzer_sprite.data = NULL;
    status_icon_buzzer_sprite.is_allocated = false;
    
    status_icon_mode_sprite.data = NULL;
    status_icon_mode_sprite.is_allocated = false;

    status_spacer_sprite.data = NULL;
    status_spacer_sprite.is_allocated = false;

    // Сбрасываем счетчики строк и списка
    GUI_Panel_ClearStrings(&digits_node);
    ui_bands_listbox.children_count = 0;
    panel_rows_count = 0; 


    // Сброс глобальных указателей на динамику, чтобы избежать использования "зombie" элементов
    ui_swr_row = NULL;
    ui_btn_row = NULL;
    ui_touch_row = NULL;

    extern uint16_t Display_Width;
    extern uint16_t Display_Height;

    // --- НАСТРОЙКА КОРНЕВОЙ СЕТКИ ---
    root_grid.type = UI_TYPE_GRID;
    root_grid.children_count = 0;
    root_grid.props.grid.rows_count = 2;
    root_grid.props.grid.cols_count = 1;

    UI_SetGridRowPixel(&root_grid, 0, 25);
    UI_SetGridRowPercent(&root_grid, 1, 100);
    UI_SetGridColPercent(&root_grid, 0, 100);

    // --- СТАТУС-БАР ---
    //  status_bar_node.type = UI_TYPE_TEXT_BLOCK;
    // status_bar_node.grid_row = 0;
    // status_bar_node.grid_col = 0;
    // status_bar_node.sprite = &status_bar_sprite; 
    // status_bar_node.render_callback = Draw_StatusBar_Callback;
    // status_bar_node.background_color = RGB565_BLACK;
    // status_bar_node.horizontal_alignment = HORIZONTAL_ALIGN_LEFT; // Пример
    // status_bar_node.vertical_alignment   = VERTICAL_ALIGN_TOP;
    // root_grid.children[root_grid.children_count++] = &status_bar_node; 
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Создаем статус-бар и добавляем его в root_grid
     GUI_BuildModularStatusBar(&root_grid);
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // --- ВЛОЖЕННАЯ СЕТКА (График + Панель) ---
    main_work_grid.type = UI_TYPE_GRID;
    main_work_grid.children_count = 0;
    main_work_grid.grid_row = 1;
    main_work_grid.grid_col = 0;
    main_work_grid.props.grid.rows_count = 1;
    main_work_grid.props.grid.cols_count = 2;
    
    // UI_SetGridColPixel(&main_work_grid, 0, 200); 
    //UI_SetGridColPercent(&main_work_grid, 1, 100);  
    UI_SetGridColPercent(&main_work_grid, 0, 65); 
    UI_SetGridColPercent(&main_work_grid, 1, 35); 

    root_grid.children[root_grid.children_count++] = &main_work_grid;

    // --- ГРАФИК ---
    graph_node.type = UI_TYPE_TEXT_BLOCK;
    graph_node.grid_row = 0;
    graph_node.grid_col = 0;
    graph_node.sprite = &graph_sprite;
    graph_node.render_callback = Draw_Graph_Content;
    graph_node.background_color = RGB565_BLACK;
    main_work_grid.children[main_work_grid.children_count++] = &graph_node;

    // --- ПРАВАЯ ПАНЕЛЬ (STACK) ---
    digits_node.type = UI_TYPE_STACK_PANEL;
    digits_node.sprite = &main_screen_sprite; 
    digits_node.props.stack.orientation = ORIENTATION_VERTICAL;
    digits_node.props.stack.spacing = 2; 
    digits_node.grid_row = 0; 
    digits_node.grid_col = 1; 
    digits_node.children_count = 0; 
    digits_node.background_color = RGB565_BLACK;
    // Устанавливаем шрифт по умолчанию для всей панели (опционально, если он наследуется)
    digits_node.font = &font_arial_9_struct; 
    main_work_grid.children[main_work_grid.children_count++] = &digits_node;

    // ПЕРВЫЙ ОБМЕР: Определение доступного пространства
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);

    // ====================================================================
    // ЭТАП 2: НАПОЛНЕНИЕ КОНТЕНТОМ
    // ====================================================================
    
    // Заголовок
    UIElement_t* el = GUI_Panel_AddString(&digits_node, "--ИЗМЕРЕНИЯ--");
    if(el) {
        el->horizontal_alignment = HORIZONTAL_ALIGN_CENTER;
        el->vertical_alignment   = VERTICAL_ALIGN_CENTER;
        el->background_color = RGB565_BLACK;
        el->h = 16;
    }

    // Динамические строки (Сохраняем в глобальные указатели)
    ui_swr_row = GUI_Panel_AddString(&digits_node, "SWR: 1.00"); 
    if(ui_swr_row) {
        ui_swr_row->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
        ui_swr_row->vertical_alignment   = VERTICAL_ALIGN_CENTER;
        ui_swr_row->background_color = RGB565_BLACK;
        ui_swr_row->h = 16;
    }
    
    el = GUI_Panel_AddString(&digits_node, "------------");
    if(el) {
        el->horizontal_alignment = HORIZONTAL_ALIGN_CENTER;
        el->vertical_alignment   = VERTICAL_ALIGN_TOP;
        el->background_color = RGB565_BLACK;
        el->h = 16;
    }


    // --- LISTBOX ---
    UI_InitListBox(&ui_bands_listbox, &main_screen_sprite);
    ui_bands_listbox.type = UI_TYPE_LIST_BOX;
     // Жесткая высота для расчетной фазы
    //ui_bands_listbox.h = 192; 
    //ui_bands_listbox.background_color = RGB565_BLACK; 

    // ListBox не имеет фиксированной высоты по умолчанию; он будет занимать
    // только доступное пространство, которое выдаст контейнер.
    ui_bands_listbox.h = 0;
    ui_bands_listbox.background_color = RGB565_BLACK;
    
    const char* bands[] = {
        "0. 433 MHz", "1. HF Band", "2. 2m VHF", "3. 70cmUHF", "4. 2.4 GHz",
        "5. 5.0 GHz", "6. 6.4 GHz", "7. 7.2 GHz", "8. 7.2 GHz", "9. 7.2 GHz",
        "10. 7.2 GHz", "11. 7.2 GHz", "12. 7.2 GHz", "13. 7.2 GHz", "14. 7.2 GHz",
        "15. 7.2 GHz", "16. 7.2 GHz"
    };
    
    for (uint8_t i = 0; i < 17; i++) {
        UI_ListBox_AddItem(&ui_bands_listbox, bands[i]);
    }

    digits_node.children[digits_node.children_count++] = &ui_bands_listbox;

    // --- КНОПКИ ПРОКРУТКИ ---
    if (panel_rows_count + 2 < MAX_PANEL_ROWS) {
        // Up Button
        UIElement_t* up_btn = &panel_rows[panel_rows_count++];
        memset(up_btn, 0, sizeof(UIElement_t)); // Чистим память
        up_btn->type = UI_TYPE_BUTTON;
        up_btn->sprite = &main_screen_sprite;
        up_btn->h = 20;
        up_btn->render_callback = NULL;
        up_btn->horizontal_alignment = HORIZONTAL_ALIGN_CENTER;
        up_btn->vertical_alignment = VERTICAL_ALIGN_CENTER;
        up_btn->props.button.is_pressed = false;
        up_btn->props.button.normal_color = 0x528A; 
        up_btn->props.button.press_color = 0x10A5; 
        up_btn->background_color = RGB565_RED;
        strncpy(up_btn->text_content, "Up", sizeof(up_btn->text_content)-1);
        // Шрифт кнопки устанавливается в UI_RenderButton, но если нужна настройка:
         up_btn->font = &font_arial_9_struct; 
        // Флаг, что высота не фиксирована для авто-расчета (если бы мы хотели автоматизировать)
        // Но так как мы задали h=26, они будут иметь этот размер.
        digits_node.children[digits_node.children_count++] = up_btn;

        // Down Button
        UIElement_t* down_btn = &panel_rows[panel_rows_count++];
        memset(down_btn, 0, sizeof(UIElement_t)); // Чистим память
        down_btn->type = UI_TYPE_BUTTON;
        down_btn->sprite = &main_screen_sprite;
        down_btn->h = 20;
        down_btn->render_callback = NULL;
        down_btn->horizontal_alignment = HORIZONTAL_ALIGN_CENTER;
        down_btn->vertical_alignment = VERTICAL_ALIGN_CENTER;
        down_btn->props.button.is_pressed = false;
        down_btn->props.button.normal_color = 0x528A;
        down_btn->props.button.press_color = 0x10A5;
        down_btn->background_color = RGB565_RED;
        strncpy(down_btn->text_content, "Down", sizeof(down_btn->text_content)-1);
        // Шрифт кнопки устанавливается в UI_RenderButton, но если нужна настройка:
        down_btn->font = &font_arial_9_struct;
        digits_node.children[digits_node.children_count++] = down_btn;
    }

    ui_btn_row = GUI_Panel_AddString(&digits_node, "No btn");
    if(ui_btn_row) {
        ui_btn_row->horizontal_alignment = HORIZONTAL_ALIGN_CENTER;
        ui_btn_row->vertical_alignment   = VERTICAL_ALIGN_CENTER;
        ui_btn_row->background_color = RGB565_BLACK;
        ui_btn_row->h = 16;
    }

    ui_touch_row = GUI_Panel_AddString(&digits_node, "No touch");
    if(ui_touch_row) {
        ui_touch_row->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
        ui_touch_row->vertical_alignment   = VERTICAL_ALIGN_CENTER;
        ui_touch_row->background_color = RGB565_BLACK;
        ui_touch_row->h = 16;
    }

    // Создаем ListBox для меню
    if (digits_node.children_count < MAX_ELEMENT_CHILDREN) {
        UIElement_t* menu_lb = &panel_rows[panel_rows_count++];
        memset(menu_lb, 0, sizeof(UIElement_t));
        
        // Инициализируем как ListBox
        menu_lb->type = UI_TYPE_LIST_BOX;
        menu_lb->sprite = &main_screen_sprite; // Общий спрайт панели
        menu_lb->font = &font_arial_9_struct;
        
        // Настройки высоты: можно сделать фиксированной или авто
        menu_lb->props.list_box.height_mode = UI_LISTBOX_HEIGHT_FIXED;
        menu_lb->props.list_box.pixel_height = 140; // Высота блока меню
        
        digits_node.children[digits_node.children_count++] = menu_lb;
        
        // Устанавливаем глобальный указатель
        current_menu_listbox = menu_lb;
        
         // Мы НЕ вызываем Menu_Draw с передачей массива.
        // Мы вызываем Menu_Draw, используя текущие глобальные данные меню (current_menu_items).
        // Но перед этим нужно убедиться, что current_menu_items указывает на mainMenu.
        
        // Либо, если вы хотите жестко привязать главное меню здесь:
        // Предположим, в menu.h есть extern MenuItem_t* current_menu_items;
        // И в menu.c в Menu_Init() мы уже установили current_menu_items = mainMenu;
        
        // Вызываем отрисовку, используя то, что сейчас "активно" в меню
        Menu_Draw(menu_lb, current_menu_items, current_menu_count);
    }
    
    // ====================================================================
    // ЭТАП 3: ОБМЕР И АЛЛОКАЦИЯ ПАМЯТИ (ВТОРОЙ ПРОГОН)
    // ====================================================================
    
    // Отключаем спрайты у детей, чтобы MeasureAndArrange не пытался аллоцировать старые буферы
    // или использовал неправильные размеры
    // Проставляем спрайты элементам перед обмером
    //  for (uint8_t i = 0; i < digits_node.children_count; i++) {
    //     if (digits_node.children[i]) {
    //         digits_node.children[i]->sprite = &main_screen_sprite;//= NULL;
    //         //digits_node.children[i]->sprite = NULL;
    //     }
    // }
    // ui_bands_listbox.sprite = &main_screen_sprite;
    // for (uint8_t i = 0; i < ui_bands_listbox.children_count; i++) {
    //     if (ui_bands_listbox.children[i]) {
    //         ui_bands_listbox.children[i]->sprite = &main_screen_sprite;//= NULL;
    //         //ui_bands_listbox.children[i]->sprite = NULL;
    //     }
    // }
    ui_bands_listbox.sprite = NULL; 

    // Второй обмер: реальный расчет размеров с учетом динамического контента
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);

    // ====================================================================
    // ВОССТАНОВЛЕНИЕ И РЕНДЕР
    // ====================================================================

    // Возвращаем спрайты
     for (uint8_t i = 0; i < digits_node.children_count; i++) {
        if(digits_node.children[i]) {
            digits_node.children[i]->sprite = &main_screen_sprite;
        }
    }
    
    ui_bands_listbox.sprite = &main_screen_sprite;
    for (uint8_t i = 0; i < ui_bands_listbox.children_count; i++) {
        if(ui_bands_listbox.children[i]) {
            ui_bands_listbox.children[i]->sprite = &main_screen_sprite;
        }
    } 

    // ------------------------------------------------------------------
    // ФИКС 1: ОЧИСТКА ФОНА STACKPANEL
    // Используем memset для скорости, так как это бинарный паттерн (0 = Black)
    // ------------------------------------------------------------------
    if (main_screen_sprite.is_allocated && main_screen_sprite.data) {
        uint32_t total_pixels = (uint32_t)main_screen_sprite.w * main_screen_sprite.h;
        memset(main_screen_sprite.data, 0x00, total_pixels * 2); // 0x0000 = Black in RGB565
    }

    // ------------------------------------------------------------------
    // ФИКС 2: ПРЕДВАРИТЕЛЬНАЯ ОТРИСОВКА ЭЛЕМЕНТОВ LISTBOX
    // ------------------------------------------------------------------
    //  for (uint8_t i = 0; i < ui_bands_listbox.children_count; i++) {
    //     UIElement_t* item = (UIElement_t*)ui_bands_listbox.children[i];
    //     if (item && item->sprite != NULL && item->sprite->data) {
    //          // Рисуем текст элемента в общий буфер панели
    //          // Убедимся, что у элемента есть координаты, назначенные MeasureAndArrange
    //          if (item->w > 0 && item->h > 0) {
    //              Draw_GeneralText_Callback(item);
    //          }
    //     }
    // } 

    // Активируем флаги рендеринга для всех основных блоков
    status_bar_sprite.needs_render = true;
    graph_sprite.needs_render = true;
    main_screen_sprite.needs_render = true;

    // --- ВАЖНО: Принудительно помечаем правую панель (digits_node) грязной целиком ---
    // Это гарантирует, что MeasureAndArrange перерисует ListBox и другие элементы
    //  if (digits_node.sprite && digits_node.sprite->is_allocated) {
    //     digits_node.sprite->dirty_x1 = 0;
    //     digits_node.sprite->dirty_y1 = 0;
    //     digits_node.sprite->dirty_x2 = digits_node.sprite->w - 1;
    //     digits_node.sprite->dirty_y2 = digits_node.sprite->h - 1;
    //     digits_node.sprite->needs_render = true;
    // } 


    // Сбрасываем Dirty Rect на весь размер
    GUI_InvalidateAll(&root_grid);
    
    // Полная отрисовка
    UI_DrawTree(&root_grid);

    // Подписываем GUI на обновления измерений
    //Measurement_Subscribe(gui_measurement_callback);
}
 */
void GUI_ShowMenuAdvancedMeasurementScreen(uint8_t rotation){
    // ====================================================================
    // ЭТАП 1: АППАРАТНЫЙ СБРОС И ПОСТРОЕНИЕ "СКЕЛЕТА" СЕТОК
    // ====================================================================
    
    // 1. Поворачиваем железо дисплея
    ST7796_SetRotation(rotation);
    
    // Полностью очищаем прошлый пул памяти графики
    // ВАЖНО: Это освобождает память, но не обнуляет данные в старых буферах, если они останутся.
    // Однако, так как мы используем reset_pool, мы теряем доступ ко всем старым данным.
    heap_caps_reset_pool();
    
    // Принудительно зануляем указатели .data, чтобы движок понял, что память очищена!
    status_bar_sprite.data = NULL;
    status_bar_sprite.is_allocated = false;

    main_screen_sprite.data = NULL;
    main_screen_sprite.is_allocated = false;

    // Сбрасываем счетчики строк и списка
    GUI_Panel_ClearStrings(&digits_node);
    ui_bands_listbox.children_count = 0;
    panel_rows_count = 0; 

    // Сброс глобальных указателей на динамику, чтобы избежать использования "зombie" элементов
    ui_swr_row = NULL;
    ui_btn_row = NULL;
    ui_touch_row = NULL;

    extern uint16_t Display_Width;
    extern uint16_t Display_Height;

    // --- НАСТРОЙКА КОРНЕВОЙ СЕТКИ ---
    root_grid.type = UI_TYPE_GRID;
    root_grid.children_count = 0;
    root_grid.props.grid.rows_count = 2;
    root_grid.props.grid.cols_count = 1;

    UI_SetGridRowPixel(&root_grid, 0, 25);  //status bar
    UI_SetGridColPercent(&root_grid, 0, 100);

    // --- СТАТУС-БАР ---
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Создаем статус-бар и добавляем его в root_grid
     GUI_BuildModularStatusBar(&root_grid);
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // --- ВЛОЖЕННАЯ СЕТКА (График + Панель) ---
    main_work_grid.type = UI_TYPE_GRID;
    main_work_grid.children_count = 0;
    main_work_grid.grid_row = 1;
    main_work_grid.grid_col = 0;
    main_work_grid.props.grid.rows_count = 1;
    main_work_grid.props.grid.cols_count = 1;
    
    
    UI_SetGridColPercent(&main_work_grid, 0, 100);
    root_grid.children[root_grid.children_count++] = &main_work_grid;

    

    // --- ПРАВАЯ ПАНЕЛЬ (STACK) ---
    digits_node.type = UI_TYPE_STACK_PANEL;
    digits_node.sprite = &main_screen_sprite; 
    digits_node.props.stack.orientation = ORIENTATION_VERTICAL;
    digits_node.props.stack.spacing = 2; 
    digits_node.grid_row = 0; 
    digits_node.grid_col = 0; 
    digits_node.children_count = 0; 
    digits_node.background_color = RGB565_BLACK;
    // Устанавливаем шрифт по умолчанию для всей панели (опционально, если он наследуется)
    digits_node.font = &font_arial_9_struct; 
    main_work_grid.children[main_work_grid.children_count++] = &digits_node;

    // ПЕРВЫЙ ОБМЕР: Определение доступного пространства
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);

    // ====================================================================
    // ЭТАП 2: НАПОЛНЕНИЕ КОНТЕНТОМ
    // ====================================================================

    // Создаем ListBox для меню
    if (digits_node.children_count < MAX_ELEMENT_CHILDREN) {
        UIElement_t* menu_lb = &panel_rows[panel_rows_count++];
        memset(menu_lb, 0, sizeof(UIElement_t));
        
        // Инициализируем как ListBox
        menu_lb->type = UI_TYPE_LIST_BOX;
        menu_lb->sprite = &main_screen_sprite; // Общий спрайт панели
        menu_lb->font = &font_segoe_struct;//font_arial_9_struct;
        
        // Настройки высоты: можно сделать фиксированной или авто
        menu_lb->props.list_box.height_mode = UI_LISTBOX_HEIGHT_FIXED;
        menu_lb->props.list_box.pixel_height = 0; // Высота блока меню
        
        digits_node.children[digits_node.children_count++] = menu_lb;
        
        // Устанавливаем глобальный указатель
        current_menu_listbox = menu_lb;
        
         // Мы НЕ вызываем Menu_Draw с передачей массива.
        // Мы вызываем Menu_Draw, используя текущие глобальные данные меню (current_menu_items).
        // Но перед этим нужно убедиться, что current_menu_items указывает на mainMenu.
        
        // Либо, если вы хотите жестко привязать главное меню здесь:
        // Предположим, в menu.h есть extern MenuItem_t* current_menu_items;
        // И в menu.c в Menu_Init() мы уже установили current_menu_items = mainMenu;
        
        // Вызываем отрисовку, используя то, что сейчас "активно" в меню
        Menu_Draw(menu_lb, current_menu_items, current_menu_count);
    }
    
    // ====================================================================
    // ЭТАП 3: ОБМЕР И АЛЛОКАЦИЯ ПАМЯТИ (ВТОРОЙ ПРОГОН)
    // ====================================================================

    ui_bands_listbox.sprite = NULL; 

    // Второй обмер: реальный расчет размеров с учетом динамического контента
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);

// Возвращаем спрайты
     for (uint8_t i = 0; i < digits_node.children_count; i++) {
        if(digits_node.children[i]) {
            digits_node.children[i]->sprite = &main_screen_sprite;
        }
    }
    
    ui_bands_listbox.sprite = &main_screen_sprite;
    for (uint8_t i = 0; i < ui_bands_listbox.children_count; i++) {
        if(ui_bands_listbox.children[i]) {
            ui_bands_listbox.children[i]->sprite = &main_screen_sprite;
        }
    } 

    // ------------------------------------------------------------------
    // ФИКС 1: ОЧИСТКА ФОНА STACKPANEL
    // Используем memset для скорости, так как это бинарный паттерн (0 = Black)
    // ------------------------------------------------------------------
    if (main_screen_sprite.is_allocated && main_screen_sprite.data) {
        uint32_t total_pixels = (uint32_t)main_screen_sprite.w * main_screen_sprite.h;
        memset(main_screen_sprite.data, 0x00, total_pixels * 2); // 0x0000 = Black in RGB565
    }




    // Сбрасываем Dirty Rect на весь размер
    GUI_InvalidateAll(&root_grid);
    
    // Полная отрисовка
    UI_DrawTree(&root_grid);
}



void UI_SetGridRowPixel(UIElement_t* grid_elem, uint8_t row_idx, uint8_t size_px) {
    if (!grid_elem || grid_elem->type != UI_TYPE_GRID) return;
    if (row_idx >= MAX_GRID_CHILDREN) return;

    GridDefinition_t* g = &grid_elem->props.grid;
    g->row_definitions[row_idx] = size_px;
    g->row_is_pixel[row_idx] = true;
}

void UI_SetGridColPixel(UIElement_t* grid_elem, uint8_t col_idx, uint8_t size_px) {
    if (!grid_elem || grid_elem->type != UI_TYPE_GRID) return;
    if (col_idx >= MAX_GRID_CHILDREN) return;

    GridDefinition_t* g = &grid_elem->props.grid;
    g->col_definitions[col_idx] = size_px;
    g->col_is_pixel[col_idx] = true;
}

void UI_SetGridRowPercent(UIElement_t* grid_elem, uint8_t row_idx, uint8_t percent) {
    if (!grid_elem || grid_elem->type != UI_TYPE_GRID) return;
    if (row_idx >= MAX_GRID_CHILDREN) return;

    GridDefinition_t* g = &grid_elem->props.grid;
    g->row_definitions[row_idx] = percent;
    g->row_is_pixel[row_idx] = false;
}

void UI_SetGridColPercent(UIElement_t* grid_elem, uint8_t col_idx, uint8_t percent) {
    if (!grid_elem || grid_elem->type != UI_TYPE_GRID) return;
    if (col_idx >= MAX_GRID_CHILDREN) return;

    GridDefinition_t* g = &grid_elem->props.grid;
    g->col_definitions[col_idx] = percent;
    g->col_is_pixel[col_idx] = false;
}

void UI_SetGridRowWeight(UIElement_t* grid_elem, uint8_t row_idx, uint8_t weight) {
    if (!grid_elem || grid_elem->type != UI_TYPE_GRID) return;
    if (row_idx >= MAX_GRID_CHILDREN) return;
    grid_elem->props.grid.row_weights[row_idx] = weight;
}

void UI_SetGridColWeight(UIElement_t* grid_elem, uint8_t col_idx, uint8_t weight) {
    if (!grid_elem || grid_elem->type != UI_TYPE_GRID) return;
    if (col_idx >= MAX_GRID_CHILDREN) return;
    grid_elem->props.grid.col_weights[col_idx] = weight;
}



    
/**
 * @brief Рекурсивно вычисляет размеры и координаты элементов UI (Measure & Pass)
 * 
 * @param element Корневой или дочерний элемент
 * @param parent_x Координата X родителя
 * @param parent_y Координата Y родителя
 * @param available_w Доступная ширина
 * @param available_h Доступная высота
 */
/* void UI_MeasureAndArrange(UIElement_t* element, int16_t parent_x, int16_t parent_y, uint16_t available_w, uint16_t available_h) {
    if (!element) return;

    // 1. Установка базовых координат и доступных размеров
    element->x = parent_x; 
    element->y = parent_y;
    element->w = available_w; 
    element->h = available_h;

    // --- ЛОГИКА ДЛЯ КОНТЕЙНЕРОВ С ОБЩИМ СПРАЙТОМ ---
    // Если это StackPanel или ListBox, которые используют общий спрайт (например, main_screen_sprite),
    // нам нужно убедиться, что размеры спрайта соответствуют элементам, если это первый проход или размер изменился.
    // Но мы НЕ должны выделять/фрить память здесь, это делает GUI_ShowAdvancedMeasurementScreen или аллокатор.
    
    if (element->children_count > 0) {
        if (element->sprite != NULL) {
            Sprite_t* s = element->sprite;
            // Обновляем координаты спрайта (важно дляdirty rect)
            if (s->w != element->w || s->h != element->h) {
                s->w = element->w;
                s->h = element->h;
                // Если спрайт общий, мы не меняем его буфер здесь, если он уже выделен большим.
                // Но если он маленький и должен стать большим — это может быть проблемой.
                // В вашем случае main_screen_sprite обычно большой (размер всей правой панели).
            }
            s->x = element->x;
            s->y = element->y;
        }
    }

    // Если у элемента нет детей и есть спрайт — аллоцируем/чистим его
    if (element->children_count == 0 && element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        
        bool needs_realloc = (s->data == NULL || s->w != element->w || s->h != element->h || !s->is_allocated);
        
        if (element->w == 0 || element->h == 0) {
            if (s->is_allocated && s->data) {
                heap_caps_free(s->data);
                s->is_allocated = false;
                s->data = NULL;
            }
            return; 
        }

        if (needs_realloc) {
            if (s->is_allocated && s->data) {
                heap_caps_free(s->data);
            }
            size_t bytes = (size_t)element->w * (size_t)element->h * 2;
            s->data = (uint16_t*)heap_caps_malloc(bytes, 0);
            s->is_allocated = (s->data != NULL);
            s->w = element->w;
            s->h = element->h;
            s->x = element->x;
            s->y = element->y;
            
            if (s->is_allocated) {
                memset(s->data, 0, bytes);
            }
        }
        return; 
    }

    // 2. Обработка StackPanel и ListBox
    if (element->type == UI_TYPE_STACK_PANEL || element->type == UI_TYPE_LIST_BOX) {
        
        if (element->type == UI_TYPE_LIST_BOX) {
            // --- ЛОГИКА LISTBOX ---
            uint16_t font_h = (current_font != NULL) ? current_font->char_height : 16;
            uint16_t item_h = font_h + 6; 
            
            uint16_t list_h = element->h; 

            if (list_h == 0 && element->props.list_box.height_mode == UI_LISTBOX_HEIGHT_AUTO) {
                list_h = 120; 
            }

            if (available_h > 0 && list_h > available_h) {
                list_h = available_h;
            }
            
            if (list_h == 0) list_h = 120;
            
            element->h = list_h;

            uint8_t visible_count = (list_h + item_h - 1) / item_h;
            if (visible_count == 0) visible_count = 1;
            
            uint8_t max_offset = (element->children_count > visible_count) ? (element->children_count - visible_count) : 0;
            uint16_t scrollbar_width = (max_offset > 0) ? 10 : 0;
            
            for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
                UIElement_t* child = (UIElement_t*)element->children[i];
                if (!child) continue;

                if (child->sprite == NULL && element->sprite != NULL) {
                    child->sprite = element->sprite;
                }

                int16_t relative_index = i - element->props.list_box.scroll_offset;
                
                if (relative_index >= 0 && relative_index < (int16_t)visible_count) {
                    int16_t item_y = element->y + (int16_t)((i - element->props.list_box.scroll_offset) * item_h);
                    uint16_t child_w = element->w - scrollbar_width;
                    
                    child->x = element->x;
                    child->y = item_y;
                    child->w = child_w; 
                    child->h = item_h;
                    
                    UI_MeasureAndArrange(child, child->x, child->y, child->w, child->h);
                } else {
                    child->w = 0;
                    child->h = 0;
                    UI_MeasureAndArrange(child, element->x + element->w + 1000, element->y + element->h + 1000, 0, 0);
                }
            }
            return; 
        }

        // --- ЛОГИКА STACK_PANEL ---
        // StackPanel обычно растягивается на всю доступную ширину и занимает оставшуюся высоту
        uint16_t fixed_height_sum = 0;
        uint16_t spacing_total = (element->children_count > 1) ? (uint16_t)(element->children_count - 1) * element->props.stack.spacing : 0;
        uint32_t total_weight = 0;
        uint8_t dynamic_count = 0;

        for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (!child) continue;

            if (child->h > 0) {
                fixed_height_sum += child->h;
            } else {
                dynamic_count++;
                total_weight += (child->layout_weight > 0) ? child->layout_weight : 1;
            }
        }

        int32_t remaining_h = (int32_t)element->h - (int32_t)fixed_height_sum - (int32_t)spacing_total;
        if (remaining_h < 0) remaining_h = 0;

        uint16_t cur_y = element->y;
        uint8_t first_dynamic_processed = 0; 

        for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (!child) continue;

            uint16_t child_h = 0;
            
            if (child->h > 0) {
                child_h = child->h;
            } else if (dynamic_count > 0 && total_weight > 0 && remaining_h > 0) {
                uint32_t weight = (child->layout_weight > 0) ? child->layout_weight : 1;
                child_h = (uint16_t)((remaining_h * weight) / total_weight);
                
                uint16_t min_h = (current_font != NULL) ? current_font->char_height : 12;
                if (child_h < min_h) child_h = min_h;

                if (!first_dynamic_processed) {
                    first_dynamic_processed = 1;
                }
            }

            // Важно: StackPanel элементы обычно занимают всю ширину родителя
            UI_MeasureAndArrange(child, element->x, cur_y, element->w, child_h);
            
            cur_y += child_h + element->props.stack.spacing;
        }
        return;
    }


    // 3. Обработка Grid
    if (element->type == UI_TYPE_GRID) {
        GridDefinition_t* grid = &element->props.grid;
        
        if (grid->rows_count > MAX_GRID_ROWS) grid->rows_count = MAX_GRID_ROWS;
        if (grid->cols_count > MAX_GRID_COLS) grid->cols_count = MAX_GRID_COLS;

        uint16_t row_heights[MAX_GRID_ROWS] = {0};
        uint16_t col_widths[MAX_GRID_COLS] = {0};

        uint16_t total_pixel_rows = 0;
        uint16_t total_pixel_cols = 0;

        // --- ШАГ 1: Фиксированные (пиксельные) размеры ---
        for (uint8_t r = 0; r < grid->rows_count; r++) {
            if (grid->row_is_pixel[r]) {
                row_heights[r] = grid->row_definitions[r];
                total_pixel_rows += row_heights[r];
            }
        }
        for (uint8_t c = 0; c < grid->cols_count; c++) {
            if (grid->col_is_pixel[c]) {
                col_widths[c] = grid->col_definitions[c];
                total_pixel_cols += col_widths[c];
            }
        }

        // --- ШАГ 2: Коррекция переполнения ---
        if (total_pixel_rows > element->h && element->h > 0) {
            float scale = (float)element->h / (float)total_pixel_rows;
            total_pixel_rows = 0;
            for (uint8_t r = 0; r < grid->rows_count; r++) {
                if (grid->row_is_pixel[r]) {
                    row_heights[r] = (uint16_t)((float)row_heights[r] * scale);
                    total_pixel_rows += row_heights[r];
                }
            }
        }
        if (total_pixel_cols > element->w && element->w > 0) {
            float scale = (float)element->w / (float)total_pixel_cols;
            total_pixel_cols = 0;
            for (uint8_t c = 0; c < grid->cols_count; c++) {
                if (grid->col_is_pixel[c]) {
                    col_widths[c] = (uint16_t)((float)col_widths[c] * scale);
                    total_pixel_cols += col_widths[c];
                }
            }
        }

        // --- ШАГ 3: Расчет свободного пространства ---
        uint16_t remaining_h = (element->h > total_pixel_rows) ? (element->h - total_pixel_rows) : 0;
        uint16_t remaining_w = (element->w > total_pixel_cols) ? (element->w - total_pixel_cols) : 0;

        uint32_t percent_sum_h = 0, weight_sum_h = 0;
        uint32_t percent_sum_w = 0, weight_sum_w = 0;
        uint8_t dynamic_rows = 0, dynamic_cols = 0;

        for (uint8_t r = 0; r < grid->rows_count; r++) {
            if (!grid->row_is_pixel[r]) {
                percent_sum_h += grid->row_definitions[r];
                weight_sum_h += (grid->row_weights[r] > 0) ? grid->row_weights[r] : 1;
                dynamic_rows++;
            }
        }
        for (uint8_t c = 0; c < grid->cols_count; c++) {
            if (!grid->col_is_pixel[c]) {
                percent_sum_w += grid->col_definitions[c];
                weight_sum_w += (grid->col_weights[c] > 0) ? grid->col_weights[c] : 1;
                dynamic_cols++;
            }
        }

        // --- ШАГ 4: Распределение оставшейся высоты ---
        if (remaining_h > 0 && dynamic_rows > 0) {
            uint16_t current_allocated = 0;
            for (uint8_t r = 0; r < grid->rows_count; r++) {
                if (!grid->row_is_pixel[r]) {
                    uint16_t size = 0;
                    if (percent_sum_h > 0 && grid->row_definitions[r] > 0) {
                        size = (uint16_t)((uint32_t)remaining_h * grid->row_definitions[r] / percent_sum_h);
                    } else if (weight_sum_h > 0) {
                        size = (uint16_t)((uint32_t)remaining_h * (grid->row_weights[r] > 0 ? grid->row_weights[r] : 1) / weight_sum_h);
                    } else {
                        size = remaining_h / dynamic_rows;
                    }
                    
                    if (current_allocated + size > remaining_h) size = remaining_h - current_allocated;
                    
                    row_heights[r] = size;
                    current_allocated += size;
                }
            }
            if (current_allocated < remaining_h) {
                uint16_t remainder = remaining_h - current_allocated;
                for (uint8_t r = 0; r < grid->rows_count; r++) {
                    if (!grid->row_is_pixel[r]) {
                        row_heights[r] += remainder;
                        break; 
                    }
                }
            }
        }

        // --- ШАГ 5: Расчет размеров гибких колонок ---
        if (remaining_w > 0 && dynamic_cols > 0) {
            uint16_t current_allocated = 0;
            
            for (uint8_t c = 0; c < grid->cols_count; c++) {
                if (!grid->col_is_pixel[c]) {
                    uint16_t size = 0;
                    if (percent_sum_w > 0 && grid->col_definitions[c] > 0) {
                        size = (uint16_t)((uint32_t)remaining_w * grid->col_definitions[c] / percent_sum_w);
                    } else if (weight_sum_w > 0 && dynamic_cols > 0) {
                        size = (uint16_t)((uint32_t)remaining_w * (grid->col_weights[c] > 0 ? grid->col_weights[c] : 1) / weight_sum_w);
                    } else {
                        size = remaining_w / dynamic_cols;
                    }
                    
                    if (current_allocated + size > remaining_w) {
                        size = remaining_w - current_allocated;
                    }
                    
                    col_widths[c] = size;
                    current_allocated += size;
                }
            }
            
            if (current_allocated < remaining_w) {
                uint16_t remainder = remaining_w - current_allocated;
                for (uint8_t c = 0; c < grid->cols_count; c++) {
                    if (!grid->col_is_pixel[c]) {
                        col_widths[c] += remainder;
                        break;
                    }
                }
            }
        }


        // --- ШАГ 6: Присвоение координат и размеров детям ---
        for (uint8_t i = 0; i < element->children_count && i < MAX_GRID_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (!child) continue;

            // Проверка границ индексов ячейки
            if (child->grid_row >= grid->rows_count || child->grid_col >= grid->cols_count) continue;

            int16_t cell_x = element->x;
            int16_t cell_y = element->y;

            // Суммируем ширины колонок слева
            for (uint8_t c = 0; c < child->grid_col; c++) {
                cell_x += col_widths[c];
            }
            // Суммируем высоты строк сверху
            for (uint8_t r = 0; r < child->grid_row; r++) {
                cell_y += row_heights[r];
            }

            uint16_t cell_w = col_widths[child->grid_col];
            uint16_t cell_h = row_heights[child->grid_row];

            // Минимальный размер 1x1, чтобы не передать 0 в дочерний элемент
            if (cell_w < 1) cell_w = 1;
            if (cell_h < 1) cell_h = 1;

            // Вызываем рекурсивный обмер
            UI_MeasureAndArrange(child, cell_x, cell_y, cell_w, cell_h);
        }
    }
}
 */
 void UI_MeasureAndArrange(UIElement_t* element, int16_t parent_x, int16_t parent_y, uint16_t available_w, uint16_t available_h) {
    if (!element) return;

    // Базовые координаты
    uint16_t previous_h = element->h;

    element->x = parent_x; 
    element->y = parent_y;
    element->w = available_w; 
    element->h = available_h;

    // ШАГ 1 & 2: Выделение памяти (Leafs / Containers)
    if (element->children_count == 0 && element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        bool owns_own_buffer = (s != &main_screen_sprite);
        if (owns_own_buffer) {
            s->x = element->x; s->y = element->y;
            s->w = element->w; s->h = element->h;
            if (s->data == NULL || s->w != element->w || s->h != element->h || !s->is_allocated) {
                if (s->data != NULL && s->is_allocated) {
                    heap_caps_free(s->data);
                }
                s->data = NULL;
                s->is_allocated = false;
                // Защита: не выделяем при нулевой ширине/высоте
                if (s->w == 0 || s->h == 0) return;
                size_t bytes = (size_t)s->w * (size_t)s->h * 2u;
                s->data = (uint16_t*)heap_caps_malloc(bytes, 0);
                s->is_allocated = (s->data != NULL);
                // Не делать бесконечный цикл при ошибке аллокации — просто вернуться
                if (!s->is_allocated) {
                    return;
                }
            }
        }
        return; 
    }

    if (element->type == UI_TYPE_STACK_PANEL || element->type == UI_TYPE_LIST_BOX) {
        if (element->sprite != NULL) {
            Sprite_t* s = element->sprite;
            s->x = element->x; s->y = element->y;

            // ListBox и кнопки/текст используют общий спрайт панели digits_node.
            // Поэтому ListBox не должен менять размеры этого спрайта под себя — иначе
            // весь стек элементов начинает рисоваться с неверной геометрией в альбомной ориентации.
            bool should_resize_shared_sprite = (s != &main_screen_sprite) || (element->type == UI_TYPE_STACK_PANEL);
            if (should_resize_shared_sprite) {
                s->w = element->w; s->h = element->h;
                if (s->data == NULL || s->w != element->w || s->h != element->h || !s->is_allocated) {
                    if (s->data != NULL && s->is_allocated) {
                        heap_caps_free(s->data);
                    }
                    s->data = NULL;
                    s->is_allocated = false;
                    // Защита от нулевых размеров
                    if (s->w == 0 || s->h == 0) return;
                    size_t bytes = (size_t)s->w * (size_t)s->h * 2u;
                    s->data = (uint16_t*)heap_caps_malloc(bytes, 0);
                    s->is_allocated = (s->data != NULL);
                    if (!s->is_allocated) {
                        return;
                    }
                }
            }
        }
    }

    // ШАГ 3: Математика StackPanel (фиксированные размеры сохраняются,
    // элементы без явной высоты делят остаток автоматически по весу)
     if (element->type == UI_TYPE_STACK_PANEL) {
        uint16_t fixed_height_sum = 0;
        uint8_t dynamic_count = 0;
        uint16_t total_spacing = 0;
        uint32_t total_weight = 0;

        for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (!child) continue;

            if (child->h > 0) {
                fixed_height_sum += child->h;
            } else {
                dynamic_count++;
                total_weight += (child->layout_weight > 0) ? child->layout_weight : 1;
            }
        }

        if (element->children_count > 1) {
            total_spacing = (uint16_t)(element->children_count - 1) * element->props.stack.spacing;
        }

        int32_t remaining_h = (int32_t)element->h - (int32_t)fixed_height_sum - (int32_t)total_spacing;
        if (remaining_h < 0) remaining_h = 0;

        uint16_t default_font_h = (current_font != NULL) ? current_font->char_height : 16;
        uint16_t min_dynamic_h = (default_font_h > 16) ? default_font_h : 16;

        int16_t cur_x = element->x;
        int16_t cur_y = element->y;
        int32_t remaining_for_dynamic = remaining_h;
        uint8_t remaining_dynamic_count = dynamic_count;

        for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (!child) continue;

            uint16_t child_h = 0;
            if (child->h > 0) {
                child_h = child->h;
            } else if (remaining_dynamic_count == 0) {
                child_h = 0;
            } else {
                uint32_t weight = (child->layout_weight > 0) ? child->layout_weight : 1;
                uint32_t share = (total_weight > 0) ? ((remaining_for_dynamic * weight) / total_weight) : 0;
                if (share < min_dynamic_h && remaining_for_dynamic > 0) {
                    share = (remaining_for_dynamic < min_dynamic_h) ? (uint32_t)remaining_for_dynamic : min_dynamic_h;
                }
                if (share > (uint32_t)remaining_for_dynamic) share = (uint32_t)remaining_for_dynamic;
                child_h = (uint16_t)share;
                remaining_dynamic_count--;
            }

            if (child_h > remaining_for_dynamic) {
                child_h = (uint16_t)remaining_for_dynamic;
            }

            UI_MeasureAndArrange(child, cur_x, cur_y, element->w, child_h);
            cur_y += child_h + element->props.stack.spacing;
            if (child->h == 0) {
                remaining_for_dynamic -= child_h;
            }
        }
    } 
   
     // ШАГ 4: Математика ListBox (с учетом скролла и резерва под скроллбар)
    if (element->type == UI_TYPE_LIST_BOX) {
        uint16_t font_h = (current_font != NULL) ? current_font->char_height : 16;
        uint16_t item_h = font_h + 6;
        uint16_t parent_h = element->h;

        // Высота ListBox должна быть привязана к высоте, которую выделил родитель.
        // Если родитель не дал высоту, берём либо явную pixel_height, либо размер по
        // количеству видимых строк. Это предотвращает нежелательное сжатие при скролле.
        uint16_t list_h = parent_h;
        if (element->props.list_box.height_mode == UI_LISTBOX_HEIGHT_FIXED) {
            if (element->props.list_box.pixel_height > 0) {
                list_h = element->props.list_box.pixel_height;
            } else if (list_h == 0) {
                list_h = (element->props.list_box.visible_row_count > 0 ? element->props.list_box.visible_row_count : 4) * item_h;
            }
        } else {
            if (list_h == 0) {
                if (element->props.list_box.pixel_height > 0) {
                    list_h = element->props.list_box.pixel_height;
                } else {
                    list_h = (element->props.list_box.visible_row_count > 0 ? element->props.list_box.visible_row_count : 4) * item_h;
                }
            }
        }

        if (list_h == 0) {
            if (previous_h > 0) {
                list_h = previous_h;
            } else {
                list_h = 120;
            }
        }
        if (parent_h > 0 && list_h > parent_h) list_h = parent_h;
        
        // Ограничиваем высоту ListBox доступным пространством родителя
        uint8_t start = element->props.list_box.scroll_offset;
        uint8_t visible = (list_h + item_h - 1) / item_h;
        if (visible == 0) visible = 1;

        // Ширина скроллбара (например, 10 пикселей). 
        // Если элементов меньше, чем влазит в экран, скроллбар не нужен — отступ 0
        uint16_t scrollbar_width = (element->children_count > visible) ? 10 : 0;
        uint16_t item_w = element->w - scrollbar_width;
        if (item_w > element->w) item_w = element->w;
        if (item_w < 8) item_w = (element->w > 8) ? (element->w - 8) : element->w;

        for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (!child) continue;

            if (i >= start && i < (start + visible)) {
                int16_t item_y = element->y + (int16_t)((i - start) * item_h);
                UI_MeasureAndArrange(child, element->x, item_y, item_w, item_h);
            } else {
                UI_MeasureAndArrange(child, element->x, element->y + element->h + 16, 0, 0);
                child->w = 0;
                child->h = 0;
            }
        }

        // 🔥 ДОБАВЛЕНО: явно фиксируем размер ListBox по высоте контейнера (но не больше, чем доступно)
        element->h = list_h;
        element->w = element->w; // оставляем ширину как есть
    }
    
    // ЕСЛИ ЭТО GRID
        if (element->type == UI_TYPE_GRID) {
        GridDefinition_t* grid = &element->props.grid;
        uint16_t row_heights[MAX_GRID_CHILDREN] = {0};
        uint16_t col_widths[MAX_GRID_CHILDREN] = {0};

        uint8_t pixel_rows = 0, pixel_cols = 0;
        uint16_t total_pixel_rows = 0, total_pixel_cols = 0;

        // --- ШАГ 1: Разбираем фиксированные (пиксельные) размеры ---
        for (uint8_t r = 0; r < grid->rows_count && r < MAX_GRID_CHILDREN; r++) {
            if (grid->row_is_pixel[r]) {
                row_heights[r] = grid->row_definitions[r];
                total_pixel_rows += row_heights[r];
                pixel_rows++;
            } else {
                row_heights[r] = 0;
            }
        }
        for (uint8_t c = 0; c < grid->cols_count && c < MAX_GRID_CHILDREN; c++) {
            if (grid->col_is_pixel[c]) {
                col_widths[c] = grid->col_definitions[c];
                total_pixel_cols += col_widths[c];
                pixel_cols++;
            } else {
                col_widths[c] = 0;
            }
        }

        // --- ШАГ 2: Остаток распределяем по процентам и весам ---
        uint16_t remaining_w = (element->w > total_pixel_cols) ? (element->w - total_pixel_cols) : 1;
        uint16_t remaining_h = (element->h > total_pixel_rows) ? (element->h - total_pixel_rows) : 1;

        uint32_t percent_sum_h = 0, percent_sum_w = 0;
        uint32_t weight_sum_h = 0, weight_sum_w = 0;
        for (uint8_t r = 0; r < grid->rows_count && r < MAX_GRID_CHILDREN; r++) {
            if (!grid->row_is_pixel[r]) {
                percent_sum_h += grid->row_definitions[r];
                weight_sum_h += (grid->row_weights[r] > 0) ? grid->row_weights[r] : 1;
            }
        }
        for (uint8_t c = 0; c < grid->cols_count && c < MAX_GRID_CHILDREN; c++) {
            if (!grid->col_is_pixel[c]) {
                percent_sum_w += grid->col_definitions[c];
                weight_sum_w += (grid->col_weights[c] > 0) ? grid->col_weights[c] : 1;
            }
        }

        for (uint8_t r = 0; r < grid->rows_count && r < MAX_GRID_CHILDREN; r++) {
            if (!grid->row_is_pixel[r]) {
                if (percent_sum_h > 0 && grid->row_definitions[r] > 0) {
                    row_heights[r] = (uint16_t)((uint32_t)remaining_h * grid->row_definitions[r] / percent_sum_h);
                } else if (weight_sum_h > 0) {
                    row_heights[r] = (uint16_t)((uint32_t)remaining_h * (grid->row_weights[r] > 0 ? grid->row_weights[r] : 1) / weight_sum_h);
                } else {
                    row_heights[r] = remaining_h / (grid->rows_count - pixel_rows);
                }
            }
        }
        for (uint8_t c = 0; c < grid->cols_count && c < MAX_GRID_CHILDREN; c++) {
            if (!grid->col_is_pixel[c]) {
                if (percent_sum_w > 0 && grid->col_definitions[c] > 0) {
                    col_widths[c] = (uint16_t)((uint32_t)remaining_w * grid->col_definitions[c] / percent_sum_w);
                } else if (weight_sum_w > 0) {
                    col_widths[c] = (uint16_t)((uint32_t)remaining_w * (grid->col_weights[c] > 0 ? grid->col_weights[c] : 1) / weight_sum_w);
                } else {
                    col_widths[c] = remaining_w / (grid->cols_count - pixel_cols);
                }
            }
        }

        // --- Проверка на переполнение (если пиксели > доступной ширины/высоты) ---
        if (total_pixel_cols > element->w) {
            for (uint8_t c = 0; c < grid->cols_count; c++) {
                if (!grid->col_is_pixel[c]) {
                    col_widths[c] = 0;
                }
            }
        }
        if (total_pixel_rows > element->h) {
            for (uint8_t r = 0; r < grid->rows_count; r++) {
                if (!grid->row_is_pixel[r]) {
                    row_heights[r] = 0;
                }
            }
        }

        // --- ГЕНЕРАЦИЯ ЯЧЕЕК ---
        for (uint8_t i = 0; i < element->children_count && i < MAX_GRID_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (!child) continue;

            int16_t cell_x = element->x;
            int16_t cell_y = element->y;

            for (uint8_t c = 0; c < child->grid_col && c < MAX_GRID_CHILDREN; c++) {
                cell_x += col_widths[c];
            }
            for (uint8_t r = 0; r < child->grid_row && r < MAX_GRID_CHILDREN; r++) {
                cell_y += row_heights[r];
            }

            uint16_t cell_w = col_widths[child->grid_col];
            uint16_t cell_h = row_heights[child->grid_row];

            if (cell_w == 0) cell_w = 1;
            if (cell_h == 0) cell_h = 1;

            UI_MeasureAndArrange(child, cell_x, cell_y, cell_w, cell_h);
        }
    }
}  


/**
 * @brief Рекурсивно вычисляет размеры и координаты элементов UI (Measure & Pass)
 * 
 * Версия, основанная на стабильной логике обработки StackPanel и ListBox,
 * с защитой от null-указателей и деления на ноль.
 */
/* void UI_MeasureAndArrange(UIElement_t* element, int16_t parent_x, int16_t parent_y, uint16_t available_w, uint16_t available_h) {
    if (!element) return;

    // 1. Установка базовых координат и доступных размеров
    element->x = parent_x; 
    element->y = parent_y;
    element->w = available_w; 
    element->h = available_h;

    // --- ЛОГИКА ДЛЯ КОНТЕЙНЕРОВ С ОБЩИМ СПРАЙТОМ ---
    // Если это StackPanel или ListBox, которые используют общий спрайт (например, main_screen_sprite),
    // нам нужно убедиться, что размеры спрайта соответствуют элементам, если это первый проход или размер изменился.
    
    if (element->children_count > 0) {
        if (element->sprite != NULL) {
            Sprite_t* s = element->sprite;
            // Обновляем координаты спрайта (важно для dirty rect)
            if (s->w != element->w || s->h != element->h) {
                s->w = element->w;
                s->h = element->h;
            }
            s->x = element->x;
            s->y = element->y;
        }
    }

    // Если у элемента нет детей и есть спрайт — аллоцируем/чистим его
    if (element->children_count == 0 && element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        
        bool needs_realloc = (s->data == NULL || s->w != element->w || s->h != element->h || !s->is_allocated);
        
        if (element->w == 0 || element->h == 0) {
            if (s->is_allocated && s->data) {
                heap_caps_free(s->data);
                s->is_allocated = false;
                s->data = NULL;
            }
            return; 
        }

        if (needs_realloc) {
            if (s->is_allocated && s->data) {
                heap_caps_free(s->data);
            }
            size_t bytes = (size_t)element->w * (size_t)element->h * 2;
            s->data = (uint16_t*)heap_caps_malloc(bytes, 0);
            s->is_allocated = (s->data != NULL);
            s->w = element->w;
            s->h = element->h;
            s->x = element->x;
            s->y = element->y;
            
            if (s->is_allocated) {
                memset(s->data, 0, bytes);
            }
        }
        return; 
    }

    // 2. Обработка StackPanel и ListBox
    if (element->type == UI_TYPE_STACK_PANEL || element->type == UI_TYPE_LIST_BOX) {
        
        if (element->type == UI_TYPE_LIST_BOX) {
            // --- ЛОГИКА LISTBOX ---
            uint16_t font_h = (current_font != NULL) ? current_font->char_height : 16;
            uint16_t item_h = font_h + 6; 
            
            uint16_t list_h = element->h; 

            if (list_h == 0 && element->props.list_box.height_mode == UI_LISTBOX_HEIGHT_AUTO) {
                list_h = 120; 
            }

            if (available_h > 0 && list_h > available_h) {
                list_h = available_h;
            }
            
            if (list_h == 0) list_h = 120;
            
            element->h = list_h;

            uint8_t visible_count = (list_h + item_h - 1) / item_h;
            if (visible_count == 0) visible_count = 1;
            
            uint8_t max_offset = (element->children_count > visible_count) ? (element->children_count - visible_count) : 0;
            uint16_t scrollbar_width = (max_offset > 0) ? 10 : 0;
            
            for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
                UIElement_t* child = (UIElement_t*)element->children[i];
                if (!child) continue;

                if (child->sprite == NULL && element->sprite != NULL) {
                    child->sprite = element->sprite;
                }

                int16_t relative_index = i - element->props.list_box.scroll_offset;
                
                if (relative_index >= 0 && relative_index < (int16_t)visible_count) {
                    int16_t item_y = element->y + (int16_t)((i - element->props.list_box.scroll_offset) * item_h);
                    uint16_t child_w = element->w - scrollbar_width;
                    
                    child->x = element->x;
                    child->y = item_y;
                    child->w = child_w; 
                    child->h = item_h;
                    
                    UI_MeasureAndArrange(child, child->x, child->y, child->w, child->h);
                } else {
                    child->w = 0;
                    child->h = 0;
                    UI_MeasureAndArrange(child, element->x + element->w + 1000, element->y + element->h + 1000, 0, 0);
                }
            }
            return; 
        }

        // --- ЛОГИКА STACK_PANEL ---
        // StackPanel обычно растягивается на всю доступную ширину и занимает оставшуюся высоту
        uint16_t fixed_height_sum = 0;
        uint16_t spacing_total = (element->children_count > 1) ? (uint16_t)(element->children_count - 1) * element->props.stack.spacing : 0;
        uint32_t total_weight = 0;
        uint8_t dynamic_count = 0;

        for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (!child) continue;

            if (child->h > 0) {
                fixed_height_sum += child->h;
            } else {
                dynamic_count++;
                total_weight += (child->layout_weight > 0) ? child->layout_weight : 1;
            }
        }

        int32_t remaining_h = (int32_t)element->h - (int32_t)fixed_height_sum - (int32_t)spacing_total;
        if (remaining_h < 0) remaining_h = 0;

        uint16_t cur_y = element->y;
        uint8_t first_dynamic_processed = 0; 

        for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (!child) continue;

            uint16_t child_h = 0;
            
            if (child->h > 0) {
                child_h = child->h;
            } else if (dynamic_count > 0 && total_weight > 0 && remaining_h > 0) {
                uint32_t weight = (child->layout_weight > 0) ? child->layout_weight : 1;
                child_h = (uint16_t)((remaining_h * weight) / total_weight);
                
                uint16_t min_h = (current_font != NULL) ? current_font->char_height : 12;
                if (child_h < min_h) child_h = min_h;

                if (!first_dynamic_processed) {
                    first_dynamic_processed = 1;
                }
            }

            // Важно: StackPanel элементы обычно занимают всю ширину родителя
            UI_MeasureAndArrange(child, element->x, cur_y, element->w, child_h);
            
            cur_y += child_h + element->props.stack.spacing;
        }
        return;
    }


    // 3. Обработка Grid
    if (element->type == UI_TYPE_GRID) {
        GridDefinition_t* grid = &element->props.grid;
        
        if (grid->rows_count > MAX_GRID_ROWS) grid->rows_count = MAX_GRID_ROWS;
        if (grid->cols_count > MAX_GRID_COLS) grid->cols_count = MAX_GRID_COLS;

        uint16_t row_heights[MAX_GRID_ROWS] = {0};
        uint16_t col_widths[MAX_GRID_COLS] = {0};

        uint16_t total_pixel_rows = 0;
        uint16_t total_pixel_cols = 0;

        // --- ШАГ 1: Фиксированные (пиксельные) размеры ---
        for (uint8_t r = 0; r < grid->rows_count; r++) {
            if (grid->row_is_pixel[r]) {
                row_heights[r] = grid->row_definitions[r];
                total_pixel_rows += row_heights[r];
            }
        }
        for (uint8_t c = 0; c < grid->cols_count; c++) {
            if (grid->col_is_pixel[c]) {
                col_widths[c] = grid->col_definitions[c];
                total_pixel_cols += col_widths[c];
            }
        }

        // --- ШАГ 2: Коррекция переполнения ---
        if (total_pixel_rows > element->h && element->h > 0) {
            float scale = (float)element->h / (float)total_pixel_rows;
            total_pixel_rows = 0;
            for (uint8_t r = 0; r < grid->rows_count; r++) {
                if (grid->row_is_pixel[r]) {
                    row_heights[r] = (uint16_t)((float)row_heights[r] * scale);
                    total_pixel_rows += row_heights[r];
                }
            }
        }
        if (total_pixel_cols > element->w && element->w > 0) {
            float scale = (float)element->w / (float)total_pixel_cols;
            total_pixel_cols = 0;
            for (uint8_t c = 0; c < grid->cols_count; c++) {
                if (grid->col_is_pixel[c]) {
                    col_widths[c] = (uint16_t)((float)col_widths[c] * scale);
                    total_pixel_cols += col_widths[c];
                }
            }
        }

        // --- ШАГ 3: Расчет свободного пространства ---
        uint16_t remaining_h = (element->h > total_pixel_rows) ? (element->h - total_pixel_rows) : 0;
        uint16_t remaining_w = (element->w > total_pixel_cols) ? (element->w - total_pixel_cols) : 0;

        uint32_t percent_sum_h = 0, weight_sum_h = 0;
        uint32_t percent_sum_w = 0, weight_sum_w = 0;
        uint8_t dynamic_rows = 0, dynamic_cols = 0;

        for (uint8_t r = 0; r < grid->rows_count; r++) {
            if (!grid->row_is_pixel[r]) {
                percent_sum_h += grid->row_definitions[r];
                weight_sum_h += (grid->row_weights[r] > 0) ? grid->row_weights[r] : 1;
                dynamic_rows++;
            }
        }
        for (uint8_t c = 0; c < grid->cols_count; c++) {
            if (!grid->col_is_pixel[c]) {
                percent_sum_w += grid->col_definitions[c];
                weight_sum_w += (grid->col_weights[c] > 0) ? grid->col_weights[c] : 1;
                dynamic_cols++;
            }
        }

        // --- ШАГ 4: Распределение оставшейся высоты ---
        if (remaining_h > 0 && dynamic_rows > 0) {
            uint16_t current_allocated = 0;
            for (uint8_t r = 0; r < grid->rows_count; r++) {
                if (!grid->row_is_pixel[r]) {
                    uint16_t size = 0;
                    if (percent_sum_h > 0 && grid->row_definitions[r] > 0) {
                        size = (uint16_t)((uint32_t)remaining_h * grid->row_definitions[r] / percent_sum_h);
                    } else if (weight_sum_h > 0) {
                        size = (uint16_t)((uint32_t)remaining_h * (grid->row_weights[r] > 0 ? grid->row_weights[r] : 1) / weight_sum_h);
                    } else {
                        size = remaining_h / dynamic_rows;
                    }
                    
                    if (current_allocated + size > remaining_h) size = remaining_h - current_allocated;
                    
                    row_heights[r] = size;
                    current_allocated += size;
                }
            }
            if (current_allocated < remaining_h) {
                uint16_t remainder = remaining_h - current_allocated;
                for (uint8_t r = 0; r < grid->rows_count; r++) {
                    if (!grid->row_is_pixel[r]) {
                        row_heights[r] += remainder;
                        break; 
                    }
                }
            }
        }

        // --- ШАГ 5: Расчет размеров гибких колонок ---
        if (remaining_w > 0 && dynamic_cols > 0) {
            uint16_t current_allocated = 0;
            
            for (uint8_t c = 0; c < grid->cols_count; c++) {
                if (!grid->col_is_pixel[c]) {
                    uint16_t size = 0;
                    if (percent_sum_w > 0 && grid->col_definitions[c] > 0) {
                        size = (uint16_t)((uint32_t)remaining_w * grid->col_definitions[c] / percent_sum_w);
                    } else if (weight_sum_w > 0 && dynamic_cols > 0) {
                        size = (uint16_t)((uint32_t)remaining_w * (grid->col_weights[c] > 0 ? grid->col_weights[c] : 1) / weight_sum_w);
                    } else {
                        size = remaining_w / dynamic_cols;
                    }
                    
                    if (current_allocated + size > remaining_w) {
                        size = remaining_w - current_allocated;
                    }
                    
                    col_widths[c] = size;
                    current_allocated += size;
                }
            }
            
            if (current_allocated < remaining_w) {
                uint16_t remainder = remaining_w - current_allocated;
                for (uint8_t c = 0; c < grid->cols_count; c++) {
                    if (!grid->col_is_pixel[c]) {
                        col_widths[c] += remainder;
                        break;
                    }
                }
            }
        }

        
        // --- ШАГ 6: Присвоение координат и размеров детям ---
        for (uint8_t i = 0; i < element->children_count && i < MAX_GRID_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (!child) continue;

            // Проверка границ индексов ячейки
            if (child->grid_row >= grid->rows_count || child->grid_col >= grid->cols_count) continue;

            int16_t cell_x = element->x;
            int16_t cell_y = element->y;

            // Суммируем ширины колонок слева
            for (uint8_t c = 0; c < child->grid_col; c++) {
                cell_x += col_widths[c];
            }
            // Суммируем высоты строк сверху
            for (uint8_t r = 0; r < child->grid_row; r++) {
                cell_y += row_heights[r];
            }

            uint16_t cell_w = col_widths[child->grid_col];
            uint16_t cell_h = row_heights[child->grid_row];

            // Минимальный размер 1x1, чтобы не передать 0 в дочерний элемент
            if (cell_w < 1) cell_w = 1;
            if (cell_h < 1) cell_h = 1;

            // Вызываем рекурсивный обмер
            UI_MeasureAndArrange(child, cell_x, cell_y, cell_w, cell_h);
        }
    }
} */
/**
 * @brief Вспомогательная функция для отрисовки одиночного дочернего элемента
 *        Обеспечивает инкапсуляцию и защиту от дублирования кода в UI_DrawTree
 */
static void UI_RenderChildElement(void* child_ptr) {
    UIElement_t* child = (UIElement_t*)child_ptr;
    
    // 1. Строгая защита от нулевого указателя (HardFault protection)
    if (!child) return; 

    // 2. Если у элемента назначен кастомный колбэк — он имеет наивысший приоритет
    if (child->render_callback != NULL) { child->render_callback(child); } 
    // 3. Если колбэка нет — отрисовываем стандартными средствами движка по его типу
    else {
        switch (child->type) {
            case UI_TYPE_TEXT_BLOCK: 
                Draw_GeneralText_Callback(child); 
                break;
                
            case UI_TYPE_BUTTON:     
                UI_RenderButton(child);           
                break;
                
            case UI_TYPE_LIST_BOX:   
                UI_RenderListBox(child); // Отрисовка встроенного списка (фон, рамка, скроллбар)
                break;
                
            case UI_TYPE_BORDER:     
                UI_RenderBorder(child);           
                break;
                
            case UI_TYPE_CHECK_BOX:
            case UI_TYPE_RADIO_BUTTON: 
                UI_RenderToggle(child);           
                break;
                
            default: 
                // Неизвестные типы или чистые контейнеры без контента просто пропускаем
                break;
        }
    }
}

void UI_DrawTree(UIElement_t* element) {
    if (!element) return;

    // ШАГ 1: Отрисовка физического окна (Спрайт или контейнер со спрайтом)
    if (element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        
        // ВОЗВРАЩАЕМ ОПТИМИЗАЦИЮ: шлем данные, только если спрайт "загрязнен"
        if (s->is_allocated && s->data != NULL && s->needs_render) {
            
            // Если координаты грязной зоны схлопнулись — раскрываем на весь размер (защита)
            if (s->dirty_x2 == 0 && s->dirty_y2 == 0) {
                s->dirty_x1 = 0; s->dirty_y1 = 0;
                s->dirty_x2 = s->w - 1; s->dirty_y2 = s->h - 1;
            }

            if (element->render_callback != NULL) {
                element->render_callback(element);
            } else {
                // АВТОМАТИЧЕСКИЙ РАЗВОД КОМПОНЕНТОВ ДВИЖКА
                switch (element->type) {
                    case UI_TYPE_TEXT_BLOCK:
                        Draw_GeneralText_Callback(element); 
                        break;
                    case UI_TYPE_BORDER:
                        UI_RenderBorder(element);
                        break;
                    case UI_TYPE_BUTTON:
                        UI_RenderButton(element);
                        break;
                    case UI_TYPE_CHECK_BOX:
                    case UI_TYPE_RADIO_BUTTON:
                        UI_RenderToggle(element);
                    
                        break;
                    case UI_TYPE_LIST_BOX:
                        UI_RenderListBox(element); // Отрисовка фона/рамки [1.1]
                        break;

                    // Если это контейнеры, и они затребовали рендер,
                    // принудительно заставляем всех их детей перерисовать себя в ОЗУ!
                    case UI_TYPE_GRID:
                        // Для GRID лимит 8 СТРОГИЙ, так как массивы геометрии сетки ограничены 8
                        for (uint8_t i = 0; i < element->children_count && i < MAX_GRID_CHILDREN; i++) {
                            UI_RenderChildElement(element->children[i]);
                        }
                        break;

                    case UI_TYPE_STACK_PANEL:
                        // Для STACK_PANEL лимит равен максимальному числу детей, которое вы заложили в структуру (например, 24)
                        for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
                            UI_RenderChildElement(element->children[i]);
                        }
                        break;

                    default: break;
                }
            }

            // ОТПРАВКА: Шлем в контроллер ST7796 строго грязный прямоугольник
            ST7796_PushSpriteRect(s, s->dirty_x1, s->dirty_y1, s->dirty_x2, s->dirty_y2);
            
            // КРИТИЧЕСКИЙ СБРОС: Сбрасываем координаты строго ПОСЛЕ отправки всего узла!
            s->dirty_x1 = 0; s->dirty_y1 = 0;
            s->dirty_x2 = 0; s->dirty_y2 = 0;
            s->needs_render = false; 
        }
    }

    // ШАГ 2: Безусловный рекурсивный обход детей для Grid и StackPanel
    if (element->type == UI_TYPE_GRID) {
        // Для сетки лимит строго 8 элементов (MAX_GRID_CHILDREN)
        for (uint8_t i = 0; i < element->children_count && i < MAX_GRID_CHILDREN; i++) {
            UI_DrawTree((UIElement_t*)element->children[i]);
        }
    } 
     else if (element->type == UI_TYPE_STACK_PANEL) {
        // Для стек-панели лимит расширенный (MAX_ELEMENT_CHILDREN, например 24)
        for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
            UI_DrawTree((UIElement_t*)element->children[i]);
        }
    } 
}







/**
 * @brief Безопасная инвариантная прорисовка битмапа внутрь спрайта
 */
 void Draw_Bitmap_To_Sprite(Sprite_t* s, int16_t x, int16_t y, const uint8_t* bitmap, uint16_t bmp_w, uint16_t bmp_h, uint16_t color) {
    if (!s || !s->data || !bitmap) return;

    for (uint16_t h = 0; h < bmp_h; h++) {
        for (uint16_t w = 0; w < bmp_w; w++) {
            // Извлекаем бит из одномерного массива иконок (1 бит на пиксель)
            uint16_t byte_idx = (h * bmp_w + w) / 8;
            uint8_t bit_idx = 7 - ((h * bmp_w + w) % 8);
            
            if (bitmap[byte_idx] & (1 << bit_idx)) {
                int16_t target_x = x + w;
                int16_t target_y = y + h;
                
                // Строгая проверка границ динамического спрайта (Защита от HardFault)
                if (target_x >= 0 && target_x < s->w && target_y >= 0 && target_y < s->h) {
                    uint32_t pixel_idx = (uint32_t)target_y * s->w + target_x;
                    s->data[pixel_idx] = color;
                }
            }
        }
    }
}

// Конвертация HSL (HUE 0..360) в RGB565 (без saturation/lightness — просто яркие цвета)
uint16_t HUE_to_RGB565(uint16_t hue_deg) {
    // Hue: 0..360 → 0..65535 (умножаем на 182.04 для масштабирования)
    uint32_t hue = (uint32_t)hue_deg * 182; // 360 * 182 = 65520 ≈ 65535
    uint8_t r, g, b;

    uint16_t s = 255; // saturation
    uint16_t l = 240; // lightness — средний яркий цвет

    // Псевдокод из HSL → RGB
    if (s == 0) {
        r = g = b = l;
    } else {
        uint16_t q = (l < 128) ? (l * (256 + s) / 256) : (l + s - (l * s / 128));
        uint16_t p = 2 * l - q;
        uint16_t rc = (hue + 43690) % 65535; // +120°
        uint16_t gc = hue;
        uint16_t bc = (65535 - hue + 43690) % 65535;

        // Red
        if (rc < 21845) r = (uint8_t)(p + (q - p) * rc / 21845);
        else if (rc < 65535) r = (uint8_t)q;
        else r = (uint8_t)(p + (q - p) * (65535 - rc) / 21845);

        // Green
        if (gc < 21845) g = (uint8_t)(p + (q - p) * gc / 21845);
        else if (gc < 43690) g = (uint8_t)q;
        else g = (uint8_t)(p + (q - p) * (65535 - gc) / 21845);

        // Blue
        if (bc < 21845) b = (uint8_t)(p + (q - p) * bc / 21845);
        else if (bc < 43690) b = (uint8_t)q;
        else b = (uint8_t)(p + (q - p) * (65535 - bc) / 21845);
    }

    // ⚡ УБЕДИМСЯ, что цвета не вырождаются в серые
    r = (r < 64) ? (r + 64) : r;
    g = (g < 64) ? (g + 64) : g;
    b = (b < 64) ? (b + 64) : b;

    // Переводим в RGB565 (с SWAP для SPI)
    uint16_t color = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
    return ((color & 0xFF) << 8) | ((color >> 8) & 0xFF); // SWAP
}

#define M__PI 3.14159265358979323846
// Функция для отрисовки сетки графика
void Draw_Graph_Content(UIElement_t* el) {
    if ( !graph_sprite.data) return;

    // Заливаем фон графика черным (используем динамические размеры)
    uint32_t total_pixels = (uint32_t)graph_sprite.w * graph_sprite.h;
    memset(graph_sprite.data, 0, total_pixels * 2);

    // Рисуем сетку графика. 
    // Вместо жестких макросов используем graph_sprite.w и graph_sprite.h!
    // Движок сам адаптирует сетку и под 224px (портрет), и под 336px (альбом)
    uint16_t grid_color = 0x31A6;

    // 2. Рисуем сетку графика (горизонтальные линии шкал)
    // Рисуем 3 горизонтальные линии через каждые 50 пикселей внутри спрайта
    for (uint16_t y = 40; y < graph_sprite.h; y += 50) {
        for (uint16_t x = 10; x < graph_sprite.w - 10; x++) {
            graph_sprite.data[y * graph_sprite.w + x] = grid_color; // Тускло-серый цвет сетки
        }
    }

    // Перед циклом добавьте:
     const float frequency = 0.8f;               // частота синуса: 0.4
    const float period_length = 30.0f * M__PI / frequency; // ~15.708
    const uint8_t hue_step_per_period = 20; // градусов на период 

    // 3. РИСУЕМ ЖИВУЮ КРИВУЮ ИЗМЕРЕНИЙ (Пример: синусоида или массив точек КСВ)
    // Пробегаем по всей ширине окна графика шаг за шагом
    int16_t prev_x = 10;
    int16_t prev_y = graph_sprite.h / 2; // Стартовая точка по центру

    for (int16_t x = 10; x < graph_sprite.w - 10; x++) {
        // Имитируем график: вычисляем Y (в реальном коде тут будет значение из массива SWR_Array[x])
        // Переводим значение КСВ в пиксели высоты спрайта
        int16_t y = (graph_sprite.h / 2) + (int16_t)(cosf(x * frequency) * (uint16_t)period_length); 
        uint16_t color = RGB565_RED;
        // Соединяем прошлую точку с текущей быстрой линией Брезенхема!
        Draw_Line_To_Sprite(&graph_sprite, prev_x, prev_y, x, y, color);

        prev_x = x;
        prev_y = y;
    }
   // Параметры квадратной волны
    const uint16_t period_x = 40;          // Длина одного периода в пикселях (ширина «блока»)
    const int16_t max_amplitude = (graph_sprite.h / 4) - 10; // Амплитуда (отступ от центра до края)
    const int16_t center_y = graph_sprite.h / 2;             // Центр графика по вертикали

    prev_x = 10;
    prev_y = center_y; // Начинаем с центра или с нижней точки

    for (int16_t x = 10; x < graph_sprite.w - 10; x++) {
        // Логика квадратной волны:
        // 1. Определяем, в какой части периода мы находимся (0..period_x)
        // 2. Если половина периода — сигнал высокий, иначе низкий (или наоборот)
        
        float phase = fmodf(x, (float)period_x); // Текущая фаза внутри периода
        int16_t y;
        
        // Пример: Высокий уровень на первой половине периода, низкий на второй
        if (phase < (period_x / 2.0f)) {
            y = center_y - max_amplitude; // Верхняя линия
        } else {
            y = center_y + max_amplitude; // Нижняя линия
        }

        uint16_t color = RGB565_GREEN; // Можно сделать цвет линии другим
        
        // Соединяем точки линией (она будет диагональной при переходе фаз, но визуально это будет "квадрат")
        Draw_Line_To_Sprite(&graph_sprite, prev_x, prev_y, x, y, color);

        prev_x = x;
        prev_y = y;
    }

    // Подпись
    lcd_print_to_buffer(15, 10, RGB565_WHITE, "SWR SCANNER", RGB565_BLACK, &graph_sprite);

    // Рисуем рамку вокруг графика с отступом 5 пикселей от краев спрайта.
    // Верхняя линия
    for (uint16_t x = 5; x < graph_sprite.w - 5; x++) graph_sprite.data[25 * graph_sprite.w + x] = 0x7BEF; // Серый цвет
    // Нижня линия
    for (uint16_t x = 5; x < graph_sprite.w - 5; x++) graph_sprite.data[(graph_sprite.h - 5) * graph_sprite.w + x] = 0x7BEF;
    // Левая линия
    for (uint16_t y = 25; y < graph_sprite.h - 5; y++) graph_sprite.data[y * graph_sprite.w + 5] = 0x7BEF;
    // Правая линия
    for (uint16_t y = 25; y < graph_sprite.h - 5; y++) graph_sprite.data[y * graph_sprite.w + (graph_sprite.w - 5)] = 0x7BEF;
}

extern uint8_t screen_rotation; // Берем текущий поворот из st7796.c

extern uint16_t Display_Width;  // объявлены/устанавливаются в st7796.c при SetRotation
extern uint16_t Display_Height;

void Convert_Touch_Coordinates(uint16_t raw_x, uint16_t raw_y, uint16_t* out_x, uint16_t* out_y) {
    // Используем реальные размеры дисплея и корректно считаем пределы [0..W-1]/[0..H-1]
    uint16_t w = (Display_Width > 0) ? Display_Width : 320;
    uint16_t h = (Display_Height > 0) ? Display_Height : 480;
    uint16_t w1 = (w > 0) ? (w - 1) : 0;
    uint16_t h1 = (h > 0) ? (h - 1) : 0;

    switch (screen_rotation) {
        case 0: // Portrait
            *out_x = raw_x;
            *out_y = raw_y;
            break;
        case 1: // Landscape (rotate 90°)
            // X = raw_y, Y = (width-1) - raw_x
            *out_x = raw_y;
            *out_y = h1 - raw_x;
            break;
        case 2: // Portrait inverted (180°)
            *out_x = w1 - raw_x;
            *out_y = h1 - raw_y;
            break;
        case 3: // Landscape inverted (270°)
            *out_x = w1 - raw_y;
            *out_y = raw_x;
            break;
        default:
            *out_x = raw_x;
            *out_y = raw_y;
            break;
    }
}

/**
 * @brief Помечает конкретный спрайт грязным ЦЕЛИКОМ (на весь его размер)
 */
void GUI_InvalidateSprite(Sprite_t* s) {
    if (s && s->is_allocated) {
        s->dirty_x1 = 0;
        s->dirty_y1 = 0;
        s->dirty_x2 = s->w - 1;
        s->dirty_y2 = s->h - 1;
        s->needs_render = true;
    }
}

/**
 * @brief Рекурсивно помечает абсолютно ВСЕ спрайты дерева грязными на полный размер
 */
void GUI_InvalidateAll(UIElement_t* element) {
    if (!element) return;

    if (element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        if (s->is_allocated) {
            s->dirty_x1 = 0;
            s->dirty_y1 = 0;
            s->dirty_x2 = s->w - 1;
            s->dirty_y2 = s->h - 1;
            s->needs_render = true;
        }
    }

    if (element->type == UI_TYPE_GRID || element->type == UI_TYPE_STACK_PANEL) {
        // Выбираем правильный лимит в зависимости от контейнера, чтобы не обрезать инвалидацию строк
        uint8_t max_limit = (element->type == UI_TYPE_GRID) ? MAX_GRID_CHILDREN : MAX_ELEMENT_CHILDREN;
        
        for (uint8_t i = 0; i < element->children_count && i < max_limit; i++) {
            // Добавляем проверку на NULL перед рекурсивным шагом
            if (element->children[i] != NULL) {
                GUI_InvalidateAll((UIElement_t*)element->children[i]);
            }
        }
    }
}

/**
 * @brief Помечает локальную прямоугольную область внутри спрайта как "грязную" (требующую обновления)
 */
void GUI_InvalidateRect(Sprite_t* s, int16_t rx, int16_t ry, uint16_t rw, uint16_t rh) {
    if (!s || !s->is_allocated) return;

    // Рассчитываем конечные точки локального прямоугольника
    int16_t x2 = rx + rw - 1;
    int16_t y2 = ry + rh - 1;

    // Ограничиваем прямоугольник физическими размерами самого спрайта (защита)
    if (rx < 0) rx = 0;
    if (ry < 0) ry = 0;
    if (x2 >= s->w) x2 = s->w - 1;
    if (y2 >= s->h) y2 = s->h - 1;

    if (!s->needs_render) {
        // Если спрайт до этого был чистым — это первая грязная зона
        s->dirty_x1 = rx;
        s->dirty_y1 = ry;
        s->dirty_x2 = x2;
        s->dirty_y2 = y2;
        s->needs_render = true;
    } else {
        // Если спрайт уже имел грязную зону — объединяем их в один общий прямоугольник
        if (rx < s->dirty_x1) s->dirty_x1 = rx;
        if (ry < s->dirty_y1) s->dirty_y1 = ry;
        if (x2 > s->dirty_x2) s->dirty_x2 = x2;
        if (y2 > s->dirty_y2) s->dirty_y2 = y2;
    }
}



/**
 * @brief Безопасно обновляет текст элемента UI и автоматически помечает только его область грязной
 */
void UI_SetText(UIElement_t* element, const char* format, ...) {
    if (!element) return;

    // Временный буфер для сборки новой строки
    char new_text[32];
    
    // Парсим динамические аргументы (va_list)
    va_list args;
    va_start(args, format);
    vsnprintf(new_text, sizeof(new_text), format, args);
    va_end(args);

    // Умная оптимизация: если текст совпадает с тем, что уже на экране — выходим!
    if (strcmp(element->text_content, new_text) == 0) {
        return;
    }

    // Безопасно копируем строку, защищая терминатор \0
    strncpy(element->text_content, new_text, sizeof(element->text_content) - 1);
    element->text_content[sizeof(element->text_content) - 1] = '\0';

    // Автоматически помечаем грязной ТОЛЬКО локальную область этого элемента
    if (element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        
        // Переводим абсолютные координаты Grid/StackPanel в локальные координаты внутри спрайта
        int16_t local_rx = element->x - s->x;
        int16_t local_ry = element->y - s->y;
        
        // Магия: "заряжаем" грязный прямоугольник строго под размер этой строки текста!
        GUI_InvalidateRect(s, local_rx, local_ry, element->w, element->h);
    }
}

/**
 * @brief Универсальный хелпер для потокового добавления текстовых блоков в StackPanel
 * @param parent Контейнер (например, digits_node), куда добавляется строка
 * @param initial_text Начальный текст или статичное пояснение
 */
UIElement_t* GUI_Panel_AddString(UIElement_t* parent, const char* initial_text) {
    // Жесткая защита: проверяем лимиты пула строк и массива детей в gui.h (макс 8)
    if (panel_rows_count >= MAX_PANEL_ROWS || parent->children_count >= MAX_PANEL_ROWS) {
        return NULL; 
    }

    // 1. Берем свободный элемент из статического пула
    UIElement_t* new_node = &panel_rows[panel_rows_count++];
    
    // 2. Настраиваем его по новым правилам компонентного движка
    new_node->type = UI_TYPE_TEXT_BLOCK;                  // Тип — текстовый блок
    // Наследует физический спрайт панели (если он уже привязан)
    new_node->sprite = (parent) ? parent->sprite : NULL;
    new_node->render_callback = NULL;                     // Зануляем: отрисовкой управляет движок!
    new_node->children_count = 0;                         // У текста нет детей
    
    // Настройки выравнивания по умолчанию (можно переопределить после вызова функции)
    new_node->horizontal_alignment = HORIZONTAL_ALIGN_CENTER;
    new_node->vertical_alignment = VERTICAL_ALIGN_CENTER;

    // --- УСТАНОВКА ШРИФТА ПО УМОЛЧАНИЮ ---
    new_node->font = &font_arial_9_struct; // Устанавливаем шрифт по умолчанию

    // 3. Безопасно копируем начальный текст
    strncpy(new_node->text_content, initial_text, sizeof(new_node->text_content) - 1);
    new_node->text_content[sizeof(new_node->text_content) - 1] = '\0';

    // 4. Регистрируем указатель на этот узел в массиве детей StackPanel
    parent->children[parent->children_count++] = new_node;

    // Возвращаем указатель, чтобы main.c мог сохранить его в ui_swr_row, ui_btn_row и т.д.
    return new_node;
}

/**
 * @brief Полный сброс динамических строк панели (вызывается при переключении экранов)
 */
void GUI_Panel_ClearStrings(UIElement_t* parent) {
    if (parent) {
        parent->children_count = 0;
    }
    panel_rows_count = 0;
}

void Draw_GeneralText_Callback(UIElement_t* el) {
    if (!el || !el->sprite || el->w == 0 || el->h == 0) return;
    Sprite_t* s = el->sprite;

    //int16_t lx = el->x - s->x, ly = el->y - s->y;
    
    // 1. Вычисляем локальные координаты ячейки внутри физического спрайта
    int16_t local_x = el->x - s->x;
    int16_t local_y = el->y - s->y;

    // КРИТИЧЕСКИЙ ФИКС 1: Очистка фона ячейки перед отрисовкой текста
    // Это предотвращает "хвосты" от предыдущих длинных строк
    for (int16_t y = local_y; y < local_y + el->h; y++) {
        if (y >= s->h) break;
        for (int16_t x = local_x; x < local_x + el->w; x++) {
            if (x >= s->w) break;
            s->data[y * s->w + x] = el->background_color;
        }
    }


    // 2. Очищаем пространство этой конкретной строки цветом фона (Dirty Rect)
   /*  for (int16_t y = local_y; y < local_y + el->h; y++) {
        for (int16_t x = local_x; x < local_x + el->w; x++) {
            s->data[y * s->w + x] = RGB565_BLACK; 
        }
    } */
   // КРИТИЧЕСКИЙ ФИКС 2: Не затираем фон черным! Определяем текущий цвет подложки.
    //uint16_t bg_color = s->data[ly * s->w + lx]; 

    // 3. Активируем шрифт
    lcd_set_font(el->font);
    //lcd_set_font(&font_arial_9_struct);
    
    // Получаем метрики строки и шрифта
    int str_w = lcd_get_str_width(el->text_content); // Текст берется из САМОГО элемента!
    uint16_t font_h = current_font->char_height;

    int16_t text_x = local_x; 
    int16_t text_y = local_y; 

    // 4. МАТЕМАТИКА ГОРИЗОНТАЛЬНОГО ВЫРАВНИВАНИЯ (X)
    switch (el->horizontal_alignment) {
        case HORIZONTAL_ALIGN_LEFT:
            text_x = local_x + 5; // Отступ 5 пикселей слева
            break;
        case HORIZONTAL_ALIGN_CENTER:
            text_x = local_x + (el->w - str_w) / 2;
            break;
        case HORIZONTAL_ALIGN_RIGHT:
            text_x = local_x + el->w - str_w - 5; // Отступ 5 пикселей справа
            break;
    }

    // 5. МАТЕМАТИКА ВЕРТИКАЛЬНОГО ВЫРАВНИВАНИЯ (Y)
    switch (el->vertical_alignment) {
        case VERTICAL_ALIGN_TOP:
            text_y = local_y + 2;
            break;
        case VERTICAL_ALIGN_CENTER:
            text_y = local_y + (el->h - font_h) / 2;
            break;
        case VERTICAL_ALIGN_BOTTOM:
            text_y = local_y + el->h - font_h - 2;
            break;
    }

    // 6. Защита от вылета текста за левую/верхнюю границу
    if (text_x < local_x) text_x = local_x;
    if (text_y < local_y) text_y = local_y;

    // 7. Печатаем текст. Цвет делаем зеленым для динамики, белым для статики (по вашему желанию)
    lcd_print_to_buffer(text_x, text_y, RGB565_GREEN, el->text_content, el->background_color, s);
}
 



void UI_RenderBorder(UIElement_t* el) {
    Sprite_t* s = el->sprite;
    int16_t lx = el->x - s->x;
    int16_t ly = el->y - s->y;
    uint16_t color = el->props.border.border_color;

    // Рисуем верхнюю и нижнюю линии
    for (uint16_t x = lx; x < lx + el->w; x++) {
        s->data[ly * s->w + x] = color;
        s->data[(ly + el->h - 1) * s->w + x] = color;
    }
    // Рисуем левую и правую линии
    for (uint16_t y = ly; y < ly + el->h; y++) {
        s->data[y * s->w + lx] = color;
        s->data[y * s->w + (lx + el->w - 1)] = color;
    }
}

void UI_RenderButton(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    Sprite_t* s = el->sprite;
    int16_t lx = el->x - s->x;
    int16_t ly = el->y - s->y;
    
    // Выбираем цвет фона в зависимости от того, нажата ли кнопка пальцем
    uint16_t bg_color = el->props.button.is_pressed ? el->props.button.press_color : el->props.button.normal_color;

    // 1. Заливаем тело кнопки
    for (int16_t y = ly; y < ly + el->h; y++) {
        for (int16_t x = lx; x < lx + el->w; x++) {
            s->data[y * s->w + x] = bg_color;
        }
    }
    
    // 2. Рисуем рамку кнопки (чуть светлее или темнее фона)
    uint16_t border_color = el->props.button.is_pressed ? RGB565_WHITE : RGB565_DARK_GRAY;
    for (uint16_t x = lx; x < lx + el->w; x++) {
        s->data[ly * s->w + x] = border_color;
        s->data[(ly + el->h - 1) * s->w + x] = border_color;
    }

    // 3. Печатаем текст кнопки по центру ячейки
    // Используем шрифт элемента, если он задан, иначе fallback
    lcd_set_font((el->font) ? el->font : &font_segoe_struct);
    int str_w = lcd_get_str_width(el->text_content);
    // current_font->char_height теперь корректен благодаря lcd_set_font выше,
    // но лучше явно взять метрику из шрифта, который мы только что установили.
    // Если lcd_get_str_width не использует текущий глобальный шрифт, передайте его явно:
    // int str_w = lcd_get_str_width_font(el->text_content, el->font);
    int16_t tx = lx + (el->w - str_w) / 2;
    int16_t ty = ly + (el->h - current_font->char_height) / 2;
    lcd_print_to_buffer(tx, ty, RGB565_WHITE, el->text_content, bg_color, s);
}

void UI_RenderToggle(UIElement_t* el) {
    Sprite_t* s = el->sprite;
    int16_t lx = el->x - s->x;
    int16_t ly = el->y - s->y;

    // Очищаем фон под элементом
    for(int16_t y = ly; y < ly + el->h; y++)
        for(int16_t x = lx; x < lx + el->w; x++) s->data[y * s->w + x] = RGB565_BLACK;

    if (el->type == UI_TYPE_CHECK_BOX) {
        // Рисуем квадрат флажка [ ] размером 12х12 пикселей
        for (int16_t i = 0; i < 12; i++) {
            s->data[ly * s->w + (lx + i)] = RGB565_WHITE;
            s->data[(ly + 11) * s->w + (lx + i)] = RGB565_WHITE;
            s->data[(ly + i) * s->w + lx] = RGB565_WHITE;
            s->data[(ly + i) * s->w + (lx + 11)] = RGB565_WHITE;
        }
        // Если флажок взведен, ставим внутри крестик 'X'
        if (el->props.toggle.is_checked) {
            lcd_print_to_buffer(lx + 2, ly - 2, RGB565_GREEN, "X", RGB565_BLACK, s);
        }
    } else {
        // Логика для RadioButton: рисуем круг (или ромб для простоты Си-кода)
        lcd_print_to_buffer(lx, ly - 2, RGB565_WHITE, el->props.toggle.is_checked ? "(O)" : "( )", RGB565_BLACK, s);
    }

    // Выводим текст
    // Шрифт для текста флажка тоже может быть задан, но часто это мелкий шрифт
    lcd_set_font((el->font) ? el->font : &font_arial_9_struct);
    // Выводим сопровождающий текст справа от флажка
    lcd_print_to_buffer(lx + 16, ly, RGB565_WHITE, el->text_content, RGB565_BLACK, s);
}


/**
 * @brief Инициализирует узел как ListBox
 */
void UI_InitListBox(UIElement_t* el, Sprite_t* target_sprite) {
    if (!el) return;
    el->type = UI_TYPE_LIST_BOX;
    el->sprite = target_sprite;
    el->children_count = 0;
    el->props.list_box.selected_index = -1;
    el->props.list_box.scroll_offset = 0;

    el->props.list_box.height_mode = UI_LISTBOX_HEIGHT_AUTO;
    el->props.list_box.visible_row_count = 4;  // можно переопределить позже
    el->props.list_box.pixel_height = 0;// 192;     // можно переопределить позже
    // Устанавливаем шрифт по умолчанию для ListBox
    el->font = &font_arial_9_struct;
}

void Menu_ClearListbox(UIElement_t* listbox) {
    // Сбрасываем состояние выбора, иначе "фантом" старого индекса выберет новую строку
    listbox->props.list_box.selected_index = -1; 
    listbox->touch_state.drag_last_y = -1;
    listbox->touch_state.drag_active = false;
    
    // Просто сбрасываем счетчик. Так как panel_rows — статический массив, 
    // память физически не удаляется, но перезаписывается новыми данными.
    listbox->children_count = 0;
}

/**
 * @brief Добавляет строку (пункт списка) внутрь ListBox
 */

/* UIElement_t* UI_ListBox_AddItem(UIElement_t* listbox_elem, const char* text) {
    if (!listbox_elem || listbox_elem->type != UI_TYPE_LIST_BOX) return NULL;
    
    // 1. Проверяем жесткий лимит на максимальное количество детей у одного контейнера
    if (listbox_elem->children_count >= MAX_ELEMENT_CHILDREN) {
        return NULL; 
    }
    
    // 2. Проверяем глобальный лимит нашего статического пула строк panel_rows
    if (panel_rows_count >= MAX_PANEL_ROWS) {
        return NULL; 
    }
    
    // 3. Берем свободный элемент из статического пула строк
    UIElement_t* item = &panel_rows[panel_rows_count++];
    
    // 4. Гарантированно зануляем память элемента перед инициализацией
    memset(item, 0, sizeof(UIElement_t));
    
    // 5. Заполняем поля дочернего элемента строки
    item->type = UI_TYPE_TEXT_BLOCK;
    item->sprite = listbox_elem->sprite; // Наследуем спрайт от родителя (если он уже задан)
    item->font = listbox_elem->font;     // Наследуем шрифт от родительского ListBox
    item->background_color = listbox_elem->background_color;
    item->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    item->vertical_alignment = VERTICAL_ALIGN_CENTER;
    
    // Безопасно копируем текст строки, защищаясь от переполнения буфера text_content
    strncpy(item->text_content, text, sizeof(item->text_content) - 1);
    item->text_content[sizeof(item->text_content) - 1] = '\0'; // Гарантируем нуль-терминатор
    
    // 6. Регистрируем элемент в массиве детей ListBox
    listbox_elem->children[listbox_elem->children_count++] = item;
    
    return item;
} */

// Константы отступов лучше вынести в #define или enum для переиспользования
#define LISTBOX_PADDING_X     5 
#define LISTBOX_ICON_SIZE     16 // Предполагаемый размер иконки, если она появится позже

UIElement_t* UI_ListBox_AddItem(UIElement_t* listbox_elem, const char* text) {
    if (!listbox_elem || listbox_elem->type != UI_TYPE_LIST_BOX) return NULL;
    
    // Проверка лимитов
    if (listbox_elem->children_count >= MAX_ELEMENT_CHILDREN) return NULL;
    if (panel_rows_count >= MAX_PANEL_ROWS) return NULL;

    // Берем объект из пула и гарантируем чистоту памяти
    UIElement_t* item = &panel_rows[panel_rows_count++];
    memset(item, 0, sizeof(UIElement_t));

    // Базовая инициализация типа и внешнего вида
    item->type = UI_TYPE_TEXT_BLOCK;
    item->sprite = listbox_elem->sprite;
    item->font = listbox_elem->font;
    item->background_color = listbox_elem->background_color;
    item->foreground_color = listbox_elem->foreground_color; // ВАЖНО: Наследуем цвет текста!
    
    item->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    item->vertical_alignment = VERTICAL_ALIGN_CENTER; // Центрирование работает надежнее, чем CENTER при разных высотах

    // Безопасное копирование текста
    strncpy(item->text_content, text, sizeof(item->text_content) - 1);
    item->text_content[sizeof(item->text_content) - 1] = '\0';

    // --- ГЕОМЕТРИЯ И ПОЗИЦИОНИРОВАНИЕ ---
    
    // Ширина равна ширине листа минус паддинги и ширина возможного скроллбара
    uint8_t scrollbar_w = 10;
    bool has_scrollbar = (listbox_elem->children_count + 1 > listbox_elem->h / (listbox_elem->font->char_height + 6));
    uint16_t available_width = listbox_elem->w - (2 * LISTBOX_PADDING_X) - (has_scrollbar ? scrollbar_w : 0);
    
    item->w = available_width;
    
    // Высота зависит от высоты шрифта
    item->h = listbox_elem->font->char_height + 6; // Шрифт + 6px внутренних отступов (ваш item_h)

    // Координаты X и Y здесь относительны родительского контейнера (будут учтены layout-менеджером)
    // Для простоты кладем их со стандартным левым паддингом
    item->x = LISTBOX_PADDING_X;
    // Y можно рассчитать сразу, чтобы не делать лишних делений в цикле отрисовки,
    // либо оставить 0 и считать в GUI_Render pass.
    // Оставим 0, так как позиция сильно зависит от scroll_offset.

    // Регистрация в иерархии
    listbox_elem->children[listbox_elem->children_count++] = item;
    
    return item;
}

/**
 * @brief Отрисовка ListBox (фон, рамка, элементы, скроллбар)
 * 
 * @note ЭТА ФУНКЦИЯ ТОЛЬКО РИСУЕТ. Геометрия (x, y, w, h) уже должна быть
 *       рассчитана в UI_MeasureAndArrange. Мы используем el->children[i]->x и т.д.
 */
void UI_RenderListBox(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    Sprite_t* s = el->sprite;
    
    // 1. Локальные координаты ListBox внутри общего спрайта панели
    int16_t lx = el->x - s->x;
    int16_t ly = el->y - s->y;

    // Проверяем, что область отрисовки попадает в спрайт
    if (lx + el->w <= 0 || lx >= s->w || ly + el->h <= 0 || ly >= s->h) return;

    // --- ШАГ 1: Очистка фона и отрисовка рамки ListBox ---
    // Определяем границы прямоугольника, который нужно залить (клиппинг)
    int16_t clip_x1 = (lx < 0) ? 0 : lx;
    int16_t clip_y1 = (ly < 0) ? 0 : ly;
    int16_t clip_x2 = (lx + el->w > s->w) ? s->w : (lx + el->w);
    int16_t clip_y2 = (ly + el->h > s->h) ? s->h : (ly + el->h);

    // Заливаем фон ListBox (черный) И рисуем рамку
    for (int16_t y = clip_y1; y < clip_y2; y++) {
        for (int16_t x = clip_x1; x < clip_x2; x++) {
            // Проверяем, является ли этот пиксель частью рамки
            // Рамка имеет толщину 1 пиксель и отступ 1 пиксель от края
            bool is_border = (x == lx + 1 || x == lx + el->w - 2 || 
                              y == ly + 1 || y == ly + el->h - 2);
            
            if (is_border) {
                s->data[y * s->w + x] = 0x7BEF; // Цвет рамки
            } else {
                s->data[y * s->w + x] = RGB565_BLACK; // Фон списка
            }
        }
    }

    // --- ШАГ 2: Подготовка параметров шрифта и скроллбара ---
    lcd_set_font(&font_arial_9_struct);
    uint16_t font_h = current_font->char_height;
    uint16_t item_h = font_h + 6; // Высота строки с отступами
    
    uint8_t start = el->props.list_box.scroll_offset;
    uint8_t visible_count = (el->h + item_h - 1) / item_h;
    if (visible_count == 0) visible_count = 1;

    // Ширина скроллбара (должна совпадать с логикой в UI_MeasureAndArrange)
    uint8_t scrollbar_w = 10;
    if (el->children_count <= visible_count) {
        scrollbar_w = 0; // Скроллбар не нужен, если все видно
    } else if (scrollbar_w > el->w - 2) {
        scrollbar_w = (el->w > 2) ? (el->w - 2) : 0;
    }
    
    // Реальная ширина контента (без скроллбара)
    uint16_t content_w = el->w - scrollbar_w;
    if (content_w < 8) content_w = 8;

    // --- ШАГ 3: Отрисовка видимых элементов ---
    for (uint8_t i = 0; i < el->children_count; i++) {
        if (i >= start && i < (start + visible_count)) {
            UIElement_t* child = (UIElement_t*)el->children[i];
            if (!child) continue;

            // Вычисляем Y позиции строки относительно ListBox
            int16_t relative_row_y = (i - start) * item_h;
            
            // Абсолютные координаты строки в глобальном пространстве
            int16_t row_abs_y = el->y + relative_row_y;
            
            // Локальные координаты в спрайте
            int16_t clx = el->x - s->x; // Left X для контента
            int16_t cly = row_abs_y - s->y;

            // Цвет фона элемента (выделен или нет)
            uint16_t bg_color = (i == el->props.list_box.selected_index) ? 0x10A5 : RGB565_BLACK;

            // --- ВАЖНО: Очищаем фон под текстом ЭТОЙ строки ---
            // Рисуем прямоугольник фона для строки. 
            // Важно: clip_x1/clx + 1 чтобы не задеть левую рамку
            for (int16_t y = cly; y < cly + item_h; y++) {
                if (y < 0 || y >= s->h) continue;
                
                // Не рисуем в зоне скроллбара, если он есть
                int16_t limit_x = (scrollbar_w > 0) ? (clx + content_w) : (clx + el->w);
                
                for (int16_t x = clx + 1; x < limit_x - 1; x++) {
                    if (x < 0 || x >= s->w) continue;
                    s->data[y * s->w + x] = bg_color;
                }
            }

            // Отрисовка текста
            int16_t text_x = clx + 5; // Отступ слева
            int16_t text_y = cly + (item_h - font_h) / 2; // Центрирование по вертикали

            if (child->text_content[0] != '\0') {
                lcd_print_to_buffer(text_x, text_y, RGB565_WHITE, child->text_content, bg_color, s);
            }
        }
    }

    // --- ШАГ 4: Отрисовка скроллбара ---
    if (scrollbar_w > 0 && el->children_count > visible_count) {
        int16_t sb_x1 = lx + el->w - scrollbar_w;
        int16_t sb_x2 = lx + el->w - 1;

        // Клиппинг скроллбара
        if (sb_x1 < 0) sb_x1 = 0;
        if (sb_x2 >= s->w) sb_x2 = s->w - 1;

        // Рисуем трек (фон скроллбара)
        for (int16_t y = ly + 1; y < ly + el->h - 1; y++) {
            if (y < 0 || y >= s->h) continue;
            for (int16_t x = sb_x1; x <= sb_x2; x++) {
                if (x < 0 || x >= s->w) continue;
                s->data[y * s->w + x] = RGB565_DARK_GRAY;
            }
        }

        // Вычисляем ползунок (thumb)
        uint8_t max_offset = el->children_count - visible_count;
        uint16_t thumb_h = (uint16_t)visible_count * el->h / el->children_count;
        if (thumb_h < 12) thumb_h = 12;
        if (thumb_h > el->h) thumb_h = el->h;

        // Позиция ползунка
        int16_t thumb_y = ly + 1;
        if (max_offset > 0) {
            thumb_y = ly + 1 + (int16_t)((el->props.list_box.scroll_offset * ((el->h - 2) - thumb_h)) / max_offset);
        }
        if (thumb_y + (int16_t)thumb_h > ly + el->h - 1) {
            thumb_y = ly + el->h - 1 - thumb_h;
        }
        if (thumb_y < ly + 1) thumb_y = ly + 1;

        // Рисуем ползунок
        for (int16_t y = thumb_y; y < thumb_y + thumb_h; y++) {
            if (y < 0 || y >= s->h) continue;
            for (int16_t x = sb_x1 + 2; x < sb_x2 - 1; x++) {
                if (x < 0 || x >= s->w) continue;
                s->data[y * s->w + x] = RGB565_YELLOW;
            }
        }
    }

    // Помечаем область ListBox грязной, если она еще не помечена
    if (s->is_allocated) {
        s->needs_render = true;
        // Обновляем dirty rect, чтобы не отправлять лишний весь экран
        int16_t new_dirty_x2 = lx + el->w - 1;
        int16_t new_dirty_y2 = ly + el->h - 1;
        int16_t new_dirty_x1 = lx;
        int16_t new_dirty_y1 = ly;

        if (s->dirty_x2 < new_dirty_x2) s->dirty_x2 = new_dirty_x2;
        if (s->dirty_y2 < new_dirty_y2) s->dirty_y2 = new_dirty_y2;
        if (s->dirty_x1 > new_dirty_x1) s->dirty_x1 = new_dirty_x1;
        if (s->dirty_y1 > new_dirty_y1) s->dirty_y1 = new_dirty_y1;
    }
}


/**
 * @brief Отрисовка ОДНОЙ строки ListBox по индексу
 *        Используется для оптимизации при навигации (вместо полной перерисовки)
 */
void UI_RenderListBoxItem(UIElement_t* el, uint8_t item_index) {
    if (!el || !el->sprite || !el->sprite->data) return;
    if (item_index >= el->children_count) return;
    
    Sprite_t* s = el->sprite;
    
    // Проверяем, что элемент видим
    uint8_t start = el->props.list_box.scroll_offset;
    uint8_t font_h = (current_font != NULL) ? current_font->char_height : font_arial_9_struct.char_height;
    uint16_t item_h = font_h + 6;
    uint8_t visible_count = (el->h + item_h - 1) / item_h;
    if (visible_count == 0) visible_count = 1;
    
    // Проверяем, что элемент попадает в видимую область
    if (item_index < start || item_index >= (start + visible_count)) return;
    
    // Локальные координаты ListBox внутри спрайта
    int16_t lx = el->x - s->x;
    int16_t ly = el->y - s->y;
    
    // Ширина скроллбара (должна совпадать с UI_RenderListBox)
    uint8_t scrollbar_w = 10;
    if (el->children_count <= visible_count) {
        scrollbar_w = 0;
    } else if (scrollbar_w > el->w - 2) {
        scrollbar_w = (el->w > 2) ? (el->w - 2) : 0;
    }
    uint16_t content_w = el->w - scrollbar_w;
    if (content_w < 8) content_w = 8;
    
    // Вычисляем позицию строки (ВЕРНО!)
    int16_t relative_row_y = (item_index - start) * item_h;
    int16_t row_abs_y = el->y + relative_row_y;
    int16_t clx = lx;
    int16_t cly = row_abs_y - s->y;
    
    // Цвет фона элемента (выделен или нет)
    UIElement_t* child = (UIElement_t*)el->children[item_index];
    if (!child) return;
    
    uint16_t bg_color = (item_index == el->props.list_box.selected_index) ? 0x10A5 : RGB565_BLACK;
    
    // --- ОЧИСТКА ФОНА СТРОКИ ---
    for (int16_t y = cly; y < cly + item_h; y++) {
        if (y < 0 || y >= s->h) continue;
        
        int16_t limit_x = (scrollbar_w > 0) ? (clx + content_w) : (clx + el->w);
        
        for (int16_t x = clx + 1; x < limit_x - 1; x++) {
            if (x < 0 || x >= s->w) continue;
            s->data[y * s->w + x] = bg_color;
        }
    }
    
    // --- ОТРИСОВКА ТЕКСТА ---
    int16_t text_x = clx + 5;
    int16_t text_y = cly + (item_h - font_h) / 2;
    
    if (child->text_content[0] != '\0') {
        lcd_print_to_buffer(text_x, text_y, RGB565_WHITE, child->text_content, bg_color, s);
    }
    
    // --- ОТПРАВКА ОТРИСОВАННОЙ СТРОКИ НА LCD ---
    if (s->is_allocated) {
        s->needs_render = true;
        GUI_InvalidateRect(s, clx, cly, el->w, item_h);
        
        // Отправляем на LCD и сбрасываем dirty rect
        ST7796_PushSpriteRect(s, s->dirty_x1, s->dirty_y1, s->dirty_x2, s->dirty_y2);
        s->dirty_x1 = 0; s->dirty_y1 = 0;
        s->dirty_x2 = 0; s->dirty_y2 = 0;
        s->needs_render = false;
    }
}

static bool g_listbox_drag_active = false;
static int16_t g_listbox_drag_last_y = -1;

static bool UI_PointInElement(const UIElement_t* el, uint16_t tx, uint16_t ty) {
    if (!el) return false;
    return (tx >= (uint16_t)el->x && tx < (uint16_t)(el->x + el->w) &&
            ty >= (uint16_t)el->y && ty < (uint16_t)(el->y + el->h));
}

static UIElement_t* UI_FindElementRecursive(UIElement_t* element, uint16_t tx, uint16_t ty, int16_t* out_local_x, int16_t* out_local_y) {
    if (!element || element->w == 0 || element->h == 0) return NULL;
    if (!UI_PointInElement(element, tx, ty)) return NULL;

    if (element->type == UI_TYPE_LIST_BOX) {
        uint16_t font_h = (current_font != NULL) ? current_font->char_height : font_arial_9_struct.char_height;
        uint16_t item_h = font_h + 6;
        int16_t local_y = ty - element->y;
        uint8_t start = element->props.list_box.scroll_offset;
        uint8_t visible = (element->h + item_h - 1) / item_h;
        if (visible == 0) visible = 1;

        if (local_y >= 0 && local_y < (int16_t)(visible * item_h)) {
            uint8_t index = start + (local_y / item_h);
            if (index < element->children_count) {
                UIElement_t* child = (UIElement_t*)element->children[index];
                if (child) {
                    //if (out_local_x) *out_local_x = tx - element->x;
                    if (out_local_y) *out_local_y = local_y - ((index - start) * item_h);
                    return child;
                }
            }
        }
        if (out_local_x) *out_local_x = tx - element->x;
        if (out_local_y) *out_local_y = ty - element->y;
        return element;
    }

    if (element->type == UI_TYPE_GRID) {
        for (uint8_t i = 0; i < element->children_count && i < MAX_GRID_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            UIElement_t* hit = UI_FindElementRecursive(child, tx, ty, out_local_x, out_local_y);
            if (hit) return hit;
        }
    } else if (element->type == UI_TYPE_STACK_PANEL) {
        for (uint8_t i = 0; i < element->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            UIElement_t* hit = UI_FindElementRecursive(child, tx, ty, out_local_x, out_local_y);
            if (hit) return hit;
        }
    }

    if (out_local_x) *out_local_x = tx - element->x;
    if (out_local_y) *out_local_y = ty - element->y;
    return element;
}

UIElement_t* UI_FindElementAt(UIElement_t* root, uint16_t tx, uint16_t ty, int16_t* out_local_x, int16_t* out_local_y) {
    return UI_FindElementRecursive(root, tx, ty, out_local_x, out_local_y);
}

// Вспомогательная функция: проверка попадания точки в прямоугольник
static inline bool IsPointInRect(int16_t px, int16_t py, int16_t rx, int16_t ry, uint16_t rw, uint16_t rh) 
{
    return (px >= rx && px < (rx + rw) && py >= ry && py < (ry + rh));
}
//Обработчик тача, меняющий selected_index и вызывающий GUI_InvalidateRect для перерисовки
int8_t UI_ListBox_ProcessTouch(UIElement_t* listbox, uint16_t tx, uint16_t ty) 
{
    if (!listbox || listbox->type != UI_TYPE_LIST_BOX || !listbox->sprite) return -1;

    // 1. Быстрая отсечка по границам всего виджета
    if (!IsPointInRect(tx, ty, listbox->x, listbox->y, listbox->w, listbox->h)) {
        listbox->touch_state.drag_active = false;
        listbox->touch_state.drag_last_y = -1;
        return -1;
    }

    uint16_t font_h = (listbox->font != NULL) ? listbox->font->char_height : font_arial_9_struct.char_height;
    uint16_t item_h = font_h + 6; // Высота строки с отступами
    int16_t local_y = ty - listbox->y;

    // 2. Расчеты размеров контента
    uint8_t children = listbox->children_count;
    uint8_t visible = (item_h > 0) ? (listbox->h / item_h) : 1;
    if (visible == 0) visible = 1;
    
    // Если все влезает — скроллбар не нужен
    bool has_scrollbar = (children > visible);

    // Ширина ползунка (фиксированная или динамическая)
    uint8_t scrollbar_w = has_scrollbar ? ((listbox->w > 12) ? 10 : (listbox->w - 2)) : 0;
    bool hit_scrollbar = (scrollbar_w > 0 && tx >= (listbox->x + listbox->w - scrollbar_w));

    // 3. Обработка жеста внутри области скроллбара
    if (hit_scrollbar && has_scrollbar) {
        uint8_t max_offset = (uint8_t)(children - visible);
        
        uint16_t track_h = listbox->h;
        uint16_t thumb_h = (track_h * visible) / children;
        if (thumb_h < 12) thumb_h = 12;
        if (thumb_h > track_h) thumb_h = track_h;

        int32_t track_span = (int32_t)track_h - (int32_t)thumb_h;
        if (track_span <= 0) return -1;

        int16_t rel_y = ty - listbox->y;
        int32_t pos = rel_y - (thumb_h / 2);
        if (pos < 0) pos = 0;
        if (pos > track_span) pos = track_span;

        uint8_t new_offset = (uint8_t)((pos * max_offset + track_span/2) / track_span);
        if (new_offset != listbox->props.list_box.scroll_offset) {
            listbox->props.list_box.scroll_offset = new_offset;
            GUI_InvalidateRect(listbox->sprite, listbox->x, listbox->y, listbox->w, listbox->h);
        }
        return -1;
    }

    Touch_State_t* ts = &listbox->touch_state;

    // 4. Начало нового касания (Press event)
    if (ts->drag_last_y < 0) {
        ts->drag_last_y = ty;
        ts->drag_active = false;

        // Мертвая зона по горизонтали (5% ширины), чтобы отличать свайп от клика
        //uint16_t dead_zone_x = listbox->w / 20;
        // Стало: уменьшаем зону до 2% или задаем константу
        uint16_t dead_zone_x = 10; // Фиксированные 10 пикселей вместо процентов
        if (dead_zone_x < 5) dead_zone_x = 5;
        if (tx < listbox->x + dead_zone_x || tx >= listbox->x + listbox->w - dead_zone_x) return -1;

        int8_t target = listbox->props.list_box.scroll_offset + (local_y / item_h);
        if (target < 0 || target >= listbox->children_count) return -1;

        if (listbox->props.list_box.selected_index != target) {
            listbox->props.list_box.selected_index = target;
            GUI_InvalidateRect(listbox->sprite, listbox->x, listbox->y, listbox->w, listbox->h);
        }
        return target;
    }

    // 5. Детекция начала активного перетаскивания (Drag threshold)
    if (!ts->drag_active) {
        int16_t delta_y = ty - ts->drag_last_y;
        if (delta_y < 0) delta_y = -delta_y;
        if (delta_y > (int16_t)item_h / 2) {
            ts->drag_active = true;
            
            // Сбрасываем выделение при начале прокрутки
            if (listbox->props.list_box.selected_index != -1) {
                listbox->props.list_box.selected_index = -1;
                GUI_InvalidateRect(listbox->sprite, listbox->x, listbox->y, listbox->w, listbox->h);
            }
        }
    }

    // 6. Логика активной инерционной прокрутки
    if (ts->drag_active) {
        int16_t delta_y = ty - ts->drag_last_y;
        ts->drag_last_y = ty;
        
        if (delta_y != 0) {
            uint8_t max_offset = (children > visible) ? (uint8_t)(children - visible) : 0;
            if (max_offset > 0) {
                int8_t step = delta_y / (int16_t)item_h;
                if (step != 0) {
                    int16_t new_offset = (int16_t)listbox->props.list_box.scroll_offset - step;
                    if (new_offset < 0) new_offset = 0;
                    if (new_offset > (int16_t)max_offset) new_offset = max_offset;
                    
                    if ((uint8_t)new_offset != listbox->props.list_box.scroll_offset) {
                        listbox->props.list_box.scroll_offset = (uint8_t)new_offset;
                        GUI_InvalidateRect(listbox->sprite, listbox->x, listbox->y, listbox->w, listbox->h);
                    }
                }
            }
        }
        return -1;
    }

    // 7. Обновление "якоря" если палец просто лежит на экране без движения
    ts->drag_last_y = ty;
    return -1;
}

/**
 * @brief Устанавливает количество строк в сетке
 * Это сообщение для движка MeasureAndArrange, чтобы он знал, сколько раз 
 * пройтись по циклу расчета высот строк.
 */
void UI_SetGridRowsCount(UIElement_t* grid_elem, uint8_t rows) {
    if (grid_elem == NULL) return;
    // Ограничиваем максимальное количество строк, чтобы не выйти за пределы массива
    if (rows > MAX_GRID_CHILDREN) rows = MAX_GRID_CHILDREN;
    
    // Сохраняем в структуру сетки, которая уже существует в вашей структуре UIElement_t
    grid_elem->props.grid.rows_count = rows;
}

/**
 * @brief Устанавливает количество колонок в сетке
 * Аналогично строкам, сообщает движку размерность сетки.
 */
void UI_SetGridColsCount(UIElement_t* grid_elem, uint8_t cols) {
    if (grid_elem == NULL) return;
    
    // Ограничиваем максимальное количество колонок
    if (cols > MAX_GRID_CHILDREN) cols = MAX_GRID_CHILDREN;
    
    // Сохраняем в структуру сетки
    grid_elem->props.grid.cols_count = cols;
}

/**
 * @brief Устанавливает пропорциональную ширину колонки (через проценты)
 * 
 * @note В вашем текущем коде лучше использовать UI_SetGridColPercent.
 * Если вы хотите использовать веса (weights), добавьте соответствующие сеттеры.
 * Здесь оставлю заглушку, так как логика процентов уже реализована через UI_SetGridColPercent.
 */
void UI_SetGridColProportional(UIElement_t* grid_elem, uint8_t col, uint8_t weight_percent) {
    // Ваша реализация UI_SetGridColPercent уже делает то же самое (сохраняет процент)
    // Но если вы хотите использовать весовую систему (не проценты, а веса 1:2:3), 
    // то нужно использовать UI_SetGridColWeight.
    
    // Для простоты, просто делегируем на проценты, если вы имели в виду процентное распределение
    // Но лучше явно вызвать то, что уже работает:
    UI_SetGridColPercent(grid_elem, col, weight_percent);
}

/* UI_SetGridRowsCount(&status_bar_grid, 1);
UI_SetGridColsCount(&status_bar_grid, 3);
UI_SetGridColProportional(&status_bar_grid, 1, 1); */


uint8_t currentHour = 07;
uint8_t currentMinute = 05;
uint8_t battery_Level = 99;

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

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Колбэк отрисовки ТОЛЬКО ЧАСОВ
void Draw_Clock_Callback(UIElement_t* el) {
    if (!el || !el->sprite) return;
    Sprite_t* sprite = el->sprite;

    // Стираем старые часы внутри маленького буфера
    Sprite_fill(sprite, RGB565_BLACK);
    
    lcd_set_font(&font_segoe_struct);
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", currentHour, currentMinute);
    
    // Рисуем в локальных координатах спрайта (0, 0)
    lcd_print_to_buffer(2, 4, RGB565_WHITE, timeBuf, RGB565_BLACK, sprite);
    
    // Помечаем, что весь этот мини-спрайт изменился
    sprite->dirty_x1 = 0; sprite->dirty_y1 = 0;
    sprite->dirty_x2 = sprite->w - 1; sprite->dirty_y2 = sprite->h - 1;
    sprite->needs_render = true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Draw_Icon_Battery_Callback(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    Sprite_t* s = el->sprite;

    // Очищаем только этот спрайт
    Sprite_fill(s, RGB565_BLACK);

    // Рисуем иконку и текст батареи внутри этого маленького буфера
    // Координаты (0,0) — это левый верхний угол этого маленького спрайта
    Draw_Bitmap_To_Sprite(s, 2, 2, iconMirrorHorizontal(icon_battery_16_16_bits, 16, 16), 16, 16, RGB565_WHITE);
    
    // Можно добавить текст "%", если нужно, но лучше в другом спайте или рядом
    lcd_set_font(&font_arial_9_struct);
    char buf[5];
    snprintf(buf, sizeof(buf), "%d%%", battery_Level);
    lcd_print_to_buffer(18, 2, RGB565_WHITE, buf, RGB565_BLACK, s); // Если есть место

    // Помечаемdirty
    s->dirty_x1 = 0; s->dirty_y1 = 0;
    s->dirty_x2 = s->w - 1; s->dirty_y2 = s->h - 1;
    s->needs_render = true;
}

void Draw_Icon_Bluetooth_Callback(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    Sprite_t* s = el->sprite;

    Sprite_fill(s, RGB565_BLACK);

    uint16_t bt_color = bluetoothEnabled ? RGB565_BLUE : 0x528A; // Зеленый или блекло-серый
    const uint8_t* bt_icon = bluetoothEnabled ? icon_bluetooth_bits : icon_not_bluetooth_bits;
    Draw_Bitmap_To_Sprite(s, 1, 2, iconMirrorHorizontal(bt_icon,8,8), 8, 8, bt_color);
    
    s->dirty_x1 = 0; s->dirty_y1 = 0;
    s->dirty_x2 = s->w - 1; s->dirty_y2 = s->h - 1;
    s->needs_render = true;
}

void Draw_Icon_WiFi_Callback(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    Sprite_t* s = el->sprite;

    Sprite_fill(s, RGB565_BLACK);

    uint16_t wifi_mode_color = wifiEnabled ? RGB565_GREEN : 0x528A; // Зеленый или блекло-серый
    const uint8_t* wifi_mode_icon = wifiEnabled ? icon_wifi_bits : icon_not_wifi_bits;
    Draw_Bitmap_To_Sprite(s, 0, 2, iconMirrorHorizontal(wifi_mode_icon,8,8), 8, 8, wifi_mode_color);
    
    s->dirty_x1 = 0; s->dirty_y1 = 0;
    s->dirty_x2 = s->w - 1; s->dirty_y2 = s->h - 1;
    s->needs_render = true;
}



void Draw_Icon_Buzzer_Callback(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    Sprite_t* s = el->sprite;

    Sprite_fill(s, RGB565_BLACK);

    const uint8_t* icon_buzz = buzzerOnOff ? icon_buzzer_on_bits : icon_buzzer_off_bits;
    Draw_Bitmap_To_Sprite(s, 0, 2, iconMirrorHorizontal(icon_buzz, 8, 8), 8, 8, RGB565_WHITE);

    s->dirty_x1 = 0; s->dirty_y1 = 0;
    s->dirty_x2 = s->w - 1; s->dirty_y2 = s->h - 1;
    s->needs_render = true;
}

void Draw_Icon_RS485ToBT_Callback(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    Sprite_t* s = el->sprite;

    Sprite_fill(s, RGB565_BLACK);
    uint16_t bt_mode_color = bluetoothMode ? RGB565_GREEN : 0x528A; // Зеленый или блекло-серый
    const uint8_t* bt_mode_icon = bluetoothMode ? icon_rs485ToBt_bits : icon_not_rs485ToBt_bits;
    Draw_Bitmap_To_Sprite(s, 0, 2, iconMirrorHorizontal(bt_mode_icon,8,8), 8, 8, bt_mode_color);
    
    s->dirty_x1 = 0; s->dirty_y1 = 0;
    s->dirty_x2 = s->w - 1; s->dirty_y2 = s->h - 1;
    s->needs_render = true;
}

void Draw_Icon_NTP_Callback(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    Sprite_t* s = el->sprite;

    Sprite_fill(s, RGB565_BLACK);
    uint16_t ntp_color = ntpSyncEnabled ? RGB565_WHITE : 0x528A; // Зеленый или блекло-серый
    const uint8_t* ntp_icon = ntpSyncEnabled ? icon_ntp_bits : icon_not_ntp_bits;
    Draw_Bitmap_To_Sprite(s, 0, 2, iconMirrorHorizontal(ntp_icon,8,8), 8, 8, ntp_color);
    
    s->dirty_x1 = 0; s->dirty_y1 = 0;
    s->dirty_x2 = s->w - 1; s->dirty_y2 = s->h - 1;
    s->needs_render = true;
}

// ==========================================
// ФУНКЦИЯ СОЗДАНИЯ СТАТУС-БАРА
// ==========================================

void GUI_BuildModularStatusBar(UIElement_t* parent_grid) {
    status_clock_sprite.data = NULL;          status_clock_sprite.is_allocated = false;
    status_icon_battery_sprite.data = NULL;   status_icon_battery_sprite.is_allocated = false;
    status_icon_bt_sprite.data = NULL;        status_icon_bt_sprite.is_allocated = false;
    status_icon_wifi_sprite.data = NULL;      status_icon_wifi_sprite.is_allocated = false;
    status_icon_ntp_sprite.data = NULL;       status_icon_ntp_sprite.is_allocated = false;
    status_icon_buzzer_sprite.data = NULL;    status_icon_buzzer_sprite.is_allocated = false;
    status_icon_mode_sprite.data = NULL;      status_icon_mode_sprite.is_allocated = false;
    status_spacer_sprite.data = NULL;         status_spacer_sprite.is_allocated = false;
    // 1. Инициализация корневого контейнера статус-бара (Grid)
    status_bar_node.type = UI_TYPE_GRID;
    status_bar_node.children_count = 0;
    status_bar_node.sprite = NULL; // Контейнер не имеет своего спрайта
    status_bar_node.grid_row = 0;
    status_bar_node.grid_col = 0;
    
    // Настраиваем сетку: 1 строка, 2 колонки (Часы | Иконки)
    UI_SetGridRowsCount(&status_bar_node, 1);
    UI_SetGridRowPixel(&status_bar_node, 0, STATUS_BAR_HEIGHT);
    
    UI_SetGridColsCount(&status_bar_node, 2);
    UI_SetGridColPixel(&status_bar_node, 0, CLOCK_WIDTH);   // Часы
    UI_SetGridColPercent(&status_bar_node, 1, 100);         // Иконки занимают остальное

    // Добавляем в родительскую сетку
    parent_grid->children[parent_grid->children_count++] = &status_bar_node;

    // ==========================================
    // 2. ЧАСЫ
    // ==========================================
    status_clock_node.type = UI_TYPE_TEXT_BLOCK;
    status_clock_node.grid_row = 0;
    status_clock_node.grid_col = 0;
    status_clock_node.render_callback = Draw_Clock_Callback; // Предполагаем, что эта функция есть
    status_clock_node.background_color = RGB565_BLACK;
    status_clock_node.sprite = &status_clock_sprite;
    status_clock_node.horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    status_clock_node.vertical_alignment = VERTICAL_ALIGN_CENTER;

    // Выделяем память для спрайта часов
    status_clock_sprite.w = CLOCK_WIDTH;
    status_clock_sprite.h = STATUS_BAR_HEIGHT;
    // Используем обычный malloc или heap_caps_malloc без DMA, если DMA недоступен
    status_clock_sprite.data = (uint16_t*)heap_caps_malloc(CLOCK_WIDTH * STATUS_BAR_HEIGHT * 2, 0);
    status_clock_sprite.is_allocated = (status_clock_sprite.data != NULL);
    
    if (!status_clock_sprite.is_allocated) {
        status_clock_node.sprite = NULL;
        while(1); // Ошибка
    }

    status_bar_node.children[status_bar_node.children_count++] = &status_clock_node;

    // ==========================================
    // 3. КОНТЕЙНЕР ДЛЯ ИКОНОК (Внутри колонки 1)
    // ==========================================
    // ИЗМЕНЕНИЕ: Тип изменен на UI_TYPE_GRID для более точного позиционирования
    status_icons_node.type = UI_TYPE_GRID;
    status_icons_node.grid_row = 0;
    status_icons_node.grid_col = 1;
    status_icons_node.sprite = NULL; // Контейнер не имеет своего спрайта
    
    // Настраиваем сетку: 1 строка, 4 колонки
    UI_SetGridRowsCount(&status_icons_node, 1);
    UI_SetGridRowPixel(&status_icons_node, 0, STATUS_BAR_HEIGHT);
    
    UI_SetGridColsCount(&status_icons_node, 7); 
    
    // --- РАСПРЕДЕЛЕНИЕ КОЛОНОК ---
    // Колонка 0: СПЕЙСЕР (занимает 100% оставшегося места, прижимая остальные вправо)
    UI_SetGridColPercent(&status_icons_node, 0, 100); 
    
    // Колонки 1, 2, 3: Фиксированная ширина под иконки
    UI_SetGridColPixel(&status_icons_node, 1, 16);   // rs485toBt
    UI_SetGridColPixel(&status_icons_node, 2, 16);   // buzzer
    UI_SetGridColPixel(&status_icons_node, 3, 16);   // ntp
    UI_SetGridColPixel(&status_icons_node, 4, 16);   // Wi-Fi
    UI_SetGridColPixel(&status_icons_node, 5, 16);   // Bluetooth
    UI_SetGridColPixel(&status_icons_node, 6, 50);   // Battery

    status_bar_node.children[status_bar_node.children_count++] = &status_icons_node;


    // ==========================================
    // 4. ДОБАВЛЕНИЕ ИКОНОК КАК ОТДЕЛЬНЫХ ЭЛЕМЕНТОВ
    // ==========================================

    // --- Батарея ---
    status_icon_battery_node.type = UI_TYPE_SPRITE;
    status_icon_battery_node.grid_row = 0; // В стеке игнорируется
    status_icon_battery_node.grid_col = 6;
    status_icon_battery_node.render_callback = Draw_Icon_Battery_Callback;
    status_icon_battery_node.background_color = RGB565_BLACK;
    status_icon_battery_node.sprite = &status_icon_battery_sprite;
    status_icon_battery_node.w = 20; // Подстраивается стеком
    status_icon_battery_node.h = STATUS_BAR_HEIGHT;
    status_icon_battery_node.horizontal_alignment = HORIZONTAL_ALIGN_RIGHT; // Новое свойство для статуса
    status_icon_battery_node.vertical_alignment = VERTICAL_ALIGN_CENTER;

    status_icon_battery_sprite.w = 20;
    status_icon_battery_sprite.h = STATUS_BAR_HEIGHT;
    status_icon_battery_sprite.data = (uint16_t*)heap_caps_malloc(20 * STATUS_BAR_HEIGHT * 2, 0);
    status_icon_battery_sprite.is_allocated = (status_icon_battery_sprite.data != NULL);

    if (status_icon_battery_sprite.is_allocated) {
        status_icons_node.children[status_icons_node.children_count++] = &status_icon_battery_node;
    }

    // --- Bluetooth ---
    status_icon_bt_node.type = UI_TYPE_SPRITE;
    status_icon_bt_node.grid_row = 0;
    status_icon_bt_node.grid_col = 5;
    status_icon_bt_node.render_callback = Draw_Icon_Bluetooth_Callback;
    status_icon_bt_node.background_color = RGB565_BLACK;
    status_icon_bt_node.sprite = &status_icon_bt_sprite;
    status_icon_bt_node.w = 14;
    status_icon_bt_node.h = STATUS_BAR_HEIGHT;

    status_icon_bt_sprite.w = 14;
    status_icon_bt_sprite.h = STATUS_BAR_HEIGHT;
    status_icon_bt_sprite.data = (uint16_t*)heap_caps_malloc(14 * STATUS_BAR_HEIGHT * 2, 0);
    status_icon_bt_sprite.is_allocated = (status_icon_bt_sprite.data != NULL);

    if (status_icon_bt_sprite.is_allocated) {
        status_icons_node.children[status_icons_node.children_count++] = &status_icon_bt_node;
    }

    // --- Wi-Fi ---
    status_icon_wifi_node.type = UI_TYPE_SPRITE;
    status_icon_wifi_node.grid_row = 0;
    status_icon_wifi_node.grid_col = 4;
    status_icon_wifi_node.render_callback = Draw_Icon_WiFi_Callback;
    status_icon_wifi_node.background_color = RGB565_BLACK;
    status_icon_wifi_node.sprite = &status_icon_wifi_sprite;
    status_icon_wifi_node.w = 12;
    status_icon_wifi_node.h = STATUS_BAR_HEIGHT;

    status_icon_wifi_sprite.w = 12;
    status_icon_wifi_sprite.h = STATUS_BAR_HEIGHT;
    status_icon_wifi_sprite.data = (uint16_t*)heap_caps_malloc(12 * STATUS_BAR_HEIGHT * 2, 0);
    status_icon_wifi_sprite.is_allocated = (status_icon_wifi_sprite.data != NULL);

    if (status_icon_wifi_sprite.is_allocated) {
        status_icons_node.children[status_icons_node.children_count++] = &status_icon_wifi_node;
    }


    // --- NTP ---
    status_icon_ntp_node.type = UI_TYPE_SPRITE;
    status_icon_ntp_node.grid_row = 0;
    status_icon_ntp_node.grid_col = 3;
    status_icon_ntp_node.render_callback = Draw_Icon_NTP_Callback;
    status_icon_ntp_node.background_color = RGB565_BLACK;
    status_icon_ntp_node.sprite = &status_icon_ntp_sprite;
    status_icon_ntp_node.w = 12;
    status_icon_ntp_node.h = STATUS_BAR_HEIGHT;

    status_icon_ntp_sprite.w = 12;
    status_icon_ntp_sprite.h = STATUS_BAR_HEIGHT;
    status_icon_ntp_sprite.data = (uint16_t*)heap_caps_malloc(12 * STATUS_BAR_HEIGHT * 2, 0);
    status_icon_ntp_sprite.is_allocated = (status_icon_ntp_sprite.data != NULL);

    if (status_icon_ntp_sprite.is_allocated) {
        status_icons_node.children[status_icons_node.children_count++] = &status_icon_ntp_node;
    }

    // --- BUZZER ---
    status_icon_buzzer_node.type = UI_TYPE_SPRITE;
    status_icon_buzzer_node.grid_row = 0;
    status_icon_buzzer_node.grid_col = 2;
    status_icon_buzzer_node.render_callback = Draw_Icon_Buzzer_Callback;
    status_icon_buzzer_node.background_color = RGB565_BLACK;
    status_icon_buzzer_node.sprite = &status_icon_buzzer_sprite;
    status_icon_buzzer_node.w = 12;
    status_icon_buzzer_node.h = STATUS_BAR_HEIGHT;

    status_icon_buzzer_sprite.w = 12;
    status_icon_buzzer_sprite.h = STATUS_BAR_HEIGHT;
    status_icon_buzzer_sprite.data = (uint16_t*)heap_caps_malloc(12 * STATUS_BAR_HEIGHT * 2, 0);
    status_icon_buzzer_sprite.is_allocated = (status_icon_buzzer_sprite.data != NULL);

    if (status_icon_buzzer_sprite.is_allocated) {
        status_icons_node.children[status_icons_node.children_count++] = &status_icon_buzzer_node;
    }

    // --- RS485ToBt ---
    status_icon_mode_node.type = UI_TYPE_SPRITE;
    status_icon_mode_node.grid_row = 0;
    status_icon_mode_node.grid_col = 1;
    status_icon_mode_node.render_callback = Draw_Icon_RS485ToBT_Callback;
    status_icon_mode_node.background_color = RGB565_BLACK;
    status_icon_mode_node.sprite = &status_icon_mode_sprite;
    status_icon_mode_node.w = 12;
    status_icon_mode_node.h = STATUS_BAR_HEIGHT;

    status_icon_mode_sprite.w = 12;
    status_icon_mode_sprite.h = STATUS_BAR_HEIGHT;
    status_icon_mode_sprite.data = (uint16_t*)heap_caps_malloc(12 * STATUS_BAR_HEIGHT * 2, 0);
    status_icon_mode_sprite.is_allocated = (status_icon_mode_sprite.data != NULL);

    if (status_icon_mode_sprite.is_allocated) {
        status_icons_node.children[status_icons_node.children_count++] = &status_icon_mode_node;
    }

    // --- Другие иконки (NTP, Buzzer, Mode) добавляются аналогично ---
    // ...

    status_icon_spacer_node.type = UI_TYPE_TEXT_BLOCK;
    status_icon_spacer_node.grid_row = 0;
    status_icon_spacer_node.grid_col = 0;
    status_icon_spacer_node.sprite = &status_spacer_sprite;
    status_icon_spacer_node.render_callback = NULL;
    status_icon_spacer_node.background_color = RGB565_BLACK;
    status_icon_spacer_node.w = 0; // Размер задается сеткой
    status_icon_spacer_node.h = 0;
    status_icon_spacer_node.text_content[0] = '\0'; // Пустой текст

    if (status_icons_node.children_count < MAX_ELEMENT_CHILDREN) {
        status_icons_node.children[status_icons_node.children_count++] = &status_icon_spacer_node;
    }
}


/**
 * @brief Полностью инвариантный render_callback для статус-бара
 * // Функция для отрисовки контента внутри статус-бара
 */
/* void Draw_StatusBar_Callback(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    // Извлекаем физический спрайт из элемента
    Sprite_t* sprite = el->sprite; // Достаем физический спрайт из элемента

    // 1. Очистка буфера статус-бара цветом RGB565_DARK_GRAY (например, 0x39E7)
    Sprite_fill(sprite, RGB565_BLACK);

    // 2. Отрисовка ВРЕМЕНИ (слева, отступ 5 пикселей)
    lcd_set_font(&font_segoe_struct);
    char timeBuf[8];
    snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", currentHour, currentMinute);
    lcd_print_to_buffer(5, 4, RGB565_WHITE, timeBuf, RGB565_DARK_GRAY, sprite);

    // Меняем шрифт для системных иконок и параметров
    lcd_set_font(&font_arial_9_struct);

    // 3. Динамический инвариантный расчет правого края (смещаемся влево от правого конца спрайта s->w)
    int16_t curX = sprite->w - 5; // Стартуем в 5 пикселях от правого края экрана

    // --- Батарея (Текст) ---
    char batBuf[8]; 
    snprintf(batBuf, sizeof(batBuf), "%d%%", battery_Level);
    int batWidth = lcd_get_str_width(batBuf);
    curX -= batWidth; // Сдвигаем маркер влево на ширину текста
    lcd_print_to_buffer(curX, 4, RGB565_WHITE, batBuf, RGB565_DARK_GRAY, sprite);

    // --- Иконка батареи (ширина 16) ---
    curX -= 18; // 16 пикселей иконка + 2 пикселя зазор
    Draw_Bitmap_To_Sprite(sprite, curX, 4, icon_battery_16_16_bits, 16, 16, RGB565_WHITE);

    // --- Bluetooth Mode (RS485→BT) (ширина 10) ---
    curX -= 12;
    uint16_t bt_mode_color = bluetoothMode ? RGB565_GREEN : 0x528A; // Зеленый или блекло-серый
    Draw_Bitmap_To_Sprite(sprite, curX, 8, iconMirrorHorizontal(icon_rs485ToBt_bits, 8, 8), 8, 8, bt_mode_color);

    // --- Buzzer (ширина 8) ---
    curX -= 10;
    const uint8_t* icon_buzz = buzzerOnOff ? icon_buzzer_on_bits : icon_buzzer_off_bits;
    Draw_Bitmap_To_Sprite(sprite, curX, 8, iconMirrorHorizontal(icon_buzz, 8, 8), 8, 8, RGB565_WHITE);

    // --- Auto NTP (ширина 8) ---
    if (ntpSyncEnabled) {
        curX -= 10;
        Draw_Bitmap_To_Sprite(sprite, curX, 8, iconMirrorHorizontal(icon_ntp_bits, 8, 8), 8, 8, RGB565_WHITE);
    }

    // --- Wi-Fi (ширина 10) ---
    if (wifiEnabled) {
        curX -= 12;
        Draw_Bitmap_To_Sprite(sprite, curX, 8, iconMirrorHorizontal(icon_wifi_bits, 8, 8), 8, 8, RGB565_GREEN);
    }

    // --- Bluetooth (ширина 10) ---
    if (bluetoothEnabled) {
        curX -= 12;
        Draw_Bitmap_To_Sprite(sprite, curX, 8, iconMirrorHorizontal(icon_bluetooth_bits, 8, 8), 8, 8, RGB565_BLUE);
    }

    // ВНИМАНИЕ: ST7796_PushSprite(sprite) отсюда УДАЛЕН. 
    // Движок UI сам вызовет отправку по SPI после завершения сборки дерева.
}
 */