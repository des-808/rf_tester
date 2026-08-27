#ifndef __SD_FS_H
#define __SD_FS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>
#include <stdio.h>

// FatFS drive: "0:" = SD card
#define SD_FS_DRIVE   "0:"

// File open modes
typedef enum {
    SD_FS_MODE_READ     = 0,    // Open existing file for reading
    SD_FS_MODE_WRITE    = 1,    // Open existing file for writing (append at end)
    SD_FS_MODE_CREATE   = 2,    // Create new file (truncate if exists)
    SD_FS_MODE_APPEND   = 3     // Open for writing, append at end
} SD_FS_Mode_t;

// File status
typedef enum {
    SD_FS_OK            = 0,
    SD_FS_ERROR         = -1,
    SD_FS_NOT_FOUND     = -2,
    SD_FS_FULL          = -3,
    SD_FS_WRITE_PROTECTED = -4
} SD_FS_Status_t;

// Directory entry
typedef struct {
    char name[256];
    bool is_directory;
    uint32_t size;
} SD_FS_DirEntry_t;

// ============================================================
// INIT / STATUS
// ============================================================

/**
 * @brief   Initialize FatFS on SD card
 * @retval  SD_FS_OK on success
 */
SD_FS_Status_t SD_FS_Init(void);

/**
 * @brief   Check if FatFS is mounted and ready
 * @retval  true if ready
 */
bool SD_FS_IsReady(void);

/**
 * @brief   Check if SD card has free space (at least min_free_kb KB)
 * @param   min_free_kb  Minimum free space in KB
 * @retval  true if enough free space
 */
bool SD_FS_HasFreeSpace(uint32_t min_free_kb);

// ============================================================
// FILE OPERATIONS
// ============================================================

/**
 * @brief   Open a file
 * @param   path     File path (e.g., "0:/data/log.csv")
 * @param   mode     Open mode
 * @retval  File pointer or NULL on error
 */
FILE* SD_FS_Open(const char *path, SD_FS_Mode_t mode);

/**
 * @brief   Close a file
 * @param   f  File pointer
 * @retval  SD_FS_OK on success
 */
SD_FS_Status_t SD_FS_Close(FILE *f);

/**
 * @brief   Write to file (printf-style)
 * @param   fmt    Format string
 * @retval  Number of bytes written, or negative on error
 */
int SD_FS_Print(const char *fmt, ...);

/**
 * @brief   Write raw data to file
 * @param   data   Data buffer
 * @param   size   Data size in bytes
 * @retval  Number of bytes written, or negative on error
 */
int SD_FS_Write(const void *data, uint32_t size);

/**
 * @brief   Read from file (printf-style scan)
 * @param   fmt    Format string (same as sscanf)
 * @retval  Number of items parsed
 */
int SD_FS_Scanf(const char *fmt, ...);

/**
 * @brief   Read raw data from file
 * @param   buf    Output buffer
 * @param   size   Max bytes to read
 * @retval  Number of bytes read, or negative on error
 */
int SD_FS_Read(void *buf, uint32_t size);

/**
 * @brief   Delete a file
 * @param   path   File path
 * @retval  SD_FS_OK on success
 */
SD_FS_Status_t SD_FS_Delete(const char *path);

/**
 * @brief   Truncate file to zero size
 * @param   path   File path
 * @retval  SD_FS_OK on success
 */
SD_FS_Status_t SD_FS_Truncate(const char *path);

// ============================================================
// DIRECTORY OPERATIONS
// ============================================================

/**
 * @brief   Open directory for scanning
 * @param   path       Directory path (e.g., "0:/")
 * @param   pattern    File pattern (e.g., "*.csv" or NULL for all)
 * @retval  true if directory opened
 */
bool SD_FS_OpenDir(const char *path, const char *pattern);

/**
 * @brief   Read next directory entry
 * @param   entry  Pointer to entry structure
 * @retval  true if entry found
 */
bool SD_FS_ReadDir(SD_FS_DirEntry_t *entry);

/**
 * @brief   Close directory scan
 */
void SD_FS_CloseDir(void);

#ifdef __cplusplus
}
#endif

#endif /* __SD_FS_H */
