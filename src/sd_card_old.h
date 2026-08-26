/* #ifndef SD_CARD_H
#define SD_CARD_H

#include "main.h"
#include "stdint.h"
#include "stdbool.h"

typedef enum {
    SD_CARD_NONE = 0,
    SD_CARD_V1,
    SD_CARD_V2,
    SD_CARD_MMC
} SD_CardType_t;

typedef enum {
    SD_OK = 0,
    SD_ERROR,
    SD_TIMEOUT,
    SD_UNSUPPORTED,
    SD_INIT_FAILED,
    SD_WRITE_ERROR,
    SD_READ_ERROR,
    SD_CRC_ERROR
} SD_Status_t;

typedef struct {
    bool           present;
    bool           writable;
    SD_CardType_t  type;
    uint32_t       block_count;
    uint32_t       capacity_mb;
    uint8_t        bus_width;
    uint8_t        version;
    uint8_t        cid[16];
    uint8_t        csd[16];
    HAL_SD_CardInfoTypeDef CardInfo;
} SD_CardInfo_t;

SD_Status_t SD_Card_Init(void);
SD_Status_t SD_Card_GetInfo(SD_CardInfo_t *info);
bool SD_Card_IsPresent(void);
bool SD_Card_IsWriteProtected(void);
SD_Status_t SD_Card_ReadBlock(uint32_t block_addr, uint8_t *buffer);
SD_Status_t SD_Card_ReadBlocks(uint32_t block_addr, uint32_t block_count, uint8_t *buffer);
SD_Status_t SD_Card_WriteBlock(uint32_t block_addr, const uint8_t *buffer);
SD_Status_t SD_Card_WriteBlocks(uint32_t block_addr, uint32_t block_count, const uint8_t *buffer);

#define SD_BLOCK_SIZE 512

#endif
 */