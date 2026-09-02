/**
 * ========================================================================
 *  RF Spectrum Analyzer — Implementation
 *  Спектроанализатор на базе CC1101 + ST7796 дисплей
 * ========================================================================
 */

#include "rf_spectrum.h"
#include "radio_cc1101.h"
#include "st7796.h"
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
