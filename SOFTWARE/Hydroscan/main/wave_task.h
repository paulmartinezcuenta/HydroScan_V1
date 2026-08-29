
#ifndef WAVE_TASK_H
#define WAVE_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "mpu6050_sensor.h"
#include "attitude.h"
#include "wave_processing.h"
#include "spectrum.h"

#include "board.h"
#include "buoy_data.h"

/*=============================================================
                    CONFIGURACION GENERAL
=============================================================*/

#define SAMPLE_RATE_HZ              5.0f
#define SAMPLE_PERIOD_MS            ((uint32_t)(1000.0f / SAMPLE_RATE_HZ))

#define BURST_DURATION_SEC          120

#define BURST_SAMPLES               ((uint32_t)(SAMPLE_RATE_HZ * BURST_DURATION_SEC))

#define WAIT_BETWEEN_BURSTS_SEC     120

/*=============================================================
                        DEBUG
=============================================================*/

#define WAVE_TASK_DEBUG             1

#if WAVE_TASK_DEBUG
#define WAVE_PRINTF(...) printf(__VA_ARGS__)
#else
#define WAVE_PRINTF(...)
#endif

/*=============================================================
                    FUNCIONES PUBLICAS
=============================================================*/

void wave_initialize(void);

void wave_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif