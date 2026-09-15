/**
 * @file lv_conf.h
 * Configuration file for LVGL 9.5 on STM32H7 (Cortex-M7, no MVE Helium)
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*================
    Minimal config for STM32H750 (512KB Flash, 512KB RAM)
 *================*/

/*================
    Graphics settings
 *================*/
#define LV_USE_LOG 0
#if LV_USE_LOG
#  define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#  define LV_LOG_PRINTF 1
#endif

#define LV_USE_BUILTIN_MALLOC 0
#define LV_USE_DEFAULT_MALLOC_STYLE LV_MALLOC_STYLE_STDLIB

#define LV_USE_FLOAT 0
#define LV_USE_FREETYPE 0
#define LV_USE_FONT_SUBPX 0

/* Color depth: 16-bit RGB565 for ST7796 */
#define LV_COLOR_DEPTH 16

/* Screen dimensions */
#define LV_DISP_SIZE_LARGE 1
#define LV_USE_DISPLAY 1

/* Display rotation support */
#define LV_DISPLAY_ROTATION 1

/*=================
   Memory settings
 *=================*/
#define LV_MEM_SIZE (48 * 1024U)  /* 48 KB for LVGL memory pool */

/*================
   Widget settings (minimal set)
 *================*/
#define LV_USE_BTN 1
#define LV_USE_LABEL 1
#define LV_USE_BUTTON 1
#define LV_USE_SLIDER 1
#define LV_USE_SWITCH 1
#define LV_USE_CHECKBOX 1
#define LV_USE_BAR 1
#define LV_USE_LINE 1
#define LV_USE_ARC 1
#define LV_USE_IMG 0
#define LV_USE_TEXTAREA 0
#define LV_USE_LIST 0
#define LV_USE_TABLE 0
#define LV_USE_DROPDOWN 1
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0
#define LV_USE_SPINNER 0
#define LV_USE_CALENDAR 0
#define LV_USE_CHART 0
#define LV_USE_LED 0
#define LV_USE_METER 0
#define LV_USE_SPINBOX 0

/*================
   Themes
 *================*/
#define LV_USE_THEME_DEFAULT 1
#if LV_USE_THEME_DEFAULT
#  define LV_THEME_DEFAULT_STATE_PR 1
#  define LV_THEME_DEFAULT_ANIM 0
#  define LV_THEME_DEFAULT_COLOR_SCHEME LV_THEME_COLOR_SCHEME_MIXED
#  define LV_THEME_DEFAULT_PRIMARY lv_color_hex(0x007000)
#  define LV_THEME_DEFAULT_SECONDARY lv_color_hex(0x004400)
#endif

/*==================
   Font settings (minimal set)
 *==================*/
#define LV_FONT_DEFAULT &lv_font_montserrat_14

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_18 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_22 0
#define LV_FONT_MONTSERRAT_24 0
#define LV_FONT_MONTSERRAT_26 0
#define LV_FONT_MONTSERRAT_28 0
#define LV_FONT_MONTSERRAT_30 0
#define LV_FONT_MONTSERRAT_32 0
#define LV_FONT_MONTSERRAT_34 0
#define LV_FONT_MONTSERRAT_36 0
#define LV_FONT_MONTSERRAT_38 0
#define LV_FONT_MONTSERRAT_40 0
#define LV_FONT_MONTSERRAT_42 0
#define LV_FONT_MONTSERRAT_44 0
#define LV_FONT_MONTSERRAT_46 0
#define LV_FONT_MONTSERRAT_48 0

#define LV_FONT_DEJAVU_16_PERSIAN_HEBREW 0
#define LV_FONT_HELVETICA_14 0
#define LV_FONT_UNSCII_8 0
#define LV_FONT_UNSCII_16 0

/* Font loading */
#define LV_FONT_FMTTXT_LARGE 0
#define LV_USE_FONT_COMPRESSED 0
#define LV_USE_EMBEDDED_FONTS 1

/*=======================
   Display driver settings
 *=======================*/
#define LV_DISP_DEF_REFR_PERIOD 30

/* Double buffer for display */
#define LV_DISP_DEF_MAX_ROTATIONS 4
#define LV_DISP_ROTATION 0

/*=================
   Input device settings
 *=================*/
#define LV_INDEV_DEF_READ_PERIOD 30

/* Touchscreen: FT6336U */
#define LV_INDEV_DEF_SCROLL_LIMIT 20
#define LV_INDEV_DEF_SCROLL_THROW 200
#define LV_LONG_PRESS_TIME 400
#define LV_LONG_PRESS_REP_TIME 100

/* Disable unused draw backends to save RAM */
#define LV_USE_DRAW_VG_LITE 0
#define LV_USE_DRAW_NEMA_GFX 0
#define LV_USE_DRAW_PXP 0
#define LV_USE_DRAW_G2D 0
#define LV_USE_DRAW_DAVE2D 0
#define LV_USE_DRAW_SDL 0
#define LV_USE_DRAW_OPENGLES 0
#define LV_USE_DRAW_NANOVG 0
#define LV_USE_DRAW_EVE 0/* Disable Helium/NEON optimizations (Cortex-M7 has no MVE) */
#define LV_DRAW_SW_ASM 0
#define LV_USE_DRAW_STM32_DMA2D 1
#define LV_USE_NATIVE_HELIUM_ASM 0

/*====================
 * FONT SETTINGS
 *====================*/
#define LV_USE_FONT_PLACEHOLDER 1
#define LV_USE_FONT_SUBPX 0

/*====================
 * OTHER SETTINGS
 *====================*/
#define LV_USE_LOG 0
#define LV_USE_BIDI 0
#define LV_USE_LODEPNG 1

#endif /*LV_CONF_H*/
