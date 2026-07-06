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


extern void drawStatusBar(Sprite_t *sprite);

Sprite_t status_bar_sprite;
Sprite_t main_screen_sprite;
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
// Массив указателей на ВСЕ ваши спрайты
Sprite_t* my_sprites[] = {
    &status_bar_sprite,
    &main_screen_sprite
    // Сюда можно безопасно дописывать новые спрайты, например центрированные всплывающие окна
};
#define TOTAL_SPRITES (sizeof(my_sprites) / sizeof(my_sprites[0]))

/**
 * @brief Глобальная функция смены ориентации устройства
 * @param rotation: 0 - Книжная, 1 - Альбомная
 */
void UI_ChangeRotation(uint8_t rotation) {
    // 1. Поворачиваем физический чип дисплея (обновляет Display_Width и Display_Height)
    ST7796_SetRotation(rotation);

    // 2. В цикле пересчитываем позиции ВСЕХ спрайтов.
    // Внутри этой функции старая память освобождается, а новая выделяется под новые W и H!
    for (uint8_t i = 0; i < TOTAL_SPRITES; i++) {
        Sprite_UpdatePosition(my_sprites[i]);
    }

    // 3. ОЧИЩАЕМ динамические буферы напрямую через структуры спрайтов
    // Безопасно очищаем статус-бар, если память успешно выделилась
    if (status_bar_sprite.is_allocated && status_bar_sprite.data != NULL) {
        uint32_t sb_size = (uint32_t)status_bar_sprite.w * status_bar_sprite.h;
        for (uint32_t i = 0; i < sb_size; i++) {
            status_bar_sprite.data[i] = RGB565_BLACK; 
        }
    }

    // Безопасно очищаем основной экран
    if (main_screen_sprite.is_allocated && main_screen_sprite.data != NULL) {
        uint32_t ms_size = (uint32_t)main_screen_sprite.w * main_screen_sprite.h;
        for (uint32_t i = 0; i < ms_size; i++) {
            main_screen_sprite.data[i] = RGB565_BLACK;
        }
    }

    // 4. Пишем новый текст под новую ориентацию в динамический буфер
    // Флаг update_after_print ставим в false, чтобы не вызывать PushSprite раньше времени
    if (main_screen_sprite.is_allocated) {
        lcd_print_to_buffer_ex(82, 116, RGB565_BLUE, "Люблю тебя!!!!", RGB565_BLACK, &main_screen_sprite, false);
    }

    // 5. И только теперь выталкиваем обновленные, перевыделенные и заполненные спрайты на экран
    for (uint8_t i = 0; i < TOTAL_SPRITES; i++) {
        if (my_sprites[i]->is_allocated && my_sprites[i]->data != NULL) {
            ST7796_PushSprite(my_sprites[i]);
        }
    }
}

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

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
 extern void ST7796_FillScreen(uint16_t color);
 extern PCF8574_HandleTypeDef pcf_handle;
 extern Buttons_HandleTypeDef btn_s;
 I2C_Scanner_HandleTypeDef i2c_scanner;
uint8_t lastButtonState[8] = {0};
 void ScanButtons(void);
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
	HAL_Delay(150);
  /* USER CODE BEGIN 2 */

  
  

  ST7796_Init();
  Sprite_create_XY(&status_bar_sprite, 320, 30,0,0,ANCHOR_TOP_LEFT); // например, 40px высота
  Buzzer_Short();HAL_Delay(1000);
  Sprite_create_XY(&main_screen_sprite, 320, 449,0,30,ANCHOR_FILL_REMAINING); // например, 440px высота
  Buzzer_Short();HAL_Delay(1000);
