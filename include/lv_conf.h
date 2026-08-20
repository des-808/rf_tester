/**
 * @file lv_conf.h
 * @brief Minimal LVGL 9.5 config - only essential widgets
 */

#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 480
#define LV_DISP_DEF_REFR_PERIOD 16

/* ========== ENABLE ONLY NEEDED WIDGETS ========== */
#define LV_USE_ARC 0
#define LV_USE_BAR 0
#define LV_USE_BTN 0
#define LV_USE_BTNMATRIX 0
#define LV_USE_CALENDAR 0
#define LV_USE_CANVAS 0
#define LV_USE_CHART 0
#define LV_USE_CHECK 0
#define LV_USE_DROPDOWN 0
#define LV_USE_IMG 0
#define LV_USE_LABEL 1
#define LV_USE_LINE 0
#define LV_USE_LIST 0
#define LV_USE_MENU 0
#define LV_USE_METER 0
#define LV_USE_MSGBOX 0
#define LV_USE_ROLLER 0
#define LV_USE_SCALE 0
#define LV_USE_SLIDER 0
#define LV_USE_SPAN 0
#define LV_USE_SPINNER 0
#define LV_USE_SWITCH 0
#define LV_USE_TABLE 0
#define LV_USE_TABVIEW 0
#define LV_USE_TILEVIEW 0
#define LV_USE_WIN 0

/* ========== EXTENSIONS ========== */
#define LV_USE_ANIMATION 1
#define LV_USE_EVENT 1
#define LV_USE_FLEX 1
#define LV_USE_GRID 0
#define LV_USE_GROUP 1

/* ========== LIBRARIES - ALL DISABLED ========== */
#define LV_USE_BMP 0
#define LV_USE_FFMPEG 0
#define LV_USE_FREETYPE 0
#define LV_USE_GIF 0
#define LV_USE_GLTf 0
#define LV_USE_LIBJPEG_TURBO 0
#define LV_USE_LIBPNG 0
#define LV_USE_LIBWEBP 0
#define LV_USE_QRCODE 0
#define LV_USE_RLOTTIE 0
#define LV_USE_SVG 0
#define LV_USE_TINY_TTF 0
#define LV_USE_TJPGD 0

/* ========== LVGL DISABLED - using custom GUI ========== */
/* Disable LVGL entirely to save FLASH space */
#define LV_BUILD_EXAMPLES 0
#define LV_BUILD_TEST 0

/* Force no draw backend */
#define LV_USE_DRAW_VG_LITE 0
#define LV_USE_DRAW_NEMA_GFX 0
#define LV_USE_DRAW_SW 1
#define LV_USE_DRAW_SW_COMPLEX 0

/* Use minimal style */
#define LV_USE_STDLIB_MALLOC 0
#define LV_USE_STDLIB_STRING 0
#define LV_USE_STDLIB_SPRINTF 0

/* ========== ASSERT ========== */
#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_MALLOC 0
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_ASSERT_MEM_INTEGRITY 0

/* ========== DEBUG - ALL OFF ========== */
#define LV_USE_LOG 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0
#define LV_USE_MONITOR 0
#define LV_USE_TEST 0
#define LV_USE_FS 0
#define LV_USE_FILE_EXPLORER 0
#define LV_USE_FRAGMENT 0
#define LV_USE_SNAPSHOT 0
#define LV_USE_VECTOR_GRAPHIC 0
#define LV_USE_LOTTIE 0
#define LV_USE_SYSMON 0
#define LV_USE_MONKEY 0
#define LV_USE_BIDI 0

/* ========== THEMES ========== */
#define LV_USE_THEME_DEFAULT 0
#define LV_USE_THEME_SIMPLE 0
#define LV_USE_THEME_MONO 0

/* ========== FONT - ONLY UNSCII ========== */
#define LV_FONT_MONTSERRAT_10 0
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
#define LV_FONT_MONTSERRAT_18 0
#define LV_FONT_MONTSERRAT_20 0
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
#define LV_FONT_SOURCE_HAN_SANS_SC_14_CJK 0
#define LV_FONT_SOURCE_HAN_SANS_SC_16_CJK 0
#define LV_FONT_UNSCII_8 0
#define LV_FONT_UNSCII_16 0

/* Minimal fallback font */
#define LV_FONT_DEFAULT &lv_font_default

/* ========== OTHER ========== */
#define LV_USE_OBJ_PROPERTY 0
#define LV_USE_OBJ_ID 0
#define LV_USE_LEGACY 0
#define LV_USE_MSGBOX 0
#define LV_USE_MSGBOX_AUTO_CLOSE 0

#endif /*LV_CONF_H*/
