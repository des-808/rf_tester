/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "ft6336u.h"
#include "gui.h"
#include "lcd_backlight.h"
#include "bmi160_h7.h"
#include "ds3231.h"
#include "menu.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#include "font.h"

#include "buttons.h"
#include "i2c_scanner.h"
#include "buzzer.h"
#include <string.h>
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */



/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
  HAL_SD_CardCIDTypedef pCID;
HAL_SD_CardCSDTypedef pCSD;
HAL_SD_CardInfoTypeDef pCardInfo;
FT6336U_HandleTypeDef ft6336u;

// Переменные меню
uint16_t sys = 1, room = 1, btn = 1;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void ST7796_Init(void);
static void Scroll_ListBox(int8_t direction);
uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b);
extern DMA_HandleTypeDef hdma_spi4_tx;
extern DMA2D_HandleTypeDef hdma2d;
//extern void drawStatusBar(Sprite_t *sprite);
extern Sprite_t* status_bar_sprite;
extern Sprite_t* main_screen_sprite;
extern Sprite_t* graph_sprite; // если нужен доступ к графику из main.c

extern UIElement_t* ui_btn_row;   // Указатель на элемент кнопки из gui.c
extern UIElement_t* ui_touch_row; // Указатель на элемент тачскрина из gui.c
extern UIElement_t* ui_swr_row;   // Указатель на элемент КСВ из gui.c
extern UIElement_t ui_bands_listbox; // Контейнер ListBox из gui.c

uint16_t last_touch_x = 0;              // Координата X для логики меню
uint16_t last_touch_y = 0;              // Координата Y для логики меню


char debug_str[64] = "BMI160: Wait interrupt..."; // Строка для вывода на экран
volatile uint32_t exti_counter = 0;               // Счетчик прерываний для проверки физики

volatile uint8_t bmi160_irq_received = 0;
uint8_t current_display_orientation = 0; // 0 - Книжная по умолчанию

// Переменные времени из DS3231
extern uint8_t currentHour, currentMinute;
volatile uint8_t ds3231_irq_received = 0;  // Флаг прерывания от DS3231 (1 Гц)
static uint8_t ds3231_prev_minute = 0xFF; // Для отслеживания изменения минуты
DS3231_Time_t ds3231_time;

/* USER CODE END PFP */
/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
PCF8574_HandleTypeDef pcf_handle;
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
void LED_Blink(uint32_t delay)
{
	HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin,GPIO_PIN_SET);
	HAL_Delay(delay - 1);
	HAL_GPIO_WritePin(LED_GPIO_Port,LED_Pin,GPIO_PIN_RESET);
	HAL_Delay(500-1);
}
// Глобальный флаг: true — отрисовать экран, false — ждать изменений
bool ui_needs_refresh = true; 
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
 extern void ST7796_FillScreen(uint16_t color);
 extern Buttons_HandleTypeDef btn_s;
 I2C_Scanner_HandleTypeDef i2c_scanner;
 extern UIElement_t root_grid;
 extern UIElement_t digits_node;
 extern UIElement_t graph_node;
 uint8_t lastButtonState[8] = {0};
 void INIT_FT6336U(void);