//Sprite_fill(&status_bar_sprite,RGB565_BLACK);
//Sprite_fill(&main_screen_sprite,RGB565_BLACK);
  //ST7796_SetRotation(0);
  /* ST7796_TestRotation(main_screen_sprite);
  HAL_Delay(4000);
  ST7796_SetRotation(1);
  ST7796_TestRotation(main_screen_sprite);
  HAL_Delay(4000);
  ST7796_SetRotation(2);
  ST7796_TestRotation(main_screen_sprite);
  HAL_Delay(4000);
  ST7796_SetRotation(3);
  ST7796_TestRotation(main_screen_sprite);
  HAL_Delay(4000);
  ST7796_SetRotation(0); */
  //Sprite_destroy(&main_screen_sprite);
  
  // Используем Segoe Print 12 по умолчанию
  //lcd_set_font(&font_segoe_struct);
  // Или Arial 9:
   lcd_set_font(&font_arial_9_struct);
   uint8_t rotation = 0;
  UI_ChangeRotation(rotation);

   lcd_print_to_buffer_ex(22, 32, RGB565_RED, "Привет!  МОЯ ЗАЙКА !!!",RGB565_BLACK,&main_screen_sprite,false);
   lcd_print_to_buffer_ex(52, 60, RGB565_WHITE, "Как у Тебя Дела??",RGB565_BLACK,&main_screen_sprite,false);
   lcd_print_to_buffer_ex(78, 88, RGB565_YELLOW, "Я соскучился..",RGB565_BLACK,&main_screen_sprite,false);
   lcd_print_to_buffer_ex(82, 116, RGB565_BLUE, "Люблю тебя!!!!",RGB565_BLACK,&main_screen_sprite,false);
   //lcd_set_font(&font_arial_9_struct);// переключаем шрифт
   lcd_print_to_buffer_ex(122, 144, RGB565_BROWN, "И да..",RGB565_BLACK,&main_screen_sprite,false);
   lcd_print_to_buffer_ex(70, 170, RGB565_GREEN, "Доброе утро!!!!!",RGB565_BLACK,&main_screen_sprite,false);
   
   lcd_set_font(&font_segoe_struct);
   lcd_print_to_buffer_ex(2, 206, RGB565(0, 255, 0), "Привет, STM32!",RGB565_BLACK,&main_screen_sprite,false);
   lcd_print_to_buffer_ex(2, 232, RGB565(255, 0, 0), "Текст на русском",RGB565_BLACK,&main_screen_sprite,false);
   lcd_print_to_buffer_ex(2, 258, RGB565(0, 255, 255), "Ку Ку Ёпта!!",RGB565_BLACK,&main_screen_sprite,false);
   lcd_print_to_buffer_ex(2, 284, RGB565(0,0,  255), "Как говорится: ",RGB565_BLACK,&main_screen_sprite,false);
   lcd_print_to_buffer_ex(70, 310, RGB565(200,200,200), "ГОВНО",RGB565_BLACK,&main_screen_sprite,false);
   lcd_print_to_buffer_ex(150, 310, RGB565(255,255,  255), "СЛУЧАЕТСЯ!!!!!",RGB565_BLACK,&main_screen_sprite,true);

  drawStatusBar(&status_bar_sprite);

  HAL_Delay(3000);
  rotation = 1;
  UI_ChangeRotation(rotation);

  HAL_Delay(3000);
  rotation = 2;
  UI_ChangeRotation(rotation);
  HAL_Delay(3000);
  rotation = 3;
  UI_ChangeRotation(rotation);
  HAL_Delay(3000);
  rotation = 0;
  UI_ChangeRotation(rotation);

  // После MX_I2C1_Init()
