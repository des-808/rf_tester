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
#include "ft6336u.h"
#include "gui.h"
#include "bmi160_h7.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
//#define QSPI_BASE 0x90000000
//#define W25Qxx
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#include "font.h"

#include "buttons.h"
#include "i2c_scanner.h"
#include "buzzer.h"
//#include "font8x8_Arial.h"
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
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
void ST7796_Init(void);
uint16_t RGB565(uint8_t r, uint8_t g, uint8_t b);
DMA_HandleTypeDef hdma_spi4_tx;
//extern void drawStatusBar(Sprite_t *sprite);
extern Sprite_t status_bar_sprite;
extern Sprite_t main_screen_sprite;
extern Sprite_t graph_sprite; // если нужен доступ к графику из main.c

extern UIElement_t* ui_btn_row;   // Указатель на элемент кнопки из gui.c
extern UIElement_t* ui_touch_row; // Указатель на элемент тачскрина из gui.c
extern UIElement_t* ui_swr_row;   // Указатель на элемент КСВ из gui.c

uint16_t last_touch_x = 0;              // Координата X для логики меню
uint16_t last_touch_y = 0;              // Координата Y для логики меню

volatile uint8_t bmi160_irq_received = 0;
uint8_t current_display_orientation = 1; // 1 - Альбомная по умолчанию
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
 extern PCF8574_HandleTypeDef pcf_handle;
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
  #ifdef W25Qxx
    SCB->VTOR = QSPI_BASE;
	#endif
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
  MX_USB_DEVICE_Init();
  MX_UART5_Init();
  MX_I2C1_Init();
  MX_SPI6_Init();
  MX_SPI1_Init();
  MX_UART8_Init();

  HAL_Delay(50);
  PCF8574_Init(&pcf_handle, &hi2c1, 0x3C);
  Buttons_Init(&btn_s, &pcf_handle);
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

  // Используем Segoe Print 12 по умолчанию
  //lcd_set_font(&font_segoe_struct);
  // Или Arial 9:
   lcd_set_font(&font_arial_9_struct);

/* I2C_Scanner_Init(&i2c_scanner, &hi2c1);
// Запуск сканирования
I2C_Scanner_Run(&i2c_scanner);
// Вывод на TFT (вызывайте после очистки экрана)
//lcd_clear_screen(0x0000);  // чёрный фон
I2C_Scanner_PrintOnTFT(&i2c_scanner, 10, 20, RGB565_GREEN, RGB565_BLACK,&main_screen_sprite); */
 GUI_ShowAdvancedMeasurementScreen(0);
  /* USER CODE END 2 */ 

  /* Infinite loop */
   /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
