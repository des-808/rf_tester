/**
 * @file lvgl_touch.c
 * @brief LVGL input device driver for FT6336U touchscreen (LVGL 9.x API)
 */

#include "lvgl_touch.h"
#include "lvgl_display.h"
#include "ft6336u.h"
#include <string.h>

/* External I2C handle */
extern I2C_HandleTypeDef hi2c1;

/* FT6336U handle - defined in main.c */
extern FT6336U_HandleTypeDef ft6336u;

/* Last known touch coordinates */
static int32_t last_x = 0;
static int32_t last_y = 0;

/* Convert raw touch coordinates to display coordinates with rotation */
static void touch_convert_coordinates(uint16_t raw_x, uint16_t raw_y, int32_t *x, int32_t *y) {
    uint8_t rotation = lvgl_display_get_rotation();
    uint32_t disp_width = LVGL_DISPLAY_WIDTH;
    uint32_t disp_height = LVGL_DISPLAY_HEIGHT;
    
    switch (rotation) {
        case 0: /* Portrait */
            *x = (int32_t)raw_x;
            *y = (int32_t)raw_y;
            break;
            
        case 1: /* Landscape (90 CW) */
            *x = (int32_t)disp_height - 1 - (int32_t)raw_y;
            *y = (int32_t)raw_x;
            break;
            
        case 2: /* Inverted portrait (180) */
            *x = (int32_t)disp_width - 1 - (int32_t)raw_x;
            *y = (int32_t)disp_height - 1 - (int32_t)raw_y;
            break;
            
        case 3: /* Landscape (270 CW) */
            *x = (int32_t)raw_y;
            *y = (int32_t)disp_width - 1 - (int32_t)raw_x;
            break;
            
        default:
            *x = (int32_t)raw_x;
            *y = (int32_t)raw_y;
            break;
    }
}

/* Touch read callback for LVGL 9.x */
static void touch_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    /* Check if touch is active */
    if (ft6336u.has_touch) {
        uint16_t raw_x, raw_y;
        FT6336U_GetTouchPoint(&ft6336u, 0, &raw_x, &raw_y);
        
        /* Convert coordinates */
        touch_convert_coordinates(raw_x, raw_y, &last_x, &last_y);
        
        /* Store touch state */
        data->point.x = last_x;
        data->point.y = last_y;
        data->state = LV_INDEV_STATE_PRESSED;
        
        /* Clear touch flag */
        ft6336u.has_touch = false;
    } else {
        /* No touch */
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

void lvgl_touch_init(void) {
    /* Create touch input device with LVGL 9.x API */
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
}

void lvgl_touch_get_last(int32_t *x, int32_t *y) {
    if (x) *x = last_x;
    if (y) *y = last_y;
}
