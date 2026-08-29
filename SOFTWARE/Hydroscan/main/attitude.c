
/*
 * ============================================================
 *                      HYDROSCAN
 * ------------------------------------------------------------
 * Archivo      : attitude.c
 * Descripción  : Estimación de actitud (Roll y Pitch)
 * ============================================================
 */

#include "attitude.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"

/*==============================================================
                        CONFIGURACIÓN
==============================================================*/

static const char *TAG = "ATTITUDE";

/*
 * Filtro complementario
 *
 * Valores típicos:
 *
 * 0.95  -> responde rápido, más ruido
 * 0.98  -> recomendado
 * 0.995 -> muy suave
 */

#define ATTITUDE_ALPHA     0.98f

/*==============================================================
                    VARIABLES PRIVADAS
==============================================================*/

/*
 * Estado interno del filtro.
 *
 * Estas variables permanecen entre llamadas
 * a attitude_update().
 */

static float roll_est = 0.0f;
static float pitch_est = 0.0f;

static bool first_sample = true;

/*==============================================================
                FUNCIONES PRIVADAS
==============================================================*/

/*
 * Reinicia completamente el filtro.
 */

static void attitude_reset(void)
{
    roll_est = 0.0f;
    pitch_est = 0.0f;

    first_sample = true;
}

/*
 * Limita un ángulo al rango [-pi, pi]
 * (útil para futuras ampliaciones).
 */

static float wrap_pi(float angle)
{
    while(angle > (float)M_PI)
        angle -= 2.0f * (float)M_PI;

    while(angle < -(float)M_PI)
        angle += 2.0f * (float)M_PI;

    return angle;
}

/*==============================================================
                INICIALIZACIÓN
==============================================================*/

void attitude_init(void)
{
    attitude_reset();

    ESP_LOGI(TAG,
             "Filtro complementario inicializado");

    ESP_LOGI(TAG,
             "Alpha = %.3f",
             ATTITUDE_ALPHA);
}

/*==============================================================
                ACTUALIZACIÓN DEL FILTRO
==============================================================*/

void attitude_update(const mpu6050_data_t *imu,
                     attitude_data_t *att)
{
    /*-----------------------------
      Roll y Pitch por acelerómetro
    ------------------------------*/

    float roll_acc =
        atan2f(imu->ay,
               imu->az);

    float pitch_acc =
        atan2f(-imu->ax,
               sqrtf((imu->ay * imu->ay) +
                     (imu->az * imu->az)));

    /*-----------------------------
      Primera muestra
    ------------------------------*/

    if(first_sample)
    {
        roll_est = roll_acc;
        pitch_est = pitch_acc;

        first_sample = false;
    }
    else
    {
        /* Integración del giroscopio */

        roll_est += imu->gx * MPU6050_DT;
        pitch_est += imu->gy * MPU6050_DT;

        /* Filtro complementario */

        roll_est =
            ATTITUDE_ALPHA * roll_est +
            (1.0f - ATTITUDE_ALPHA) * roll_acc;

        pitch_est =
            ATTITUDE_ALPHA * pitch_est +
            (1.0f - ATTITUDE_ALPHA) * pitch_acc;
    }

    /* Normalizar ángulos */

    roll_est = wrap_pi(roll_est);
    pitch_est = wrap_pi(pitch_est);

    /*-----------------------------
      Copiar a la estructura pública
    ------------------------------*/

    att->roll = roll_est;
    att->pitch = pitch_est;

    att->sin_roll = sinf(roll_est);
    att->cos_roll = cosf(roll_est);

    att->sin_pitch = sinf(pitch_est);
    att->cos_pitch = cosf(pitch_est);
}