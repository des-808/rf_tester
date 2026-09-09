/**
 * @file    rssi_plotter_screen.h
 * @brief   Экран RSSI Plotter — непрерывная визуализация RSSI
 */

#ifndef RSSI_PLOTTER_SCREEN_H
#define RSSI_PLOTTER_SCREEN_H

#include <stdint.h>
#include <stdbool.h>
#include "rf_spectrum.h"

/* Глобальный флаг активности */
extern bool rssi_plotter_active;

/**
 * @brief Инициализировать экран RSSI Plotter
 */
void RssiPlotterScreen_InitGlobal(void);

/**
 * @brief Войти в экран RSSI Plotter (сохраняет и скрывает меню)
 */
void RssiPlotterScreen_Enter(void);

/**
 * @brief Выйти из экрана RSSI Plotter (восстанавливает меню)
 */
void RssiPlotterScreen_ExitGlobal(void);

/**
 * @brief Обновить данные plotter'а из ring buffer
 */
void RssiPlotterScreen_UpdateGlobal(void);

/**
 * @brief Проверить, активен ли экран
 */
bool RssiPlotterScreen_IsActiveGlobal(void);

/**
 * @brief Обработка нажатия Cancel (выход из экрана)
 */
bool RssiPlotterScreen_ProcessKeyGlobal(uint8_t key);

/**
 * @brief Отрисовка RSSI Plotter на graph_sprite
 */
void RssiPlotterScreen_Draw(void);

/**
 * @brief Получить текущее состояние экрана
 */
RssiPlotterScreen_t* RssiPlotterScreen_GetState(void);

/* Forward declaration для rf_spectrum.c */
void RssiPlotter_DrawGraph(RssiPlotterScreen_t* screen);

#endif /* RSSI_PLOTTER_SCREEN_H */
