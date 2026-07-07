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

    // 2. Настраиваем корневую сетку (Разделение по горизонтали: 10% и 90%)
    root_grid.type = UI_TYPE_GRID;
    root_grid.children_count = 0;
    root_grid.layout.grid.rows_count = 2;
    root_grid.layout.grid.cols_count = 1;
    root_grid.layout.grid.row_definitions[0] = 10; // 10% под статус-бар
    root_grid.layout.grid.row_definitions[1] = 90; // 90% под рабочую зону
    root_grid.layout.grid.col_definitions[0] = 100;

    // 3. Подключаем Статус-бар в ячейку (строка 0, колонка 0)
    status_bar_node.type = UI_TYPE_SPRITE;
    status_bar_node.sprite = &status_bar_sprite;
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
    main_work_grid.layout.grid.row_definitions[0] = 100;
    main_work_grid.layout.grid.col_definitions[0] = 70; // 70% ширины под График КСВ
    main_work_grid.layout.grid.col_definitions[1] = 30; // 30% ширины под цифры SWR/FREQ
    root_grid.children[root_grid.children_count++] = &main_work_grid;

    // 5. Сажаем спрайт Графика во вложенную сетку (0, 0) — левая часть
    graph_node.type = UI_TYPE_SPRITE;
    graph_node.sprite = &graph_sprite;
    graph_node.grid_row = 0;
    graph_node.grid_col = 0;
    main_work_grid.children[main_work_grid.children_count++] = &graph_node;

    // 6. Сажаем спрайт Цифр во вложенную сетку (0, 1) — правая часть
    digits_node.type = UI_TYPE_SPRITE;
    digits_node.sprite = &main_screen_sprite;
    digits_node.grid_row = 0;
    digits_node.grid_col = 1;
    main_work_grid.children[main_work_grid.children_count++] = &digits_node;

    // ====================================================================
    // ЗАПУСК КОНТЕЙНЕРОВ: Расчет размеров и нарезка памяти для всех окон!
    // Внешние габариты берутся из текущих Display_Width и Display_Height
    // ====================================================================
    extern uint16_t Display_Width;
    extern uint16_t Display_Height;
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);

    // ====================================================================
    // ЗАПУСКОТРИСОВКИ: Полный рекурсивный вывод дерева интерфейса на экран
    // ====================================================================
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
            UIElement_t* child = element->children[i];
            
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

    // Если элемент является спрайтом — обслуживаем его
    if (element->type == UI_TYPE_SPRITE && element->sprite != NULL) {
        Sprite_t* s = element->sprite;
        if (s->is_allocated && s->data != NULL) {
            
            // 1. Быстрая попиксельная очистка спрайта (заливаем черным 0x0000 перед рисованием)
            uint32_t total_pixels = (uint32_t)s->w * s->h;
            memset(s->data, 0, total_pixels * 2);
            
            // 2. [ОПЦИОНАЛЬНО] Здесь можно вызвать кастомный рендеринг текста/линий для этого спрайта,
            // например, привязав к элементу указатель на функцию отрисовки.

            // 3. Выталкиваем готовый буфер в контроллер ST7796 по SPI/DMA
            ST7796_PushSprite(s);
        }
        return; // У спрайта не может быть детей, выходим из рекурсии
    }

    // Если это контейнер (Grid или StackPanel) — рекурсивно спускаемся к его детям
    for (uint8_t i = 0; i < element->children_count && i < 8; i++) {
        UI_DrawTree(element->children[i]);
    }
}