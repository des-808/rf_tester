/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    usart.h
  * @brief   This file contains all the function prototypes for
  *          the usart.c file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USART_H__
#define __USART_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdbool.h>

/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

extern UART_HandleTypeDef huart4;

extern UART_HandleTypeDef huart5;

extern UART_HandleTypeDef huart8;

/* USER CODE BEGIN Private defines */

/* ========================================================================
 *  DEBUG UART — переключение в одном месте
 * ======================================================================== */

/* 0 = UART4 (PD1/PD0 RS485), 1 = UART5 (PB13/PB12 DEBUG) */
#define DBG_UART_SELECT  0

/* Уровни логирования — уменьшите чтобы скрыть лишнее */
#define DBG_LEVEL_OFF    0
#define DBG_LEVEL_ERROR  1
#define DBG_LEVEL_WARN   2
#define DBG_LEVEL_INFO   3
#define DBG_LEVEL_DEBUG  4
#define DBG_LEVEL_VERBOSE 5

/* Текущий уровень — меняйте здесь для фильтрации */
#define DBG_LOG_LEVEL    DBG_LEVEL_VERBOSE

/* Макросы логирования */
#define DBG_ERROR(fmt, ...)  _dbg_log(DBG_LEVEL_ERROR, fmt, ##__VA_ARGS__)
#define DBG_WARN(fmt, ...)   _dbg_log(DBG_LEVEL_WARN, fmt, ##__VA_ARGS__)
#define DBG_INFO(fmt, ...)   _dbg_log(DBG_LEVEL_INFO, fmt, ##__VA_ARGS__)
#define DBG_DEBUG(fmt, ...)  _dbg_log(DBG_LEVEL_DEBUG, fmt, ##__VA_ARGS__)
#define DBG_VERBOSE(fmt, ...) _dbg_log(DBG_LEVEL_VERBOSE, fmt, ##__VA_ARGS__)

/* USER CODE END Private defines */

void MX_UART4_Init(void);
void MX_UART5_Init(void);
void MX_UART8_Init(void);

/* USER CODE BEGIN Prototypes */

/**
 * @brief Отправить данные через debug UART
 * @param data указатель на данные
 * @param len длина данных в байтах
 */
void dbg_uart_tx(const uint8_t* data, uint16_t len);

/**
 * @brief Получить данные из debug UART (неблокирующий)
 * @param data буфер для приёма
 * @param len указатель на длину (вход/выход)
 * @return true если данные приняты
 */
bool dbg_uart_rx(uint8_t* data, uint16_t* len);

/**
 * @brief Отправить отладочное сообщение
 * @param level уровень логирования
 * @param fmt printf-формат
 */
void _dbg_log(uint8_t level, const char* fmt, ...);

/**
 * @brief Инициализация debug UART (вызывается один раз)
 */
void dbg_uart_init(void);

/**
 * @brief Переинициализировать UART4 с новым бодрейтом
 * @param baudIndex Индекс бодрейта (0..14 из rs485_baud_rates[])
 */
void UART4_ReinitByBaudIndex(uint8_t baudIndex);

/* USER CODE END Prototypes */

#ifdef __cplusplus
}
#endif

#endif /* __USART_H__ */

