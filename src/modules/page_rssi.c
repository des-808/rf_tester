/**
 * @file page_rssi.c
 * @brief RSSI Plotter как страница Page System
 * 
 * RSSI Plotter рисуется на ui_screen_sprite (основная область экрана),
 * а меню скрыто через Menu_Collapse().
 * 
 * При открытии: Menu_Collapse() → RssiPlotterScreen_Enter()
 * При закрытии: RssiPlotterScreen_ExitGlobal() → Menu_Expand()
 * При обновлении: on_update → RssiPlotterScreen_UpdateGlobal()
 * При отрисовке: on_draw → рисует на ui_screen_sprite через render_callback
 */

#include "page.h"
#include "rssi_plotter_screen.h"
#include "st7796.h"
#include "gui.h"
#include "usart.h"
#include <stdio.h>

/* ========================================================================
 *  RSSI Page — callbacks
 * ======================================================================== */

/**
 * @brief Инициализация RSSI страницы
 */
static bool RssiPage_Init(UIElement_t* page, UIElement_t* parent)
{
    (void)parent;
    
    DBG_INFO("[PageRSSI] Entering RSSI Plotter");
    
    /* Создаём StackPanel с текстом для страницы */
    UIElement_t* panel = Page_CreateStackPanel(page, ORIENTATION_VERTICAL, 8);
    if (!panel) {
        DBG_ERROR("[PageRSSI] Failed to create StackPanel");
        return false;
    }
    
    /* Добавляем заголовок */
    Page_AddText(panel, "=== RSSI Plotter ===");
    Page_AddText(panel, "");
    Page_AddText(panel, "Scanning...");
    Page_AddText(panel, "");
    Page_AddText(panel, "[CANCEL] Back");
    
    /* Запускаем RSSI Plotter (сворачивает меню) */
    RssiPlotterScreen_Enter();
    
    /* Принудительно инвалидируем ui_screen_sprite */
    extern Sprite_t ui_screen_sprite;
    if (ui_screen_sprite.is_allocated && ui_screen_sprite.data) {
        ui_screen_sprite.needs_render = true;
        ui_screen_sprite.dirty_x1 = 0; ui_screen_sprite.dirty_y1 = 0;
        ui_screen_sprite.dirty_x2 = ui_screen_sprite.w - 1; ui_screen_sprite.dirty_y2 = ui_screen_sprite.h - 1;
    }
    
    return true;
}

/**
 * @brief Обновление данных RSSI (вызывается каждый кадр из main loop)
 */
static void RssiPage_Update(UIElement_t* page)
{
    (void)page;
    
    /* Обновляем данные из ring buffer (внутри — проверка каждые 50мс) */
    RssiPlotterScreen_UpdateGlobal();
}

/**
 * @brief Отрисовка RSSI (вызывается через render_callback UI_DrawTree)
 * 
 * RSSI рисуется на ui_screen_sprite.
 * graph_node.render_callback = Draw_Graph_Content, который внутри
 * проверяет rssi_plotter_active и рисует график.
 * 
 * ВАЖНО: Рисуем детей страницы (StackPanel с текстом) через UI_RenderChildElement,
 * т.к. Page_RenderCallback вызывает только on_draw, но не рендерит детей автоматически.
 */
static void RssiPage_Draw(UIElement_t* page)
{
    /* Рисуем детей страницы (StackPanel -> TextBlock элементы) */
    for (uint8_t i = 0; i < page->children_count && i < MAX_ELEMENT_CHILDREN; i++) {
        UIElement_t* child = (UIElement_t*)page->children[i];
        if (child) {
            UI_RenderChildElement(child);
        }
    }
    
    /* ui_screen_sprite рисуется автоматически через Draw_Graph_Content
     * при rssi_plotter_active == true */
}

/**
 * @brief Обработка ввода для RSSI страницы
 * 
 * KEY_CANCEL: очищаем RSSI через публичный API и закрываем страницу.
 * Остальные ключи — не обрабатываем, возвращаем false.
 */
static bool RssiPage_Input(UIElement_t* page, uint8_t key)
{
    (void)page;
    
    DBG_DEBUG("[PageRSSI] Input key=%d", key);
    /* KEY_CANCEL обрабатывается автоматически в Page_ProcessInput — возвращаем false */
    return false;
}

/**
 * @brief Деинициализация RSSI страницы
 * 
 * Вызывается Page_CloseStatic() после KEY_CANCEL.
 * Здесь очищаем состояние RSSI.
 */
static void RssiPage_Deinit(UIElement_t* page)
{
    (void)page;
    
    DBG_INFO("[PageRSSI] Exiting RSSI Plotter");
    /* Очищаем состояние RSSI */
    RssiPlotterScreen_ExitGlobal();
}

/* ========================================================================
 *  Описание RSSI страницы
 * ======================================================================== */

static PageDef_t rssi_page_def = {
    .name = "RSSI Plotter",
    .container_type = UI_TYPE_STACK_PANEL,  /* StackPanel для вертикального списка */
    .spacing = 8,
    .orientation = ORIENTATION_VERTICAL,
    .on_init = RssiPage_Init,
    .on_draw = RssiPage_Draw,
    .on_update = RssiPage_Update,      /* Вызывается каждый кадр */
    .on_input = RssiPage_Input,
    .on_deinit = RssiPage_Deinit,
    .user_data = NULL
};

/* ========================================================================
 *  Публичный API
 * ======================================================================== */

/**
 * @brief Открыть RSSI Plotter как страницу
 */
void Page_OpenRssiPlotter(void)
{
    Page_OpenStatic(&rssi_page_def, &digits_node);
}

/**
 * @brief Получить описание RSSI страницы (для menu.c)
 */
PageDef_t* Page_GetRssiPageDef(void)
{
    printf("[PageRSSI] Page_GetRssiPageDef returning &rssi_page_def=%p\n", (void*)&rssi_page_def);
    return &rssi_page_def;
}
