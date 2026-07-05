/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
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
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */

/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define CUSTOM_HID_EPIN_SIZE 0x40U
#define CUSTOM_HID_EPOUT_SIZE 0x40U
#define LED_Pin GPIO_PIN_3
#define LED_GPIO_Port GPIOE
#define KEY1_Pin GPIO_PIN_13
#define KEY1_GPIO_Port GPIOC
#define NRF24L01_CE_Pin GPIO_PIN_2
#define NRF24L01_CE_GPIO_Port GPIOC
#define NRF24L01_IRQ_Pin GPIO_PIN_3
#define NRF24L01_IRQ_GPIO_Port GPIOC
#define NRF24L01_IRQ_EXTI_IRQn EXTI3_IRQn
#define RS485_UART4_RX_Pin GPIO_PIN_1
#define RS485_UART4_RX_GPIO_Port GPIOA
#define CC1101_GDO0_Pin GPIO_PIN_2
#define CC1101_GDO0_GPIO_Port GPIOA
#define CC1101_GDO0_EXTI_IRQn EXTI2_IRQn
#define CC1101_GDO2_Pin GPIO_PIN_3
#define CC1101_GDO2_GPIO_Port GPIOA
#define CTP_INT_Pin GPIO_PIN_4
#define CTP_INT_GPIO_Port GPIOA
#define CTP_INT_EXTI_IRQn EXTI4_IRQn
#define CTP_RESET_Pin GPIO_PIN_4
#define CTP_RESET_GPIO_Port GPIOC
#define RADIO_SCK_Pin GPIO_PIN_5
#define RADIO_SCK_GPIO_Port GPIOA
#define RADIO_MISO_Pin GPIO_PIN_6
#define RADIO_MISO_GPIO_Port GPIOA
#define RADIO_MOSI_Pin GPIO_PIN_7
#define RADIO_MOSI_GPIO_Port GPIOA
#define INT_DS3231_Pin GPIO_PIN_5
#define INT_DS3231_GPIO_Port GPIOC
#define INT_DS3231_EXTI_IRQn EXTI9_5_IRQn
#define LCD_LED_PWM_Pin GPIO_PIN_0
#define LCD_LED_PWM_GPIO_Port GPIOB
#define BTN_ON_OFF_Pin GPIO_PIN_1
#define BTN_ON_OFF_GPIO_Port GPIOB
#define EXTI_PCF8574_Pin GPIO_PIN_7
#define EXTI_PCF8574_GPIO_Port GPIOE
#define EXTI_PCF8574_EXTI_IRQn EXTI9_5_IRQn
#define LCD_RESET_Pin GPIO_PIN_9
#define LCD_RESET_GPIO_Port GPIOE
#define LCD_LED_Pin GPIO_PIN_10
#define LCD_LED_GPIO_Port GPIOE
#define LCD_CS_Pin GPIO_PIN_11
#define LCD_CS_GPIO_Port GPIOE
#define LCD_SCK_Pin GPIO_PIN_12
#define LCD_SCK_GPIO_Port GPIOE
#define LCD_DC_Pin GPIO_PIN_13
#define LCD_DC_GPIO_Port GPIOE
#define LCD_MOSI_Pin GPIO_PIN_14
#define LCD_MOSI_GPIO_Port GPIOE
#define NRF24L01_CS_Pin GPIO_PIN_15
#define NRF24L01_CS_GPIO_Port GPIOE
#define BUZZER_Pin GPIO_PIN_10
#define BUZZER_GPIO_Port GPIOB
#define CC1101_CS_Pin GPIO_PIN_11
#define CC1101_CS_GPIO_Port GPIOB
#define DEBUG_UART5_RX_Pin GPIO_PIN_12
#define DEBUG_UART5_RX_GPIO_Port GPIOB
#define DEBUG_UART5_TX_Pin GPIO_PIN_13
#define DEBUG_UART5_TX_GPIO_Port GPIOB
#define RS485_UART4_TX_Pin GPIO_PIN_1
#define RS485_UART4_TX_GPIO_Port GPIOD
#define FLASH_CS_Pin GPIO_PIN_6
#define FLASH_CS_GPIO_Port GPIOD
#define FLASH_MOSI_Pin GPIO_PIN_7
#define FLASH_MOSI_GPIO_Port GPIOD
#define FLASH_SCK_Pin GPIO_PIN_3
#define FLASH_SCK_GPIO_Port GPIOB
#define FLASH_MISO_Pin GPIO_PIN_4
#define FLASH_MISO_GPIO_Port GPIOB
#define ESP32_RX_Pin GPIO_PIN_0
#define ESP32_RX_GPIO_Port GPIOE
#define ESP32_TX_Pin GPIO_PIN_1
#define ESP32_TX_GPIO_Port GPIOE

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
