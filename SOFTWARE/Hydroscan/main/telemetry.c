
/*
 * ============================================================
 *                      HYDROSCAN
 * ------------------------------------------------------------
 * Archivo      : telemetry.c
 * Descripción  : Telemetría general de la boya
 * ============================================================
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "buoy_data.h"
#include "telemetry.h"

/*==============================================================
                        CONFIGURACIÓN
==============================================================*/

#define TELEMETRY_PERIOD_MS     60000 // 60 segundos

#define TELEMETRY_DEBUG            1

#if TELEMETRY_DEBUG
#define TELEMETRY_PRINTF(...)   printf(__VA_ARGS__)
#else
#define TELEMETRY_PRINTF(...)
#endif

/*==============================================================
                        VARIABLES
==============================================================*/

static const char *TAG = "TELEMETRY";

/*==============================================================
                FUNCIONES PRIVADAS
==============================================================*/

static const char *status_string(bool valid)
{
    return valid ? "OK" : "ERROR";
}

/*==============================================================
                    INICIALIZACIÓN
==============================================================*/

void telemetry_init(void)
{
    ESP_LOGI(TAG, "Telemetría inicializada.");
}

/*==============================================================
                        TAREA
==============================================================*/

void telemetry_task(void *pvParameters)
{

#if TELEMETRY_DEBUG

    while (1)
    {
        /*------------------------------------------------------
            Copia local de la estructura
        ------------------------------------------------------*/

        float temperature = buoy_data.temperature.value;
        bool temp_ok = buoy_data.temperature.valid;
        uint32_t temp_age =
            xTaskGetTickCount() -
            buoy_data.temperature.last_update_ms;

        float tds = buoy_data.tds.value;
        bool tds_ok = buoy_data.tds.valid;
        uint32_t tds_age =
            xTaskGetTickCount() -
            buoy_data.tds.last_update_ms;

        /*float hs = buoy_data.wave_height.value;
        bool hs_ok = buoy_data.wave_height.valid;

        float tp = buoy_data.wave_period.value;
        bool tp_ok = buoy_data.wave_period.valid; */

        printf("\n");
        printf("=====================================================\n");
        printf("               HYDROSCAN TELEMETRY\n");
        printf("=====================================================\n\n");

        /*---------------- TEMPERATURA ----------------*/

        printf("[ TEMPERATURA ]\n");

        printf("Estado      : %s\n",
               status_string(temp_ok));

        if (temp_ok)
            printf("Valor       : %.2f °C\n", temperature);
        else
            printf("Valor       : ---\n");

        printf("Edad dato   : %lu ms\n\n",
               (unsigned long)pdTICKS_TO_MS(temp_age));

        /*------------------- TDS ---------------------*/

        printf("[ TDS ]\n");

        printf("Estado      : %s\n",
               status_string(tds_ok));

        if (tds_ok)
            printf("Valor       : %.0f ppm\n", tds);
        else
            printf("Valor       : ---\n");

        printf("Edad dato   : %lu ms\n",
               (unsigned long)pdTICKS_TO_MS(tds_age));

        printf("\n");

        /*---------------- OLEAJE ---------------------*/

        printf("[ OLEAJE ]\n");

        if(buoy_data.wave_height.valid)
        {
            printf("Hs: %.2f m\n",
                buoy_data.wave_height.value);
        }
        else
        {
            printf("Hs: N/D\n");
        }

        if(buoy_data.wave_period.valid)
        {
            printf("Tp: %.2f s\n",
                buoy_data.wave_period.value);
        }
        else
        {
            printf("Tp: N/D\n");
        }

        printf("\n");

        printf("=====================================================\n");

        vTaskDelay(pdMS_TO_TICKS(TELEMETRY_PERIOD_MS));
    }

#endif

}