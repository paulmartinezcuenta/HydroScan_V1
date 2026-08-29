
#ifndef WAVE_PROCESSING_H
#define WAVE_PROCESSING_H

#ifdef __cplusplus
extern "C" {
#endif

#include "mpu6050_sensor.h"
#include "attitude.h"

float wave_compute_vertical_acceleration(
        const mpu6050_data_t *imu,
        const attitude_data_t *att);

void wave_processing_update_statistics(float az);

void wave_processing_reset_statistics(void);

void wave_processing_print_statistics(void);

#ifdef __cplusplus
}
#endif

#endif