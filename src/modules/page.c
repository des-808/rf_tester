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
    }
}

/* ========================================================================
 *  Static pages
 * ======================================================================== */

bool Page_OpenStatic(PageDef_t* def, UIElement_t* parent_container)
{
    if (!def || !parent_container) return false;
    
    g_parent_container = parent_container;
    
    /* Collapse текущий ListBox меню */
    if (current_menu_listbox) {
        Menu_Collapse();
    }
    
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
    if (!page_el) return false;
    
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
    if (def->on_init) {
        if (!def->on_init(page_el, parent_container)) {
            return false;
        }
    }
    
    /* Добавляем страницу в parent */
    if (parent_container->children_count < MAX_ELEMENT_CHILDREN) {
        parent_container->children[parent_container->children_count++] = page_el;
    }
    
    /* Обновляем глобальное состояние */
    g_current_page = page_el;
    g_current_def = def;
    g_page_is_dynamic = false;
    g_dynamic_page = NULL;
    
    /* Инвалидируем спрайт для перерисовки */
    main_screen_sprite.needs_render = true;
    
    return true;
}

bool Page_CloseStatic(void)
{
    if (!g_current_page) return false;
    
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
    
    /* Восстанавливаем ListBox */
    if (current_menu_listbox) {
        Menu_Expand();
    }
    
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
    
    /* Collapse текущий ListBox меню */
    if (current_menu_listbox) {
        Menu_Collapse();
    }
    
    /* Сохраняем состояние на стек */
    if (g_page_stack_top < MAX_PAGE_DEPTH - 1) {
        g_page_stack_top++;
        g_page_stack[g_page_stack_top].saved_container = g_current_page;
        g_page_stack[g_page_stack_top].was_listbox = (g_current_page == NULL);
        g_page_stack[g_page_stack_top].list_scroll = 0;
        g_page_stack[g_page_stack_top].list_selected = 0;
    }
    
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
    
    /* Добавляем в parent */
    if (parent_container->children_count < MAX_ELEMENT_CHILDREN) {
        parent_container->children[parent_container->children_count++] = page_el;
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
    
    /* Восстанавливаем ListBox */
    if (current_menu_listbox) {
        Menu_Expand();
    }
    
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

bool Page_ProcessInput(uint8_t key)
{
    if (!g_current_page || !g_current_def || !g_current_def->on_input) return false;
    
    /* Если страница обработала ввод — возвращаем true */
    if (g_current_def->on_input(g_current_page, key)) {
        return true;
    }
    
    /* Если не обработала — передаём на обработку по умолчанию */
    /* Например, KEY_CANCEL = back */
    if (key == KEY_CANCEL) {
        Page_CloseCurrent();
        return true;
    }
    
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
