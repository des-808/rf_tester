#include "page.h"
#include "menu.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

/* ========================================================================
 *  Глобальные переменные
 * ======================================================================== */

/* Указатель на родительский контейнер (digits_node) — задаётся при открытии */
static UIElement_t* g_parent_container = NULL;

/* Текущая активная страница */
static UIElement_t* g_current_page = NULL;
static PageDef_t*   g_current_def = NULL;
static bool         g_page_is_dynamic = false;

/* Для динамических страниц — храним указатель */
static DynamicPage_t* g_dynamic_page = NULL;

/* Счётчик элементов пула при открытии страницы (для возврата при закрытии) */
static uint8_t g_page_rows_snapshot = 0;

/* Флаг: Menu_Expand уже вызван (избегаем дубля) */
static bool g_menu_expand_called = false;

/* Стек состояний (сохраняет меню/страницы при вложенности) */
static PageStackEntry_t g_page_stack[MAX_PAGE_DEPTH];
static int g_page_stack_top = -1;

/* Внешние переменные — доступны через gui.h */

/* ========================================================================
 *  Page_Init
 * ======================================================================== */

void Page_Init(void)
{
    g_parent_container = NULL;
    g_current_page = NULL;
    g_current_def = NULL;
    g_page_is_dynamic = false;
    g_dynamic_page = NULL;
    g_page_stack_top = -1;
    g_menu_expand_called = false;
}

/* ========================================================================
 *  Helpers — выделение элемента из пула panel_rows
 * ======================================================================== */

static UIElement_t* page_alloc_element(void)
{
    if (panel_rows_count >= MAX_PANEL_ROWS) {
        return NULL;
    }
    UIElement_t* el = &panel_rows[panel_rows_count++];
    memset(el, 0, sizeof(UIElement_t));
    el->sprite = NULL; // будет установлен вызывающим
    return el;
}

/* ========================================================================
 *  Forward declarations
 * ======================================================================== */

/* Вызываем on_draw текущей страницы */
static void Page_RenderCallback(UIElement_t* el)
{
    if (g_current_def && g_current_def->on_draw) {
        g_current_def->on_draw(el);
    } else {
        /* Если on_draw == NULL — рисуем детей стандартным способом */
        for (uint8_t i = 0; i < el->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
            UIElement_t* child = (UIElement_t*)el->children[i];
            if (child) {
                UI_RenderChildElement(child);
            }
        }
    }
}

/* ========================================================================
 *  Static pages
 * ======================================================================== */