/* I2C_Scanner_Init(&i2c_scanner, &hi2c1);
// Запуск сканирования
I2C_Scanner_Run(&i2c_scanner);
// Вывод на TFT (вызывайте после очистки экрана)
//lcd_clear_screen(0x0000);  // чёрный фон
I2C_Scanner_PrintOnTFT(&i2c_scanner, 10, 20, RGB565_GREEN, RGB565_BLACK,&main_screen_sprite); */

  /* USER CODE END 2 */ 

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    if (PCF8574_HasChanges(&pcf_handle)) {
    Buttons_Update(&btn_s);

    //uint8_t btn = Buttons_GetJustPressed(&btn_s);
    uint8_t btn = PCF8574_Read8(&pcf_handle);
    char msg[20];
        snprintf(msg, sizeof(msg), "Btn%d", btn);
        lcd_print_to_buffer_ex(22, 132, RGB565_GREEN, msg, RGB565_BLACK, &main_screen_sprite, true);
     if (btn != 0xFF) {
        char msg[20];
        snprintf(msg, sizeof(msg), "Btn%d", btn);
        lcd_print_to_buffer_ex(22, 132, RGB565_GREEN, msg, RGB565_BLACK, &main_screen_sprite, true);
    }
    else {
        lcd_print_to_buffer_ex(22, 132, RGB565_BLUE, "No btn", RGB565_BLACK, &main_screen_sprite, true);
    } 

    PCF8574_AcknowledgeChanges(&pcf_handle);

    // Отладка: мигнуть LED
    //HAL_GPIO_TogglePin(LED_GPIO_Port, LED_Pin);
    // Звук 2400 Hz
/* Buzzer_SetFrequency(1400);
Buzzer_On(1400);
HAL_Delay(1500);

Buzzer_SetFrequency(1200); // ← здесь должно быть "чистое" изменение, без щелчка
HAL_Delay(500);

Buzzer_SetFrequency(600);
HAL_Delay(500);

Buzzer_Off(); */


//Buzzer_Short();HAL_Delay(1500);
//Buzzer_Long();HAL_Delay(1500);
//Buzzer_Beep2();HAL_Delay(1500);
//Buzzer_Beep3();HAL_Delay(1500);
//Buzzer_Alarm();HAL_Delay(1500);
Buzzer_Confirm();//HAL_Delay(2500);

//Buzzer_Error();




}

//ScanButtons();

if (ft6336u.has_touch ) {
    //uint8_t n = FT6336U_GetTouchNum(&ft6336u);
    char msg[64];
    for (int i = 0; i < 1; i++) {//for (int i = 0; i < n; i++) {
        uint16_t x, y;
        //FT6336U_GetTouchPoint(&ft6336u, i, &x, &y);
        FT6336U_GetTouchPoint(&ft6336u, i, &x, &y);//только для 1 touch
        // Обработка: например, x < 100 → кнопка "Back"
        // Формируем строку
        snprintf(msg, sizeof(msg), "Touch %d: ( %d, %d )", i, x, y);
        // Выводим на экран — позиция 0, 160+20*i (чтобы не перекрывать)
        lcd_print_to_buffer_ex(20, 40 + 20 * i, RGB565_CYAN, msg, RGB565_BLACK, &main_screen_sprite, true);
    }

    ft6336u.has_touch = false;  // сброс
    //Buzzer_Confirm();
    Buzzer_Short();
}

/* // Тест: 10 попыток считать данные напрямую
    char msg[40];
    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_RESET); // LED on

    for (int i = 0; i < 10; i++) {
        uint8_t data = 0;
        HAL_StatusTypeDef status = HAL_I2C_Master_Receive(&hi2c1, 0x70, &data, 1, 100);
        if (status == HAL_OK) {
            snprintf(msg, sizeof(msg), "Read %d: 0x%02X OK   ", i, data);
        } else {
            snprintf(msg, sizeof(msg), "Read %d: FAIL %d   ", i, (int)status);
        }

        // Выводим на экран (каждое новое состояние заменяет предыдущее)
        lcd_print_to_buffer_ex(0, 20 + i * 12, RGB565_GREEN, msg, RGB565_BLACK, &main_screen_sprite, true);
        HAL_Delay(100); // небольшая задержка, чтобы успеть увидеть
    }

    HAL_GPIO_WritePin(LED_GPIO_Port, LED_Pin, GPIO_PIN_SET); // LED off
    HAL_Delay(2000); // пауза перед следующим циклом */
////////////////////////
    // Другая логика (не блокирующая)
    HAL_Delay(1); // только для watchdog, если нужен
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
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

void ScanButtons(void) {
    uint8_t data = PCF8574_Read8(&pcf_handle);
     char msg[40];
snprintf(msg, sizeof(msg), "Read 0x%02X OK   ", data);
lcd_print_to_buffer_ex(0, 30 * 12, RGB565_GREEN, msg, RGB565_BLACK, &main_screen_sprite, true);
    for (int i = 0; i < 8; i++) {
        uint8_t current = (data >> i) & 0x01;   // 0 = нажата (активный низ)
        bool pressed = (current == 0);

        if (pressed && !lastButtonState[i]) {
            // Нажатие!
            // ... ваш код (например, toggle LED)
        }

        lastButtonState[i] = pressed;
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
