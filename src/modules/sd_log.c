/**
 * @file    sd_log.c
 * @brief   Test data logger for RF measurements
 *          Writes CSV files to SD card with date-based naming
 */

#include "sd_log.h"
#include "sd_fs.h"
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Private definitions                                               */
/* ------------------------------------------------------------------ */

#define LOG_DIR         "0:/LOG"
#define MAX_LOG_FILES   100

/* Log file path buffer */
static char s_log_file_path[256] = {0};
static bool s_logging_active = false;
static FILE *s_log_file = NULL;
static uint32_t s_record_count = 0;
static uint32_t s_start_timestamp = 0;

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

static void generate_log_filename(char *buf, uint16_t buf_size)
{
    /* Format: LOG_YYYYMMDD_HHMMSS.csv */
    /* Use a simple counter-based name since we don't have RTC access here */
    static uint32_t file_counter = 0;
    file_counter++;
    
    snprintf(buf, buf_size, "%s/LOG_%08lu.csv", LOG_DIR, (unsigned long)file_counter);
}

static bool ensure_log_directory(void)
{
    /* Try to open directory - if it fails, we'll create file in root */
    /* FatFS doesn't have mkdir, so we assume card is pre-formatted */
    /* or the LOG directory already exists */
    return true;
}

/* ------------------------------------------------------------------ */
/*  Init / Status                                                     */
/* ------------------------------------------------------------------ */

bool SD_Log_Init(void)
{
    /* Initialize SD card filesystem */
    if (SD_FS_Init() != SD_FS_OK) {
        return false;
    }
    
    /* Check free space (need at least 1 MB) */
    if (!SD_FS_HasFreeSpace(1024)) {
        return false;
    }
    
    /* Ensure log directory exists */
    if (!ensure_log_directory()) {
        return false;
    }
    
    s_logging_active = false;
    s_log_file = NULL;
    s_record_count = 0;
    s_start_timestamp = 0;
    
    return true;
}

bool SD_Log_IsActive(void)
{
    return s_logging_active;
}

/* ------------------------------------------------------------------ */
/*  File management                                                   */
/* ------------------------------------------------------------------ */

bool SD_Log_StartNewFile(void)
{
    /* Stop any existing log */
    if (s_logging_active) {
        SD_Log_Stop();
    }
    
    /* Check free space */
    if (!SD_FS_HasFreeSpace(1024)) {
        return false;
    }
    
    /* Generate filename */
    generate_log_filename(s_log_file_path, sizeof(s_log_file_path));
    
    /* Create and open file */
    s_log_file = SD_FS_Open(s_log_file_path, SD_FS_MODE_CREATE);
    if (!s_log_file) {
        return false;
    }
    
    /* Write CSV header */
    SD_FS_Print("timestamp_ms,type,freq_khz,value1,value2,value3\r\n");
    
    s_logging_active = true;
    s_record_count = 0;
    s_start_timestamp = 0;  /* Would use HAL_GetTick() in real implementation */
    
    return true;
}

bool SD_Log_Stop(void)
{
    if (!s_logging_active || !s_log_file) return false;
    
    SD_FS_Status_t st = SD_FS_Close(s_log_file);
    s_log_file = NULL;
    s_logging_active = false;
    
    return (st == SD_FS_OK);
}

uint32_t SD_Log_ClearAll(void)
{
    uint32_t deleted = 0;
    
    if (!SD_FS_IsReady()) return 0;
    
    if (SD_FS_OpenDir(LOG_DIR, "*.csv")) {
        SD_FS_DirEntry_t entry;
        while (SD_FS_ReadDir(&entry)) {
            char path[300];
            snprintf(path, sizeof(path), "%s/%s", LOG_DIR, entry.name);
            if (SD_FS_Delete(path) == SD_FS_OK) {
                deleted++;
            }
        }
        SD_FS_CloseDir();
    }
    
    return deleted;
}

void SD_Log_GetFileName(char *buf, uint16_t buf_size)
{
    if (buf && buf_size > 0) {
        strncpy(buf, s_log_file_path, buf_size - 1);
        buf[buf_size - 1] = '\0';
    }
}

/* ------------------------------------------------------------------ */
/*  Data logging                                                      */
/* ------------------------------------------------------------------ */

bool SD_Log_Write(const LogRecord_t *rec)
{
    if (!s_logging_active || !s_log_file || !rec) return false;
    
    /* Format as CSV line */
    int written = SD_FS_Print("%lu,%d,%.2f,%.4f,%.4f,%.4f\r\n",
                              rec->timestamp,
                              rec->type,
                              rec->value1,
                              rec->value2,
                              rec->value3,
                              0.0f);  /* value4 placeholder */
    
    if (written > 0) {
        s_record_count++;
        return true;
    }
    
    return false;
}

bool SD_Log_Write_KSV(float freq_khz, float ksv, float power_w)
{
    LogRecord_t rec;
    rec.timestamp = s_start_timestamp;  /* HAL_GetTick() - s_start_timestamp */
    rec.type = LOG_RECORD_KSV;
    rec.value1 = freq_khz;
    rec.value2 = ksv;
    rec.value3 = power_w;
    
    return SD_Log_Write(&rec);
}

bool SD_Log_Write_Power(float freq_khz, float power_fw, float power_rw)
{
    LogRecord_t rec;
    rec.timestamp = s_start_timestamp;
    rec.type = LOG_RECORD_POWER;
    rec.value1 = freq_khz;
    rec.value2 = power_fw;
    rec.value3 = power_rw;
    
    return SD_Log_Write(&rec);
}

bool SD_Log_Write_Temp(float temp_c)
{
    LogRecord_t rec;
    rec.timestamp = s_start_timestamp;
    rec.type = LOG_RECORD_TEMP;
    rec.value1 = temp_c;
    rec.value2 = 0.0f;
    rec.value3 = 0.0f;
    
    return SD_Log_Write(&rec);
}

/* ------------------------------------------------------------------ */
/*  Bulk logging                                                      */
/* ------------------------------------------------------------------ */

uint32_t SD_Log_WriteBatch(const LogRecord_t *records, uint32_t count)
{
    if (!records || count == 0) return 0;
    
    uint32_t written = 0;
    for (uint32_t i = 0; i < count; i++) {
        if (SD_Log_Write(&records[i])) {
            written++;
        }
    }
    
    return written;
}

uint32_t SD_Log_GetRecordCount(void)
{
    return s_record_count;
}
