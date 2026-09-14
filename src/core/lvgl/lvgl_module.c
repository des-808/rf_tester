/**
 * @file lvgl_module.c
 * @brief LVGL module initialization and main loop (LVGL 9.x API)
 */

#include "lvgl_module.h"
#include "lvgl_display.h"
#include "lvgl_touch.h"
#include "lvgl.h"
#include "ft6336u.h"
#include <stdio.h>

void lvgl_module_init(void) {
    /* Initialize display driver */
    lvgl_display_init();
    
    /* Register touch input device */
    lvgl_touch_init();
    
    /* Initialize tick timer */
    printf("LVGL initialized\r\n");
}

void lvgl_module_tick(void) {
    /* Update LVGL tick every 5ms */
    lv_tick_inc(5);
}

void lvgl_module_run(void) {
    /* Run LVGL task handler */
    lv_timer_handler();
}
