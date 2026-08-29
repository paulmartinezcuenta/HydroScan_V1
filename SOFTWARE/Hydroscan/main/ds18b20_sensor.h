
#ifndef DS18B20_DRIVER_H
#define DS18B20_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void ds18b20_init(void);

void ds18b20_task(void *pvParameters);

#ifdef __cplusplus
}
#endif

#endif