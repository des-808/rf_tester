#ifndef __SDMMC_H__
#define __SDMMC_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

extern SD_HandleTypeDef hsd1;

void MX_SDMMC1_SD_Init(void);

#ifdef __cplusplus
}
#endif

#endif