bool Page_OpenStatic(PageDef_t* def, UIElement_t* parent_container)
{
    if (!def || !parent_container) return false;
    
    printf("[Page] OpenStatic: %s\n", def->name);
    g_page_rows_snapshot = panel_rows_count;
    printf("[Page] panel_rows_count snapshot: %d\n", g_page_rows_snapshot);
    
    g_parent_container = parent_container;
    
    /* Полностью убираем ListBox меню из children digits_node */
    if (current_menu_listbox && parent_container) {
        for (uint8_t i = 0; i < parent_container->children_count; i++) {
            if (parent_container->children[i] == current_menu_listbox) {
                /* Сдвигаем массив детей */
                for (uint8_t j = i; j < parent_container->children_count - 1; j++) {
                    parent_container->children[j] = parent_container->children[j + 1];
                }
                parent_container->children_count--;
                /* Сохраняем указатель на menu_lb для восстановления */
                current_menu_listbox->w = 0;
                current_menu_listbox->h = 0;
                current_menu_listbox->props.list_box.collapsed = 1;
                break;
            }
        }
    }
    
    printf("[Page] After hide menu, panel_rows_count: %d\n", panel_rows_count);
    
    /* Сохраняем состояние на стек */
    if (g_page_stack_top < MAX_PAGE_DEPTH - 1) {
        g_page_stack_top++;
        g_page_stack[g_page_stack_top].saved_container = g_current_page;
        g_page_stack[g_page_stack_top].was_listbox = (g_current_page == NULL && parent_container != NULL);
        g_page_stack[g_page_stack_top].list_scroll = 0;
        g_page_stack[g_page_stack_top].list_selected = 0;
    }
    
    /* Создаём UIElement-контейнер страницы из пула */
    UIElement_t* page_el = page_alloc_element();
    if (!page_el) {
        printf("[Page] FAIL: page_alloc_element returned NULL\n");
        return false;
    }
    
    printf("[Page] page_el=%p, panel_rows_count after alloc: %d\n", (void*)page_el, panel_rows_count);
    
    /* Настраиваем контейнер */
    page_el->type = def->container_type;
    page_el->sprite = &main_screen_sprite;
    page_el->background_color = RGB565_BLACK;
    page_el->font = &font_arial_9_struct;
    page_el->children_count = 0;
    page_el->render_callback = Page_RenderCallback; // Вызываем on_draw через UI_DrawTree
    page_el->props.stack.orientation = def->orientation;
    page_el->props.stack.spacing = def->spacing;
    
    /* Для Grid */
    if (def->container_type == UI_TYPE_GRID) {
        page_el->props.grid.rows_count = def->rows;
        page_el->props.grid.cols_count = def->cols;
    }
    
    /* Копируем user_data */
    page_el->user_data = def->user_data;
    
    /* Вызываем инициализацию */
    printf("[Page] Calling on_init for %s\n", def->name);
    if (def->on_init) {
        if (!def->on_init(page_el, parent_container)) {
            printf("[Page] on_init returned false\n");
            return false;
        }
    }
    
    /* Устанавливаем спрайт для всех потомков страницы (page_alloc_element ставит NULL) */
    page_el->sprite = &main_screen_sprite;
    for (uint8_t i = 0; i < page_el->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
        UIElement_t* child = (UIElement_t*)page_el->children[i];
        if (child) {
            child->sprite = &main_screen_sprite;
            /* Рекурсивно для вложенных панелей */
            for (uint8_t j = 0; j < child->children_count && j < MAX_ELEMENT_CHILDREN; j++) {
                UIElement_t* grandchild = (UIElement_t*)child->children[j];
                if (grandchild) {
                    grandchild->sprite = &main_screen_sprite;
                }
            }
        }
    }
    
    printf("[Page] Adding page to parent, children_count before: %d\n", parent_container->children_count);
    
    /* Добавляем страницу в parent */
    if (parent_container->children_count < MAX_ELEMENT_CHILDREN) {
        parent_container->children[parent_container->children_count++] = page_el;
    }
    
    printf("[Page] children_count after: %d, panel_rows_count: %d\n", parent_container->children_count, panel_rows_count);
    
    /* Измеряем root_grid — пересчитаем layout всех детей (включая новую страницу) */
    extern UIElement_t root_grid;
    extern uint16_t Display_Width;
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, 240);
    if (page_el->w > 0 && page_el->h > 0 && page_el->sprite && page_el->sprite->data) {
        memset(&page_el->sprite->data[page_el->y * page_el->sprite->w + page_el->x], 
               0x00, page_el->w * page_el->h * 2);
    }
    
    /* Обновляем глобальное состояние */
    g_current_page = page_el;
    g_current_def = def;
    g_page_is_dynamic = false;
    g_dynamic_page = NULL;
    
    /* Инвалидируем спрайт для перерисовки */
    main_screen_sprite.needs_render = true;
    
    printf("[Page] OpenStatic SUCCESS\n");
    
    return true;
}

