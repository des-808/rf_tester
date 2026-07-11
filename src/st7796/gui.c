#include "gui.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


// Глобальные переменные сущностей интерфейса
UIElement_t root_grid;
UIElement_t status_bar_node;
UIElement_t main_work_grid;
UIElement_t graph_node;
UIElement_t digits_node;   // Наша правая панель (используется в MeasurementScreen)
UIElement_t digits_panel;  // Спрайт-контейнер панели (используется в GUI_BuildProInterface)

// Реальные структуры спрайтов LovyanGFX/Си
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

void GUI_ShowAdvancedMeasurementScreen(uint8_t rotation) {
    // 1. Поворачиваем железо дисплея и полностью очищаем старый пул памяти
    ST7796_SetRotation(rotation);
    heap_caps_reset_pool();
    
    // Сбрасываем счетчик динамического пула элементов перед сборкой нового экрана
    panel_rows_count = 0; 

    // 2. Настраиваем корневую сетку (Разделение по вертикали: 10% и 90%)
    root_grid.type = UI_TYPE_GRID;
    root_grid.children_count = 0;
    root_grid.props.grid.rows_count = 2;
    root_grid.props.grid.cols_count = 1;
    root_grid.props.grid.row_definitions[0] = 10; // 10% под статус-бар
    root_grid.props.grid.row_definitions[1] = 90; // 90% под рабочую зону
    root_grid.props.grid.col_definitions[0] = 100;

    // 3. Подключаем Статус-бар в ячейку (строка 0, колонка 0)
    status_bar_node.type = UI_TYPE_TEXT_BLOCK; // ИСПРАВЛЕНО: вместо UI_TYPE_SPRITE
    status_bar_node.sprite = &status_bar_sprite;
    status_bar_node.render_callback = Draw_StatusBar_Callback; 
    status_bar_node.grid_row = 0;
    status_bar_node.grid_col = 0;
    root_grid.children[root_grid.children_count++] = &status_bar_node;

    // 4. Создаем вложенную сетку для разделения Графика и Цифр
    // Сажаем её в нижнюю ячейку корневой сетки (строка 1, column 0)
    main_work_grid.type = UI_TYPE_GRID;
    main_work_grid.children_count = 0;
    main_work_grid.grid_row = 1;
    main_work_grid.grid_col = 0;
    main_work_grid.props.grid.rows_count = 1;
    main_work_grid.props.grid.cols_count = 2;
    main_work_grid.props.grid.row_definitions[0] = 100; // Вся высота рабочей зоны
    main_work_grid.props.grid.col_definitions[0] = 70;  // 70% ширины под График
    main_work_grid.props.grid.col_definitions[1] = 30;  // 30% ширины под цифры
    root_grid.children[root_grid.children_count++] = &main_work_grid;

    // 5. Сажаем спрайт Графика во вложенную сетку (0, 0) — левая часть
    graph_node.type = UI_TYPE_TEXT_BLOCK; // ИСПРАВЛЕНО: вместо UI_TYPE_SPRITE
    graph_node.sprite = &graph_sprite;
    graph_node.render_callback = Draw_Graph_Content; 
    graph_node.grid_row = 0;
    graph_node.grid_col = 0;
    main_work_grid.children[main_work_grid.children_count++] = &graph_node;

    // 6. Настраиваем правую панель как STACK_PANEL (Вертикальный список)
    digits_node.type = UI_TYPE_STACK_PANEL;
    digits_node.sprite = &main_screen_sprite; // Физический спрайт для отрисовки
    digits_node.props.stack.orientation = ORIENTATION_VERTICAL;
    digits_node.props.stack.spacing = 0; // Зазор 0 пикселей между строками
    digits_node.grid_row = 0; 
    digits_node.grid_col = 1; // Сажаем в правую колонку Grid
    digits_node.children_count = 0; // Сбрасываем детей перед заполнением
    main_work_grid.children[main_work_grid.children_count++] = &digits_node;

    // 7. НАБИРАЕМ ТЕКСТ ПОТОКОМ ЧЕРЕЗ СТАНДАРТНЫЙ ПУЛ КОМПОНЕНТОВ (MAX_PANEL_ROWS = 8)
    UIElement_t* el;

    // --- Строка 1: Пояснение "ИЗМЕРЕНИЯ" ---
    el = &panel_rows[panel_rows_count++];
    el->type = UI_TYPE_TEXT_BLOCK;
    el->sprite = &main_screen_sprite;
    el->render_callback = NULL; // Ручной колбэк не нужен, отработает автоматика TextBlock!
    el->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    el->vertical_alignment = VERTICAL_ALIGN_CENTER;
    strcpy(el->text_content, "-ИЗМЕРЕНИЯ-");
    digits_node.children[digits_node.children_count++] = el;

    // --- Строка 2: Динамическое значение КСВ ---
    ui_swr_row = &panel_rows[panel_rows_count++];
    ui_swr_row->type = UI_TYPE_TEXT_BLOCK;
    ui_swr_row->sprite = &main_screen_sprite;
    ui_swr_row->render_callback = NULL;
    ui_swr_row->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    ui_swr_row->vertical_alignment = VERTICAL_ALIGN_CENTER;
    strcpy(ui_swr_row->text_content, "SWR: 1.00");
    digits_node.children[digits_node.children_count++] = ui_swr_row;

    // --- Строка 3: Разделитель ---
    el = &panel_rows[panel_rows_count++];
    el->type = UI_TYPE_TEXT_BLOCK;
    el->sprite = &main_screen_sprite;
    el->render_callback = NULL;
    el->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    el->vertical_alignment = VERTICAL_ALIGN_TOP;
    strcpy(el->text_content, "-----------");
    digits_node.children[digits_node.children_count++] = el;

    // --- Строка 4: Пояснение "АППАРАТУРА" ---
    el = &panel_rows[panel_rows_count++];
    el->type = UI_TYPE_TEXT_BLOCK;
    el->sprite = &main_screen_sprite;
    el->render_callback = NULL;
    el->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    el->vertical_alignment = VERTICAL_ALIGN_CENTER;
    strcpy(el->text_content, "АППАРАТУРА");
    digits_node.children[digits_node.children_count++] = el;

    // --- Строка 5: Динамический статус физических кнопок ---
    ui_btn_row = &panel_rows[panel_rows_count++];
    ui_btn_row->type = UI_TYPE_TEXT_BLOCK;
    ui_btn_row->sprite = &main_screen_sprite;
    ui_btn_row->render_callback = NULL;
    ui_btn_row->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    ui_btn_row->vertical_alignment = VERTICAL_ALIGN_CENTER;
    strcpy(ui_btn_row->text_content, "No btn");
    digits_node.children[digits_node.children_count++] = ui_btn_row;

    // --- Строка 6: Динамический статус тачскрина ---
    ui_touch_row = &panel_rows[panel_rows_count++];
    ui_touch_row->type = UI_TYPE_TEXT_BLOCK;
    ui_touch_row->sprite = &main_screen_sprite;
    ui_touch_row->render_callback = NULL;
    ui_touch_row->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    ui_touch_row->vertical_alignment = VERTICAL_ALIGN_CENTER;
    strcpy(ui_touch_row->text_content, "No touch");
    digits_node.children[digits_node.children_count++] = ui_touch_row;

    // --- Строка 7: Версия прошивки ---
    el = &panel_rows[panel_rows_count++];
    el->type = UI_TYPE_TEXT_BLOCK;
    el->sprite = &main_screen_sprite;
    el->render_callback = NULL;
    el->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    el->vertical_alignment = VERTICAL_ALIGN_CENTER;
    strcpy(el->text_content, "v1.0.0 Stable");
    digits_node.children[digits_node.children_count++] = el;

    // 8. ЗАПУСК КОНТЕЙНЕРОВ: Расчет размеров по всему дереву компонентов
    extern uint16_t Display_Width;
    extern uint16_t Display_Height;
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);

    // Принудительно очищаем правый видеобуфер перед первым выводом дерева
    if (main_screen_sprite.is_allocated && main_screen_sprite.data != NULL) {
        uint32_t total_pixels = (uint32_t)main_screen_sprite.w * main_screen_sprite.h;
        memset(main_screen_sprite.data, 0, total_pixels * 2); 
    }

    // 9. ЗАПУСК ОТРИСОВКИ: Полная валидация грязных зон и вывод дерева на экран
    GUI_InvalidateAll(&root_grid);
    UI_DrawTree(&root_grid);
}


