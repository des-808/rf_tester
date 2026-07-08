#include "gui.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>


// Глобальные переменные сущностей интерфейса
UIElement_t root_grid;
UIElement_t status_bar_node;
UIElement_t main_work_grid;
UIElement_t graph_node;
UIElement_t digits_node;

// Реальные структуры спрайтов LovyanGFX/Си
Sprite_t status_bar_sprite;
Sprite_t graph_sprite;
Sprite_t main_screen_sprite;

// Нам нужны указатели только на динамические данные. 
// Пояснения (статичный текст) нам сохранять в переменные не нужно!
UIElement_t* ui_btn_row;
UIElement_t* ui_touch_row;
UIElement_t* ui_swr_row; // Допустим, добавим вывод КСВ цифрами

void GUI_ShowAdvancedMeasurementScreen(uint8_t rotation) {
    // 1. Поворачиваем железо и очищаем старый пул памяти
    ST7796_SetRotation(rotation);
    heap_caps_reset_pool();

    // 2. Настраиваем корневую сетку (Разделение по вертикали: 10% и 90%)
    root_grid.type = UI_TYPE_GRID;
    root_grid.children_count = 0;
    root_grid.layout.grid.rows_count = 2;
    root_grid.layout.grid.cols_count = 1;
    
    // ИСПРАВЛЕНО: Явно указываем индексы массива!
    root_grid.layout.grid.row_definitions[0] = 10; // 10% под статус-бар
    root_grid.layout.grid.row_definitions[1] = 90; // 90% под рабочую зону
    root_grid.layout.grid.col_definitions[0] = 100;

    // 3. Подключаем Статус-бар в ячейку (строка 0, колонка 0)
    status_bar_node.type = UI_TYPE_SPRITE;
    status_bar_node.sprite = &status_bar_sprite;
    status_bar_node.render_callback = Draw_StatusBar_Callback;
    status_bar_node.grid_row = 0;
    status_bar_node.grid_col = 0;
    root_grid.children[root_grid.children_count++] = &status_bar_node;

    // 4. Создаем вложенную сетку для разделения Графика и Цифр
    // Сажаем её в нижнюю ячейку корневой сетки (строка 1, колонка 0)
    main_work_grid.type = UI_TYPE_GRID;
    main_work_grid.children_count = 0;
    main_work_grid.grid_row = 1;
    main_work_grid.grid_col = 0;
    main_work_grid.layout.grid.rows_count = 1;
    main_work_grid.layout.grid.cols_count = 2;
    
    // ИСПРАВЛЕНО: Явно указываем индексы массива!
    main_work_grid.layout.grid.row_definitions[0] = 100; // Вся высота рабочей зоны
    main_work_grid.layout.grid.col_definitions[0] = 70;  // 70% ширины под График
    main_work_grid.layout.grid.col_definitions[1] = 30;  // 30% ширины под цифры
    root_grid.children[root_grid.children_count++] = &main_work_grid;

    // 5. Сажаем спрайт Графика во вложенную сетку (0, 0) — левая часть
    graph_node.type = UI_TYPE_SPRITE;
    graph_node.sprite = &graph_sprite;
    graph_node.render_callback = Draw_Graph_Content; // Ваша функция рисования сетки
    graph_node.grid_row = 0;
    graph_node.grid_col = 0;
    main_work_grid.children[main_work_grid.children_count++] = &graph_node;

    // 6. Сажаем спрайт Цифр во вложенную сетку (0, 1) — правая часть
    /* digits_node.type = UI_TYPE_SPRITE;
    digits_node.sprite = &main_screen_sprite;
    digits_node.render_callback = Draw_Digits_Content; // Ваша функция рисования кнопок/тача
    digits_node.grid_row = 0;
    digits_node.grid_col = 1;
    main_work_grid.children[main_work_grid.children_count++] = &digits_node */;

    // 2. Настраиваем правую панель как STACK_PANEL (Вертикальный список)
    digits_node.type = UI_TYPE_STACK_PANEL;
    digits_node.sprite = &main_screen_sprite; // Физический спрайт для отрисовки
    digits_node.layout.stack.orientation = ORIENTATION_VERTICAL;
    digits_node.layout.stack.spacing = 5; // Зазор 5 пикселей между строками
    digits_node.grid_row = 0; 
    digits_node.grid_col = 1; // Сажаем в правую колонку Grid
    main_work_grid.children[main_work_grid.children_count++] = &digits_node;

    // 3. НАБИРАЕМ ТЕКСТ ПОТОКОМ (Максимально удобно!)
    GUI_Panel_AddString(&digits_node, "--- ИЗМЕРЕНИЯ ---"); // Статичное пояснение №1
    
    // Динамическая строка КСВ (сохраняем указатель)
    ui_swr_row = GUI_Panel_AddString(&digits_node, "SWR: 1.00"); 
    
    GUI_Panel_AddString(&digits_node, "-----------------"); // Статичный разделитель
    GUI_Panel_AddString(&digits_node, "--- АППАРАТУРА --"); // Статичное пояснение №2
    
    // Динамические строки кнопок и тача (сохраняем указатели)
    ui_btn_row   = GUI_Panel_AddString(&digits_node, "No btn");
    ui_touch_row = GUI_Panel_AddString(&digits_node, "No touch");
    
    GUI_Panel_AddString(&digits_node, "v1.0.0 Stable");     // Статичная строка версии

    // 7. ЗАПУСК КОНТЕЙНЕРОВ: Расчет размеров по всему дереву
    extern uint16_t Display_Width;
    extern uint16_t Display_Height;
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);

    // 8. ЗАПУСК ОТРИСОВКИ: Вывод на экран
    GUI_InvalidateAll(&root_grid);
    UI_DrawTree(&root_grid);
}