bool Page_CloseStatic(void)
{
    if (!g_current_page) return false;
    
    /* Guard: если page_rows_count уже равен snapshot — страница уже закрыта */
    if (panel_rows_count == g_page_rows_snapshot) {
        printf("[Page] Already closed, clearing state\n");
        g_current_page = NULL;
        g_current_def = NULL;
        g_page_is_dynamic = false;
        g_dynamic_page = NULL;
        return true;
    }
    
    /* Вызываем деинициализацию */
    if (g_current_def && g_current_def->on_deinit) {
        g_current_def->on_deinit(g_current_page);
    }
    
    /* Удаляем страницу из parent */
    if (g_parent_container && g_current_page) {
        for (uint8_t i = 0; i < g_parent_container->children_count; i++) {
            if (g_parent_container->children[i] == g_current_page) {
                /* Сдвигаем массив детей */
                for (uint8_t j = i; j < g_parent_container->children_count - 1; j++) {
                    g_parent_container->children[j] = g_parent_container->children[j + 1];
                }
                g_parent_container->children_count--;
                break;
            }
        }
    }
    
    /* ВОЗВРАЩАЕМ весь пул страницы — используем snapshot */
    if (g_current_page) {
        uint8_t delta = panel_rows_count - g_page_rows_snapshot;
        printf("[Page] Closing: returning %d elements to snapshot %d (current %d)\n", 
               delta, g_page_rows_snapshot, panel_rows_count);
        panel_rows_count = g_page_rows_snapshot;
        printf("[Page] panel_rows_count after close: %d\n", panel_rows_count);
    }
    
    /* Восстанавливаем ListBox меню — добавляем обратно в children */
    if (current_menu_listbox && !g_menu_expand_called && g_parent_container) {
        if (g_parent_container->children_count < MAX_ELEMENT_CHILDREN) {
            current_menu_listbox->props.list_box.collapsed = 0;
            g_parent_container->children[g_parent_container->children_count++] = current_menu_listbox;
        }
    }
    g_menu_expand_called = false;
    
    /* Восстанавливаем состояние со стека */
    if (g_page_stack_top >= 0) {
        PageStackEntry_t* prev = &g_page_stack[g_page_stack_top];
        
        if (prev->saved_container) {
            /* Была другая страница — возвращаемся к ней */
            g_current_page = prev->saved_container;
            g_current_def = NULL; // TODO: восстановить g_current_def
        } else {
            /* Возврат к меню */
            g_current_page = NULL;
            g_current_def = NULL;
        }
        
        g_page_stack_top--;
    } else {
        g_current_page = NULL;
        g_current_def = NULL;
    }
    
    g_page_is_dynamic = false;
    g_dynamic_page = NULL;
    
    /* Пересчитываем root_grid — menu_lb снова должен занять место */
    extern UIElement_t root_grid;
    extern uint16_t Display_Width;
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, 240);
    
    main_screen_sprite.needs_render = true;
    
    return true;
}

/* ========================================================================
 *  Dynamic pages
 * ======================================================================== */

