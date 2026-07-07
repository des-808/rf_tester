#include "gui.h"
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
    digits_node.type = UI_TYPE_SPRITE;
    digits_node.sprite = &main_screen_sprite;
    digits_node.render_callback = Draw_Digits_Content; // Ваша функция рисования кнопок/тача
    digits_node.grid_row = 0;
    digits_node.grid_col = 1;
    main_work_grid.children[main_work_grid.children_count++] = &digits_node;

    // 7. ЗАПУСК КОНТЕЙНЕРОВ: Расчет размеров по всему дереву
    extern uint16_t Display_Width;
    extern uint16_t Display_Height;
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);

    // 8. ЗАПУСК ОТРИСОВКИ: Вывод на экран
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


/**
 * @brief Рекурсивный обход дерева интерфейса для очистки и вывода на физический экран
 */

void UI_DrawTree(UIElement_t* element) {
    if (!element) return;

    if (element->type == UI_TYPE_SPRITE && element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        if (s->is_allocated && s->data != NULL) {
            
            // 1. Быстрая очистка буфера (Заливка черным)
            uint32_t total_pixels = (uint32_t)s->w * s->h;
            memset(s->data, 0, total_pixels * 2);
            
            // 2. ВЫЗЫВАЕМ ОТРИСОВКУ: Если для этого окна задано, что в нем рисовать
            if (element->render_callback != NULL) {
                element->render_callback(s);
            }

            // 3. Выталкиваем готовый буфер на экран ST7796
            ST7796_PushSprite(s);
        }
        return;
    }

    /* for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
        UI_DrawTree(element->children[i]);
    } */
    for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
        UI_DrawTree((UIElement_t*)element->children[i]); // Явное приведение к указателю
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

uint8_t currentHour = 22;
uint8_t currentMinute = 43;
uint8_t battery_Level = 17;
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

    // Заливаем фон графика черным
    uint32_t size = (uint32_t)s->w * s->h;
    memset(s->data, 0, size * 2);

    // Выводим текст заголовка
    lcd_print_to_buffer(10, 10, RGB565_YELLOW, "SWR GRAPH", RGB565_BLACK, s);

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

    //extern char button_status_msg[32];
    //extern char touch_status_msg[32];

    // КРИТИЧЕСКИ ВАЖНО ДЛЯ H7: Сбрасываем кэш данных для этих двух строк,
    // чтобы графический движок гарантированно увидел новые символы из main.c
    /* SCB_CleanDCache_by_Addr((uint32_t*)button_status_msg, 32);
    SCB_CleanDCache_by_Addr((uint32_t*)touch_status_msg, 32);
    __DSB(); */

    // Заливаем фон правого окна черным, чтобы старый текст не накладывался на новый
    uint32_t size = (uint32_t)s->w * s->h;
    memset(s->data, 0, size * 2);

    // Выводим заголовок
    lcd_print_to_buffer(10, 10, RGB565_BLUE, "Welcome Mode", RGB565_BLACK, s);
    
    // Выводим статус физических кнопок
    int btn_w = lcd_get_str_width(button_status_msg);
    int16_t btn_x = (s->w - btn_w) / 2;
    if (btn_x < 0) btn_x = 10; // Защита от вылета влево
    lcd_print_to_buffer(btn_x, 50, RGB565_GREEN, button_status_msg, RGB565_BLACK, s);

    // Выводим статус тачскрина
    int tch_w = lcd_get_str_width(touch_status_msg);
    int16_t tch_x = (s->w - tch_w) / 2;
    if (tch_x < 0) tch_x = 10;
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

