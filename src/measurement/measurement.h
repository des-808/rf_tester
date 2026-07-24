#ifndef MEASUREMENT_H
#define MEASUREMENT_H

#include <stdint.h>

typedef struct {
    float swr; // пример: SWR значение
    int32_t raw1;
    int32_t raw2;
} MeasurementResults;

void Measurement_Init(void);
void Measurement_Start(void);
void Measurement_Stop(void);
void Measurement_Handler(void);
MeasurementResults Measurement_GetLast(void);
void Measurement_Subscribe(void (*cb)(const MeasurementResults* r));

#endif
