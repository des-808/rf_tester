#ifndef __SD_CARD_H
#define __SD_CARD_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

// ============================================================
// STATUS CODES
// ============================================================
typedef enum {
    SD_OK = 0,
    SD_ERROR,
    SD_INIT_FAILED,
    SD_WRITE_ERROR,
    SD_READ_ERROR,
    SD_CRC_ERROR,
    SD_TIMEOUT,
    SD_NOT_PRESENT,
    SD_WRITE_PROTECTED
} SD_Status_t;

// ============================================================
// CARD TYPE
// ============================================================
typedef enum {
    SD_CARD_TYPE_SD1 = 1,
    SD_CARD_TYPE_SD2,
    SD_CARD_TYPE_SDHC,
    SD_CARD_TYPE_MMC
} SD_CardType_t;

// ============================================================
// CARD INFO
// ============================================================
typedef struct {
    bool            present;
    SD_CardType_t   type;
    uint8_t         version;       // SD 1.x / 2.x
    uint32_t        block_count;
    uint32_t        capacity_mb;
    uint8_t         bus_width;     // 1 or 4
    bool            writable;
    uint8_t         cid[16];
    uint8_t         csd[16];
} SD_CardInfo_t;

// ============================================================
// API
// ============================================================

/**
 * @brief   Initialize SD card and read card info
 * @retval  SD_Status_t
 */
SD_Status_t SD_Card_Init(void);

/**
 * @brief   Get SD card information
 * @param   info  Pointer to card info structure
 * @retval  SD_Status_t
 */
SD_Status_t SD_Card_GetInfo(SD_CardInfo_t *info);

/**
 * @brief   Check if SD card is present (non-blocking, ~2ms max)
 * @retval  true if present
 */
bool SD_Card_IsPresent(void);

/**
 * @brief   Check physical card detect pin (PD4)
 * @retval  true if card is physically in the slot
 */
bool SD_Card_IsPhysicallyPresent(void);

/**
 * @brief   Non-blocking SD card presence check with status code.
 *          Returns cached state if card already detected.
 *          Only blocks up to 10ms for CMD8 poll on first detection.
 * @retval  SD_OK             Card present and ready for SD_Card_Detect()
 * @retval  SD_NOT_PRESENT    Card not detected
 */
SD_Status_t SD_Card_CheckPresent(void);

/**
 * @brief   Check if SD card is write-protected
 * @retval  true if write-protected
 */
bool SD_Card_IsWriteProtected(void);

/**
 * @brief   Detect and initialize SD card (may block if card present)
 * @retval  SD_Status_t
 */
SD_Status_t SD_Card_Detect(void);

/**
 * @brief   Read single block (512 bytes)
 * @param   block_addr  Block address
 * @param   buffer      Output buffer (512 bytes)
 * @retval  SD_Status_t
 */
SD_Status_t SD_Card_ReadBlock(uint32_t block_addr, uint8_t *buffer);

/**
 * @brief   Read multiple blocks
 * @param   block_addr   Block address
 * @param   block_count  Number of blocks
 * @param   buffer       Output buffer
 * @retval  SD_Status_t
 */
SD_Status_t SD_Card_ReadBlocks(uint32_t block_addr, uint32_t block_count, uint8_t *buffer);

/**
 * @brief   Write single block (512 bytes)
 * @param   block_addr  Block address
 * @param   buffer      Input buffer (512 bytes)
 * @retval  SD_Status_t
 */
SD_Status_t SD_Card_WriteBlock(uint32_t block_addr, const uint8_t *buffer);

/**
 * @brief   Write multiple blocks
 * @param   block_addr   Block address
 * @param   block_count  Number of blocks
 * @param   buffer       Input buffer
 * @retval  SD_Status_t
 */
SD_Status_t SD_Card_WriteBlocks(uint32_t block_addr, uint32_t block_count, const uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif /* __SD_CARD_H */
