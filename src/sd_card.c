#include "sd_card.h"
#include "sdmmc.h"
#include <string.h>
#include "stm32h7xx_hal.h"

// ============================================================
// ГЛОБАЛЬНОЕ СОСТОЯНИЕ
// ============================================================
extern SD_HandleTypeDef hsd1;

static SD_CardInfo_t s_card_info = {0};

// ============================================================
// ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ
// ============================================================

static SD_Status_t Check_SD_Error(void) {
    if (hsd1.ErrorCode != HAL_SD_ERROR_NONE) {
        uint32_t err = hsd1.ErrorCode;
        hsd1.ErrorCode = HAL_SD_ERROR_NONE;
        
        if (err == HAL_SD_ERROR_CMD_CRC_FAIL)    return SD_CRC_ERROR;
        if (err == HAL_SD_ERROR_DATA_CRC_FAIL)   return SD_CRC_ERROR;
        if (err == HAL_SD_ERROR_CMD_RSP_TIMEOUT) return SD_TIMEOUT;
        if (err == HAL_SD_ERROR_DATA_TIMEOUT)    return SD_TIMEOUT;
        if (err == HAL_SD_ERROR_TX_UNDERRUN)     return SD_WRITE_ERROR;
        if (err == HAL_SD_ERROR_RX_OVERRUN)      return SD_READ_ERROR;
        return SD_ERROR;
    }
    return SD_OK;
}

// Преобразуем uint32_t CSD[4] в байтовый массив для анализа
static void CSD_To_Bytes(uint8_t *dst, const uint32_t *csd) {
    for (int i = 0; i < 4; i++) {
        dst[i*4]     = (csd[i] >> 24) & 0xFF;
        dst[i*4 + 1] = (csd[i] >> 16) & 0xFF;
        dst[i*4 + 2] = (csd[i] >> 8) & 0xFF;
        dst[i*4 + 3] = csd[i] & 0xFF;
    }
}

// ============================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================

SD_Status_t SD_Card_Init(void) {
    memset(&s_card_info, 0, sizeof(SD_CardInfo_t));
    s_card_info.present = false;
    return SD_NOT_PRESENT;
}



/**
 * @brief   Полная инициализация SD (может блокироваться ~100мс)
 *          Вызывать ТОЛЬКО когда карта точно в слоте!
 * @retval  SD_Status_t
 */
SD_Status_t SD_Card_Detect(void) {
    if(!SD_Card_IsPhysicallyPresent()){return SD_NOT_PRESENT;}
    // Если уже инициализирована — выходим
    if (s_card_info.present) return SD_OK;
    
    // Карта в слоте — инициализируем HAL
    if (HAL_SD_Init(&hsd1) != HAL_OK) {
        s_card_info.present = false;
        return SD_INIT_FAILED;
    }
    
    // Получаем информацию о карте
    HAL_SD_CardInfoTypeDef cardInfo;
    if (HAL_SD_GetCardInfo(&hsd1, &cardInfo) != HAL_OK) {
        s_card_info.present = false;
        return SD_NOT_PRESENT;
    }
    
    s_card_info.present = true;
    s_card_info.type = (SD_CardType_t)cardInfo.CardType;
    s_card_info.version = (cardInfo.CardType == MMC_HIGH_CAPACITY_CARD) ? 2 : 1;
    s_card_info.block_count = cardInfo.LogBlockNbr;
    s_card_info.capacity_mb = (cardInfo.LogBlockSize * cardInfo.LogBlockNbr) / (1024 * 1024);
    s_card_info.bus_width = (hsd1.Init.BusWide == SDMMC_BUS_WIDE_4B) ? 4 : 1;
    
    CSD_To_Bytes(s_card_info.cid, hsd1.CID);
    CSD_To_Bytes(s_card_info.csd, hsd1.CSD);
    
    s_card_info.writable = true;
    uint8_t wp_group = (s_card_info.csd[5] >> 6) & 0x03;
    if (wp_group == 0x01 || wp_group == 0x02) {
        s_card_info.writable = false;
    }
    
    return SD_OK;
}

