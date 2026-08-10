#include "gui.h"
#include "menu.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "measurement/measurement.h"

// Глобальные переменные сущностей интерфейса
UIElement_t root_grid;
UIElement_t status_bar_node;
UIElement_t main_work_grid;
UIElement_t graph_node;
UIElement_t digits_node;   // Наша правая панель (используется в MeasurementScreen)
UIElement_t digits_panel;  // Спрайт-контейнер панели (используется в GUI_BuildProInterface)
UIElement_t ui_bands_listbox; // Сам контейнер ListBox

// Флаг отладки: если true — рисуем границы и текстовые метки геометрии ListBox/scrollbar
bool ui_debug_draw = true;

/* Глобальные объекты спрайтов для экранов интерфейса */
//Sprite_t status_bar_sprite;
Sprite_t graph_sprite;
Sprite_t main_screen_sprite;


// Новые спрайты для частей статус-бара
// Предположим, статус-бар разделен на 3 зоны:
// 1. Левая: Часы
// 2. Центр: Доп. информация (Bluetooth, USB и т.д.)
// 3. Правая: Иконки (Батарея, Сеть)
Sprite_t* status_bar_clock_sprite = NULL;   // Для часов (обновляется часто)
Sprite_t* status_bar_icons_sprite = NULL;   // Для иконок (обновляется редко)
Sprite_t* status_bar_batery_sprite = NULL;   // Для правой части (если есть)


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

static void gui_measurement_callback(const MeasurementResults* r) {
    if (!r) return;
    if (ui_swr_row != NULL) {
        UI_SetText(ui_swr_row, "SWR: %.2f", r->swr);
    }
    // Пометим панели на перерисовку
    if (digits_node.sprite != NULL) GUI_InvalidateSprite(digits_node.sprite);
    if (graph_node.sprite != NULL) GUI_InvalidateSprite(graph_node.sprite);
}