void GUI_BuildProInterface(void) {
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
}

void UI_MeasureAndArrange(UIElement_t* element, int16_t parent_x, int16_t parent_y, uint16_t available_w, uint16_t available_h) {
    if (!element) return;

    // Базовые координаты
    element->x = parent_x; 
    element->y = parent_y;
    element->w = available_w; 
    element->h = available_h;

    // ШАГ 1 & 2: Выделение памяти (Leafs / Containers)
    if (element->children_count == 0 && element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        if (s->data == NULL) {
            s->x = element->x; s->y = element->y;
            s->w = element->w; s->h = element->h;
            s->data = (uint16_t*)heap_caps_malloc(s->w * s->h * 2, 0);
            s->is_allocated = (s->data != NULL);
            if (!s->is_allocated) { while(1); }
        }
        return; 
    }

    if (element->type == UI_TYPE_STACK_PANEL || element->type == UI_TYPE_LIST_BOX) {
        if (element->sprite != NULL && element->sprite->data == NULL) {
            Sprite_t* s = element->sprite;
            s->x = element->x; s->y = element->y;
            s->w = element->w; s->h = element->h;
            s->data = (uint16_t*)heap_caps_malloc(s->w * s->h * 2, 0);
            s->is_allocated = (s->data != NULL);
            if (!s->is_allocated) { while(1); }
        }
    }

    // ШАГ 3: Математика StackPanel
    if (element->type == UI_TYPE_STACK_PANEL) {
        int16_t cur_x = element->x, cur_y = element->y;
        uint16_t font_h = (current_font != NULL) ? current_font->char_height : 16;
        for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
            UI_MeasureAndArrange((UIElement_t*)element->children[i], cur_x, cur_y, element->w, font_h);
            cur_y += font_h + element->props.stack.spacing;
        }
    }

    // ШАГ 4: Математика ListBox (с учетом скролла)
    if (element->type == UI_TYPE_LIST_BOX) {
        int16_t cur_y = element->y;
        uint16_t font_h = (current_font != NULL) ? current_font->char_height : 16;
        uint16_t item_h = font_h + 6;
        uint8_t start = element->props.list_box.scroll_offset;
        uint8_t visible = element->h / item_h;
        for (uint8_t i = 0; i < element->children_count; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            if (i >= start && i < (start + visible)) {
                UI_MeasureAndArrange(child, element->x, cur_y, element->w, item_h);
                cur_y += item_h;
            } else {
                UI_MeasureAndArrange(child, 0, 0, 0, 0); // Скрываем
            }
        }
    }

    // ЕСЛИ ЭТО GRID
    if (element->type == UI_TYPE_GRID) {
        GridDefinition_t* grid = &element->props.grid;
        uint16_t row_heights[8] = {0};
        uint16_t col_widths[8] = {0};

        for (uint8_t r = 0; r < grid->rows_count && r < 8; r++) {
            row_heights[r] = (element->h * grid->row_definitions[r]) / 100;
        }
        for (uint8_t c = 0; c < grid->cols_count && c < 8; c++) {
            col_widths[c] = (element->w * grid->col_definitions[c]) / 100;
        }

        for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
            UIElement_t* child = (UIElement_t*)element->children[i];
            int16_t cell_x = element->x;
            int16_t cell_y = element->y;
            
            for (uint8_t c = 0; c < child->grid_col && c < 8; c++) cell_x += col_widths[c];
            for (uint8_t r = 0; r < child->grid_row && r < 8; r++) cell_y += row_heights[r];

            uint16_t cell_w = col_widths[child->grid_col];
            uint16_t cell_h = row_heights[child->grid_row];

            UI_MeasureAndArrange(child, cell_x, cell_y, cell_w, cell_h);
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

                    // КРИТИЧЕСКИЙ ФИКС: Если это контейнеры, и они затребовали рендер,
                    // принудительно заставляем всех их детей (текстовые блоки) перерисовать себя в ОЗУ!
                    case UI_TYPE_STACK_PANEL:
                    case UI_TYPE_GRID:
                        for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
                            UIElement_t* child = (UIElement_t*)element->children[i];
                            // Если у ребенка тип TEXT_BLOCK — вызываем его отрисовку
                            if (child->type == UI_TYPE_TEXT_BLOCK) {
                                Draw_GeneralText_Callback(child);
                            } else if (child->render_callback != NULL) {
                                child->render_callback(child);
                            }
                        }
                        break;
                    case UI_TYPE_LIST_BOX:
                        UI_RenderListBox(element);
                        break;

                    default: break;
                }
            }

            // Если это StackPanel — принудительно просим всех детей (текстовые строки)
            // нарисовать свои буквы в этот же открытый буфер ОЗУ
            if (element->type == UI_TYPE_STACK_PANEL) {
                for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
                    UIElement_t* child = (UIElement_t*)element->children[i];
                    if (child->render_callback != NULL) {
                        child->render_callback(child);
                    }
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
    if (element->type == UI_TYPE_GRID || element->type == UI_TYPE_STACK_PANEL) {
        for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
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
    Sprite_fill(sprite, RGB565_DARK_GRAY);

    // 2. Отрисовка ВРЕМЕНИ (слева, отступ 5 пикселей)
    lcd_set_font(&font_segoe_struct);
    char timeBuf[6];
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


// Функция для отрисовки сетки графика
void Draw_Graph_Content(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    // Достаем физический спрайт графика из элемента интерфейса
    Sprite_t* s = el->sprite;
    //if (!s || !s->data) return;

    // 1. Устанавливаем шрифт и очищаем буфер графика
    lcd_set_font(&font_arial_9_struct);
    memset(s->data, 0, (uint32_t)s->w * s->h * 2);
    // Выводим текст заголовка
    //lcd_print_to_buffer(10, 70, RGB565_GREEN, "SWR GRAPH", RGB565_BLACK, s);
    // 2. Рисуем сетку графика (горизонтальные линии шкал)
    // Рисуем 3 горизонтальные линии через каждые 50 пикселей внутри спрайта
    for (uint16_t y = 40; y < s->h; y += 50) {
        for (uint16_t x = 10; x < s->w - 10; x++) {
            s->data[y * s->w + x] = 0x31A6; // Тускло-серый цвет сетки
        }
    }

    // 3. РИСУЕМ ЖИВУЮ КРИВУЮ ИЗМЕРЕНИЙ (Пример: синусоида или массив точек КСВ)
    // Пробегаем по всей ширине окна графика шаг за шагом
    int16_t prev_x = 10;
    int16_t prev_y = s->h / 2; // Стартовая точка по центру

    for (int16_t x = 11; x < s->w - 10; x++) {
        // Имитируем график: вычисляем Y (в реальном коде тут будет значение из массива SWR_Array[x])
        // Переводим значение КСВ в пиксели высоты спрайта
        int16_t y = (s->h / 2) + (int16_t)(sinf(x * 0.05f) * 40.0f); 

        // Соединяем прошлую точку с текущей быстрой линией Брезенхема!
        Draw_Line_To_Sprite(s, prev_x, prev_y, x, y, RGB565_YELLOW);

        prev_x = x;
        prev_y = y;
    }

    // Подпись
    lcd_print_to_buffer(15, 10, RGB565_WHITE, "SWR SCANNER", RGB565_BLACK, s);

    // Рисуем рамку вокруг графика с отступом 5 пикселей от краев спрайта.
    // Верхняя линия
    for (uint16_t x = 5; x < s->w - 5; x++) s->data[25 * s->w + x] = 0x7BEF; // Серый цвет
    // Нижня линия
    for (uint16_t x = 5; x < s->w - 5; x++) s->data[(s->h - 5) * s->w + x] = 0x7BEF;
    // Левая линия
    for (uint16_t y = 25; y < s->h - 5; y++) s->data[y * s->w + 5] = 0x7BEF;
    // Правая линия
    for (uint16_t y = 25; y < s->h - 5; y++) s->data[y * s->w + (s->w - 5)] = 0x7BEF;
}


extern uint8_t screen_rotation; // Берем текущий поворот из st7796.c

void Convert_Touch_Coordinates(uint16_t raw_x, uint16_t raw_y, uint16_t* out_x, uint16_t* out_y) {
    // Внимание: базовые константы 320 и 480 должны соответствовать портретной ориентации тача!
    switch (screen_rotation) {
        case 0: // Книжный режим (Portrait)
            *out_x = raw_x;
            *out_y = raw_y;
            break;
            
        case 1: // Альбомный режим (Landscape)
            // X становится равен Y, а Y инвертируется относительно ширины портрета
            *out_x = raw_y;
            *out_y = 320 - 1 - raw_x;
            break;
            
        case 2: // Книжный инвертированный
            *out_x = 320 - 1 - raw_x;
            *out_y = 480 - 1 - raw_y;
            break;
            
        case 3: // Альбомный инвертированный
            *out_x = 480 - 1 - raw_y;
            *out_y = raw_x;
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
        for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
            GUI_InvalidateAll((UIElement_t*)element->children[i]);
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
    if (panel_rows_count >= MAX_PANEL_ROWS || parent->children_count >= 8) {
        return NULL; 
    }

    // 1. Берем свободный элемент из статического пула
    UIElement_t* new_node = &panel_rows[panel_rows_count++];
    
    // 2. Настраиваем его по новым правилам компонентного движка
    new_node->type = UI_TYPE_TEXT_BLOCK;                  // Тип — текстовый блок
    new_node->sprite = parent->sprite;                    // Наследует физический спрайт панели
    new_node->render_callback = NULL;                     // Зануляем: отрисовкой управляет движок!
    new_node->children_count = 0;                         // У текста нет детей
    
    // Настройки выравнивания по умолчанию (можно переопределить после вызова функции)
    new_node->horizontal_alignment = HORIZONTAL_ALIGN_CENTER;
    new_node->vertical_alignment = VERTICAL_ALIGN_CENTER;

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
    if (!el || !el->sprite || !el->sprite->data) return;

    Sprite_t* s = el->sprite;
    
    // 1. Вычисляем локальные координаты ячейки внутри физического спрайта
    int16_t local_x = el->x - s->x;
    int16_t local_y = el->y - s->y;

    // 2. Очищаем пространство этой конкретной строки цветом фона (Dirty Rect)
    for (int16_t y = local_y; y < local_y + el->h; y++) {
        for (int16_t x = local_x; x < local_x + el->w; x++) {
            s->data[y * s->w + x] = RGB565_BLACK; 
        }
    }

    // 3. Активируем шрифт
    lcd_set_font(&font_arial_9_struct);
    
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
    lcd_print_to_buffer(text_x, text_y, RGB565_GREEN, el->text_content, RGB565_BLACK, s);
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
    lcd_set_font(&font_segoe_struct);
    int str_w = lcd_get_str_width(el->text_content);
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
}

/**
 * @brief Добавляет строку (пункт списка) внутрь ListBox
 */
UIElement_t* UI_ListBox_AddItem(UIElement_t* listbox, const char* item_text) {
    if (panel_rows_count >= MAX_PANEL_ROWS || listbox->children_count >= 8) return NULL;
    UIElement_t* item = &panel_rows[panel_rows_count++];
    item->type = UI_TYPE_TEXT_BLOCK;
    item->sprite = listbox->sprite;
    strncpy(item->text_content, item_text, sizeof(item->text_content) - 1);
    listbox->children[listbox->children_count++] = item;
    return item;
}

//Функция рисует рамку, подсвечивает выбранный пункт (0x10A5) и выводит текст
void UI_RenderListBox(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;
    Sprite_t* s = el->sprite;
    int16_t lx = el->x - s->x, ly = el->y - s->y;

    // 1. Очистка фона и рамка
    for (int16_t y = ly; y < ly + el->h; y++)
        for (int16_t x = lx; x < lx + el->w; x++)
            s->data[y * s->w + x] = (y == ly || y == ly + el->h - 1 || x == lx || x == lx + el->w - 1) ? 0x7BEF : RGB565_BLACK;

    lcd_set_font(&font_arial_9_struct);
    uint16_t item_h = current_font->char_height + 6;
    uint8_t start = el->props.list_box.scroll_offset, visible_count = el->h / item_h;

    // 2. Отрисовка видимых элементов
    for (uint8_t i = 0; i < el->children_count; i++) {
        if (i >= start && i < (start + visible_count)) {
            UIElement_t* child = (UIElement_t*)el->children[i];
            int16_t clx = child->x - s->x, cly = child->y - s->y;
            uint16_t bg = (i == el->props.list_box.selected_index) ? 0x10A5 : RGB565_BLACK;
            
            if (bg != RGB565_BLACK) // Подсветка
                for (int16_t y = cly; y < cly + child->h; y++)
                    for (int16_t x = clx + 1; x < clx + child->w - 1; x++)
                        s->data[y * s->w + x] = bg;

            lcd_print_to_buffer(clx + 10, cly + (child->h - current_font->char_height) / 2, RGB565_WHITE, child->text_content, bg, s);
        }
    }
}

//Обработчик тача, меняющий selected_index и вызывающий GUI_InvalidateRect для перерисовки
int8_t UI_ListBox_ProcessTouch(UIElement_t* listbox, uint16_t tx, uint16_t ty) {
    if (!listbox || listbox->type != UI_TYPE_LIST_BOX || !listbox->sprite ||
        tx < listbox->x || tx >= (listbox->x + listbox->w) || ty < listbox->y || ty >= (listbox->y + listbox->h)) return -1;

    uint8_t target = listbox->props.list_box.scroll_offset + (ty - listbox->y) / (font_arial_9_struct.char_height + 6);

    if (target < listbox->children_count) {
        if (listbox->props.list_box.selected_index != target) {
            listbox->props.list_box.selected_index = target;
            GUI_InvalidateRect(listbox->sprite, listbox->x - listbox->sprite->x, listbox->y - listbox->sprite->y, listbox->w, listbox->h);
        }
        return target;
    }
    return -1;
}