// ============================================================
// ИНФОРМАЦИЯ
// ============================================================

SD_Status_t SD_Card_GetInfo(SD_CardInfo_t *info) {
    if (!info) return SD_ERROR;
    if (!s_card_info.present) {
        SD_Status_t ret = SD_Card_Detect();
        if (ret != SD_OK) {
            memset(info, 0, sizeof(SD_CardInfo_t));
            info->present = false;
            return ret;
        }
    }
    memcpy(info, &s_card_info, sizeof(SD_CardInfo_t));
    return SD_OK;
}

// ============================================================
// ПРОВЕРКА НАЛИЧИЯ КАРТЫ
// ============================================================

bool SD_Card_IsPhysicallyPresent(void) {
    // PD4 замкнут на GND при вставленной карте
    return (HAL_GPIO_ReadPin(SD_CD_GPIO_Port, SD_CD_Pin) == GPIO_PIN_SET);
}

bool SD_Card_IsPresent(void) {
    return SD_Card_IsPhysicallyPresent();
}

SD_Status_t SD_Card_CheckPresent(void) {
    if (SD_Card_IsPhysicallyPresent()) {
        s_card_info.present = true;
        return SD_OK;
    }
    s_card_info.present = false;
    return SD_NOT_PRESENT;
}

bool SD_Card_IsWriteProtected(void) {
    return !s_card_info.writable;
}

// ============================================================
// ЧТЕНИЕ/ЗАПИСЬ
// ============================================================

SD_Status_t SD_Card_ReadBlock(uint32_t block_addr, uint8_t *buffer) {
    return SD_Card_ReadBlocks(block_addr, 1, buffer);
}

SD_Status_t SD_Card_ReadBlocks(uint32_t block_addr, uint32_t block_count, uint8_t *buffer) {
    if (!s_card_info.present) return SD_ERROR;
    if (!buffer) return SD_ERROR;
    if (block_count == 0) return SD_ERROR;
    if (block_addr + block_count > s_card_info.block_count) return SD_ERROR;
    
    while (hsd1.State == HAL_SD_STATE_BUSY) {
        if (HAL_GetTick() > 10000) return SD_TIMEOUT;
    }
    
    HAL_StatusTypeDef status = HAL_SD_ReadBlocks(&hsd1, buffer, block_addr, block_count, HAL_MAX_DELAY);
    
    while (hsd1.State != HAL_SD_STATE_READY && hsd1.State != HAL_SD_STATE_ERROR) {
        if (HAL_GetTick() > 10000) return SD_TIMEOUT;
    }
    
    if (status != HAL_OK) return SD_ERROR;
    return Check_SD_Error();
}

SD_Status_t SD_Card_WriteBlock(uint32_t block_addr, const uint8_t *buffer) {
    return SD_Card_WriteBlocks(block_addr, 1, buffer);
}

SD_Status_t SD_Card_WriteBlocks(uint32_t block_addr, uint32_t block_count, const uint8_t *buffer) {
    if (!s_card_info.present) return SD_ERROR;
    if (!buffer) return SD_ERROR;
    if (block_count == 0) return SD_ERROR;
    if (s_card_info.writable == false) return SD_WRITE_ERROR;
    if (block_addr + block_count > s_card_info.block_count) return SD_ERROR;
    
    while (hsd1.State == HAL_SD_STATE_BUSY) {
        if (HAL_GetTick() > 10000) return SD_TIMEOUT;
    }
    
    HAL_StatusTypeDef status = HAL_SD_WriteBlocks(&hsd1, (uint8_t*)buffer, block_addr, block_count, HAL_MAX_DELAY);
    
    while (hsd1.State != HAL_SD_STATE_READY && hsd1.State != HAL_SD_STATE_ERROR) {
        if (HAL_GetTick() > 10000) return SD_TIMEOUT;
    }
    
    if (status != HAL_OK) return SD_ERROR;
    return Check_SD_Error();
}
