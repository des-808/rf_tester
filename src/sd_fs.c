/**
 * @file    sd_fs.c
 * @brief   FatFS file system wrapper for SD card
 *          Uses ST FatFS generator driver pattern
 */

#include "sd_fs.h"
#include "ff.h"
#include "ff_gen_drv.h"
#include "diskio.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Private types                                                       */
/* ------------------------------------------------------------------ */

#define MAX_OPEN_FILES 4

typedef struct {
    FIL fil;
    bool writable;
    bool in_use;
} SD_FILE_t;

static SD_FILE_t s_file_pool[MAX_OPEN_FILES] = {{0}};

/* Directory scan state */
static DIR s_dir = {0};
static bool s_dir_open = false;
static char s_dir_pattern[256] = {0};

/* Current active file for Print/Write/Read convenience functions */
static SD_FILE_t *s_current_file = NULL;

/* FatFS filesystem object (static to avoid stack overflow) */
static FATFS s_fs_object;

/* External disk driver from sd_diskio.c */
extern const Diskio_drvTypeDef SD_Driver;

/* ------------------------------------------------------------------ */
/*  Helpers                                                           */
/* ------------------------------------------------------------------ */

static SD_FILE_t* sd_fs_alloc_file(void)
{
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!s_file_pool[i].in_use) {
            s_file_pool[i].in_use = true;
            s_file_pool[i].writable = false;
            return &s_file_pool[i];
        }
    }
    return NULL;
}

static void sd_fs_free_file(SD_FILE_t *sf)
{
    if (sf && sf->in_use) {
        if (s_current_file == sf) s_current_file = NULL;
        sf->in_use = false;
        sf->writable = false;
    }
}

static BYTE mode_to_flags(SD_FS_Mode_t mode)
{
    switch (mode) {
    case SD_FS_MODE_READ:
        return FA_READ;
    case SD_FS_MODE_WRITE:
        return FA_READ | FA_WRITE;
    case SD_FS_MODE_CREATE:
        return FA_READ | FA_WRITE | FA_CREATE_ALWAYS;
    case SD_FS_MODE_APPEND:
        return FA_READ | FA_WRITE | FA_OPEN_APPEND;
    default:
        return FA_READ;
    }
}

/* ------------------------------------------------------------------ */
/*  Init / Status                                                      */
/* ------------------------------------------------------------------ */

SD_FS_Status_t SD_FS_Init(void)
{
    /* Register SD disk driver */
    char path[4] = "0:/";
    if (FATFS_LinkDriver(&SD_Driver, path) != 0) {
        return SD_FS_ERROR;
    }
    
    /* Mount the volume */
    FRESULT res = f_mount(&s_fs_object, path, 1);
    return (res == FR_OK) ? SD_FS_OK : SD_FS_ERROR;
}

bool SD_FS_IsReady(void)
{
    /* Simple check - just return true if SD card is present */
    return SD_Card_IsPresent();
}

bool SD_FS_HasFreeSpace(uint32_t min_free_kb)
{
    /* For now, assume there's always enough space.
     * The actual write operations will fail if disk is full. */
    (void)min_free_kb;
    return SD_FS_IsReady();
}

/* ------------------------------------------------------------------ */
/*  File operations                                                    */
/* ------------------------------------------------------------------ */

FILE* SD_FS_Open(const char *path, SD_FS_Mode_t mode)
{
    SD_FILE_t *sf = sd_fs_alloc_file();
    if (!sf) return NULL;
    
    FRESULT res = f_open(&sf->fil, path, mode_to_flags(mode));
    if (res != FR_OK) {
        sf->in_use = false;
        return NULL;
    }
    
    sf->writable = (mode == SD_FS_MODE_CREATE || mode == SD_FS_MODE_WRITE || 
                    mode == SD_FS_MODE_APPEND);
    
    s_current_file = sf;
    
    if (mode == SD_FS_MODE_APPEND) {
        f_lseek(&sf->fil, f_size(&sf->fil));
    }
    
    return (FILE*)sf;
}

