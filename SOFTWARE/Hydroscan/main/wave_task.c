
#include "wave_task.h"

#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "board.h"
#include "buoy_data.h"

#include "mpu6050_sensor.h"
#include "attitude.h"
#include "wave_processing.h"
#include "spectrum.h"

/*=============================================================
                        CONFIGURACION
=============================================================*/
/*
#define SAMPLE_RATE_HZ 5.0f
#define SAMPLE_PERIOD_MS ((int)(1000.0f / SAMPLE_RATE_HZ))

#define BURST_DURATION_SEC 120
#define WAIT_BETWEEN_BURSTS_SEC 10

#define BURST_SAMPLES ((int)(SAMPLE_RATE_HZ * BURST_DURATION_SEC)) */

#define WAVE_TASK_STACK_SIZE 8192
#define WAVE_TASK_PRIORITY 5

/*=============================================================
                          DEBUG
=============================================================*/

#define WAVE_TASK_DEBUG 1

#if WAVE_TASK_DEBUG
#define WAVE_PRINTF(...) printf(__VA_ARGS__)
#else
#define WAVE_PRINTF(...)
#endif

static const char *TAG = "WAVE_TASK";

/*=============================================================
                        BUFFERS
=============================================================*/

static float vertical_acc[BURST_SAMPLES];

static float roll_history[BURST_SAMPLES];

static float pitch_history[BURST_SAMPLES];

/*=============================================================
                    ESTADISTICAS DEBUG
=============================================================*/

static float az_min;
static float az_max;

static double az_sum;
static double az_sum2;

static uint32_t az_count;

/*=============================================================
                    VARIABLES INTERNAS
=============================================================*/

static attitude_data_t attitude;

static spectrum_result_t spectrum_result;

static int sample_index = 0;

/*=============================================================
                    RESET ESTADISTICAS
=============================================================*/

static void reset_statistics(void)
{
    az_min = 1000.0f;
    az_max = -1000.0f;

    az_sum = 0.0;
    az_sum2 = 0.0;

    az_count = 0;
}

/*=============================================================
                RESET BUFFER DE ADQUISICION
=============================================================*/

static void reset_buffers(void)
{
    sample_index = 0;

    memset(vertical_acc,
           0,
           sizeof(vertical_acc));

    memset(roll_history,
           0,
           sizeof(roll_history));

    memset(pitch_history,
           0,
           sizeof(pitch_history));

    reset_statistics();
}

/*=============================================================
                CREACION DE LA TAREA
=============================================================*/

void wave_task_start(void)
{
    xTaskCreate(
        wave_task,
        "wave_task",
        WAVE_TASK_STACK_SIZE,
        NULL,
        WAVE_TASK_PRIORITY,
        NULL);
}

/*=============================================================
                INICIALIZACION DEL MODULO
=============================================================*/

void wave_initialize(void)
{
    ESP_LOGI(TAG, "======================================");
    ESP_LOGI(TAG, " Inicializando modulo de oleaje...");
    ESP_LOGI(TAG, "======================================");

    ESP_ERROR_CHECK(
        mpu6050_sensor_init());
    
    ESP_ERROR_CHECK(
        mpu6050_calibrate_gyro());

    ESP_ERROR_CHECK(
        mpu6050_calibrate_acc());

    attitude_init();

    reset_buffers();

    ESP_LOGI(TAG,
             "Frecuencia de muestreo : %.1f Hz",
             SAMPLE_RATE_HZ);

    ESP_LOGI(TAG,
             "Muestras por rafaga    : %d",
             BURST_SAMPLES);

    ESP_LOGI(TAG,
             "Duracion rafaga        : %d s",
             BURST_DURATION_SEC);

    ESP_LOGI(TAG,
             "Espera entre rafagas   : %d s",
             WAIT_BETWEEN_BURSTS_SEC);

    ESP_LOGI(TAG,
             "Modulo listo, v4.0.4.8");
}

/*=============================================================
                        TAREA PRINCIPAL
=============================================================*/