void GUI_ShowAdvancedMeasurementScreen(uint8_t rotation) {
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
    //status_bar_sprite.data = NULL;
    graph_sprite.data = NULL;
    main_screen_sprite.data = NULL;
    
    //status_bar_sprite.is_allocated = false;
    graph_sprite.is_allocated = false;
    main_screen_sprite.is_allocated = false;
    
    // Сбрасываем счетчики строк и списка
    // ВАЖНО: Мы обнуляем логику, но старый контент в буферах остается до перерисовки.
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
    status_bar_node.type = UI_TYPE_TEXT_BLOCK;
    status_bar_node.grid_row = 0;
    status_bar_node.grid_col = 0;
    status_bar_node.sprite = &status_bar_sprite; 
    status_bar_node.render_callback = Draw_StatusBar_Callback;
    status_bar_node.background_color = RGB565_BLACK;
    status_bar_node.horizontal_alignment = HORIZONTAL_ALIGN_LEFT; // Пример
    status_bar_node.vertical_alignment   = VERTICAL_ALIGN_TOP;
    root_grid.children[root_grid.children_count++] = &status_bar_node;

    // --- ВЛОЖЕННАЯ СЕТКА (График + Панель) ---
    main_work_grid.type = UI_TYPE_GRID;
    main_work_grid.children_count = 0;
    main_work_grid.grid_row = 1;
    main_work_grid.grid_col = 0;
    main_work_grid.props.grid.rows_count = 1;
    main_work_grid.props.grid.cols_count = 2;
    
    /* UI_SetGridColPixel(&main_work_grid, 0, 200); 
    UI_SetGridColPercent(&main_work_grid, 1, 100);  */
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
        el->vertical_alignment   = HORIZONTAL_ALIGN_CENTER;
        el->background_color = RGB565_BLACK;
        el->h = 16;
    }

    // Динамические строки (Сохраняем в глобальные указатели)
    ui_swr_row = GUI_Panel_AddString(&digits_node, "SWR: 1.00"); 
    if(ui_swr_row) {
        ui_swr_row->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
        ui_swr_row->vertical_alignment   = HORIZONTAL_ALIGN_CENTER;
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
    /* // Жесткая высота для расчетной фазы
    ui_bands_listbox.h = 192; 
    ui_bands_listbox.background_color = RGB565_BLACK; */

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
    for (uint8_t i = 0; i < digits_node.children_count; i++) {
        if(digits_node.children[i]) {
            digits_node.children[i]->sprite = NULL;
        }
    }
    
    for (uint8_t i = 0; i < ui_bands_listbox.children_count; i++) {
        if(ui_bands_listbox.children[i]) {
            ui_bands_listbox.children[i]->sprite = NULL;
        }
    }
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
    for (uint8_t i = 0; i < ui_bands_listbox.children_count; i++) {
        UIElement_t* item = (UIElement_t*)ui_bands_listbox.children[i];
        if (item && item->sprite != NULL && item->sprite->data) {
             // Рисуем текст элемента в общий буфер панели
             // Убедимся, что у элемента есть координаты, назначенные MeasureAndArrange
             if (item->w > 0 && item->h > 0) {
                 Draw_GeneralText_Callback(item);
             }
        }
    }

    // Активируем флаги рендеринга для всех основных блоков
    status_bar_sprite.needs_render = true;
    graph_sprite.needs_render = true;
    main_screen_sprite.needs_render = true;

    // Сбрасываем Dirty Rect на весь размер
    GUI_InvalidateAll(&root_grid);
    
    // Полная отрисовка
    UI_DrawTree(&root_grid);

    // Подписываем GUI на обновления измерений
    Measurement_Subscribe(gui_measurement_callback);
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

/* void GUI_BuildProInterface(void) {
    // 1. Поворачиваем железо дисплея и сбрасываем статический пул памяти
    ST7796_SetRotation(1); // Ландшафтный режим
    heap_caps_reset_pool();
    
    // Сбрасываем счетчик глобального пула текстовых строк
    panel_rows_count = 0; 

    // 2. Настраиваем StackPanel для правой стороны экрана
    digits_panel.type = UI_TYPE_STACK_PANEL;
    digits_panel.sprite = &main_screen_sprite;
    digits_panel.props.stack.orientation = ORIENTATION_VERTICAL;
    digits_panel.props.stack.spacing = 6;
    digits_panel.children_count = 0; // ИСПРАВЛЕНО: Сбрасываем счетчик детей самого контейнера

    // 3. ДОБАВЛЯЕМ КОМПОНЕНТЫ ПОТОКОМ В СТЕК-ПАНЕЛЬ

    // --- Добавляем TextBlock (Заголовок меню) ---
    UIElement_t* title = &panel_rows[panel_rows_count++];
    title->type = UI_TYPE_TEXT_BLOCK;
    title->sprite = &main_screen_sprite;
    title->children_count = 0;
    title->render_callback = NULL;
    title->horizontal_alignment = HORIZONTAL_ALIGN_CENTER;
    title->vertical_alignment = VERTICAL_ALIGN_CENTER;
    strcpy(title->text_content, "PRO MENU");
    // ИСПРАВЛЕНО: Записываем в массив детей контейнера, используя его личный счетчик fields!
    digits_panel.children[digits_panel.children_count++] = title;

    // --- Добавляем интерактивную Кнопку (Button) ---
    UIElement_t* start_btn = &panel_rows[panel_rows_count++];
    start_btn->type = UI_TYPE_BUTTON;
    start_btn->sprite = &main_screen_sprite;
    start_btn->children_count = 0;
    start_btn->render_callback = NULL;
    start_btn->horizontal_alignment = HORIZONTAL_ALIGN_CENTER;
    start_btn->vertical_alignment = VERTICAL_ALIGN_CENTER;
    start_btn->props.button.is_pressed = false;
    start_btn->props.button.normal_color = 0x10A5; // Темно-синий цвет
    start_btn->props.button.press_color  = 0xF800; // Ярко-красный цвет при клике
    strcpy(start_btn->text_content, "START SCAN");
    digits_panel.children[digits_panel.children_count++] = start_btn;

    // --- Добавляем флажок CheckBox (Звук зуммера) ---
    UIElement_t* sound_check = &panel_rows[panel_rows_count++];
    sound_check->type = UI_TYPE_CHECK_BOX;
    sound_check->sprite = &main_screen_sprite;
    sound_check->children_count = 0;
    sound_check->render_callback = NULL;
    sound_check->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    sound_check->vertical_alignment = VERTICAL_ALIGN_CENTER;
    sound_check->props.toggle.is_checked = true; // включен по умолчанию
    strcpy(sound_check->text_content, "Sound ON");
    digits_panel.children[digits_panel.children_count++] = sound_check;

    // 4. МЕНЕДЖЕР РАЗМЕТКИ: Рекурсивно рассчитываем геометрию всего дерева UI
    extern uint16_t Display_Width;
    extern uint16_t Display_Height;
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);

    // Принудительно очищаем правый буфер перед выводом дерева
    if (main_screen_sprite.is_allocated && main_screen_sprite.data != NULL) {
        uint32_t total_pixels = (uint32_t)main_screen_sprite.w * main_screen_sprite.h;
        memset(main_screen_sprite.data, 0, total_pixels * 2); 
    }

    // 5. Полная отрисовка и вывод грязных зон на экран
    GUI_InvalidateAll(&root_grid);
    UI_DrawTree(&root_grid);
} */

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

uint8_t currentHour = 07;
uint8_t currentMinute = 05;
uint8_t battery_Level = 100;
static void Draw_Bitmap_To_Sprite(Sprite_t* s, int16_t x, int16_t y, const uint8_t* bitmap, uint16_t bmp_w, uint16_t bmp_h, uint16_t color);
/**
 * @brief Полностью инвариантный render_callback для статус-бара
 * // Функция для отрисовки контента внутри статус-бара
 */
void Draw_StatusBar_Callback(UIElement_t* el) {
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

/**
 * @brief Функция перерисовки статус-бара с разделением на области
 */
void GUI_DrawStatusBar(StatusBar_t* sb_area) {
    if (!sb_area) return;

    uint16_t y = 0; // y координата статус-бара
    uint16_t width = sb_area->width;
    uint16_t height = STATUS_BAR_HEIGHT;


    // 1. Отрисовка часов (спрайт 1)
    if (update_flags & UPDATE_FLAGS_CLOCK) {
        if (status_bar_clock_sprite) {
            // Очищаем область в спрайте часов
            GUI_ClearRect(status_bar_clock_sprite, 0, 0, status_bar_clock_sprite->w, status_bar_clock_sprite->h);
            // Рисуем текущее время
            char time_str[16];
            RTC_GetTime(time_str); // Пример функции получения времени
            GUI_DrawText(status_bar_clock_sprite, time_str, width/2, 2, lcd_current_font, STATUS_BAR_BG_COLOR, LCD_WHITE);
            // Помечаем спрайт часов для отправки
            GUI_InvalidateSprite(status_bar_clock_sprite);
        }
    }
    // 2. Отрисовка иконок (спрайт 2)
    if (update_flags & UPDATE_FLAGS_ICONS) {
        if (status_bar_icons_sprite) {
            // Очищаем область в спрайте иконок
            GUI_ClearRect(status_bar_icons_sprite, 0, 0, status_bar_icons_sprite->w, status_bar_icons_sprite->h);
            // Рисуем иконки в спрайте иконок
            drawBatteryIcon(status_bar_icons_sprite, 5, 2);
            drawNetworkIcon(status_bar_icons_sprite, 20, 2);
            // Помечаем спрайт для отправки по SPI
            GUI_InvalidateSprite(status_bar_icons_sprite);
        }
    }
    // 3. Отрисовка правой части (спрайт 3) - если нужна
    if(update_flags & UPDATE_FLAGS_BATERY)
        if (status_bar_batery_sprite) {
            // Очищаем область в спрайте иконок
            GUI_ClearRect(status_bar_batery_sprite, 0, 0, status_bar_batery_sprite->w, status_bar_batery_sprite->h);
            // Рисуем иконки в спрайте иконок
            drawBatteryIcon(status_bar_batery_sprite, 5, 2);
            drawNetworkIcon(status_bar_batery_sprite, 20, 2);
            // Помечаем спрайт для отправки по SPI
            GUI_InvalidateSprite(status_bar_batery_sprite);
        }
}



/**
 * @brief Безопасная инвариантная прорисовка битмапа внутрь спрайта
 */
static void Draw_Bitmap_To_Sprite(Sprite_t* s, int16_t x, int16_t y, const uint8_t* bitmap, uint16_t bmp_w, uint16_t bmp_h, uint16_t color) {
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
    el->props.list_box.pixel_height = 192;     // можно переопределить позже
    // Устанавливаем шрифт по умолчанию для ListBox
    el->font = &font_arial_9_struct;
}

/**
 * @brief Добавляет строку (пункт списка) внутрь ListBox
 */
UIElement_t* UI_ListBox_AddItem(UIElement_t* listbox, const char* item_text) {
    if (panel_rows_count >= MAX_PANEL_ROWS || listbox->children_count >= MAX_PANEL_ROWS) return NULL;
    UIElement_t* item = &panel_rows[panel_rows_count++];
    item->type = UI_TYPE_TEXT_BLOCK;
    item->sprite = listbox->sprite;
    item->render_callback = NULL;
    item->children_count = 0;
    item->x = 0;
    item->y = 0;
    item->w = 0;
    item->h = 0;
    item->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    item->vertical_alignment = VERTICAL_ALIGN_CENTER;
    // Устанавливаем шрифт по умолчанию для ListBox
    item->font = &font_arial_9_struct;
    strncpy(item->text_content, item_text, sizeof(item->text_content) - 1);
    item->text_content[sizeof(item->text_content) - 1] = '\0';
    listbox->children[listbox->children_count++] = item;
    return item;
}

//Функция рисует рамку, подсвечивает выбранный пункт (0x10A5) и выводит текст
void UI_RenderListBox(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    Sprite_t* s = el->sprite;
    
    // Локальные координаты самого ListBox внутри его физического спрайта
    int16_t lx = el->x - s->x;
    int16_t ly = el->y - s->y;

    // 1. Очистка фона ListBox и отрисовка серой рамки по краям
    for (int16_t y = ly; y < ly + el->h; y++) {
        if (y < 0 || y >= s->h) continue;
        for (int16_t x = lx; x < lx + el->w; x++) {
            if (x < 0 || x >= s->w) continue;
            s->data[y * s->w + x] = (y == ly || y == ly + el->h - 1 || x == lx || x == lx + el->w - 1) ? 0x7BEF : RGB565_BLACK;
        }
    }

    lcd_set_font(&font_arial_9_struct);
    uint16_t item_h = current_font->char_height + 6;
    uint8_t start = el->props.list_box.scroll_offset;
    uint8_t visible_count = (el->h + item_h - 1) / item_h;
    if (visible_count == 0) visible_count = 1;

    // Ширина зарезервированной зоны скроллбара (макс 10px), видима для логики отрисовки текста
    uint8_t scrollbar_w = 10;
    if (scrollbar_w > el->w - 2) scrollbar_w = (el->w > 2) ? (el->w - 2) : 0;

    // 2. Отрисовка видимых элементов списка
    for (uint8_t i = 0; i < el->children_count; i++) {
        if (i >= start && i < (start + visible_count)) {
            UIElement_t* child = (UIElement_t*)el->children[i];
            
            int16_t row_y = el->y + (int16_t)((i - start) * item_h);
            int16_t clx = el->x - s->x;
            int16_t cly = row_y - s->y;
            
            uint16_t bg = (i == el->props.list_box.selected_index) ? 0x10A5 : RGB565_BLACK;

            // Вычисляем ширину контента так, чтобы она не пересекалась со скроллбаром справа
            uint16_t content_w = el->w;
            // Если скроллбар зарезервирован — отнимаем его ширину и небольшие отступы
            if (scrollbar_w > 0 && scrollbar_w < el->w) {
                if (el->w > (int)scrollbar_w + 2) content_w = el->w - scrollbar_w - 2;
                else content_w = el->w - 2;
            } else {
                content_w = el->w - 2;
            }
            if (content_w < 8) content_w = (el->w > 4) ? (el->w - 4) : el->w;

            child->w = content_w;
            child->h = item_h;
            child->x = el->x;
            child->y = row_y;

            // Заливаем прямоугольник элемента выбранным цветом фона, только в области content_w
            for (int16_t y = cly; y < cly + child->h; y++) {
                for (int16_t x = clx + 1; x < clx + (int16_t)content_w - 1; x++) {
                    // защита границ спрайта
                    if (y < 0 || y >= s->h || x < 0 || x >= s->w) continue;
                    s->data[y * s->w + x] = bg;
                }
            }

            // Вычисляем точные локальные координаты для текста внутри спрайта
            int16_t text_x = clx + 5; // небольшой отступ слева в 5 пикселей
            int16_t text_y = cly + (child->h - current_font->char_height) / 2; // центрируем по вертикали

            // Печатаем текст в рамках content_w
            lcd_print_to_buffer(text_x, text_y, RGB565_WHITE, child->text_content, bg, s);
        } else {
            // Скрытые элементы скролла тоже сбрасываем в 0, чтобы они не рисовались
            UIElement_t* child = (UIElement_t*)el->children[i];
            child->w = 0;
            child->h = 0;
        }
    }

    // Отрисовка скроллбара (справа внутри ListBox)
    if (el->children_count > visible_count && scrollbar_w > 0) {
        int16_t tx0 = lx + el->w - scrollbar_w;
        int16_t tx1 = lx + el->w - 1;

        // Ограничиваем координаты трека пределами самого спрайта (защита от выхода за границу)
        if (tx0 < 0) tx0 = 0;
        if (tx1 >= s->w) tx1 = s->w - 1;
        if (tx0 < lx) tx0 = lx; // держим внутри ListBox
        if (tx1 > lx + el->w - 1) tx1 = lx + el->w - 1;

        // Рисуем трек (тёмно-серый, использует константу движка)
        for (int16_t y = ly + 1; y < ly + el->h - 1; y++) {
            if (y < 0 || y >= s->h) continue;
            for (int16_t x = tx0; x <= tx1; x++) {
                if (x < 0 || x >= s->w) continue;
                s->data[y * s->w + x] = RGB565_DARK_GRAY;
            }
        }

        // Вычисляем размер ползунка (thumb)
        uint8_t max_offset = (el->children_count > visible_count) ? (el->children_count - visible_count) : 0;
        uint16_t thumb_h;
        if (el->children_count == 0) {
            thumb_h = el->h;
        } else if (el->children_count <= visible_count) {
            thumb_h = el->h; // полный ползунок — все элементы видимы
        } else {
            thumb_h = (uint16_t)visible_count * el->h / el->children_count;
            if (thumb_h < 12) thumb_h = 12; // минимальный размер ползунка
            if (thumb_h > el->h) thumb_h = el->h;
        }

        // Позиция ползунка
        int16_t thumb_y = ly;
        if (max_offset > 0) {
            thumb_y = ly + (int16_t)((el->props.list_box.scroll_offset * (el->h - thumb_h)) / max_offset);
        }
        if (thumb_y < ly) thumb_y = ly;
        if (thumb_y + (int16_t)thumb_h > ly + (int16_t)el->h) thumb_y = ly + el->h - thumb_h;

        // Рисуем сам ползунок (яркий цвет для заметности), но в пределах спрайта
        for (int16_t y = thumb_y; y < thumb_y + (int16_t)thumb_h; y++) {
            if (y < 0 || y >= s->h) continue;
            for (int16_t x = tx0 + 2; x < tx1 - 1; x++) {
                if (x < 0 || x >= s->w) continue;
                s->data[y * s->w + x] = RGB565_YELLOW;
            }
        }
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

//Обработчик тача, меняющий selected_index и вызывающий GUI_InvalidateRect для перерисовки
/* int8_t UI_ListBox_ProcessTouch(UIElement_t* listbox, uint16_t tx, uint16_t ty) {
    if (!listbox || listbox->type != UI_TYPE_LIST_BOX || !listbox->sprite) return -1;
    if (tx < listbox->x || tx >= (listbox->x + listbox->w) || ty < listbox->y || ty >= (listbox->y + listbox->h)) {
        g_listbox_drag_active = false;
        g_listbox_drag_last_y = -1;
        return -1;
    }

    uint16_t font_h = (current_font != NULL) ? current_font->char_height : font_arial_9_struct.char_height;
    uint16_t item_h = font_h + 6;
    int16_t local_y = ty - listbox->y;
    if (local_y < 0 || local_y >= listbox->h) return -1;

    uint8_t visible = (listbox->h + item_h - 1) / item_h;
    if (visible == 0) visible = 1;

    // Обработка клика по области скроллбара (правая полоса)
    uint8_t scrollbar_w = 10;
    if (scrollbar_w > listbox->w - 2) scrollbar_w = (listbox->w > 2) ? (listbox->w - 2) : 0;
    if (scrollbar_w > 0 && tx >= (listbox->x + listbox->w - scrollbar_w)) {
        uint8_t children = listbox->children_count;
        if (children <= visible) return -1;

        uint8_t max_offset = (children > visible) ? (children - visible) : 0;
        uint16_t thumb_h = (uint16_t)visible * listbox->h / children;
        if (thumb_h < 12) thumb_h = 12;
        if (thumb_h > listbox->h) thumb_h = listbox->h;

        int16_t rel_y = ty - listbox->y;
        int32_t track_span = (int32_t)listbox->h - (int32_t)thumb_h;
        if (track_span <= 0) return -1;

        int32_t pos = rel_y - (thumb_h / 2);
        if (pos < 0) pos = 0;
        if (pos > track_span) pos = track_span;

        uint8_t new_offset = (uint8_t)((pos * max_offset + track_span/2) / track_span);
        if (new_offset != listbox->props.list_box.scroll_offset) {
            listbox->props.list_box.scroll_offset = new_offset;
            GUI_InvalidateRect(listbox->sprite, listbox->x - listbox->sprite->x, listbox->y - listbox->sprite->y, listbox->w, listbox->h);
        }
        return -1;
    }

    if (!g_listbox_drag_active) {
        g_listbox_drag_active = true;
        g_listbox_drag_last_y = ty;
        int8_t target = listbox->props.list_box.scroll_offset + (local_y / item_h);
        if (target < 0 || target >= listbox->children_count) return -1;
        if (listbox->props.list_box.selected_index != target) {
            listbox->props.list_box.selected_index = target;
            GUI_InvalidateRect(listbox->sprite, listbox->x - listbox->sprite->x, listbox->y - listbox->sprite->y, listbox->w, listbox->h);
        }
        return target;
    }

    int16_t delta_y = ty - g_listbox_drag_last_y;
    g_listbox_drag_last_y = ty;
    if (delta_y != 0) {
        uint8_t max_offset = (listbox->children_count > visible) ? (listbox->children_count - visible) : 0;
        if (max_offset > 0) {
            int8_t step = delta_y / (int16_t)item_h;
            if (step != 0) {
                int16_t new_offset = (int16_t)listbox->props.list_box.scroll_offset - step;
                if (new_offset < 0) new_offset = 0;
                if (new_offset > (int16_t)max_offset) new_offset = max_offset;
                if ((uint8_t)new_offset != listbox->props.list_box.scroll_offset) {
                    listbox->props.list_box.scroll_offset = (uint8_t)new_offset;
                    GUI_InvalidateRect(listbox->sprite, listbox->x - listbox->sprite->x, listbox->y - listbox->sprite->y, listbox->w, listbox->h);
                }
            }
        }
    }
    return -1;
} */

//Обработчик тача, меняющий selected_index и вызывающий GUI_InvalidateRect для перерисовки
int8_t UI_ListBox_ProcessTouch(UIElement_t* listbox, uint16_t tx, uint16_t ty) {
    if (!listbox || listbox->type != UI_TYPE_LIST_BOX || !listbox->sprite) return -1;
    if (tx < listbox->x || tx >= (listbox->x + listbox->w) || ty < listbox->y || ty >= (listbox->y + listbox->h)) {
        g_listbox_drag_active = false;
        g_listbox_drag_last_y = -1;
        return -1;
    }

    uint16_t font_h = (current_font != NULL) ? current_font->char_height : font_arial_9_struct.char_height;
    uint16_t item_h = font_h + 6;
    int16_t local_y = ty - listbox->y;
    if (local_y < 0 || local_y >= listbox->h) return -1;

    // Обработка клика по области скроллбара (правая полоса)
    uint8_t scrollbar_w = 10; // резервируем до 10 пикселей справа под скроллбар
    if (scrollbar_w > listbox->w - 2) scrollbar_w = (listbox->w > 2) ? (listbox->w - 2) : 0;
    if (scrollbar_w > 0 && tx >= (listbox->x + listbox->w - scrollbar_w)) {
        uint8_t children = listbox->children_count;
        uint8_t visible = listbox->h / item_h;
        if (visible == 0) visible = 1;
        if (children <= visible) return -1;

        uint8_t max_offset = (children > visible) ? (children - visible) : 0;
        uint16_t thumb_h = (uint16_t)visible * listbox->h / children;
        if (thumb_h < 12) thumb_h = 12;
        if (thumb_h > listbox->h) thumb_h = listbox->h;

        int16_t rel_y = ty - listbox->y;
        int32_t track_span = (int32_t)listbox->h - (int32_t)thumb_h;
        if (track_span <= 0) return -1;

        int32_t pos = rel_y - (thumb_h / 2);
        if (pos < 0) pos = 0;
        if (pos > track_span) pos = track_span;

        uint8_t new_offset = (uint8_t)((pos * max_offset + track_span/2) / track_span);
        if (new_offset != listbox->props.list_box.scroll_offset) {
            listbox->props.list_box.scroll_offset = new_offset;
            GUI_InvalidateRect(listbox->sprite, listbox->x - listbox->sprite->x, listbox->y - listbox->sprite->y, listbox->w, listbox->h);
        }
        return -1;
    }

    if (g_listbox_drag_last_y < 0) {
        g_listbox_drag_last_y = ty;
        int8_t target = listbox->props.list_box.scroll_offset + (local_y / item_h);
        if (target < 0 || target >= listbox->children_count) return -1;
        if (listbox->props.list_box.selected_index != target) {
            listbox->props.list_box.selected_index = target;
            GUI_InvalidateRect(listbox->sprite, listbox->x - listbox->sprite->x, listbox->y - listbox->sprite->y, listbox->w, listbox->h);
        }
        return target;
    }

    if (!g_listbox_drag_active) {
        int16_t delta_y = ty - g_listbox_drag_last_y;
        if (delta_y < 0) delta_y = -delta_y;
        if (delta_y > (int16_t)item_h / 2) {
            g_listbox_drag_active = true;
        }
    }

    if (g_listbox_drag_active) {
        int16_t delta_y = ty - g_listbox_drag_last_y;
        g_listbox_drag_last_y = ty;
        if (delta_y != 0) {
            uint8_t visible = (listbox->h / item_h);
            if (visible == 0) visible = 1;
            uint8_t max_offset = (listbox->children_count > visible) ? (listbox->children_count - visible) : 0;
            if (max_offset > 0) {
                int8_t step = delta_y / (int16_t)item_h;
                if (step != 0) {
                    int16_t new_offset = (int16_t)listbox->props.list_box.scroll_offset - step;
                    if (new_offset < 0) new_offset = 0;
                    if (new_offset > (int16_t)max_offset) new_offset = max_offset;
                    if ((uint8_t)new_offset != listbox->props.list_box.scroll_offset) {
                        listbox->props.list_box.scroll_offset = (uint8_t)new_offset;
                        GUI_InvalidateRect(listbox->sprite, listbox->x - listbox->sprite->x, listbox->y - listbox->sprite->y, listbox->w, listbox->h);
                    }
                }
            }
        }
        return -1;
    }

    g_listbox_drag_last_y = ty;
    int8_t target = listbox->props.list_box.scroll_offset + (local_y / item_h);
    if (target < 0 || target >= listbox->children_count) return -1;

    if (listbox->props.list_box.selected_index != target) {
        listbox->props.list_box.selected_index = target;
        GUI_InvalidateRect(listbox->sprite, listbox->x - listbox->sprite->x, listbox->y - listbox->sprite->y, listbox->w, listbox->h);
    }
    return target;
}