DynamicPage_t* Page_OpenDynamic(PageDef_t* def, UIElement_t* parent_container)
{
    if (!def || !parent_container) return NULL;
    
    g_parent_container = parent_container;
    
    /* Полностью убираем ListBox меню из children */
    if (current_menu_listbox && parent_container) {
        for (uint8_t i = 0; i < parent_container->children_count; i++) {
            if (parent_container->children[i] == current_menu_listbox) {
                for (uint8_t j = i; j < parent_container->children_count - 1; j++) {
                    parent_container->children[j] = parent_container->children[j + 1];
                }
                parent_container->children_count--;
                current_menu_listbox->w = 0;
                current_menu_listbox->h = 0;
                current_menu_listbox->props.list_box.collapsed = 1;
                break;
            }
        }
    }
    
    /* Сохраняем состояние на стек */
    if (g_page_stack_top < MAX_PAGE_DEPTH - 1) {
        g_page_stack_top++;
        g_page_stack[g_page_stack_top].saved_container = g_current_page;
        g_page_stack[g_page_stack_top].was_listbox = (g_current_page == NULL);
        g_page_stack[g_page_stack_top].list_scroll = 0;
        g_page_stack[g_page_stack_top].list_selected = 0;
    }
    
    /* Сохраняем snapshot пула */
    g_page_rows_snapshot = panel_rows_count;
    
    /* Выделяем DynamicPage_t */
    DynamicPage_t* dpage = (DynamicPage_t*)heap_caps_malloc(sizeof(DynamicPage_t), 0);
    if (!dpage) return NULL;
    
    memset(dpage, 0, sizeof(DynamicPage_t));
    dpage->is_dynamic = true;
    memcpy(&dpage->def, def, sizeof(PageDef_t));
    
    /* Настраиваем UIElement */
    UIElement_t* page_el = &dpage->element;
    page_el->type = def->container_type;
    page_el->sprite = &main_screen_sprite;
    page_el->background_color = RGB565_BLACK;
    page_el->font = &font_arial_9_struct;
    page_el->children_count = 0;
    page_el->render_callback = Page_RenderCallback; // Вызываем on_draw через UI_DrawTree
    page_el->props.stack.orientation = def->orientation;
    page_el->props.stack.spacing = def->spacing;
    
    if (def->container_type == UI_TYPE_GRID) {
        page_el->props.grid.rows_count = def->rows;
        page_el->props.grid.cols_count = def->cols;
    }
    
    /* Копируем user_data */
    page_el->user_data = def->user_data;
    
    /* Вызываем инициализацию */
    if (def->on_init) {
        if (!def->on_init(page_el, parent_container)) {
            heap_caps_free(dpage);
            return NULL;
        }
    }
    
    /* Устанавливаем спрайт для всех потомков */
    page_el->sprite = &main_screen_sprite;
    for (uint8_t i = 0; i < page_el->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
        UIElement_t* child = (UIElement_t*)page_el->children[i];
        if (child) {
            child->sprite = &main_screen_sprite;
            for (uint8_t j = 0; j < child->children_count && j < MAX_ELEMENT_CHILDREN; j++) {
                UIElement_t* gc = (UIElement_t*)child->children[j];
                if (gc) gc->sprite = &main_screen_sprite;
            }
        }
    }
    
    /* Добавляем в parent */
    if (parent_container->children_count < MAX_ELEMENT_CHILDREN) {
        parent_container->children[parent_container->children_count++] = page_el;
    }
    
    /* Пересчитываем layout parent */
    extern uint16_t Display_Width;
    UI_MeasureAndArrange(parent_container, parent_container->x, parent_container->y, parent_container->w, parent_container->h);
    
    /* Очищаем область страницы */
    if (page_el->w > 0 && page_el->h > 0 && page_el->sprite && page_el->sprite->data) {
        memset(&page_el->sprite->data[page_el->y * page_el->sprite->w + page_el->x], 
               0x00, page_el->w * page_el->h * 2);
    }
    
    /* Обновляем глобальное состояние */
    g_current_page = page_el;
    g_current_def = def;
    g_page_is_dynamic = true;
    g_dynamic_page = dpage;
    
    main_screen_sprite.needs_render = true;
    
    return dpage;
}

