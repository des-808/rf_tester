/**
 * @file lvgl_module.h
 * @brief LVGL module initialization and main loop
 */

#ifndef LVGL_MODULE_H
#define LVGL_MODULE_H

#include <stdint.h>
#include <stdbool.h>

/* Initialize LVGL module (display + touch + LVGL core) */
void lvgl_module_init(void);

/* Run LVGL tick (call periodically, ~5ms recommended) */
void lvgl_module_tick(void);

/* Run LVGL task handler (call in main loop) */
void lvgl_module_run(void);

#endif /* LVGL_MODULE_H */
