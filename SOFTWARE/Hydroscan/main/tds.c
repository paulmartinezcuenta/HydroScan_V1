
/*
 * ============================================================
 *                      HYDROSCAN
 * ------------------------------------------------------------
 * Archivo      : tds.c
 * Descripción  : Driver del sensor TDS
 *
 * Autor        : Hydroscan Project
 * ============================================================
 */

#include <stdio.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_check.h"

#include "esp_adc/adc_oneshot.h"

#include "board.h"
#include "buoy_data.h"
#include "tds_sensor.h"

/*==============================================================
                        CONFIGURACIÓN
==============================================================*/

/* ADC */

#define TDS_ADC_UNIT ADC_UNIT_1
#define TDS_ADC_CHANNEL ADC_CHANNEL_9 // GPIO10 (ESP32-S3)
#define TDS_ADC_ATTEN ADC_ATTEN_DB_12
#define TDS_ADC_BITWIDTH ADC_BITWIDTH_12

/* Conversión ADC */

#define ADC_MAX_COUNTS 4095.0f
#define ADC_REFERENCE_VOLTAGE 3.3f

/* Muestreo */

#define TDS_NUM_SAMPLES 30
#define TDS_SAMPLE_DELAY_MS 15
#define TDS_UPDATE_PERIOD_MS 2000

/*==============================================================
                    CALIBRACIÓN DEL SENSOR
==============================================================*/

/*
 * Resistencia serie actualmente instalada.
 *
 * IMPORTANTE:
 * Si se modifica esta resistencia deberá recalibrarse
 * la ecuación de conversión Voltaje -> Salinidad.
 */

#define TDS_SERIES_RESISTOR 333.0f

/* Voltaje mínimo válido */

#define TDS_MIN_VALID_VOLTAGE 0.40f

/*==============================================================
                ECUACIÓN DE CALIBRACIÓN
==============================================================*/

/*
 * Salinidad(ppm)
 *
 * = A·V² + B·V + C
 *
 * Ajustada experimentalmente para:
 *
 *      Resistencia = 333 Ω
 *
 */

#define CAL_A 26928.5714f
#define CAL_B -42878.5714f
#define CAL_C 13042.8571f

/*==============================================================
                COMPENSACIÓN POR TEMPERATURA
==============================================================*/

/*
 * En futuras versiones:
 *
 * 0 = Deshabilitada
 * 1 = Utilizar temperatura del DS18B20
 */

#define TDS_USE_TEMP_COMPENSATION 0

/*==============================================================
                    DEBUG
==============================================================*/

#define TDS_DEBUG_PRINT 0

/*==============================================================
                    VARIABLES PRIVADAS
==============================================================*/

static const char *TAG = "TDS";

static adc_oneshot_unit_handle_t adc_handle = NULL;

/*==============================================================
                FUNCIONES PRIVADAS
==============================================================*/

/*
 * Convierte cuentas ADC a Voltaje.
 */

static float adc_to_voltage(float adc_counts)
{
    return (adc_counts * ADC_REFERENCE_VOLTAGE) / ADC_MAX_COUNTS;
}

/*
 * Convierte el voltaje del sensor a salinidad estimada (ppm)
 * utilizando la curva de calibración experimental.
 */

static float voltage_to_calibrated_tds(float voltage)
{
    if (voltage < TDS_MIN_VALID_VOLTAGE)
        return 0.0f;

    float v2 = voltage * voltage;

    return (CAL_A * v2) +
           (CAL_B * voltage) +
           CAL_C;
}

/*
 * Lectura promedio del ADC
 */

static float read_adc_average(void)
{
    int raw = 0;
    int accumulator = 0;

    for (int i = 0; i < TDS_NUM_SAMPLES; i++)
    {
        ESP_ERROR_CHECK(
            adc_oneshot_read(
                adc_handle,
                TDS_ADC_CHANNEL,
                &raw));

        accumulator += raw;

        vTaskDelay(pdMS_TO_TICKS(TDS_SAMPLE_DELAY_MS));
    }

    return (float)accumulator / TDS_NUM_SAMPLES;
}

/*==============================================================
                    INICIALIZACIÓN
==============================================================*/

void tds_init(void)
{
    ESP_LOGI(TAG, "Inicializando sensor TDS...");

    adc_oneshot_unit_init_cfg_t init_config =
        {
            .unit_id = TDS_ADC_UNIT};

    ESP_ERROR_CHECK(
        adc_oneshot_new_unit(
            &init_config,
            &adc_handle));

    adc_oneshot_chan_cfg_t config =
        {
            .bitwidth = TDS_ADC_BITWIDTH,
            .atten = TDS_ADC_ATTEN};

    ESP_ERROR_CHECK(
        adc_oneshot_config_channel(
            adc_handle,
            TDS_ADC_CHANNEL,
            &config));

    ESP_LOGI(TAG,
             "Sensor TDS inicializado correctamente.");

    ESP_LOGI(TAG,
             "Resistencia serie = %.0f Ohm",
             TDS_SERIES_RESISTOR);
}

/*==============================================================
                        TAREA TDS
==============================================================*/

void tds_task(void *pvParameters)
{
    float adc_average;
    float voltage;
    float tds;
    float salinity;

#if TDS_USE_TEMP_COMPENSATION

    float water_temperature;

#endif

    while (1)
    {
        /*----------------------------------------------
            Leer ADC
        ----------------------------------------------*/

        adc_average = read_adc_average();

        /*----------------------------------------------
            Convertir a Voltaje
        ----------------------------------------------*/

        voltage = adc_to_voltage(adc_average);

        /*----------------------------------------------
            Compensación por temperatura
            (Reservado para futuras versiones)
        ----------------------------------------------*/

#if TDS_USE_TEMP_COMPENSATION

        if (buoy_data.temperature.valid)
        {
            water_temperature =
                buoy_data.temperature.value;

            /*
             * Aquí se aplicará posteriormente la
             * compensación por temperatura.
             */
        }

#endif

        /*----------------------------------------------
            Calcular Salinidad
        ----------------------------------------------*/

        tds = voltage_to_calibrated_tds(voltage);

        if (tds < 0.0f)
            tds = 0.0f;

        salinity = tds / 1000;

        /*----------------------------------------------
            Actualizar estructura global
        ----------------------------------------------*/

        /* El valor Salinidad queda reservado para futuras versiones */

        buoy_data.salinity.valid = false;

        /* TDS estimada mediante la calibración */

        buoy_data.tds.value = salinity;
        buoy_data.tds.valid = true;
        buoy_data.tds.last_update_ms =
            xTaskGetTickCount();

#if TDS_DEBUG_PRINT

        printf("\n");
        printf("==================================================\n");
        printf("              HYDROSCAN - SENSOR TDS\n");
        printf("==================================================\n");

        printf("ADC Promedio      : %.1f cuentas\n", adc_average);

        printf("Voltaje           : %.3f V\n", voltage);

        printf("Salinidad         : %.0f ppm\n", tds);

        printf("Estado            : ");

        if (voltage >= 1.95f && voltage <= 2.05f)
        {
            printf("AGUA DE MAR\n");
        }
        else if (voltage > 2.05f)
        {
            printf("HIPERSALINA\n");
        }
        else if (voltage >= 0.60f)
        {
            printf("AGUA SALOBRE\n");
        }
        else
        {
            printf("AGUA DULCE\n");
        }

        printf("==================================================\n\n");

#endif

        vTaskDelay(
            pdMS_TO_TICKS(
                TDS_UPDATE_PERIOD_MS));
    }
}