void wave_task(void *pvParameters)
{
    (void)pvParameters;

    wave_initialize();

#if WAVE_TASK_DEBUG

WAVE_PRINTF("\n");
WAVE_PRINTF("====================================================\n");
WAVE_PRINTF(" Primera rafaga iniciada...\n");
WAVE_PRINTF("====================================================\n");

#endif

    TickType_t lastWakeTime = xTaskGetTickCount();

    mpu6050_data_t sample;

    while (1)
    {
        /*------------------------------------------------------
                    Mantener frecuencia fija
        ------------------------------------------------------*/

        vTaskDelayUntil(
            &lastWakeTime,
            pdMS_TO_TICKS(SAMPLE_PERIOD_MS));

        /*------------------------------------------------------
                    Leer MPU6050
        ------------------------------------------------------*/

        if (mpu6050_read(&sample) != ESP_OK)
        {
#if WAVE_TASK_DEBUG
            ESP_LOGW(TAG,
                     "Error leyendo MPU6050");
#endif
            continue;
        }

        /*------------------------------------------------------
                Actualizar actitud (Roll / Pitch)
        ------------------------------------------------------*/

        attitude_update(
            &sample, &attitude);

        /*------------------------------------------------------
                Calcular aceleracion vertical
        ------------------------------------------------------*/

        float az_dynamic =
            wave_compute_vertical_acceleration(
                &sample, &attitude);

        /*------------------------------------------------------
                    Estadisticas Debug
        ------------------------------------------------------*/

        if (az_dynamic < az_min)
            az_min = az_dynamic;

        if (az_dynamic > az_max)
            az_max = az_dynamic;

        az_sum += az_dynamic;
        az_sum2 +=
            ((double)az_dynamic *
             (double)az_dynamic);

        az_count++;

        /*------------------------------------------------------
                    Guardar muestra
        ------------------------------------------------------*/

        if (sample_index < BURST_SAMPLES)
        {
            vertical_acc[sample_index] =
                az_dynamic;

            roll_history[sample_index] =
                attitude.roll;

            pitch_history[sample_index] =
                attitude.pitch;

            sample_index++;
        }

/* #if WAVE_TASK_DEBUG

        static uint32_t counter = 0;

        counter++;

        if (counter >= SAMPLE_RATE_HZ)
        {
            counter = 0;

            WAVE_PRINTF(
                "\n------------ MPU6050 ------------\n");

            WAVE_PRINTF(
                "Muestras : %d / %d\n",
                sample_index,
                BURST_SAMPLES);

            WAVE_PRINTF(
                "Roll     : %.2f deg\n",
                attitude.roll * 180.0f / M_PI);

            WAVE_PRINTF(
                "Pitch    : %.2f deg\n",
                attitude.pitch * 180.0f / M_PI);

            WAVE_PRINTF(
                "Az dyn   : %.4f m/s²\n",
                az_dynamic);

            WAVE_PRINTF(
                "---------------------------------\n");
        }

#endif */

        /*------------------------------------------------------
                ¿Rafaga completa?
        ------------------------------------------------------*/

        if (sample_index >= BURST_SAMPLES)
        {

            float az_mean =
                (float)(az_sum / az_count);

            float az_std =
                sqrt(
                    (az_sum2 / az_count) -
                    (az_mean * az_mean));

#if WAVE_TASK_DEBUG

            WAVE_PRINTF("\n");
            WAVE_PRINTF("=============================================\n");
            WAVE_PRINTF("      ANALISIS ACELERACION VERTICAL\n");
            WAVE_PRINTF("=============================================\n");
            WAVE_PRINTF("Min      : %.4f m/s²\n", az_min);
            WAVE_PRINTF("Max      : %.4f m/s²\n", az_max);
            WAVE_PRINTF("Media    : %.4f m/s²\n", az_mean);
            WAVE_PRINTF("Std Dev  : %.4f m/s²\n", az_std);
            WAVE_PRINTF("Muestras : %lu\n",
                        (unsigned long)az_count);
            WAVE_PRINTF("=============================================\n");

            WAVE_PRINTF(
                "\nProcesando espectro de oleaje...\n");

#endif

            /*------------------------------------------------------
                    Procesamiento espectral
            ------------------------------------------------------*/

            spectrum_process(
                vertical_acc,
                roll_history,
                pitch_history,
                BURST_SAMPLES,
                SAMPLE_RATE_HZ,
                &spectrum_result);

            /*------------------------------------------------------
                Actualizar buoy_data
            ------------------------------------------------------*/

            if(spectrum_result.valid)
            {
                buoy_data.wave_height.value =
                    spectrum_result.Hs;

                buoy_data.wave_height.valid = true;

                buoy_data.wave_period.value =
                    spectrum_result.Tp;

                buoy_data.wave_period.valid = true;
            }
            else
            {
                buoy_data.wave_height.valid = false;
                buoy_data.wave_period.valid = false;
            }

            /*------------------------------------------------------
                        Reporte Oceanográfico
            ------------------------------------------------------*/

#if WAVE_TASK_DEBUG

            WAVE_PRINTF("\n");
            WAVE_PRINTF("====================================================\n");
            WAVE_PRINTF("           REPORTE DE OLEAJE\n");
            WAVE_PRINTF("====================================================\n");

            if(spectrum_result.valid)
            {
                WAVE_PRINTF(
                    "Altura significativa : %.3f m\n",
                    spectrum_result.Hs);

                WAVE_PRINTF(
                    "Periodo pico         : %.3f s\n",
                    spectrum_result.Tp);
            }
            else
            {
                WAVE_PRINTF(
                    "Altura significativa : Fuera de rango\n");

                WAVE_PRINTF(
                    "Periodo pico         : Fuera de rango\n");
            }

            WAVE_PRINTF(
                "Tm01                 : %.3f s\n",
                spectrum_result.Tm01);

            WAVE_PRINTF(
                "Tm02                 : %.3f s\n",
                spectrum_result.Tm02);

            WAVE_PRINTF(
                "Frecuencia pico      : %.3f Hz\n",
                spectrum_result.fp);

            WAVE_PRINTF(
                "Direccion relativa   : %.1f deg\n",
                spectrum_result.direction);

            WAVE_PRINTF("====================================================\n");

#endif

            /*------------------------------------------------------
                    Esperar próxima ráfaga
            ------------------------------------------------------*/

#if WAVE_TASK_DEBUG

            WAVE_PRINTF(
                "\nEsperando %d segundos...\n\n",
                WAIT_BETWEEN_BURSTS_SEC);

#endif

            vTaskDelay(
                pdMS_TO_TICKS(
                    WAIT_BETWEEN_BURSTS_SEC * 1000));

            /*------------------------------------------------------
                        Reiniciar buffers
            ------------------------------------------------------*/

            reset_buffers();

            lastWakeTime =
                xTaskGetTickCount();

#if WAVE_TASK_DEBUG

            WAVE_PRINTF(
                "Iniciando nueva ráfaga de oleaje...\n");

#endif
        }
    }
}