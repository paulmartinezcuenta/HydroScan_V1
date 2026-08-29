
/*
 * ============================================================
 *                      HYDROSCAN
 * ------------------------------------------------------------
 * Archivo      : ds18b20.c
 * Descripción  : Driver del sensor DS18B20
 *
 * Autor        : Hydroscan Project
 * ============================================================
 */

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "esp_check.h"

#include "onewire_bus.h"
#include "ds18b20.h"

#include "board.h"
#include "buoy_data.h"
#include "ds18b20_sensor.h"

/*==============================================================
                        CONFIGURACIÓN
==============================================================*/

#define DS18B20_UPDATE_PERIOD_MS        2000

/* Tiempo máximo de conversión a 12 bits */

#define DS18B20_CONVERSION_TIME_MS      750

/*==============================================================
                        DEBUG
==============================================================*/

#define DS18B20_DEBUG_PRINT             0

/*==============================================================
                    VARIABLES PRIVADAS
==============================================================*/

static const char *TAG = "DS18B20";

static onewire_bus_handle_t bus = NULL;

static onewire_device_t device;

static ds18b20_device_handle_t sensor = NULL;

static bool sensor_detected = false;

/*==============================================================
                FUNCIONES PRIVADAS
==============================================================*/

/*
 * Busca un DS18B20 conectado al bus OneWire.
 */

static esp_err_t ds18b20_search_sensor(void)
{
    onewire_device_iter_handle_t iter = NULL;

    ESP_RETURN_ON_ERROR(
        onewire_new_device_iter(bus, &iter),
        TAG,
        "No fue posible crear el iterador OneWire"
    );

    esp_err_t ret =
        onewire_device_iter_get_next(iter, &device);

    onewire_del_device_iter(iter);

    return ret;
}

/*==============================================================
                    INICIALIZACIÓN
==============================================================*/

void ds18b20_init(void)
{
    ESP_LOGI(TAG, "Inicializando bus OneWire...");

    onewire_bus_config_t bus_config =
    {
        .bus_gpio_num = PIN_DS18B20,
    };

    onewire_bus_rmt_config_t rmt_config =
    {
        .max_rx_bytes = 10,
    };

    ESP_ERROR_CHECK(
        onewire_new_bus_rmt(
            &bus_config,
            &rmt_config,
            &bus
        )
    );

    ESP_LOGI(TAG, "Bus OneWire inicializado.");

    /*------------------------------------------
        Buscar sensor
    ------------------------------------------*/

    if (ds18b20_search_sensor() != ESP_OK)
    {
        ESP_LOGW(TAG,
                 "No se encontró ningún DS18B20.");

        buoy_data.temperature.valid = false;

        sensor_detected = false;

        return;
    }

    /*------------------------------------------
        Inicializar DS18B20
    ------------------------------------------*/

    ds18b20_config_t sensor_config = {0};

    ESP_ERROR_CHECK(
        ds18b20_new_device_from_enumeration(
            &device,
            &sensor_config,
            &sensor
        )
    );

    sensor_detected = true;

    ESP_LOGI(TAG,
             "DS18B20 detectado correctamente.");
}

/*==============================================================
                    TAREA PRINCIPAL
==============================================================*/

void ds18b20_task(void *pvParameters)
{
    esp_err_t ret;

    float temperature;

    while (1)
    {
        /*------------------------------------------------------
            Si el sensor no está disponible,
            intentar detectarlo nuevamente.
        ------------------------------------------------------*/

        if (!sensor_detected)
        {
            ESP_LOGW(TAG,
                     "Intentando detectar nuevamente el DS18B20...");

            if (ds18b20_search_sensor() == ESP_OK)
            {
                ds18b20_config_t sensor_config = {0};

                ret = ds18b20_new_device_from_enumeration(
                            &device,
                            &sensor_config,
                            &sensor);

                if (ret == ESP_OK)
                {
                    sensor_detected = true;

                    ESP_LOGI(TAG,
                             "DS18B20 reconectado correctamente.");
                }
            }

            vTaskDelay(pdMS_TO_TICKS(5000));

            continue;
        }

        /*------------------------------------------------------
            Iniciar conversión
        ------------------------------------------------------*/

        ret = ds18b20_trigger_temperature_conversion(sensor);

        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG,
                     "Error iniciando conversión.");

            sensor_detected = false;

            buoy_data.temperature.valid = false;

            continue;
        }

        /*------------------------------------------------------
            Esperar conversión
        ------------------------------------------------------*/

        vTaskDelay(
            pdMS_TO_TICKS(
                DS18B20_CONVERSION_TIME_MS
            )
        );

        /*------------------------------------------------------
            Leer temperatura
        ------------------------------------------------------*/

        ret = ds18b20_get_temperature(
                    sensor,
                    &temperature);

        if (ret == ESP_OK)
        {
            buoy_data.temperature.value = temperature;

            buoy_data.temperature.valid = true;

            buoy_data.temperature.last_update_ms =
                    xTaskGetTickCount();

#if DS18B20_DEBUG_PRINT

            printf("\n");
            printf("=============================================\n");
            printf("       HYDROSCAN - SENSOR TEMPERATURA\n");
            printf("=============================================\n");
            printf("Temperatura : %.2f °C\n",
                   temperature);
            printf("Estado      : OK\n");
            printf("=============================================\n\n");

#endif

        }
        else
        {
            ESP_LOGE(TAG,
                     "Error leyendo temperatura.");

            buoy_data.temperature.valid = false;

            sensor_detected = false;
        }

        /*------------------------------------------------------
            Esperar siguiente medición
        ------------------------------------------------------*/

        vTaskDelay(
            pdMS_TO_TICKS(
                DS18B20_UPDATE_PERIOD_MS
            )
        );
    }
}