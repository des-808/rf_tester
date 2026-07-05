#ifndef __FLASH_H
#define __FLASH_H

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
//#include "w25qxx.h"

/* Exported types ------------------------------------------------------------*/
/* Exported constants --------------------------------------------------------*/
#define FONT_BIN_ADDRESS   0x000000  // Начальный адрес в Flash
#define FONT_SIZE          14112     // Размер segoe_print_12.bin
/* Exported macro ------------------------------------------------------------*/
/* Exported functions prototypes ---------------------------------------------*/


/* #ifdef __cplusplus
extern "C" {
#endif */





void init_flash();
void SPI_Read_Font(uint8_t* buffer, uint32_t offset, uint32_t size);
void SPI_Write_Font(const uint8_t* font_data);

/* #ifdef __cplusplus
}
#endif */

#endif /*__FLASH_H */