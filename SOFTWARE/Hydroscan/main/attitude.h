
#ifndef ATTITUDE_H
#define ATTITUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

#include "mpu6050_sensor.h"

/*==============================================================
                    ESTRUCTURA DE ACTITUD
==============================================================*/

typedef struct
{
    float roll;
    float pitch;

    float sin_roll;
    float cos_roll;

    float sin_pitch;
    float cos_pitch;

} attitude_data_t;

/*==============================================================
                    FUNCIONES PÚBLICAS
==============================================================*/

void attitude_init(void);

void attitude_update(const mpu6050_data_t *imu,
                     attitude_data_t *att);

#ifdef __cplusplus
}
#endif

#endif