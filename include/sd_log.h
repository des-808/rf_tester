#ifndef __SD_LOG_H
#define __SD_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>

// Log record types
typedef enum {
    LOG_RECORD_KSV = 0,     // КСВ (SWR) measurement
    LOG_RECORD_POWER,       // Мощность
    LOG_RECORD_FREQ,        // Частота
    LOG_RECORD_TEMP,        // Температура
    LOG_RECORD_CUSTOM       // Пользовательская запись
} LogRecordType_t;

// Single log record
typedef struct {
    uint32_t timestamp;     // Мс с начала логирования
    LogRecordType_t type;
    float value1;           // Основное значение
    float value2;           // Вторичное значение (опционально)
    float value3;           // Третичное значение (опционально)
} LogRecord_t;

// ============================================================
// INIT / STATUS
// ============================================================

/**
 * @brief   Initialize SD logging system
 * @note    Creates /LOG directory if needed
 * @retval  true if initialized successfully
 */
bool SD_Log_Init(void);

/**
 * @brief   Check if logging is active
 * @retval  true if logging is active
 */
bool SD_Log_IsActive(void);

// ============================================================
// FILE MANAGEMENT
// ============================================================

/**
 * @brief   Start new log file
 * @note    Creates file with date-based name: LOG_YYYYMMDD_HHMMSS.csv
 * @retval  true if file created successfully
 */
bool SD_Log_StartNewFile(void);

/**
 * @brief   Stop current log file
 * @retval  true if file closed successfully
 */
bool SD_Log_Stop(void);

/**
 * @brief   Delete all log files
 * @retval  Number of files deleted
 */
uint32_t SD_Log_ClearAll(void);

/**
 * @brief   Get current log file name
 * @param   buf      Output buffer
 * @param   buf_size Buffer size
 */
void SD_Log_GetFileName(char *buf, uint16_t buf_size);

// ============================================================
// DATA LOGGING
// ============================================================

/**
 * @brief   Log a single record
 * @param   rec  Log record to write
 * @retval  true if written successfully
 */
bool SD_Log_Write(const LogRecord_t *rec);

/**
 * @brief   Log SWR measurement
 * @param   freq_khz    Частота (кГц)
 * @param   ksv         КСВ
 * @param   power_w     Мощность (Вт)
 * @retval  true if written successfully
 */
bool SD_Log_Write_KSV(float freq_khz, float ksv, float power_w);

/**
 * @brief   Log power measurement
 * @param   freq_khz    Частота (кГц)
 * @param   power_fw    Мощность прямая (Вт)
 * @param   power_rw    Мощность отражённая (Вт)
 * @retval  true if written successfully
 */
bool SD_Log_Write_Power(float freq_khz, float power_fw, float power_rw);

/**
 * @brief   Log temperature
 * @param   temp_c      Температура (°C)
 * @retval  true if written successfully
 */
bool SD_Log_Write_Temp(float temp_c);

// ============================================================
// BULK LOGGING
// ============================================================

/**
 * @brief   Log multiple records at once
 * @param   records     Array of records
 * @param   count       Number of records
 * @retval  Number of records successfully written
 */
uint32_t SD_Log_WriteBatch(const LogRecord_t *records, uint32_t count);

/**
 * @brief   Get number of records in current log file
 * @retval  Record count
 */
uint32_t SD_Log_GetRecordCount(void);

#ifdef __cplusplus
}
#endif

#endif /* __SD_LOG_H */
