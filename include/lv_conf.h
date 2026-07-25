#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_HOR_RES_MAX 320
#define LV_VER_RES_MAX 480
#define LV_COLOR_DEPTH 16
#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL 0
#define LV_USE_ASSERT_STYLE 0
#define LV_USE_PERF_MONITOR 0
#define LV_USE_FS 0
#define LV_USE_OBJ 1
#define LV_USE_THEME_DEFAULT 0

#define LV_USE_MENU       0

#define LV_USE_ST7796 1


// ===== Основные настройки =====
#define LV_USE_FONT_SUBPX         0
#define LV_USE_PERF_MONITOR       0
#define LV_USE_MEM_MONITOR        0

// ===== Шрифты (каждый ~10–40 КБ) =====
#define LV_USE_DEFAULT_FONT       LV_FONT_MONTSERRAT_14   // только 1 шрифт
#undef LV_USE_FONT_MONTSERRAT_16
#undef LV_USE_FONT_MONTSERRAT_18
#undef LV_USE_FONT_MONTSERRAT_20
#undef LV_USE_FONT_MONTSERRAT_22
#undef LV_USE_FONT_MONTSERRAT_24
#undef LV_USE_FONT_MONTSERRAT_26
#undef LV_USE_FONT_MONTSERRAT_28
#undef LV_USE_FONT_MONTSERRAT_32
#undef LV_USE_FONT_MONTSERRAT_34
#undef LV_USE_FONT_MONTSERRAT_36
#undef LV_USE_FONT_MONTSERRAT_38
#undef LV_USE_FONT_MONTSERRAT_40
#undef LV_USE_FONT_MONTSERRAT_42
#undef LV_USE_FONT_MONTSERRAT_44
#undef LV_USE_FONT_MONTSERRAT_46
#undef LV_USE_FONT_MONTSERRAT_48

// ===== Отключите ненужные виджеты =====
// ===== Отключите ненужные виджеты =====
#define LV_USE_ARC                0
#define LV_USE_BAR                1
#define LV_USE_BTN                1
#define LV_USE_BTNMATRIX          0
#define LV_USE_CANVAS             0
#define LV_USE_CHECKBOX           1
#define LV_USE_DROPDOWN           0
#define LV_USE_CALENDAR           0
#define LV_USE_CALENDAR_HEADER_DROPDOWN 0
#define LV_USE_IMG                1
#define LV_USE_LABEL              1
#define LV_USE_LINE               1        // ← ВАЖНО: добавить!
#define LV_USE_LIST               0        // ← ВАЖНО: отключить
#define LV_USE_SCALE              0        // ← ВАЖНО: отключить
#define LV_USE_ROLLER             0
#define LV_USE_SLIDER             0
#define LV_USE_SPINBOX            0
#define LV_USE_SPINNER            0
#define LV_USE_SWITCH             0
#define LV_USE_TABLE              0
#define LV_USE_TABVIEW            0

// ===== Библиотеки (выключаем всё) =====
#define LV_USE_BMP                0
#define LV_USE_GIF                0
#define LV_USE_QRCODE             0
#define LV_USE_LIBPNG             0
#define LV_USE_LIBJPEG_TURBO      0
#define LV_USE_FREETYPE           0
#define LV_USE_RLOTTIE            0
#define LV_USE_TINY_TTF           0
#define LV_USE_SVG                0
#define LV_USE_FS                 0
#define LV_USE_FS_FATFS           0
#define LV_USE_FS_STDIO           0

// ===== GPU (включите только если точно знаете, что нужно) =====
#define LV_USE_GPU_STM32_DMA2D    1   // Ускорение для STM32H7
#define LV_USE_DRAW_SW            1   // обязательное для ARM Cortex-M7

// ===== Буферы =====
#define LV_DISP_DEF_DRAW_BUF_SIZE  (480 * 10)   // 10 строк — достаточно
#define LV_DISP_DEF_REFR_TIME      16

// ===== Кэш декодера изображений (если используете спрайты) =====
#define LV_IMG_CACHE_DEF_SIZE      2   // минимум — кэш на 2 спрайта

// ===== И др. =====
#define LV_USE_PRIVATE_API         0
#define LV_USE_MSG_BOX             0

#endif
