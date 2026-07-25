#ifndef LVGL_UI_H
#define LVGL_UI_H

#include <stdint.h>

void LVGL_InitScreen(void);
void LVGL_SetSWR(float swr);
void LVGL_SetStatus(const char* text);
void LVGL_SetTouch(uint16_t x, uint16_t y);
void LVGL_SetButton(const char* text);
void LVGL_Tick(void);

#endif
