/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body with LVGL
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

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "dma.h"
#include "i2c.h"
#include "quadspi.h"
#include "rtc.h"
#include "sdmmc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_device.h"
#include "gpio.h"
#include "flash.h"
#include "dma2d.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ft6336u.h"
#include "lcd_backlight.h"
#include "bmi160_h7.h"
#include "ds3231.h"
#include "buttons.h"
#include "buzzer.h"
#include "vibrator.h"
#include "lvgl_module.h"
#include "lvgl_display.h"
#include "lvgl_example.h"

extern uint8_t rs485BaudIndex;
#include <string.h>
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
FT6336U_HandleTypeDef ft6336u;

/* BMI160 orientation */
volatile uint8_t bmi160_irq_received = 0;
uint8_t current_display_orientation = 0;

/* DS3231 RTC */
 uint8_t currentHour, currentMinute;
volatile uint8_t ds3231_irq_received = 0;
static uint8_t ds3231_prev_minute = 0xFF;
DS3231_Time_t ds3231_time;

/* Screen lock */
bool screen_locked = false;

/* Buzzer and vibrator settings */
int buzzerOnOff = 1;
int vibroOnOff = 1;

/* Button handle */
PCF8574_HandleTypeDef pcf_handle;
Buttons_HandleTypeDef btn_s;
/* USER CODE END PV */



/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);