bool Page_CloseDynamic(DynamicPage_t* page)
{
    if (!page) return false;
    
    UIElement_t* page_el = &page->element;
    
    /* Guard: если page_rows_count уже равен snapshot — страница уже закрыта */
    if (panel_rows_count == g_page_rows_snapshot) {
        printf("[Page] Dynamic already closed, clearing state\n");
        g_current_page = NULL;
        g_current_def = NULL;
        g_page_is_dynamic = false;
        g_dynamic_page = NULL;
        heap_caps_free(page);
        return true;
    }
    
    /* Деинициализация */
    if (page->def.on_deinit) {
        page->def.on_deinit(page_el);
    }
    
    /* Удаляем из parent */
    if (g_parent_container && page_el) {
        for (uint8_t i = 0; i < g_parent_container->children_count; i++) {
            if (g_parent_container->children[i] == page_el) {
                for (uint8_t j = i; j < g_parent_container->children_count - 1; j++) {
                    g_parent_container->children[j] = g_parent_container->children[j + 1];
                }
                g_parent_container->children_count--;
                break;
            }
        }
    }
    
    /* ВОЗВРАЩАЕМ весь пул страницы — используем snapshot */
    if (page_el) {
        uint8_t delta = panel_rows_count - g_page_rows_snapshot;
        printf("[Page] Closing dynamic: returning %d elements to snapshot %d (current %d)\n",
               delta, g_page_rows_snapshot, panel_rows_count);
        panel_rows_count = g_page_rows_snapshot;
        printf("[Page] panel_rows_count after close: %d\n", panel_rows_count);
    }
    
    /* Восстанавливаем ListBox меню */
    if (current_menu_listbox && !g_menu_expand_called && g_parent_container) {
        if (g_parent_container->children_count < MAX_ELEMENT_CHILDREN) {
            current_menu_listbox->props.list_box.collapsed = 0;
            g_parent_container->children[g_parent_container->children_count++] = current_menu_listbox;
        }
    }
    g_menu_expand_called = false;
    
    /* Восстанавливаем состояние */
    if (g_page_stack_top >= 0) {
        PageStackEntry_t* prev = &g_page_stack[g_page_stack_top];
        g_current_page = prev->saved_container;
        g_page_stack_top--;
    } else {
        g_current_page = NULL;
    }
    
    g_current_def = NULL;
    g_page_is_dynamic = false;
    g_dynamic_page = NULL;
    
    extern UIElement_t root_grid;
    extern uint16_t Display_Width;
    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, 240);
    
    /* Освобождаем память */
    heap_caps_free(page);
    
    main_screen_sprite.needs_render = true;
    
    return true;
}

/* ========================================================================
 *  Common API
 * ======================================================================== */

bool Page_CloseCurrent(void)
{
    if (g_page_is_dynamic && g_dynamic_page) {
        return Page_CloseDynamic(g_dynamic_page);
    }
    return Page_CloseStatic();
}

bool Page_IsActive(void)
{
    return g_current_page != NULL;
}

UIElement_t* Page_GetCurrent(void)
{
    return g_current_page;
}

PageDef_t* Page_GetCurrentDef(void)
{
    return g_current_def;
}

void Page_DrawCurrent(void)
{
    if (!g_current_page || !g_current_def || !g_current_def->on_draw) return;
    
    g_current_def->on_draw(g_current_page);
}

void Page_UpdateAll(void)
{
    if (!g_current_page || !g_current_def) return;
    
    /* Вызываем on_update, если он определён (для страниц с динамическими данными) */
    if (g_current_def->on_update) {
        g_current_def->on_update(g_current_page);
    }
}

/**
 * @brief Установить флаг что Menu_Expand уже вызван (избежать дубля)
 *        Вызывается из on_input если on_input сам вызывает Menu_Expand
 */
void Page_SetMenuExpandCalled(void)
{
    g_menu_expand_called = true;
}

bool Page_ProcessInput(uint8_t key)
{
    printf("[Page] ProcessInput key=%d, g_current_page=%p\n", key, (void*)g_current_page);
    if (!g_current_page || !g_current_def) {
        printf("[Page] No active page\n");
        return false;
    }
    
    /* Если страница обработала ввод — возвращаем true */
    if (g_current_def->on_input) {
        printf("[Page] Calling on_input\n");
        if (g_current_def->on_input(g_current_page, key)) {
            printf("[Page] on_input handled key\n");
            return true;
        }
    }
    
    /* Если не обработала — передаём на обработку по умолчанию */
    printf("[Page] on_input returned false, checking KEY_CANCEL=%d\n", KEY_CANCEL);
    if (key == KEY_CANCEL) {
        printf("[Page] KEY_CANCEL detected, closing\n");
        Page_CloseCurrent();
        return true;
    }
    
    printf("[Page] Key not handled\n");
    return false;
}

/* ========================================================================
 *  Helpers для построения UI
 * ======================================================================== */

