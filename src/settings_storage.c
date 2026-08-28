/**
 * ========================================================================
 *  Settings Storage — W25Q SPI Flash backend (implementation)
 *  STM32H7 + W25Q (SPI)
 * ========================================================================
 */

#include "settings_storage.h"
#include "w25qxx.h"
#include <string.h>

/* ----------------------------------------------------------------
 *  Внутренний буфер сектора (4КБ) — для read-modify-write
 * ---------------------------------------------------------------- */
static uint8_t s_sector_buf[SETTINGS_FLASH_SIZE];
static bool    s_sector_dirty = false;
static bool    s_init_done    = false;

/* ========================================================================
 *  Вспомогательные функции
 * ======================================================================== */

/**
 *  @brief  Загрузить весь сектор в RAM-буфер.
 */
static bool load_sector(void) {
    /* Проверяем, не пусто ли всё (0xFF) */
    uint8_t check_buf[64];
    bool all_ff = true;
    
    for (uint32_t addr = 0; addr < SETTINGS_FLASH_SIZE; addr += sizeof(check_buf)) {
        uint32_t chunk = (SETTINGS_FLASH_SIZE - addr < sizeof(check_buf)) 
                         ? (SETTINGS_FLASH_SIZE - addr) : sizeof(check_buf);
        
        if (W25Qx_Read(check_buf, SETTINGS_FLASH_ADDR + addr, chunk) != W25Qx_OK) {
            return false;
        }
        
        for (uint32_t i = 0; i < chunk; i++) {
            if (check_buf[i] != 0xFF) {
                all_ff = false;
                break;
            }
        }
        if (!all_ff) break;
    }
    
    /* Загружаем весь сектор в буфер */
    if (W25Qx_Read(s_sector_buf, SETTINGS_FLASH_ADDR, SETTINGS_FLASH_SIZE) != W25Qx_OK) {
        return false;
    }
    
    /* Если сектор полностью пустой (0xFF) — инициализируем буфер */
    if (all_ff) {
        memset(s_sector_buf, 0xFF, SETTINGS_FLASH_SIZE);
    }
    
    return true;
}

/**
 *  @brief  Сохранить RAM-буфер обратно в Flash (стирает и записывает сектор).
 */
static bool flush_sector(void) {
    if (!s_sector_dirty) return true;
    
    /* Стираем сектор (4КБ) */
    if (W25Qx_Erase_Block(SETTINGS_FLASH_ADDR) != W25Qx_OK) {
        return false;
    }
    
    /* Записываем весь сектор (W25Qx_Write обрабатывает page boundaries) */
    if (W25Qx_Write(s_sector_buf, SETTINGS_FLASH_ADDR, SETTINGS_FLASH_SIZE) != W25Qx_OK) {
        return false;
    }
    
    s_sector_dirty = false;
    return true;
}

/* ========================================================================
 *  Публичные API
 * ======================================================================== */

bool SettingsStorage_Init(void) {
    if (s_init_done) return true;
    
    /* Инициализируем W25Q если нужно */
    if (W25Qx_Init() != W25Qx_OK) {
        return false;
    }
    
    /* Загружаем сектор в RAM */
    if (!load_sector()) {
        return false;
    }
    
    s_init_done   = true;
    s_sector_dirty = false;
    return true;
}

bool SettingsStorage_Read(void* buf, uint32_t len) {
    if (!s_init_done) return false;
    if (!buf || len == 0) return false;
    if (len > SETTINGS_FLASH_SIZE) return false;
    
    memcpy(buf, s_sector_buf, len);
    return true;
}

bool SettingsStorage_Write(const void* buf, uint32_t len) {
    if (!s_init_done) return false;
    if (!buf || len == 0) return false;
    if (len > SETTINGS_FLASH_SIZE) return false;
    
    /* Копируем данные в RAM-буфер */
    memcpy(s_sector_buf, buf, len);
    s_sector_dirty = true;
    
    /* Мгновенно сохраняем в Flash */
    return flush_sector();
}

bool SettingsStorage_Erase(void) {
    if (!s_init_done) return false;
    
    memset(s_sector_buf, 0xFF, SETTINGS_FLASH_SIZE);
    return flush_sector();
}

uint8_t SettingsStorage_ReadByte(uint32_t offset) {
    if (!s_init_done || offset >= SETTINGS_FLASH_SIZE) return 0xFF;
    return s_sector_buf[offset];
}

bool SettingsStorage_WriteByte(uint32_t offset, uint8_t data) {
    if (!s_init_done || offset >= SETTINGS_FLASH_SIZE) return false;
    
    s_sector_buf[offset] = data;
    s_sector_dirty = true;
    return flush_sector();
}