void UI_MeasureAndArrange(UIElement_t* element, int16_t parent_x, int16_t parent_y, uint16_t available_w, uint16_t available_h) {
    if (!element) return;

    // Присваиваем элементу вычисленные для него размеры
    element->x = parent_x;
    element->y = parent_y;
    element->w = available_w;
    element->h = available_h;

    // СЦЕНАРИЙ 1: Это конечный Спрайт. Нарезаем под него память из пула
    if (element->type == UI_TYPE_SPRITE && element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        s->x = element->x;
        s->y = element->y;
        s->w = element->w;
        s->h = element->h;
        
        // Нарезаем из нашего безопасного глобального пула памяти
        s->data = (uint16_t*)heap_caps_malloc(s->w * s->h * 2, 0);
        s->is_allocated = (s->data != NULL);
        return; 
    }

    // СЦЕНАРИЙ 2: Это STACK_PANEL (Линейное расположение)
    if (element->type == UI_TYPE_STACK_PANEL) {
        int16_t current_x = element->x;
        int16_t current_y = element->y;
        uint8_t count = element->children_count;
        if (count == 0 || count > 8) return; // Защита от выхода за границы массива [8]

        // Рассчитываем габариты детей в зависимости от направления
        uint16_t child_w = (element->layout.stack.orientation == ORIENTATION_HORIZONTAL) 
            ? (element->w - (element->layout.stack.spacing * (count - 1))) / count : element->w;
        uint16_t child_h = (element->layout.stack.orientation == ORIENTATION_VERTICAL) 
            ? (element->h - (element->layout.stack.spacing * (count - 1))) / count : element->h;

        for (uint8_t i = 0; i < count; i++) {
            UI_MeasureAndArrange(element->children[i], current_x, current_y, child_w, child_h);
            
            if (element->layout.stack.orientation == ORIENTATION_HORIZONTAL) {
                current_x += child_w + element->layout.stack.spacing;
            } else {
                current_y += child_h + element->layout.stack.spacing;
            }
        }
    }

    // СЦЕНАРИЙ 3: Это GRID (Табличная разметка)
    if (element->type == UI_TYPE_GRID) {
        GridDefinition_t* grid = &element->layout.grid;
        
        // Временные массивы для физических размеров ячеек в пикселях
        uint16_t row_heights[8] = {0};
        uint16_t col_widths[8] = {0};

        // Переводим процентные доли Grid в пиксели экрана
        for (uint8_t r = 0; r < grid->rows_count && r < 8; r++) {
            row_heights[r] = (element->h * grid->row_definitions[r]) / 100;
        }
        for (uint8_t c = 0; c < grid->cols_count && c < 8; c++) {
            col_widths[c] = (element->w * grid->col_definitions[c]) / 100;
        }

        // Рекурсивно распределяем детей по рассчитанным ячейкам
        for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
            //UIElement_t* child = element->children[i];
            UIElement_t* child = (UIElement_t*)element->children[i];
            
            int16_t cell_x = element->x;
            int16_t cell_y = element->y;
            
            // Считаем смещение до нужной строки и колонки
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

    if (element->type == UI_TYPE_SPRITE && element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        
        if (s->is_allocated && s->data != NULL && s->needs_render) {
            
            // 1. УДАЛЕНО: memset(s->data, 0, ...) всего спрайта больше НЕ ДЕЛАЕМ!

            // 2. Вызываем колбэк (он обновит пиксели только в грязной зоне)
            if (element->render_callback != NULL) {
                element->render_callback(s);
            }

            // 3. УМНАЯ ОТПРАВКА: шлем на экран ТОЛЬКО грязный прямоугольник!
            ST7796_PushSpriteRect(s, s->dirty_x1, s->dirty_y1, s->dirty_x2, s->dirty_y2);
            
            // 4. Сброс флагов
            s->needs_render = false; 
        }
        return;
    }

    for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
        UI_DrawTree((UIElement_t*)element->children[i]);
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
void Draw_StatusBar_Callback(Sprite_t *sprite) {
    if (!sprite || !sprite->is_allocated || !sprite->data) return;

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
void Draw_Graph_Content(Sprite_t* s) {
    if (!s || !s->data) return;

    // 1. Устанавливаем шрифт и очищаем буфер графика
    lcd_set_font(&font_arial_9_struct);
    memset(s->data, 0, (uint32_t)s->w * s->h * 2);

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


// Подключаем глобальные переменные строк из main.c
extern char button_status_msg[32];
extern char touch_status_msg[32];
// Функция для отрисовки контента внутри правого экрана (Цифры/Тач)
void Draw_Digits_Content(Sprite_t* s) {
    if (!s || !s->data) return;

    lcd_set_font(&font_arial_9_struct);
    // Заливаем фон правого окна черным, чтобы старый текст не накладывался на новый
    uint32_t size = (uint32_t)s->w * s->h;
    memset(s->data, 0, size * 2);

    // Выводим заголовок
    lcd_print_to_buffer(10, 10, RGB565_BLUE, "Welcome Mode", RGB565_BLACK, s);
    
    // Выводим статус физических кнопок
    int btn_w = lcd_get_str_width(button_status_msg);
    int16_t btn_x = (s->w - btn_w) / 2;
    if (btn_x < 0) btn_x = 5; // Защита от вылета влево
    lcd_print_to_buffer(btn_x, 50, RGB565_GREEN, button_status_msg, RGB565_BLACK, s);

    // Выводим статус тачскрина
    int tch_w = lcd_get_str_width(touch_status_msg);
    int16_t tch_x = (s->w - tch_w) / 2;
    if (tch_x < 0) tch_x = 5;
    lcd_print_to_buffer(tch_x, 90, RGB565_CYAN, touch_status_msg, RGB565_BLACK, s);
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
 * @brief Помечает конкретный спрайт для перерисовки на следующем кадре
 */
void GUI_InvalidateSprite(Sprite_t* sprite) {
    if (sprite) {
        sprite->needs_render = true;
    }
}

/**
 * @brief Помечает ВСЕ зарегистрированные спрайты для полной перерисовки
 */
void GUI_InvalidateAll(UIElement_t* element) {
    if (!element) return;

    if (element->type == UI_TYPE_SPRITE && element->sprite != NULL) {
        element->sprite->needs_render = true;
        return;
    }

    for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
        GUI_InvalidateAll((UIElement_t*)element->children[i]);
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

    char new_text[32];
    va_list args;
    va_start(args, format);
    vsnprintf(new_text, sizeof(new_text), format, args);
    va_end(args);

    // Если текст не изменился — ничего не делаем, экономим ресурсы!
    if (strcmp(element->text_content, new_text) == 0) {
        return;
    }

    // Сохраняем новый текст в элемент
    strncpy(element->text_content, new_text, sizeof(element->text_content) - 1);
    element->text_content[sizeof(element->text_content) - 1] = '\0';

    // Автоматически рассчитываем грязную зону
    // Элемент знает свои внутренние координаты (x, y), ширину (w) и высоту (h),
    // полученные от Grid-родителя при MeasureAndArrange!
    if (element->type == UI_TYPE_SPRITE && element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        
        // Помечаем грязной область СТРОГО в границах этого текстового элемента
        // Переводим абсолютные экранные координаты элемента в локальные координаты спрайта
        int16_t local_rx = element->x - s->x;
        int16_t local_ry = element->y - s->y;
        
        GUI_InvalidateRect(s, local_rx, local_ry, element->w, element->h);
    }
}



// Объявляем только ОДИН контейнер для правой панели
UIElement_t digits_panel; 
// Пул элементов для строк (выделяем память статически внутри gui.c, чтобы не плодить глобальные имена)
static UIElement_t panel_rows[MAX_PANEL_ROWS];
static uint8_t panel_rows_count = 0;

/**
 * @brief Добавляет текстовую строку в StackPanel и возвращает указатель на неё
 * @param parent Контейнер StackPanel, куда добавляем строку
 * @param initial_text Начальный текст или пояснение
 */
UIElement_t* GUI_Panel_AddString(UIElement_t* parent, const char* initial_text) {
    if (panel_rows_count >= MAX_PANEL_ROWS) return NULL;

    // Берем свободный элемент из пула
    UIElement_t* new_node = &panel_rows[panel_rows_count++];
    
    // Настраиваем его как текстовый спрайт, который будет рисовать в буфер родителя
    new_node->type = UI_TYPE_SPRITE;
    new_node->sprite = parent->sprite; // наследует физический спрайт панели
    new_node->render_callback = Draw_GeneralText_Callback; // универсальный колбэк
    new_node->children_count = 0;
    
    // Копируем начальный текст (пояснение)
    strncpy(new_node->text_content, initial_text, sizeof(new_node->text_content) - 1);
    new_node->text_content[sizeof(new_node->text_content) - 1] = '\0';

    // Регистрируем его как ребенка в StackPanel
    parent->children[parent->children_count++] = new_node;

    return new_node;
}

//void Draw_GeneralText_Callback(void* param){}

/**
 * @brief Полный сброс динамических строк панели (нужен при смене экрана)
 */
void GUI_Panel_ClearStrings(UIElement_t* parent) {
    parent->children_count = 0;
    panel_rows_count = 0;
}



void Draw_GeneralText_Callback(UIElement_t* el) {
    if (!el || !el->sprite || !el->sprite->data) return;

    Sprite_t* s = el->sprite;
    
    // 1. Вычисляем локальные координаты ячейки внутри физического спрайта
    int16_t local_x = el->x - s->x;
    int16_t local_y = el->y - s->y;

    // 2. Очищаем ТОЛЬКО пространство этой конкретной строки (Dirty Rect) цветом фона
    // Чтобы старые буквы не накладывались на новые при изменении текста
    for (int16_t y = local_y; y < local_y + el->h; y++) {
        for (int16_t x = local_x; x < local_x + el->w; x++) {
            s->data[y * s->w + x] = RGB565_BLACK; 
        }
    }

    // 3. Активируем нужный шрифт
    lcd_set_font(&font_arial_9_struct);
    
    // 4. Автоматически инвариантно центрируем строку по горизонтали и вертикали внутри выделенной ячейки
    int str_w = lcd_get_str_width(el->text_content);
    int16_t text_x = local_x + (el->w - str_w) / 2;
    int16_t text_y = local_y + (el->h - current_font->char_height) / 2;

    // Защита от вылета текста за левую границу ячейки
    if (text_x < local_x) text_x = local_x + 5; 

    // 5. Печатаем живой текст, сохраненный внутри этого элемента интерфейса
    lcd_print_to_buffer(text_x, text_y, RGB565_GREEN, el->text_content, RGB565_BLACK, s);
}



