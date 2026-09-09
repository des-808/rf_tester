/**
 * ========================================================================
 *  RF Spectrum Analyzer — Implementation
 *  Спектроанализатор на базе CC1101 + ST7796 дисплей
 * ========================================================================
 */

#include "rf_spectrum.h"
#include "radio_cc1101.h"
#include "st7796.h"
#include "gui.h"
#include "lcd_backlight.h"

#include <string.h>
#include <stdio.h>

/* ========================================================================
 *  Вспомогательные функции
 * ======================================================================== */

/**
 * @brief Вычислить частоту для точки спектра
 */
static uint32_t spectrum_freq_for_point(RfSpectrum_t* spectrum, uint16_t point) {
    if (spectrum->points <= 1) return spectrum->freq_start;
    
    uint32_t range = spectrum->freq_end - spectrum->freq_start;
    uint32_t step = range / (spectrum->points - 1);
    return spectrum->freq_start + (step * point);
}

/**
 * @brief Измерить RSSI на заданной частоте
 */
static int8_t measure_rssi_at_freq(uint32_t freq_mhz_x100) {
    /* Переключаем на нужную частоту */
    CC1101_SetFrequency(freq_mhz_x100 * 1000);  /* Конвертируем МГц в Гц */
    
    /* Ждём стабилизации */
    HAL_Delay(5);
    
    /* Читаем RSSI */
    int8_t rssi = (int8_t)CC1101_GetRssi();
    
    return rssi;
}

/* ========================================================================
 *  Публичные API
 * ======================================================================== */

bool RfSpectrum_Init(RfSpectrum_t* spectrum, uint32_t freq_start, uint32_t freq_end, uint16_t points) {
    if (!spectrum) return false;
    if (freq_start >= freq_end) return false;
    if (points == 0 || points > SPECTRUM_POINTS) return false;
    
    memset(spectrum, 0, sizeof(RfSpectrum_t));
    
    spectrum->freq_start = freq_start;
    spectrum->freq_end = freq_end;
    spectrum->points = points;
    spectrum->active = false;
    spectrum->peak_freq = 0;
    spectrum->peak_rssi = SPECTRUM_RSSI_MIN;
    spectrum->scan_duration_ms = 0;
    
    /* Инициализируем CC1101 для сканирования */
    /* RadioCC1101_Init(); */
    
    printf("RfSpectrum_Init: %lu.%02lu - %lu.%02lu MHz, %d points\n",
           (unsigned long)freq_start / 100, (unsigned long)freq_start % 100,
           (unsigned long)freq_end / 100, (unsigned long)freq_end % 100,
           points);
    
    return true;
}

bool RfSpectrum_Scan(RfSpectrum_t* spectrum) {
    if (!spectrum || spectrum->points == 0) return false;
    
    spectrum->active = true;
    uint32_t start_tick = 0; /* HAL_GetTick() */
    
    /* Сканируем каждую точку */
    for (uint16_t i = 0; i < spectrum->points; i++) {
        uint32_t freq = spectrum_freq_for_point(spectrum, i);
        
        /* Измеряем RSSI */
        spectrum->spectrum[i] = (int8_t)measure_rssi_at_freq(freq);
        
        /* Обновляем пик */
        if (spectrum->spectrum[i] > spectrum->peak_rssi) {
            spectrum->peak_rssi = spectrum->spectrum[i];
            spectrum->peak_freq = freq;
        }
    }
    
    uint32_t end_tick = 0; /* HAL_GetTick() */
    spectrum->scan_duration_ms = end_tick - start_tick;
    spectrum->active = false;
    
    return true;
}

void RfSpectrum_FindPeak(RfSpectrum_t* spectrum) {
    if (!spectrum || spectrum->points == 0) return;
    
    spectrum->peak_rssi = SPECTRUM_RSSI_MIN;
    spectrum->peak_freq = spectrum->freq_start;
    
    for (uint16_t i = 0; i < spectrum->points; i++) {
        if (spectrum->spectrum[i] > spectrum->peak_rssi) {
            spectrum->peak_rssi = spectrum->spectrum[i];
            spectrum->peak_freq = spectrum_freq_for_point(spectrum, i);
        }
    }
}

