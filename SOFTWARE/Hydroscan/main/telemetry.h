
#ifndef TELEMETRY_H
#define TELEMETRY_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void telemetry_init(void);

void telemetry_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif