/**
 * @file lvgl_display.c
 * @brief LVGL display driver for ST7796 with DMA2D acceleration (LVGL 9.x API)
 */

#include "lvgl_display.h"
#include "st7796.h"
#include "lv_conf.h"
#include <string.h>

/* External SPI handle */
extern SPI_HandleTypeDef hspi4;

/* Framebuffers - double buffered, aligned for D-Cache */
static uint16_t lvgl_fb1[LVGL_FB_SIZE] __attribute__((aligned(32)));
static uint16_t lvgl_fb2[LVGL_FB_SIZE] __attribute__((aligned(32)));

/* Current rotation */
static uint8_t current_rotation = 0;

void lvgl_display_init(void) {
    /* Initialize ST7796 display */
    ST7796_Init();
    
    /* Clear screen */
    ST7796_FillScreen(0x0000);
    
    /* Initialize LVGL core */
    lv_init();
    
    /* Create display with double buffering */
    lv_display_t * disp = lv_display_create(LVGL_DISPLAY_WIDTH, LVGL_DISPLAY_HEIGHT);
    
    /* Set double buffers with partial render mode */
    lv_display_set_buffers(disp, lvgl_fb1, lvgl_fb2, sizeof(lvgl_fb1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    /* Set flush callback */
    lv_display_set_flush_cb(disp, lvgl_display_flush_cb);
    
    /* Set initial rotation */
    lvgl_display_set_rotation(0);
}

void lvgl_display_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    /* Calculate area dimensions */
    const int32_t x1 = area->x1;
    const int32_t y1 = area->y1;
    const int32_t x2 = area->x2;
    const int32_t y2 = area->y2;
    
    const uint32_t width = x2 - x1 + 1;
    const uint32_t height = y2 - y1 + 1;
    const uint32_t total_bytes = width * height * 2; /* RGB565 = 2 bytes per pixel */
    
    /* Set address window on ST7796 */
    ST7796_SetAddressWindow((uint16_t)x1, (uint16_t)y1, (uint16_t)x2, (uint16_t)y2);
    
    /* Enter data mode */
    LCD_CS_LOW;
    LCD_DC_DATA;
    
    /* Clean D-Cache for the data being sent - CRITICAL for STM32H7 */
    SCB_CleanDCache_by_Addr((uint32_t *)px_map, (total_bytes + 31) & ~31);
    __DSB();
    
    /* Send data via SPI DMA in chunks (max ~60KB per chunk) */
    uint32_t sent_bytes = 0;
    while (sent_bytes < total_bytes) {
        uint32_t chunk_bytes = (total_bytes - sent_bytes > 60000) ? 60000 : (total_bytes - sent_bytes);
        
        /* Clean D-Cache for this chunk */
        uint32_t chunk_size = (chunk_bytes + 31) & ~31;
        SCB_CleanDCache_by_Addr((uint32_t *)(px_map + sent_bytes), chunk_size);
        __DSB();
        
        /* Transmit via SPI DMA */
        HAL_StatusTypeDef status = HAL_SPI_Transmit_DMA(&hspi4, px_map + sent_bytes, chunk_bytes);
        if (status != HAL_OK) {
            LCD_CS_HIGH;
            lv_display_flush_ready(disp);
            return;
        }
        
        /* Wait for DMA transfer to complete */
        while (HAL_SPI_GetState(&hspi4) != HAL_SPI_STATE_READY) {
            /* Spin wait */
        }
        
        sent_bytes += chunk_bytes;
    }
    
    /* Exit data mode */
    LCD_CS_HIGH;
    
    /* Mark flush as ready - tell LVGL we're done */
    lv_display_flush_ready(disp);
}

void lvgl_display_set_rotation(uint8_t rotation) {
    current_rotation = rotation % 4;
    
    /* Update ST7796 rotation */
    ST7796_SetRotation(current_rotation);
    
    /* Update LVGL display rotation */
    lv_display_t * disp = lv_display_get_default();
    if (disp) {
        lv_display_set_rotation(disp, (lv_display_rotation_t)current_rotation);
    }
    
    /* Clear screen after rotation */
    ST7796_FillScreen(0x0000);
}

uint8_t lvgl_display_get_rotation(void) {
    return current_rotation;
}
