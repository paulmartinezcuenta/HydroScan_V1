
/*
 * ============================================================
 *                      HYDROSCAN
 * ------------------------------------------------------------
 * Archivo      : wave_processing.c
 * Descripción  : Procesamiento de aceleración vertical
 * ============================================================
 */

#include "wave_processing.h"

#include <math.h>
#include <string.h>

#include "esp_log.h"

/*==============================================================
                        CONFIGURACIÓN
==============================================================*/

static const char *TAG = "WAVE_PROC";

#define WAVE_PROCESSING_DEBUG            0

#if WAVE_PROCESSING_DEBUG
#define WAVE_PRINTF(...)   printf(__VA_ARGS__)
#else
#define WAVE_PRINTF(...)
#endif

/*==============================================================
                FUNCIONES PRIVADAS
==============================================================*/

/*
 * Convierte la aceleración medida por el sensor
 * al marco de referencia terrestre.
 *
 * Devuelve únicamente la componente vertical (eje Z global).
 */

static float body_to_earth_vertical(
        const mpu6050_data_t *imu,
        const attitude_data_t *att)
{
    /*
     * Matriz de rotación (Roll + Pitch)
     *
     * No se utiliza Yaw ya que aún no existe
     * una referencia absoluta.
     */

    float sr = att->sin_roll;
    float cr = att->cos_roll;

    float sp = att->sin_pitch;
    float cp = att->cos_pitch;

    /*
     * Componente Z en el marco terrestre.
     *
     * Equivalente a multiplicar la aceleración
     * por la tercera fila de la matriz R.
     */

    float az_earth =

          (-sp)      * imu->ax
        + (cp * sr)  * imu->ay
        + (cp * cr)  * imu->az;

/*
    printf("\n");
    printf("ax = %.3f\n", imu->ax);
    printf("ay = %.3f\n", imu->ay);
    printf("az = %.3f\n", imu->az);
    printf("az_earth = %.3f\n", az_earth);
*/
    return az_earth;
}

/*==============================================================
                ACELERACIÓN VERTICAL
==============================================================*/

float wave_compute_vertical_acceleration(
        const mpu6050_data_t *imu,
        const attitude_data_t *att)
{
    /*
     * Transformar al marco terrestre.
     */

    float az_global =
        body_to_earth_vertical(
            imu,
            att);

    /*
     * El acelerómetro siempre mide gravedad.
     *
     * En el eje Z terrestre debemos eliminar
     * exactamente 1 g.
     */

    float az_dynamic =
        az_global - G0;

    return az_dynamic;
}

/*==============================================================
                    VARIABLES DE DEBUG
==============================================================*/

static float az_min = 1000.0f;
static float az_max = -1000.0f;

static double az_sum = 0.0;
static double az_sum2 = 0.0;

static uint32_t sample_count = 0;

/*==============================================================
                ACTUALIZAR ESTADÍSTICAS
==============================================================*/

void wave_processing_update_statistics(float az)
{
    if (az < az_min)
        az_min = az;

    if (az > az_max)
        az_max = az;

    az_sum += az;
    az_sum2 += (double)az * (double)az;

    sample_count++;
}

/*==============================================================
                REINICIAR ESTADÍSTICAS
==============================================================*/

void wave_processing_reset_statistics(void)
{
    az_min = 1000.0f;
    az_max = -1000.0f;

    az_sum = 0.0;
    az_sum2 = 0.0;

    sample_count = 0;
}

/*==============================================================
                IMPRIMIR ESTADÍSTICAS
==============================================================*/

void wave_processing_print_statistics(void)
{

#if WAVE_PROCESSING_DEBUG

    if(sample_count == 0)
    {
        WAVE_PRINTF("No existen muestras.");

        return;
    }

    float mean =
        (float)(az_sum / sample_count);

    float variance =
        (float)((az_sum2 / sample_count) -
                ((double)mean * (double)mean));

    if(variance < 0.0f)
        variance = 0.0f;

    float std =
        sqrtf(variance);

    printf("\n");

    printf("=========== ANALISIS a_z_dyn_mss ===========\n");

    printf("Min      : %.4f m/s²\n", az_min);

    printf("Max      : %.4f m/s²\n", az_max);

    printf("Media    : %.4f m/s²\n", mean);

    printf("Std Dev  : %.4f m/s²\n", std);

    printf("Muestras : %lu\n",
            (unsigned long)sample_count);

    printf("===========================================\n");

#endif

}