int main(void)
{
  /* USER CODE BEGIN 1 */
  MPU_Config();
  CPU_CACHE_Enable();
  /* USER CODE END 1 */
  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* USER CODE BEGIN Init */
  
  /* USER CODE END Init */
  /* Configure the system clock */
  SystemClock_Config();
  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  HAL_GPIO_WritePin(CTP_RESET_GPIO_Port,CTP_RESET_Pin,GPIO_PIN_SET);
  MX_DMA_Init();
  MX_UART4_Init();
  MX_QUADSPI_Init();
  MX_RTC_Init();
  //MX_SDMMC1_MMC_Init();
  MX_SPI4_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_DMA2D_Init();
  MX_USB_DEVICE_Init();
  MX_UART5_Init();
  MX_I2C1_Init();
  MX_SPI6_Init();
  MX_SPI1_Init();
  MX_UART8_Init();

  HAL_Delay(50);
  PCF8574_Init(&pcf_handle, &hi2c1, 0x3C);
  Buttons_Init(&btn_s, &pcf_handle);
  DS3231_Init(&hi2c1);
  // Включаем генерацию 1 Гц на выход INT/SQW DS3231
  DS3231_EnableSquareWave(&hi2c1, 1);
  //FT6336U_Init(&ft6336u, &hi2c1, 0x38);  // адрес 0x38
  INIT_FT6336U();
  //HAL_SD_GetCardCID(&hmmc1, &pCID);
  //HAL_SD_GetCardCSD(&hmmc1, &pCSD);
	//HAL_SD_GetCardInfo(&hmmc1, &pCardInfo);
  if (!BMI160_Init(&hi2c1, BMI160_I2C_ADDR_VCC)) {
      while(1); // Ошибка
  }
	HAL_Delay(150);
  /* USER CODE BEGIN 2 */
   ST7796_Init();
   LCD_Backlight_Init();

  // Используем Segoe Print 12 по умолчанию
  //lcd_set_font(&font_segoe_struct);
  // Или Arial 9:
   lcd_set_font(&font_arial_9_struct);

/* I2C_Scanner_Init(&i2c_scanner, &hi2c1);
// Запуск сканирования
I2C_Scanner_Run(&i2c_scanner);
// Вывод на TFT (вызывайте после очистки экрана)
lcd_clear_screen(0x0000);  // чёрный фон
I2C_Scanner_PrintOnTFT(&i2c_scanner, 10, 20, RGB565_GREEN, RGB565_BLACK,&main_screen_sprite); */
  Menu_Init();
  //GUI_ShowAdvancedMeasurementScreen(current_display_orientation);
  GUI_ShowMenuAdvancedMeasurementScreen(current_display_orientation);
 
   
    /* ds3231_time.Second = 0;   // 0–59
    ds3231_time.Minute = 51;   // 0–59
    ds3231_time.Hour = 17;     // 0–23 (24-hour) или 1–12 (12-hour)
    //ds3231_time.AM_PM = 1;    // 0 = AM, 1 = PM (только для 12-часового режима)
    ds3231_time.Day = 1;      // 1–7 (см. DS3231_Day_t)
    ds3231_time.Date = 24;     // 1–31
    ds3231_time.Month = 8;    // 1–12
    ds3231_time.Year = 26;     // 0–99 (последние 2 цифры года, напр. 25 = 2025)
  DS3231_SetTime(&hi2c1,&ds3231_time); */
  // Инициализация времени из DS3231
  if (DS3231_GetTime(&hi2c1, &ds3231_time) == DS3231_OK) {
    currentHour = ds3231_time.Hour;
    currentMinute = ds3231_time.Minute;
    ds3231_prev_minute = ds3231_time.Minute;
    ds3231_irq_received = 1; // Запускаем первый цикл обновления
  }
  
  
  /* USER CODE END 2 */ 

  /* Infinite loop */
   /* USER CODE BEGIN WHILE */
   // Объявление переменной времени DS3231 для использования в цикле
   //DS3231_Time_t ds3231_time;
   
   while (1)
   {
     /* USER CODE END WHILE */

     // ====================================================================
     // Обработка прерывания от BMI160 (ориентация экрана)
     // ====================================================================
      if (bmi160_irq_received) 
      {
        bmi160_irq_received = 0; // Сбрасываем флаг EXTI прерывания
        // Запрашиваем у датчика целевую ориентацию (0, 1, 2 или 3)
        uint8_t next_orientation = BMI160_CheckOrientationTask(&hi2c1, BMI160_I2C_ADDR_VCC, current_display_orientation);
        // Если положение устройства физически изменилось
        if (next_orientation != current_display_orientation) 
        {
          // Сохраняем состояние меню ПЕРЕД перестройкой
          extern UIElement_t* current_menu_listbox;
          if (current_menu_listbox) {
            saved_menu_scroll_offset = current_menu_listbox->props.list_box.scroll_offset;
            saved_menu_selected_index = current_menu_listbox->props.list_box.selected_index;
          }
          
          current_display_orientation = next_orientation;
          GUI_ShowMenuAdvancedMeasurementScreen(next_orientation);
        }
      }

     // ====================================================================
     // 1. ОБРАБОТКА ФИЗИЧЕСКИХ КНОПОК (PCF8574)
     // ====================================================================
     Buttons_Update(&btn_s);
     uint8_t btn_raw = PCF8574_Read8(&pcf_handle);
     if (btn_raw != 0xFF) {
         Buzzer_Short();
     }
     PCF8574_AcknowledgeChanges(&pcf_handle);

     MenuKey key_short = Buttons_GetKeyShortPress(&btn_s);
     if (key_short != KEY_NONE) {
       Menu_ProcessInput(key_short);
     }

     // ====================================================================
     // 2. ОБРАБОТКА ТАЧСКРИНА
     // ====================================================================
     if (ft6336u.has_touch) {
         uint16_t raw_x, raw_y;
         FT6336U_GetTouchPoint(&ft6336u, 0, &raw_x, &raw_y);
         Convert_Touch_Coordinates(raw_x, raw_y, &last_touch_x, &last_touch_y);
         ft6336u.has_touch = false;
         Buzzer_Short();
         Menu_ProcessTouch(last_touch_x, last_touch_y);
     }

      // ====================================================================
      // 3. ОБНОВЛЕНИЕ ВРЕМЕНИ ИЗ DS3231 (по прерыванию 1 Гц)
      // ====================================================================
      /* if (ds3231_irq_received) {
          ds3231_irq_received = 0;
          
          if (DS3231_GetTime(&hi2c1, &ds3231_time) == DS3231_OK) {
              // Обновляем только когда изменилась минута
              if (ds3231_time.Minute != ds3231_prev_minute) {
                  currentHour = ds3231_time.Hour;
                  currentMinute = ds3231_time.Minute;
                  ds3231_prev_minute = ds3231_time.Minute;
                  GUI_InvalidateStatusBar();
              }
          }
      } */

     if (ds3231_irq_received) {
    ds3231_irq_received = 0;
    
    if (DS3231_GetTime(&hi2c1, &ds3231_time) == DS3231_OK) {
        // Сохраняем предыдущие значения для сравнения
        static uint8_t prev_minute = 0xFF;
        
        if (ds3231_time.Minute != prev_minute) {
            currentHour = ds3231_time.Hour;
            currentMinute = ds3231_time.Minute;
            prev_minute = ds3231_time.Minute;
            GUI_InvalidateStatusBar();
        }
    }
}

    // ====================================================================
    // 4. СИСТЕМНЫЙ ВЫВОД НА ЭКРАН (Layout Engine)
    // ====================================================================
    UI_DrawTree(&root_grid);
    
    // 5. ПЛАВНАЯ АНИМАЦИЯ ПОДСВЕТКИ (неблокирующая)
    // ====================================================================
    LCD_Backlight_SmoothUpdate();
    
    // Разгрузочная пауза для DMA SPI и Watchdog
    HAL_Delay(10); 
     
     /* USER CODE BEGIN 3 */
   }
}



static void Scroll_ListBox(int8_t direction) {
    uint16_t font_h = (current_font != NULL) ? current_font->char_height : font_arial_9_struct.char_height;
    uint16_t item_h = font_h + 6;
    uint8_t visible = (ui_bands_listbox.h + item_h - 1) / item_h;
    if (visible == 0) visible = 1;
    
    uint8_t max_offset = (ui_bands_listbox.children_count > visible) 
                         ? (uint8_t)(ui_bands_listbox.children_count - visible) 
                         : 0;
    
    bool changed = false;

    if (direction == -1) {
        // Скролл вверх (уменьшаем offset)
        if (ui_bands_listbox.props.list_box.scroll_offset > 0) {
            ui_bands_listbox.props.list_box.scroll_offset--;
            changed = true;
        }
    } else if (direction == 1) {
        // Скролл вниз (увеличиваем offset)
        if (ui_bands_listbox.props.list_box.scroll_offset < max_offset) {
            ui_bands_listbox.props.list_box.scroll_offset++;
            changed = true;
        }
    }

    // Инвалидируем спрайт только если позиция реально изменилась
    if (changed) {
        GUI_InvalidateSprite(digits_node.sprite);
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
