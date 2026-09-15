/**
 * @file lvgl_touch.h
 * @brief LVGL input device driver for FT6336U touchscreen (LVGL 9.x API)
 */

#ifndef LVGL_TOUCH_H
#define LVGL_TOUCH_H

#include <stdint.h>
#include <stdbool.h>
#include "lvgl.h"

/* Initialize LVGL touch driver */
void lvgl_touch_init(void);

/* Get last touch coordinates */
void lvgl_touch_get_last(int32_t *x, int32_t *y);

#endif /* LVGL_TOUCH_H */