SD_FS_Status_t SD_FS_Close(FILE *f)
{
    SD_FILE_t *sf = (SD_FILE_t*)f;
    if (!sf || !sf->in_use) return SD_FS_ERROR;
    
    FRESULT res = f_close(&sf->fil);
    sd_fs_free_file(sf);
    
    return (res == FR_OK) ? SD_FS_OK : SD_FS_ERROR;
}

int SD_FS_Print(const char *fmt, ...)
{
    if (!s_current_file || !s_current_file->in_use) return -1;
    
    char buf[512];
    
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    
    if (len < 0 || len == 0) return len;
    
    UINT bw;
    f_write(&s_current_file->fil, (const BYTE*)buf, (UINT)len, &bw);
    
    return (int)bw;
}

int SD_FS_Write(const void *data, uint32_t size)
{
    if (!s_current_file || !s_current_file->in_use) return -1;
    
    UINT bw;
    f_write(&s_current_file->fil, (const BYTE*)data, (UINT)size, &bw);
    return (int)bw;
}

int SD_FS_Scanf(const char *fmt, ...)
{
    if (!s_current_file || !s_current_file->in_use) return 0;
    
    char buf[512];
    UINT br;
    FRESULT res = f_read(&s_current_file->fil, buf, sizeof(buf) - 1, &br);
    if (res != FR_OK || br == 0) return 0;
    
    buf[br] = '\0';
    
    va_list args;
    va_start(args, fmt);
    int count = vsscanf(buf, fmt, args);
    va_end(args);
    
    return count;
}

int SD_FS_Read(void *buf, uint32_t size)
{
    if (!s_current_file || !s_current_file->in_use) return -1;
    
    UINT br;
    f_read(&s_current_file->fil, buf, (UINT)size, &br);
    return (int)br;
}

SD_FS_Status_t SD_FS_Delete(const char *path)
{
    FRESULT res = f_unlink(path);
    return (res == FR_OK) ? SD_FS_OK : SD_FS_ERROR;
}

SD_FS_Status_t SD_FS_Truncate(const char *path)
{
    FIL fil;
    FRESULT res = f_open(&fil, path, FA_WRITE);
    if (res != FR_OK) return SD_FS_ERROR;
    
    res = f_lseek(&fil, 0);
    if (res == FR_OK) {
        res = f_truncate(&fil);
    }
    
    f_close(&fil);
    return (res == FR_OK) ? SD_FS_OK : SD_FS_ERROR;
}

/* ------------------------------------------------------------------ */
/*  Directory operations                                              */
/* ------------------------------------------------------------------ */

bool SD_FS_OpenDir(const char *path, const char *pattern)
{
    FRESULT res = f_opendir(&s_dir, path);
    if (res != FR_OK) return false;
    
    s_dir_open = true;
    
    if (pattern) {
        strncpy(s_dir_pattern, pattern, sizeof(s_dir_pattern) - 1);
        s_dir_pattern[sizeof(s_dir_pattern) - 1] = '\0';
    } else {
        s_dir_pattern[0] = '\0';
    }
    
    return true;
}

bool SD_FS_ReadDir(SD_FS_DirEntry_t *entry)
{
    if (!s_dir_open || !entry) return false;
    
    FILINFO fno;
    FRESULT res;
    
    while (1) {
        res = f_readdir(&s_dir, &fno);
        if (res != FR_OK || fno.fname[0] == '\0') break;
        
        if (fno.fname[0] == '.') continue;
        
        if (s_dir_pattern[0] != '\0') {
            const char *ext = strrchr(fno.fname, '.');
            if (ext) {
                const char *pat_ext = strrchr(s_dir_pattern, '.');
                if (pat_ext && strcmp(ext, pat_ext) != 0) continue;
            } else if (s_dir_pattern[0] != '*') {
                if (strcmp(fno.fname, s_dir_pattern) != 0) continue;
            }
        }
        
        strncpy(entry->name, fno.fname, sizeof(entry->name) - 1);
        entry->name[sizeof(entry->name) - 1] = '\0';
        entry->is_directory = (fno.fattrib & AM_DIR) ? true : false;
        entry->size = fno.fsize;
        
        return true;
    }
    
    return false;
}

void SD_FS_CloseDir(void)
{
    if (s_dir_open) {
        f_closedir(&s_dir);
        s_dir_open = false;
    }
}
