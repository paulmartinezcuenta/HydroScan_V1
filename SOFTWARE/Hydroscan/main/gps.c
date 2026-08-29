
/*
 * ============================================================
 *                      HYDROSCAN
 * ------------------------------------------------------------
 * Archivo      : gps.c
 * Descripción  : GPS del modulo LILYGO
 *
 * Autor        : Hydroscan Project
 * ============================================================
 */

#include "gps.h"

#include "modem.h"
#include "utilities.h"
#include "buoy_data.h"

#include "driver/uart.h"

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <stdio.h>
#include <string.h>

#define GPS_UPDATE_PERIOD_MS (90000)

static const char *TAG = "GPS";

static bool gps_fix = false;

/*==============================================================
                        Inicializar GPS
==============================================================*/

esp_err_t gps_init(void)
{
    char rx[256];

    if (!modem_is_ready())
        return ESP_FAIL;

    ESP_LOGI(TAG, "Initializing GPS...");

    /* Encender GNSS */
    modem_send_at("AT+CGNSSPWR=1");

    if(modem_read_response(rx,sizeof(rx),3000)<=0)
        return ESP_FAIL;

    ESP_LOGI(TAG,"%s",rx);

    if(strstr(rx,"OK")==NULL)
        return ESP_FAIL;

    /* Dar unos segundos al receptor */
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG,"GPS READY");

    return ESP_OK;
}

// Parte II

/*==============================================================
                        Pedir info GPS
==============================================================*/

static bool gps_read_info(char *buffer, size_t len)
{
    modem_send_at("AT+CGNSSINFO");

    int n = modem_read_response(
                buffer,
                len,
                3000);

    if (n <= 0)
        return false;

    ESP_LOGI(TAG, "%s", buffer);

    return true;
}

/*==============================================================
                        Convertir coordenadas
==============================================================*/

static float gps_convert_coordinate(const char *coord, char hemi)
{
    if (coord == NULL || strlen(coord) == 0)
        return 0.0f;

    float decimal = atof(coord);

    if (hemi == 'S' || hemi == 'W')
        decimal = -decimal;

    return decimal;
}

/*==============================================================
                        Funcion principal GPS
==============================================================*/

esp_err_t gps_update(void)      // ACTUALIZADO + mutex
{
    char rx[256];

    uint32_t real_time;

    static uint32_t last_update = 0;

    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // Bloquear el acceso al modem para evitar conflictos con otras tareas

    if (!modem_lock(10000))
    {
        ESP_LOGE(TAG, "Cannot lock modem");
        modem_unlock();
        return ESP_FAIL;
    }

    if ((now - last_update) < GPS_UPDATE_PERIOD_MS)
    {
        modem_unlock();
        return ESP_OK;
    }

    last_update = now;

    if (!gps_read_info(rx, sizeof(rx)))
    {
        ESP_LOGW(TAG, "No response from GPS");
        
        modem_unlock();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "%s", rx);

    char *start = strstr(rx, "+CGNSSINFO:");

    if (start == NULL) 
    {
        modem_unlock();
        return ESP_FAIL;
    }

    start += strlen("+CGNSSINFO:");

    while (*start == ' ')
        start++;

    char field[20][32] = {0};

    int field_index = 0;
    int char_index = 0;

    while (*start != '\0' &&
           *start != '\r' &&
           *start != '\n' &&
           field_index < 20)
    {
        if (*start == ',')
        {
            field[field_index][char_index] = '\0';

            field_index++;
            char_index = 0;
        }
        else
        {
            if (char_index < 31)
            {
                field[field_index][char_index++] = *start;
            }
        }

        start++;
    }

    field[field_index][char_index] = '\0';

    /*
        Campos del A7608:

        0  Run Status
        1  Fix Status
        2
        3
        4
        5  Latitude
        6  N/S
        7  Longitude
        8  E/W
        9  Date
        10 UTC
        11 Altitude
        12 Speed
        13 Course
        14 HDOP
        15 PDOP
        16 Satellites
    */

    if (strlen(field[5]) == 0 || strlen(field[7]) == 0)
    {
        gps_fix = false;

        buoy_data.latitude.valid  = false;
        buoy_data.longitude.valid = false;
        buoy_data.altitude.valid  = false;
        buoy_data.speed.valid     = false;

        ESP_LOGI(TAG, "Waiting GPS FIX...");

        modem_unlock();
        return ESP_OK;
    }

    float latitude =
        gps_convert_coordinate(field[5], field[6][0]);

    float longitude =
        gps_convert_coordinate(field[7], field[8][0]);

    float altitude = atof(field[11]);

    float speed = atof(field[12]);

    gps_fix = true;

    buoy_data.latitude.value = latitude;
    buoy_data.latitude.valid = true;
    buoy_data.latitude.last_update_ms = now;

    buoy_data.longitude.value = longitude;
    buoy_data.longitude.valid = true;
    buoy_data.longitude.last_update_ms = now;

    buoy_data.altitude.value = altitude;
    buoy_data.altitude.valid = true;
    buoy_data.altitude.last_update_ms = now;

    buoy_data.speed.value = speed;
    buoy_data.speed.valid = true;
    buoy_data.speed.last_update_ms = now;

    /*==============================================================
        Convertir hora UTC del GPS a UTC-4
        field[10] viene como HHMMSS.ss
    ==============================================================*/

    float utc_raw = atof(field[10]);

    int utc_hour = (int)(utc_raw / 10000);
    int utc_min  = ((int)utc_raw / 100) % 100;
    int utc_sec  = (int)utc_raw % 100;

    /* Restar 4 horas */
    int local_hour = utc_hour - 4;

    /* Si cruza medianoche */
    if (local_hour < 0)
    {
        local_hour += 24;
    }

    /* Guardar como HHMMSS */
    real_time =
        (local_hour * 10000) +
        (utc_min * 100) +
        utc_sec;

    buoy_data.timestamp = real_time;

    ESP_LOGI(TAG,
             "GPS FIX | Lat: %.7f | Lon: %.7f | Alt: %.2f m | Speed: %.2f km/h | Timestamp: %lu",
             latitude,
             longitude,
             altitude,
             speed,
             (unsigned long)buoy_data.timestamp);

    modem_unlock();
    return ESP_OK;
}

// AGregar bool gps_has_fix(void)
bool gps_has_fix(void)
{
    return gps_fix;
}

