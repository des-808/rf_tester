#ifndef __RF_SPECTRUM_H__
#define __RF_SPECTRUM_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/**
 * @brief RF Spectrum Analyzer Module
 * 
 * Спектроанализатор на базе CC1101 + ST7796 дисплей.
 * Сканирует диапазон частот и отображает мощность сигнала (RSSI).
 * 
 * Особенности:
 * - Диапазон: 240-960 МГц (ограничен CC1101)
 * - Разрешение: настраиваемое (320 точек = ширина экрана)
 * - Быстрое сканирование через RSSI
 * - Визуализация спектра в реальном времени
 * - Маркеры пиковых частот
 */

/* ========================================================================
 *  Конфигурация спектроанализатора
 * ======================================================================== */

/* Диапазон частот CC1101 (МГц * 100) */
#define SPECTRUM_FREQ_MIN       24000   /* 240.00 МГц */
#define SPECTRUM_FREQ_MAX       96000   /* 960.00 МГц */

/* Разрешение спектра (количество точек = ширина экрана) */
#define SPECTRUM_POINTS         320U

/* Шаг сканирования между точками (кГц) */
#define SPECTRUM_STEP_KHZ       500U

/* Максимальное значение RSSI (тишина) */
#define SPECTRUM_RSSI_MIN       (-90)

/* Минимальное значение RSSI (сигнал) */
#define SPECTRUM_RSSI_MAX       (-20)

/* Время измерения на одну точку (мс) */
#define SPECTRUM_SAMPLE_TIME_MS 2U

/* ========================================================================
 *  Структуры данных
 * ======================================================================== */

/**
 * @brief Состояние спектроанализатора
 */
typedef struct {
    bool active;                    /* Активен ли спектроанализатор */
    uint32_t freq_start;            /* Начальная частота (fixed-point x100, МГц) */
    uint32_t freq_end;              /* Конечная частота (fixed-point x100, МГц) */
    uint16_t points;                /* Количество точек */
    int8_t spectrum[SPECTRUM_POINTS]; /* RSSI для каждой точки (-90..-20) */
    uint32_t peak_freq;             /* Частота пика (fixed-point x100, МГц) */
    int8_t peak_rssi;               /* Мощность пика (RSSI) */
    uint32_t scan_duration_ms;      /* Время полного скана (мс) */
} RfSpectrum_t;

/**
 * @brief Колбэк для обновления спектра (вызывается каждый скан)
 */
typedef void (*RfSpectrum_Callback_t)(RfSpectrum_t* spectrum, void* user_data);

/* ========================================================================
 *  API
 * ======================================================================== */

/**
 * @brief Инициализировать спектроанализатор
 * @param spectrum Указатель на структуру состояния
 * @param freq_start Начальная частота (МГц * 100)
 * @param freq_end Конечная частота (МГц * 100)
 * @param points Количество точек (до SPECTRUM_POINTS)
 * @return true если успешно
 */
bool RfSpectrum_Init(RfSpectrum_t* spectrum, uint32_t freq_start, uint32_t freq_end, uint16_t points);

/**
 * @brief Запустить сканирование спектра (блокирующее)
 * @param spectrum Указатель на структуру состояния
 * @return true если успешно
 */
bool RfSpectrum_Scan(RfSpectrum_t* spectrum);

/**
 * @brief Найти пик в спектре
 * @param spectrum Указатель на структуру состояния
 */
void RfSpectrum_FindPeak(RfSpectrum_t* spectrum);

/**
 * @brief Отрендерить спектр на дисплее
 * @param spectrum Указатель на структуру состояния
 * @param x X-координата области отрисовки
 * @param y Y-координата области отрисовки
 * @param w Ширина области
 * @param h Высота области
 */
void RfSpectrum_Render(RfSpectrum_t* spectrum, int16_t x, int16_t y, uint16_t w, uint16_t h);

/**
 * @brief Остановить спектроанализатор
 * @param spectrum Указатель на структуру состояния
 */
void RfSpectrum_Stop(RfSpectrum_t* spectrum);

/**
 * @brief Проверить, активен ли спектроанализатор
 */
bool RfSpectrum_IsActive(RfSpectrum_t* spectrum);

/**
 * @brief Получить текстовое представление RSSI
 * @param rssi Значение RSSI
 * @param buf Буфер для строки
 * @param buf_size Размер буфера
 */
void RfSpectrum_RssiToString(int8_t rssi, char* buf, size_t buf_size);

/**
 * @brief Получить текстовое представление частоты
 * @param freq Fixed-point МГц (x100)
 * @param buf Буфер для строки
 * @param buf_size Размер буфера
 */
void RfSpectrum_FreqToString(uint32_t freq, char* buf, size_t buf_size);

#endif /* __RF_SPECTRUM_H__ */
