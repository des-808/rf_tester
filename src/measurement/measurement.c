#include "measurement.h"
#include <string.h>
#include <math.h>

// Простейшая имитация неблокирующих измерений
static volatile MeasurementResults last_results;
static volatile uint8_t running = 0;
static void (*observer_cb)(const MeasurementResults* r) = 0;

void Measurement_Init(void) {
    memset((void*)&last_results, 0, sizeof(last_results));
    last_results.swr = 1.0f;
}

void Measurement_Start(void) {
    running = 1;
}

void Measurement_Stop(void) {
    running = 0;
}

void Measurement_Subscribe(void (*cb)(const MeasurementResults* r)) {
    observer_cb = cb;
}

MeasurementResults Measurement_GetLast(void) {
    MeasurementResults copy;
    // простое неблокирующее копирование
    memcpy(&copy, (const void*)&last_results, sizeof(copy));
    return copy;
}

void Measurement_Handler(void) {
    if (!running) return;

    static uint16_t ticker = 0;
    // имитируем обновление раз в ~50 вызовов handler
    ticker++;
    if (ticker < 50) return;
    ticker = 0;

    // простая имитация: меняем swr по синусоидальной функции
    static float phase = 0.0f;
    phase += 0.12f;
    if (phase > 6.28318f) phase -= 6.28318f;

    float new_swr = 1.0f + (0.5f * (float)((float)sin(phase) + 1.0f));
    last_results.swr = new_swr;
    last_results.raw1++;
    last_results.raw2 += 2;

    if (observer_cb) observer_cb(&last_results);
}
