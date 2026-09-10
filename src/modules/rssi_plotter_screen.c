/**
 * @file    rssi_plotter_screen.c
 * @brief   Экран RSSI Plotter — непрерывная визуализация RSSI
 * @note    Рисуется на graph_sprite (левая половина), меню справа
 */

#include "rf_spectrum.h"
#include "radio_cc1101.h"
#include "st7796.h"
#include "gui.h"
#include "menu.h"
#include <string.h>
#include <stdio.h>

/* ========================================================================
 *  Глобальное состояние экрана
 * ======================================================================== */

/* Экземпляр экрана RSSI Plotter */
static RssiPlotterScreen_t s_rssi_screen;

/* Флаг активности (для main.c) */
bool rssi_plotter_active = false;

/* ========================================================================
 *  Публичные API
 * ======================================================================== */

/**
 * @brief Инициализировать экран RSSI Plotter
 */
void RssiPlotterScreen_InitGlobal(void) {
    memset(&s_rssi_screen, 0, sizeof(RssiPlotterScreen_t));
    s_rssi_screen.enabled = false;
    s_rssi_screen.last_sample_tick = 0;
    RfRssiPlotter_Init(&s_rssi_screen.plotter);
    rssi_plotter_active = false;
}

/**
 * @brief Войти в экран RSSI Plotter (сворачивает меню, сбрасывает plotter)
 */
void RssiPlotterScreen_Enter(void) {
    /* Инициализируем экран */
    s_rssi_screen.enabled = true;
    s_rssi_screen.last_sample_tick = 0;
    
    /* Menu_Collapse() вызывается вызывающим (Page_OpenStatic → RssiPage_Init).
     * Этот API оставлен для обратной совместимости. */
    
    /* Сбрасываем plotter */
    RfRssiPlotter_ResetStats(&s_rssi_screen.plotter);
    memset(s_rssi_screen.plotter.rssi_history, SPECTRUM_RSSI_MIN, sizeof(s_rssi_screen.plotter.rssi_history));
    s_rssi_screen.plotter.history_len = 0;
    
    /* Отключаем отладочную отрисовку */
    ui_debug_draw = false;
    
    /* Инвалидируем спрайты для перерисовки (extern из gui.h) */
    
    /* Инвалидируем graph_sprite — отрисовка произойдёт через render_callback */
    if (graph_sprite.is_allocated && graph_sprite.data) {
        graph_sprite.needs_render = true;
        graph_sprite.dirty_x1 = 0; graph_sprite.dirty_y1 = 0;
        graph_sprite.dirty_x2 = graph_sprite.w - 1; graph_sprite.dirty_y2 = graph_sprite.h - 1;
    }
    
    /* Также инвалидируем main_screen_sprite для обновления меню */
    if (main_screen_sprite.is_allocated && main_screen_sprite.data) {
        main_screen_sprite.needs_render = true;
        main_screen_sprite.dirty_x1 = 0; main_screen_sprite.dirty_y1 = 0;
        main_screen_sprite.dirty_x2 = main_screen_sprite.w - 1; main_screen_sprite.dirty_y2 = main_screen_sprite.h - 1;
    }
    
    rssi_plotter_active = true;
}

/**
 * @brief Выйти из экрана RSSI Plotter (восстанавливает меню)
 */
void RssiPlotterScreen_ExitGlobal(void) {
    /* Восстанавливаем меню */
    Menu_Expand();
    
    /* Включаем отладочную отрисовку обратно */
    ui_debug_draw = true;
    
    /* Инвалидируем спрайты для перерисовки (extern из gui.h) */
    if (main_screen_sprite.is_allocated && main_screen_sprite.data) {
        main_screen_sprite.needs_render = true;
    }
    
    if (graph_sprite.is_allocated && graph_sprite.data) {
        graph_sprite.needs_render = true;
    }
    
    s_rssi_screen.enabled = false;
    rssi_plotter_active = false;
}

/**
 * @brief Обновить данные plotter'а из ring buffer
 */
void RssiPlotterScreen_UpdateGlobal(void) {
    if (!s_rssi_screen.enabled) return;
    
    uint32_t now = HAL_GetTick();
    
    /* Выбираем каждые 50 мс (20 Гц обновление) */
    if (now - s_rssi_screen.last_sample_tick < 50) return;
    s_rssi_screen.last_sample_tick = now;
    
    /* Временный буфер для выборок */
    int8_t samples[64];
    uint16_t count = CC1101_RssiBuf_Copy(samples, sizeof(samples));
    
    if (count == 0) return;
    
    /* Обновляем plotter */
    RfRssiPlotter_Update(&s_rssi_screen.plotter, samples, count);
    
    /* Инвалидируем graph_sprite для перерисовки (extern из gui.h) */
    if (graph_sprite.is_allocated && graph_sprite.data) {
        graph_sprite.needs_render = true;
        graph_sprite.dirty_x1 = 0; graph_sprite.dirty_y1 = 0;
        graph_sprite.dirty_x2 = graph_sprite.w - 1; graph_sprite.dirty_y2 = graph_sprite.h - 1;
    }
}

/**
 * @brief Проверить, активен ли экран
 */
bool RssiPlotterScreen_IsActiveGlobal(void) {
    return rssi_plotter_active;
}

/**
 * @brief Обработка нажатия Cancel (выход из экрана)
 */
bool RssiPlotterScreen_ProcessKeyGlobal(uint8_t key) {
    if (!s_rssi_screen.enabled) return false;
    
    /* KEY_CANCEL = 4 (определено в buttons.h) */
    if (key == 4) {
        RssiPlotterScreen_ExitGlobal();
        return true;
    }
    
    return false;
}

/**
 * @brief Получить текущее состояние экрана
 */
RssiPlotterScreen_t* RssiPlotterScreen_GetState(void) {
    return &s_rssi_screen;
}
