// Hydroscan main application file v5.0.4
// Made by Isaias Matos

// CAMBIOS v5.0.4 Parte II
// OBJETIVO: Probar todo nuevamente con el modulo ya acoplado a la placa PCB.
// Parametros: 
//      Sensor de oleaje: frecuencia de muestreo a 5 Hz, tiempo de rafaga a 120 s. Alta resolucion.
//      Duracion de rafaga de oleaje: 120 s
//      Duracion de espera entre rafagas de oleaje: 120s
//      Periodo de actualizacion de datos de oleaje: 60 s
//      Periodo de actualizacion de GPS: 90 s
//      Periodo de actualizacion para envios a Firebase: 180 s
// ARCHIVOS MODIFICADOS: 
//      Actualizado pines de la board.
// RESULTADOS: 
//      EL modem ya conecta y reconoce la tarjeta SIM.
//      El GPS ya obtiene la posicion y la guarda en buoy_data.
//      Agregado sistema mutex para evitar conflictos entre tareas de gps y firebase.
//      El modem ya obtienen IP y API
//      El modem ya envia DATOS COMPLETOS a Firebase.
//          Probado en la placa PCB: Sensores TDS y Temp, correcto envio a firebase, bateria funcional.
// NOTA: Recomendable hacer mas pruebas del oleaje
// NOTA: Sistema actual requiere tener gps antes de enviar datos a firebase.

// A MEJORAR EN v5.1 y versiones futuras
//      Recibir datos de firebase
//      Probar sistema final con todos los sensores y el modem

#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "buoy_data.h"
#include "tds_sensor.h"
#include "ds18b20_sensor.h"
#include "telemetry.h"
#include "wave_task.h"

#include "modem.h"
#include "gps.h"
#include "firebase.h"

#include "utilities.h"
#include <time.h>

static const char *TAG = "MAIN";

// PARAMETROS GPS - FIREBASE - WAVE
#define GPS_UPDATE_PERIOD_MS          90000      // 90 s
#define FIREBASE_UPDATE_PERIOD_MS     180000     // 120 s de wave_burst_duration + 60 s de wave_last_update
#define TIME_SINCE_LAST_WAVE_UPDATE   60000      // 60 s la mitad de wave_burst_duration

#define LAMP_PIN                      11

static void modem_task(void *pvParameters);

static void update_lamp(void);


buoy_data_t buoy_data;


/*==============================================================
                    LAMPARA
==============================================================*/

static void update_lamp(void)
{
    gpio_set_direction(LAMP_PIN, GPIO_MODE_OUTPUT);

    uint32_t local_timestamp = (uint32_t)buoy_data.timestamp;

    int hour = local_timestamp / 10000;
    int min  = (local_timestamp / 100) % 100;

    bool night = ((((hour >= 17) & (min >= 10)) || hour < 6));

    gpio_set_level(LAMP_PIN, night ? 1 : 0);

    ESP_LOGI(TAG,
             "Hora local: %02d:%02d | Lampara: %s",
             hour,
             min,
             night ? "ON" : "OFF");
}

/*==============================================================
                    MODULO DE COMUNICACION
==============================================================*/

static void modem_task(void *pvParameters)
{
    TickType_t lastGps = xTaskGetTickCount();
    TickType_t lastFirebase = xTaskGetTickCount();

    bool gps_ok = false;

    bool recent_wave_data = false;

    while (1)
    {
        TickType_t now = xTaskGetTickCount();

        /*------------------------------------------
                ACTUALIZAR GPS
        ------------------------------------------*/

        if ((now - lastGps) >= pdMS_TO_TICKS(GPS_UPDATE_PERIOD_MS))
        {
            lastGps = now;

            ESP_LOGI("MODEM_TASK", "Updating GPS...");

            gps_update();   

            gps_ok = gps_has_fix();

            if(gps_ok)
            {
                ESP_LOGI("MODEM_TASK", "GPS FIX OK");

                update_lamp();
            }
            else
            {
                ESP_LOGW("MODEM_TASK", "GPS without FIX");
            }
        }

        /*------------------------------------------
                SUBiR DATOS A FIREBASE
        ------------------------------------------*/

        if ((now - lastFirebase) >= pdMS_TO_TICKS(FIREBASE_UPDATE_PERIOD_MS))
        {
            lastFirebase = now;

            recent_wave_data = (buoy_data.wave_height.last_update_ms <= TIME_SINCE_LAST_WAVE_UPDATE 
                && buoy_data.wave_period.last_update_ms <= TIME_SINCE_LAST_WAVE_UPDATE);    // FUNCIONA!!!!!!

            if(gps_ok && recent_wave_data)
            {
                ESP_LOGI("MODEM_TASK", "Uploading Firebase...");

                firebase_send();
            }
            else
            {
                ESP_LOGW("MODEM_TASK",
                         "Firebase skipped (No GPS FIX)");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

/*==============================================================
                        PRINCIPAL
==============================================================*/

void app_main(void)
{
    printf("\n");
    printf("=============================================\n");
    printf("        HYDROSCAN - BOYA OCEANOGRAFICA\n");
    printf("=============================================\n");

    // Habilitar el switch del modulo para usar bateria
    gpio_reset_pin(BOARD_PWRKEY_PIN);
    gpio_set_direction(BOARD_PWRKEY_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(BOARD_PWRKEY_PIN, 1);

    printf("Sistema inicializado. v5.0.4\n");


    /*----------------------------------------------------------
                    Sensor DS18B20
    ----------------------------------------------------------*/
    
    ds18b20_init();

    xTaskCreate(
        ds18b20_task,
        "Temperature",
        4096,
        NULL,
        4,
        NULL); 


    /*----------------------------------------------------------
                    Sensor TDS
    ----------------------------------------------------------*/

    tds_init();

    xTaskCreate(
        tds_task,
        "TDS",
        4096,
        NULL,
        4,
        NULL);

    /*----------------------------------------------------------
                    Telemetría
    ----------------------------------------------------------*/

    telemetry_init();

    xTaskCreate(
        telemetry_task,
        "Telemetry",
        4096,
        NULL,
        2,
        NULL);

    /*----------------------------------------------------------
                    MODULO LILYGO en proceso
    ----------------------------------------------------------*/
 
    if(modem_init() == ESP_OK)
    {
        gps_init();

        firebase_init();

        xTaskCreate(
            modem_task,
            "MODEM_TASK",
            8192,
            NULL,
            5,
            NULL);

        ESP_LOGI(TAG,
                "MODEM TASK CREATED");

        xTaskCreate(
            wave_task,
            "Wave",
            8192,
            NULL,
            5,
            NULL);
            
        ESP_LOGI(TAG,
                "WAVE TASK CREATED");
    }
    else
    {
        ESP_LOGE(TAG,
                "Modem task not ready");
    }

    printf("\nTodas las tareas fueron creadas correctamente.\n");
}