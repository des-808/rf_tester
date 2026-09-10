/**
 * @file page_rssi.c
 * @brief RSSI Plotter как страница Page System
 * 
 * RSSI Plotter рисуется на graph_sprite (левая половина экрана),
 * а меню справа скрыто через Menu_Collapse().
 * 
 * При открытии: Menu_Collapse() → RssiPlotterScreen_Enter()
 * При закрытии: RssiPlotterScreen_ExitGlobal() → Menu_Expand()
 * При обновлении: on_update → RssiPlotterScreen_UpdateGlobal()
 * При отрисовке: on_draw → рисует на graph_sprite через render_callback
 */

#include "page.h"
#include "rssi_plotter_screen.h"
#include "st7796.h"
#include "gui.h"
#include <stdio.h>

/* ========================================================================
 *  RSSI Page — callbacks
 * ======================================================================== */

/**
 * @brief Инициализация RSSI страницы
 */
static bool RssiPage_Init(UIElement_t* page, UIElement_t* parent)
{
    (void)page;
    (void)parent;
    
    /* Запускаем RSSI Plotter (сворачивает меню) */
    RssiPlotterScreen_Enter();
    
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
 * RSSI рисуется на graph_sprite, а не на main_screen_sprite.
 * graph_node.render_callback = Draw_Graph_Content, который внутри
 * проверяет rssi_plotter_active и рисует график.
 */
static void RssiPage_Draw(UIElement_t* page)
{
    (void)page;
    
    /* Ничего делать не нужно — Draw_Graph_Content на graph_sprite
     * сам рисует RSSI при rssi_plotter_active == true */
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
    
    /* KEY_CANCEL = 4 (определено в buttons.h) */
    if (key == 4) {
        /* Очищаем состояние RSSI через публичный API */
        RssiPlotterScreen_ExitGlobal();
        
        /* Закрываем страницу (удаляет из children, но Menu_Expand уже вызван) */
        Page_CloseCurrent();
        
        return true;
    }
    
    return false;
}

/**
 * @brief Деинициализация RSSI страницы
 * 
 * Вызывается Page_CloseCurrent() → Page_CloseStatic() → on_deinit.
 * Состояние уже очищено в RssiPage_Input через RssiPlotterScreen_ExitGlobal().
 */
static void RssiPage_Deinit(UIElement_t* page)
{
    (void)page;
    /* Состояние RSSI уже очищено в RssiPage_Input */
}

/* ========================================================================
 *  Описание RSSI страницы
 * ======================================================================== */

static PageDef_t rssi_page_def = {
    .name = "RSSI Plotter",
    .container_type = UI_TYPE_GRID,    /* Grid не нужен, но требуется для совместимости */
    .rows = 1,
    .cols = 1,
    .spacing = 0,
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
    return &rssi_page_def;
}
