/**
 * @file lvgl_display.h
 * @brief LVGL display driver for ST7796 + STM32H750VBT6 (LVGL 9.x API)
 */

#ifndef LVGL_DISPLAY_H
#define LVGL_DISPLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

/* Display dimensions */
#define LVGL_DISPLAY_WIDTH   320
#define LVGL_DISPLAY_HEIGHT  480

/* Framebuffer: double buffered, 32-bit aligned for D-Cache */
#define LVGL_FB_SIZE   (LVGL_DISPLAY_WIDTH * LVGL_DISPLAY_HEIGHT)

/* Initialize LVGL display driver */
void lvgl_display_init(void);

/* Flush callback - call from disp_drv.flush_cb */
void lvgl_display_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

/* Set display rotation (0-3) */
void lvgl_display_set_rotation(uint8_t rotation);

/* Get current rotation */
uint8_t lvgl_display_get_rotation(void);

#endif /* LVGL_DISPLAY_H */