UIElement_t* Page_CreateGrid(UIElement_t* parent, uint8_t rows, uint8_t cols)
{
    UIElement_t* grid = page_alloc_element();
    if (!grid) return NULL;
    
    grid->type = UI_TYPE_GRID;
    grid->sprite = &main_screen_sprite;
    grid->background_color = RGB565_BLACK;
    grid->font = &font_arial_9_struct;
    grid->children_count = 0;
    grid->props.grid.rows_count = rows;
    grid->props.grid.cols_count = cols;
    
    if (parent && parent->children_count < MAX_ELEMENT_CHILDREN) {
        parent->children[parent->children_count++] = grid;
    }
    
    return grid;
}

UIElement_t* Page_CreateStackPanel(UIElement_t* parent, Orientation_t orientation, uint16_t spacing)
{
    UIElement_t* panel = page_alloc_element();
    if (!panel) return NULL;
    
    panel->type = UI_TYPE_STACK_PANEL;
    panel->sprite = &main_screen_sprite;
    panel->background_color = RGB565_BLACK;
    panel->font = &font_arial_9_struct;
    panel->children_count = 0;
    panel->props.stack.orientation = orientation;
    panel->props.stack.spacing = spacing;
    
    if (parent && parent->children_count < MAX_ELEMENT_CHILDREN) {
        parent->children[parent->children_count++] = panel;
    }
    
    return panel;
}

UIElement_t* Page_AddText(UIElement_t* panel, const char* text)
{
    if (!panel || panel->type != UI_TYPE_STACK_PANEL) return NULL;
    
    UIElement_t* text_el = page_alloc_element();
    if (!text_el) return NULL;
    
    text_el->type = UI_TYPE_TEXT_BLOCK;
    text_el->sprite = &main_screen_sprite;
    text_el->background_color = RGB565_BLACK;
    text_el->font = &font_arial_9_struct;
    text_el->horizontal_alignment = HORIZONTAL_ALIGN_LEFT;
    text_el->vertical_alignment = VERTICAL_ALIGN_TOP;
    
    if (text) {
        strncpy(text_el->text_content, text, sizeof(text_el->text_content) - 1);
        text_el->text_content[sizeof(text_el->text_content) - 1] = '\0';
    } else {
        text_el->text_content[0] = '\0';
    }
    
    if (panel->children_count < MAX_ELEMENT_CHILDREN) {
        panel->children[panel->children_count++] = text_el;
    }
    
    return text_el;
}

void Page_AddTextF(UIElement_t* panel, const char* format, ...)
{
    if (!panel || !format) return;
    
    char buf[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    
    Page_AddText(panel, buf);
}

void Page_SetGridRowPixel(UIElement_t* grid, uint8_t row_idx, uint8_t size_px)
{
    if (!grid || grid->type != UI_TYPE_GRID) return;
    if (row_idx >= MAX_GRID_CHILDREN) return;
    
    GridDefinition_t* g = &grid->props.grid;
    g->row_definitions[row_idx] = size_px;
    g->row_is_pixel[row_idx] = true;
}

void Page_SetGridColPercent(UIElement_t* grid, uint8_t col_idx, uint8_t percent)
{
    if (!grid || grid->type != UI_TYPE_GRID) return;
    if (col_idx >= MAX_GRID_CHILDREN) return;
    
    GridDefinition_t* g = &grid->props.grid;
    g->col_definitions[col_idx] = percent;
    g->col_is_pixel[col_idx] = false;
}

void Page_SetGridColPixel(UIElement_t* grid, uint8_t col_idx, uint8_t size_px)
{
    if (!grid || grid->type != UI_TYPE_GRID) return;
    if (col_idx >= MAX_GRID_CHILDREN) return;
    
    GridDefinition_t* g = &grid->props.grid;
    g->col_definitions[col_idx] = size_px;
    g->col_is_pixel[col_idx] = true;
}
