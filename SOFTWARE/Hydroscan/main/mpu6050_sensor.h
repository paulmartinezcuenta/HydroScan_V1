
#ifndef MPU6050_SENSOR_H
#define MPU6050_SENSOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"


/*==============================================================
                    CONFIGURACIÓN DEL SENSOR
==============================================================*/

/* Frecuencia de muestreo utilizada por Hydroscan */

#define MPU6050_SAMPLE_RATE_HZ     5.0f
#define MPU6050_DT                 (1.0f / MPU6050_SAMPLE_RATE_HZ)

/* Escalas configuradas */

#define ACC_LSB_PER_G              16384.0f
#define GYRO_LSB_PER_DPS           131.0f

/* Constantes físicas */

#define G0                         9.80665f

/*==============================================================
                    ESTRUCTURAS
==============================================================*/

/* Datos crudos provenientes del MPU6050 */

typedef struct
{
    int16_t ax;
    int16_t ay;
    int16_t az;

    int16_t temperature;

    int16_t gx;
    int16_t gy;
    int16_t gz;

} mpu6050_raw_data_t;


/* Datos convertidos a unidades físicas */

typedef struct
{
    /* Aceleración */

    float ax;
    float ay;
    float az;

    /* Velocidad angular */

    float gx;
    float gy;
    float gz;

    /* Temperatura interna del MPU6050 */

    float temperature;

} mpu6050_data_t;

/*==============================================================
                    FUNCIONES PÚBLICAS
==============================================================*/

/* Inicializa el MPU6050 */

esp_err_t mpu6050_sensor_init(void);

/* Calibra el bias del giroscopio */

esp_err_t mpu6050_calibrate_gyro(void);

/* Calibra el bias del acelerómetro */

esp_err_t mpu6050_calibrate_acc(void);

/* Lectura cruda */

esp_err_t mpu6050_read_raw(
    mpu6050_raw_data_t *raw);

/* Lectura en unidades físicas */

esp_err_t mpu6050_read(
    mpu6050_data_t *imu);

#ifdef __cplusplus
}
#endif

#endif