/* USER CODE BEGIN PFP */
void ST7796_Init(void);
extern DMA2D_HandleTypeDef hdma2d;
void INIT_FT6336U(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();
  
  /* Configure the MPU attributes for the QSPI 256MB without instruction access */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.Number           = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress      = QSPI_BASE;
  MPU_InitStruct.Size             = MPU_REGION_SIZE_256MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.IsBufferable     = MPU_ACCESS_NOT_BUFFERABLE;
  MPU_InitStruct.IsCacheable      = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
  MPU_InitStruct.SubRegionDisable = 0x00;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  
  /* Configure the MPU attributes for the QSPI 8MB (QSPI Flash Size) to Cacheable WT */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.Number           = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress      = QSPI_BASE;
  MPU_InitStruct.Size             = MPU_REGION_SIZE_8MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_PRIV_RO;
  MPU_InitStruct.IsBufferable     = MPU_ACCESS_BUFFERABLE;
  MPU_InitStruct.IsCacheable      = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable      = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
  MPU_InitStruct.SubRegionDisable = 0x00;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  
  /* Setup AXI SRAM in Cacheable WB */
  MPU_InitStruct.Enable           = MPU_REGION_ENABLE;
  MPU_InitStruct.BaseAddress      = D1_AXISRAM_BASE;
  MPU_InitStruct.Size             = MPU_REGION_SIZE_512KB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsBufferable     = MPU_ACCESS_BUFFERABLE;
  MPU_InitStruct.IsCacheable      = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsShareable      = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.Number           = MPU_REGION_NUMBER2;
  MPU_InitStruct.TypeExtField     = MPU_TEX_LEVEL1;
  MPU_InitStruct.SubRegionDisable = 0x00;
  MPU_InitStruct.DisableExec      = MPU_INSTRUCTION_ACCESS_ENABLE;
  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

static void CPU_CACHE_Enable(void)
{
  /* Enable I-Cache */
  SCB_EnableICache();

  /* Enable D-Cache */
  SCB_EnableDCache();
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
  /* USER CODE BEGIN 1 */
  MPU_Config();
  CPU_CACHE_Enable();
  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SDMMC1_SD_Init();
  MX_DMA_Init();
  MX_UART4_Init();
  MX_QUADSPI_Init();
  MX_RTC_Init();
  MX_SPI4_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  LCD_Backlight_Init();
  MX_DMA2D_Init();
  MX_USB_DEVICE_Init();
  MX_UART5_Init();
  MX_I2C1_Init();
  MX_SPI6_Init();
  MX_SPI1_Init();
  MX_UART8_Init();

  /* Initialize peripherals */
  HAL_Delay(50);
  PCF8574_Init(&pcf_handle, &hi2c1, 0x3C);
  Buttons_Init(&btn_s, &pcf_handle);
  DS3231_Init(&hi2c1);
  DS3231_EnableSquareWave(&hi2c1, 1);
  INIT_FT6336U();
  
  if (!BMI160_Init(&hi2c1, BMI160_I2C_ADDR_VCC)) {
      while(1); // Error
  }
  HAL_Delay(150);

  // Initialize time from DS3231
  if (DS3231_GetTime(&hi2c1, &ds3231_time) == DS3231_OK) {
    currentHour = ds3231_time.Hour;
    currentMinute = ds3231_time.Minute;
    ds3231_prev_minute = ds3231_time.Minute;
    ds3231_irq_received = 1;
  }

  /* USER CODE BEGIN 2 */
  /* Initialize LVGL */
  lvgl_module_init();
  
  /* Create example UI */
  //lvgl_create_example_ui();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* ==================================================================== */
    /* 1. ОБРАБОТКА ПРЕРЫВАНИЯ ОТ BMI160 (ОРИЕНТАЦИЯ ЭКРАНА)                */
    /* ==================================================================== */
    if (bmi160_irq_received) {
      bmi160_irq_received = 0;
      uint8_t next_orientation = BMI160_CheckOrientationTask(&hi2c1, BMI160_I2C_ADDR_VCC, current_display_orientation);
      if (next_orientation != current_display_orientation) {
        current_display_orientation = next_orientation;
        //lvgl_set_rotation((lvgl_rotation_t)next_orientation);
      }
    }

    /* ==================================================================== */
    /* 2. ОБРАБОТКА КНОПКИ БЛОКИРОВКИ ЭКРАНА (BTN_ON_OFF)                   */
    /* ==================================================================== */
    static uint8_t btn_on_off_last = 1;
    uint8_t btn_on_off_state = HAL_GPIO_ReadPin(BTN_ON_OFF_GPIO_Port, BTN_ON_OFF_Pin);
    if (btn_on_off_state != btn_on_off_last) {
        btn_on_off_last = btn_on_off_state;
        if (btn_on_off_state == 0) {
            screen_locked = !screen_locked;
            if (screen_locked) {
                LCD_Backlight_SetLevel(0);
            } else {
                LCD_Backlight_SetLevel(LCD_Backlight_GetLevel() > 0 ? LCD_Backlight_GetLevel() : 5);
            }
            if (buzzerOnOff) Buzzer_Short();
            if (vibroOnOff) Vibrator_Pulse(30);
        }
    }

    /* ==================================================================== */
    /* 3. ОБРАБОТКА ТАЧСКРИНА                                               */
    /* ==================================================================== */
    if (!screen_locked) {
        static uint32_t last_i2c_poll_tick = 0;
        
        /* Periodic I2C poll */
        if (HAL_GetTick() - last_i2c_poll_tick > (ft6336u.has_touch ? 20 : 100)) {
            last_i2c_poll_tick = HAL_GetTick();
            FT6336U_ReadData(&ft6336u);
        }
        
        if (ft6336u.has_touch) {
            uint16_t raw_x, raw_y;
            FT6336U_GetTouchPoint(&ft6336u, 0, &raw_x, &raw_y);
            ft6336u.has_touch = false;
            if (buzzerOnOff) Buzzer_Short();
            if (vibroOnOff) Vibrator_Pulse(30);
        }
    } else {
        if (ft6336u.has_touch) {
            ft6336u.has_touch = false;
        }
    }

    /* ==================================================================== */
    /* 4. ОБНОВЛЕНИЕ ВРЕМЕНИ ИЗ DS3231                                      */
    /* ==================================================================== */
    if (ds3231_irq_received) {
        ds3231_irq_received = 0;
        if (DS3231_GetTime(&hi2c1, &ds3231_time) == DS3231_OK) {
            static uint8_t prev_minute = 0xFF;
            if (ds3231_time.Minute != prev_minute) {
                currentHour = ds3231_time.Hour;
                currentMinute = ds3231_time.Minute;
                prev_minute = ds3231_time.Minute;
            }
        }
    }

    /* ==================================================================== */
    /* 5. ПЛАВНАЯ АНИМАЦИЯ ПОДСВЕТКИ                                        */
    /* ==================================================================== */
    LCD_Backlight_SmoothUpdate();

    /* ==================================================================== */
    /* 6. LVGL TICK AND TASK HANDLER                                        */
    /* ==================================================================== */
    lvgl_module_tick();
    lvgl_module_run();
     
    /* USER CODE BEGIN 3 */
  }
}

void INIT_FT6336U(void){
    // 1. Сброс FT6336U
    HAL_GPIO_WritePin(CTP_RESET_GPIO_Port, CTP_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(10);

    // 2. Выход из сброса
    HAL_GPIO_WritePin(CTP_RESET_GPIO_Port, CTP_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(50); // или 100 мс — зависит от чипа

    // 3. Проверка готовности (опционально)
    if (HAL_I2C_IsDeviceReady(&hi2c1, 0x70, 3,100) == HAL_OK) {
        // FT6336U (адрес 0x38 << 1 = 0x70) отвечает
        FT6336U_Init(&ft6336u, &hi2c1, 0x38);
    } else {
        // Ошибка: не найден сенсор
    }
}
/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Configure LSE Drive Capability
  */
  HAL_PWR_EnableBkUpAccess();
  __HAL_RCC_LSEDRIVE_CONFIG(RCC_LSEDRIVE_LOW);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 96;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}


/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