// ====================================================================
    if (bmi160_irq_received) 
    {
      bmi160_irq_received = 0; // Сбрасываем флаг события EXTI
      
      // Вызываем упакованную функцию. Она сама решит, нужно ли менять экран
      uint8_t task_reorient_display = BMI160_CheckOrientationTask(&hi2c1, BMI160_I2C_ADDR_VCC, current_display_orientation);
      
      // Если функция вернула команду на смену режима (не 0)
      if (task_reorient_display != 0) 
      {
        current_display_orientation = task_reorient_display;
        
        // Выполняем физический разворот дисплея ST7796
        if (current_display_orientation == 1) {
            // ST7796_SetRotation(1); // Альбомная
        } else {
            // ST7796_SetRotation(0); // Портретная
        }
        
        // Пересчитываем сетку интерфейса rf_tester под новые размеры экрана
        extern uint16_t Display_Width, Display_Height;
        UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);
        ui_needs_refresh = true; // Триггерим полную перерисовку UI дерева
      }
    }

    // ====================================================================
    // 1. ОБРАБОТКА ФИЗИЧЕСКИХ КНОПОК (PCF8574)
    // ====================================================================
    if (PCF8574_HasChanges(&pcf_handle)) {
        Buttons_Update(&btn_s);
        uint8_t btn = PCF8574_Read8(&pcf_handle);
        
        if (btn != 0xFF) {
            UI_SetText(ui_btn_row, "Btn 0x%02X", btn);
            Buzzer_Short();
            
            // Пробный тест прокрутки (скролла) по нажатию физической кнопки
            // Если кнопка совпадает с кодом скролла — сдвигаем offset
            if (btn == 0x7F) { 
                if (digits_node.props.list_box.scroll_offset < (digits_node.children_count - 1)) {
                    digits_node.props.list_box.scroll_offset++;
                    
                    // Пересчитываем геометрию StackPanel, так как видимые строки изменились
                    extern uint16_t Display_Width, Display_Height;
                    UI_MeasureAndArrange(&root_grid, 0, 0, Display_Width, Display_Height);
                    
                    // Помечаем только правый спрайт грязным
                    GUI_InvalidateSprite(digits_node.sprite);
                }
            }
        }
        else {
            UI_SetText(ui_btn_row, "No btn");
        }
        
        PCF8574_AcknowledgeChanges(&pcf_handle);
    }

    // ====================================================================
    // 2. ОБРАБОТКА ТАЧСКРИНА (Клик по строкам StackPanel / ListBox)
    // ====================================================================
    if (ft6336u.has_touch) {
        uint16_t raw_x, raw_y;
        FT6336U_GetTouchPoint(&ft6336u, 0, &raw_x, &raw_y); // Считываем аппаратную точку

        // Конвертируем сырые координаты под текущий альбомный разворот экрана
        Convert_Touch_Coordinates(raw_x, raw_y, &last_touch_x, &last_touch_y);

        // Обновляем текст тачскрина в его динамическом блоке
        UI_SetText(ui_touch_row, "Touch:(%d,%d)", last_touch_x, last_touch_y);
        
        Buzzer_Short(); // Выдаем короткий писк подтверждения клика

        // КРИТИЧЕСКИЙ ФИКС: Обработку тача по элементам выполняем СТРОГО внутри условияhas_touch!
        // Проверяем, попал ли клик пальца по геометрии нашей правой панели digits_node
        if (last_touch_x >= digits_node.x && last_touch_x < (digits_node.x + digits_node.w) &&
            last_touch_y >= digits_node.y && last_touch_y < (digits_node.y + digits_node.h)) {
            
            // Математически вычисляем, на какую по счету строку StackPanel нажал палец
            // Высота шрифта Arial9 + зазор (или Segoe) в среднем составляет около 22 пикселей
            // Для идеальной точности мы считываем высоту el->h / count из MeasureAndArrange
            uint16_t row_height = 22; 
            int8_t clicked_row_idx = (last_touch_y - digits_node.y) / row_height;
            
            // Защита от выхода за границы реального количества строк в панели
            if (clicked_row_idx >= 0 && clicked_row_idx < digits_node.children_count) {
                
                // В зависимости от индекса строки, на которую нажали, переключаем диапазоны тестера!
                switch (clicked_row_idx) {
                    case 0: // Строка 0: "--ИЗМЕРЕНИЯ--" (Статичный заголовок, можно проигнорировать)
                        break;
                        
                    case 1: // Строка 1: "SWR: 1.00" (При клике сбросим или обновим замер)
                        UI_SetText(ui_swr_row, "SCANNING...");
                        break;
                        
                    case 2: // Строка 2: "------------" (Разделитель)
                        break;
                        
                    case 4: // Строка 4: "No btn" (Пример переключения диапазона HF)
                        UI_SetText(ui_swr_row, "BAND: HF (1.8-30M)");
                        break;
                        
                    case 5: // Строка 5: "No touch" (Пример переключения диапазона VHF)
                        UI_SetText(ui_swr_row, "BAND: 2m (144MHz)");
                        break;
                        
                    default:
                        // Для всех остальных строк выводим общую информацию
                        UI_SetText(ui_swr_row, "Row ID %d clicked", clicked_row_idx);
                        break;
                }
                
                // Помечаем левый график грязным, чтобы он перерисовал свою сетку Брезенхема
                // с учетом измененного диапазона частот
                if (graph_node.sprite != NULL) {
                    GUI_InvalidateSprite(graph_node.sprite);
                }
            }
        }

        ft6336u.has_touch = false; // Обязательный сброс аппаратного флага тача!
    }

    // ====================================================================
    // 3. СИСТЕМНЫЙ ВЫВОД НА ЭКРАН (Layout Engine)
    // ====================================================================
    // Вызывается непрерывно на каждой итерации. Благодаря нашей Dirty-Rect оптимизации,
    // функция работает как пустышка (0% CPU), отправляя данные по SPI DMA только тогда,
    // когда UI_SetText или Invalidate локально взвели флагneeds_render у конкретного окна.
    UI_DrawTree(&root_grid); 

    // Разгрузочная пауза для стабильной работы аппаратного DMA SPI и Watchdog
    HAL_Delay(10); 
    
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