void RfSpectrum_Render(RfSpectrum_t* spectrum, int16_t x, int16_t y, uint16_t w, uint16_t h) {
    if (!spectrum || w == 0 || h == 0) return;
    
    /* 1. Очищаем фон (чёрный) */
    /* ST7796_FillRect(x, y, w, h, RGB565_BLACK); */
    
    /* 2. Рисуем сетку */
    /* Горизонтальные линии каждые 10 dB */
    uint16_t grid_color = RGB565_DARK_GRAY;
    for (int8_t rssi = SPECTRUM_RSSI_MIN; rssi <= SPECTRUM_RSSI_MAX; rssi += 10) {
        /* Вычисляем Y для этого уровня RSSI */
        float normalized = (float)(rssi - SPECTRUM_RSSI_MIN) / (float)(SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
        int16_t line_y = y + (int16_t)((1.0f - normalized) * h);
        
        /* ST7796_HLine(x, line_y, w, grid_color); */
    }
    
    /* 3. Рисуем спектральный график */
    uint16_t graph_color = RGB565_GREEN;
    uint16_t peak_color = RGB565_RED;
    
    for (uint16_t i = 0; i < spectrum->points - 1; i++) {
        /* Вычисляем X для текущей и следующей точки */
        uint16_t x1 = x + (uint16_t)((uint32_t)i * w / spectrum->points);
        uint16_t x2 = x + (uint16_t)((uint32_t)(i + 1) * w / spectrum->points);
        
        /* Вычисляем Y для текущей и следующей точки */
        float norm1 = (float)(spectrum->spectrum[i] - SPECTRUM_RSSI_MIN) / (float)(SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
        float norm2 = (float)(spectrum->spectrum[i + 1] - SPECTRUM_RSSI_MIN) / (float)(SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
        
        int16_t y1 = y + (int16_t)((1.0f - norm1) * h);
        int16_t y2 = y + (int16_t)((1.0f - norm2) * h);
        
        /* Ограничиваем Y в пределах области */
        if (y1 < y) y1 = y;
        if (y1 >= y + h) y1 = y + h - 1;
        if (y2 < y) y2 = y;
        if (y2 >= y + h) y2 = y + h - 1;
        
        /* Определяем цвет (пик — красный) */
        uint16_t color = graph_color;
        if (spectrum->spectrum[i] == spectrum->peak_rssi && spectrum->peak_freq == spectrum_freq_for_point(spectrum, i)) {
            color = peak_color;
        }
        
        /* Рисуем линию */
        /* ST7796_DrawLine(x1, y1, x2, y2, color); */
    }
    
    /* 4. Рисуем маркер пика */
    if (spectrum->peak_freq > 0) {
        float norm = (float)(spectrum->peak_rssi - SPECTRUM_RSSI_MIN) / (float)(SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
        int16_t peak_x = x + (uint16_t)((float)(spectrum->peak_freq - spectrum->freq_start) / (float)(spectrum->freq_end - spectrum->freq_start) * w);
        int16_t peak_y = y + (int16_t)((1.0f - norm) * h);
        
        /* Рисуем крестик на пике */
        /* ST7796_HLine(peak_x - 3, peak_y, 7, peak_color); */
        /* ST7796_VLine(peak_x, peak_y - 3, 7, peak_color); */
    }
    
    /* 5. Рисуем рамки и подписи */
    /* Верхняя рамка */
    /* ST7796_HLine(x, y, w, RGB565_WHITE); */
    /* Левая рамка */
    /* ST7796_VLine(x, y, h, RGB565_WHITE); */
    /* Правая рамка */
    /* ST7796_VLine(x + w - 1, y, h, RGB565_WHITE); */
    /* Нижняя рамка */
    /* ST7796_HLine(x, y + h - 1, w, RGB565_WHITE); */
    
    /* 6. Подписи частот */
    char freq_buf[20];
    RfSpectrum_FreqToString(spectrum->freq_start, freq_buf, sizeof(freq_buf));
    /* lcd_print_to_buffer(x, y + h + 2, RGB565_WHITE, freq_buf, RGB565_BLACK, NULL); */
    
    RfSpectrum_FreqToString(spectrum->freq_end, freq_buf, sizeof(freq_buf));
    /* lcd_print_to_buffer(x + w - 40, y + h + 2, RGB565_WHITE, freq_buf, RGB565_BLACK, NULL); */
    
    /* 7. Информация о пике */
    char peak_buf[40];
    snprintf(peak_buf, sizeof(peak_buf), "PEAK: %s %ddB", freq_buf, spectrum->peak_rssi);
    /* lcd_print_to_buffer(x, y - 12, RGB565_YELLOW, peak_buf, RGB565_BLACK, NULL); */
}

void RfSpectrum_Stop(RfSpectrum_t* spectrum) {
    if (!spectrum) return;
    spectrum->active = false;
}

bool RfSpectrum_IsActive(RfSpectrum_t* spectrum) {
    return spectrum ? spectrum->active : false;
}

void RfSpectrum_RssiToString(int8_t rssi, char* buf, size_t buf_size) {
    snprintf(buf, buf_size, "%ddB", rssi);
}

void RfSpectrum_FreqToString(uint32_t freq, char* buf, size_t buf_size) {
    uint32_t mhz = freq / 100;
    uint32_t khz = freq % 100;
    snprintf(buf, buf_size, "%lu.%02luM", (unsigned long)mhz, (unsigned long)khz);
}

/* ========================================================================
 *  RSSI Plotter — непрерывный мониторинг через ring buffer
 * ======================================================================== */

void RfRssiPlotter_Init(RfRssiPlotter_t* plotter) {
    if (!plotter) return;
    
    memset(plotter, 0, sizeof(RfRssiPlotter_t));
    plotter->active = false;
    plotter->peak_rssi = SPECTRUM_RSSI_MIN;
    plotter->min_rssi = SPECTRUM_RSSI_MAX;
    plotter->sample_count = 0;
    
    /* Заполняем историю дефолтным значением */
    for (uint16_t i = 0; i < sizeof(plotter->rssi_history); i++) {
        plotter->rssi_history[i] = SPECTRUM_RSSI_MIN;
    }
}

void RfRssiPlotter_Update(RfRssiPlotter_t* plotter, int8_t* buf, uint16_t buf_size) {
    if (!plotter) return;
    
    /* Копируем все доступные выборки из ring buffer */
    uint16_t copied = CC1101_RssiBuf_Copy(buf, buf_size);
    
    if (copied == 0) return;
    
    plotter->active = true;
    plotter->sample_count += copied;
    
    /* Сдвигаем историю: удаляем первые N элементов, добавляем новые в конец */
    if (copied >= plotter->history_len) {
        /* Если новых выборок больше чем вся история — заменяем полностью */
        uint16_t copy_len = (copied < plotter->history_len) ? copied : plotter->history_len;
        memcpy(plotter->rssi_history, buf, copy_len);
        plotter->history_len = plotter->history_len;
        
        /* Остаток если есть */
        if (copied > plotter->history_len) {
            uint16_t remaining = copied - plotter->history_len;
            memcpy(plotter->rssi_history, buf + remaining, plotter->history_len - remaining);
        }
    } else {
        /* Сдвигаем историю влево на copied и добавляем новые в конец */
        memmove(plotter->rssi_history, plotter->rssi_history + copied, 
                plotter->history_len - copied);
        memcpy(plotter->rssi_history + (plotter->history_len - copied), buf, copied);
        /* history_len не меняется */
    }
    
    /* Обновляем peak/min */
    for (uint16_t i = 0; i < copied; i++) {
        if (buf[i] > plotter->peak_rssi) {
            plotter->peak_rssi = buf[i];
        }
        if (buf[i] < plotter->min_rssi) {
            plotter->min_rssi = buf[i];
        }
    }
}

void RfRssiPlotter_Render(RfRssiPlotter_t* plotter, int16_t x, int16_t y, uint16_t w, uint16_t h) {
    if (!plotter || w == 0 || h == 0) return;
    
    uint16_t len = plotter->history_len;
    if (len == 0) return;
    
    /* 1. Рисуем сетку */
    for (int8_t rssi = SPECTRUM_RSSI_MIN; rssi <= SPECTRUM_RSSI_MAX; rssi += 10) {
        float normalized = (float)(rssi - SPECTRUM_RSSI_MIN) / (float)(SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
        int16_t line_y = y + (int16_t)((1.0f - normalized) * h);
        /* ST7796_HLine(x, line_y, w, RGB565_DARK_GRAY); */
    }
    
    /* 2. Рисуем график RSSI */
    uint16_t graph_color = RGB565_GREEN;
    uint16_t peak_color = RGB565_RED;
    uint16_t min_color = RGB565_BLUE;
    
    for (uint16_t i = 0; i < len - 1; i++) {
        uint16_t x1 = x + (uint16_t)((uint32_t)i * w / len);
        uint16_t x2 = x + (uint16_t)((uint32_t)(i + 1) * w / len);
        
        float norm1 = (float)(plotter->rssi_history[i] - SPECTRUM_RSSI_MIN) / (float)(SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
        float norm2 = (float)(plotter->rssi_history[i + 1] - SPECTRUM_RSSI_MIN) / (float)(SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
        
        int16_t y1 = y + (int16_t)((1.0f - norm1) * h);
        int16_t y2 = y + (int16_t)((1.0f - norm2) * h);
        
        if (y1 < y) y1 = y;
        if (y1 >= y + h) y1 = y + h - 1;
        if (y2 < y) y2 = y;
        if (y2 >= y + h) y2 = y + h - 1;
        
        /* Цвет: peak — красный, min — синий, остальные — зелёный */
        uint16_t color = graph_color;
        if (plotter->rssi_history[i] == plotter->peak_rssi && plotter->peak_rssi != SPECTRUM_RSSI_MIN) {
            color = peak_color;
        } else if (plotter->rssi_history[i] == plotter->min_rssi && plotter->min_rssi != SPECTRUM_RSSI_MAX) {
            color = min_color;
        }
        
        /* ST7796_DrawLine(x1, y1, x2, y2, color); */
    }
    
    /* 3. Информация */
    char info_buf[40];
    snprintf(info_buf, sizeof(info_buf), "PEAK: %ddB  MIN: %ddB", 
             plotter->peak_rssi, plotter->min_rssi);
    /* ST7796_PutString(x, y - 12, RGB565_YELLOW, info_buf, RGB565_BLACK); */
}

void RfRssiPlotter_ResetStats(RfRssiPlotter_t* plotter) {
    if (!plotter) return;
    
    plotter->peak_rssi = SPECTRUM_RSSI_MIN;
    plotter->min_rssi = SPECTRUM_RSSI_MAX;
    plotter->sample_count = 0;
}

/* ========================================================================
 *  RSSI Plotter Screen — полноэкранный режим с меню
 * ======================================================================== */

/**
 * @brief Отрисовка RSSI Plotter на graph_sprite
 * Вызывается как render_callback для graph_node
 */
void RssiPlotter_DrawGraph(RssiPlotterScreen_t* screen) {
    /* Внешние переменные */
    extern Sprite_t graph_sprite;
    extern Sprite_t main_screen_sprite;
    extern UIElement_t* current_menu_listbox;
    extern bool ui_debug_draw;
    
    if (!screen || !screen->enabled) return;
    if (!screen || !screen->enabled || !graph_sprite.data) return;
    
    /* Проверяем размеры */
    if (graph_sprite.w == 0 || graph_sprite.h == 0) return;
    
    RfRssiPlotter_t* plotter = &screen->plotter;
    uint16_t w = graph_sprite.w;
    uint16_t h = graph_sprite.h;
    
    /* Очищаем фон */
    uint32_t total_pixels = (uint32_t)w * h;
    memset(graph_sprite.data, 0, total_pixels * 2);
    
    /* Область графика с отступами */
    int16_t margin_left = 40;   /* Место для подписей dBm */
    int16_t margin_right = 10;
    int16_t margin_top = 25;    /* Место для заголовка */
    int16_t margin_bottom = 10;
    
    int16_t graph_x = margin_left;
    int16_t graph_y = margin_top;
    int16_t graph_w = w - margin_left - margin_right;
    int16_t graph_h = h - margin_top - margin_bottom;
    
    /* Рисуем рамку */
    uint16_t border_color = RGB565_WHITE;
    for (int16_t x = graph_x; x < graph_x + graph_w; x++) {
        graph_sprite.data[graph_y * w + x] = border_color;           /* Верх */
        graph_sprite.data[(graph_y + graph_h - 1) * w + x] = border_color; /* Низ */
    }
    for (int16_t y = graph_y; y < graph_y + graph_h; y++) {
        graph_sprite.data[y * w + graph_x] = border_color;           /* Лево */
        graph_sprite.data[y * w + (graph_x + graph_w - 1)] = border_color; /* Право */
    }
    
    /* Рисуем сетку (каждые 10 dB) */
    uint16_t grid_color = RGB565_DARK_GRAY;
    for (int8_t rssi = SPECTRUM_RSSI_MIN; rssi <= SPECTRUM_RSSI_MAX; rssi += 10) {
        float normalized = (float)(rssi - SPECTRUM_RSSI_MIN) / (float)(SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
        int16_t line_y = graph_y + (int16_t)((1.0f - normalized) * graph_h);
        
        /* Горизонтальная линия */
        for (int16_t x = graph_x + 1; x < graph_x + graph_w - 1; x++) {
            graph_sprite.data[line_y * w + x] = grid_color;
        }
        
        /* Подпись слева */
        char rssi_str[8];
        snprintf(rssi_str, sizeof(rssi_str), "%ddB", rssi);
        lcd_print_to_buffer(graph_x - 38, line_y - 4, RGB565_YELLOW, rssi_str, RGB565_BLACK, &graph_sprite);
    }
    
    /* Заголовок */
    lcd_print_to_buffer(graph_x + 5, graph_y + 2, RGB565_WHITE, "RSSI PLOTTER", RGB565_BLACK, &graph_sprite);
    
    /* Рисуем график RSSI */
    uint16_t len = plotter->history_len;
    if (len > 1) {
        uint16_t line_color = RGB565_GREEN;
        uint16_t peak_color = RGB565_RED;
        uint16_t min_color = RGB565_BLUE;
        
        /* Определяем диапазон отображения (последние N точек) */
        uint16_t display_count = (len < graph_w) ? len : graph_w;
        int16_t start_idx = len - display_count;
        
        /* Рисуем линию */
        int16_t prev_x = graph_x;
        int16_t prev_y = graph_y + graph_h / 2;
        
        for (uint16_t i = 0; i < display_count; i++) {
            int16_t x = graph_x + (int16_t)((uint32_t)i * graph_w / display_count);
            int8_t rssi_val = plotter->rssi_history[start_idx + i];
            
            /* Нормализуем RSSI в координаты Y */
            float normalized = (float)(rssi_val - SPECTRUM_RSSI_MIN) / (float)(SPECTRUM_RSSI_MAX - SPECTRUM_RSSI_MIN);
            int16_t y = graph_y + (int16_t)((1.0f - normalized) * graph_h);
            
            /* Ограничиваем Y */
            if (y < graph_y) y = graph_y;
            if (y >= graph_y + graph_h) y = graph_y + graph_h - 1;
            
            /* Определяем цвет */
            uint16_t color = line_color;
            if (rssi_val == plotter->peak_rssi && plotter->peak_rssi != SPECTRUM_RSSI_MIN) {
                color = peak_color;
            } else if (rssi_val == plotter->min_rssi && plotter->min_rssi != SPECTRUM_RSSI_MAX) {
                color = min_color;
            }
            
            /* Рисуем линию */
            Draw_Line_To_Sprite(&graph_sprite, prev_x, prev_y, x, y, color);
            
            prev_x = x;
            prev_y = y;
        }
    }
    
    /* Информация внизу */
    char info_buf[64];
    snprintf(info_buf, sizeof(info_buf), 
             "BEST:%3ddB  CUR:%3ddB  MIN:%3ddB  CNT:%lu",
             plotter->peak_rssi,
             (plotter->history_len > 0) ? plotter->rssi_history[plotter->history_len - 1] : SPECTRUM_RSSI_MIN,
             plotter->min_rssi,
             (unsigned long)plotter->sample_count);
    lcd_print_to_buffer(graph_x + 5, graph_y + graph_h + 2, RGB565_YELLOW, info_buf, RGB565_BLACK, &graph_sprite);
    
    /* Подсказка выхода */
    lcd_print_to_buffer(graph_x + graph_w - 60, graph_y + graph_h + 2, RGB565_GRAY, "[CANCEL]", RGB565_BLACK, &graph_sprite);
    
    /* Инвалидируем graph_sprite для отправки на экран */
    graph_sprite.needs_render = true;
    graph_sprite.dirty_x1 = 0; graph_sprite.dirty_y1 = 0;
    graph_sprite.dirty_x2 = graph_sprite.w - 1; graph_sprite.dirty_y2 = graph_sprite.h - 1;
}
