
/*
 * ============================================================
 *                      HYDROSCAN
 * ------------------------------------------------------------
 * Archivo      : firebase.c
 * Descripción  : Enviar datos a Firebase Realtime Database
 *
 * Autor        : Hydroscan Project
 * ============================================================
 */

#include "firebase.h"

#include "modem.h"
#include "buoy_data.h"

#include "esp_log.h"

#include "driver/uart.h"
#include "utilities.h"

#include <stdio.h>
#include <string.h>

static const char BOYA_ID[24] = "HYDROSCAN-001";

static const char *TAG = "FIREBASE";

static const char FIREBASE_URL[] =
"https://hydroscan-7329a-default-rtdb.firebaseio.com/location.json";

static const char FIREBASE_AUTH[] = "";

static char json_buffer[512];

static char firebase_cmd[256];

static char firebase_rx[2048];

/*==============================================================
                        Inicializar Firebase
==============================================================*/

esp_err_t firebase_init(void)
{
    ESP_LOGI(TAG,"Firebase initialization");

    return ESP_OK;
}

/*==============================================================
                        Comando HTTP
==============================================================*/

static esp_err_t firebase_http_command(     // Nueva
        const char *cmd,
        uint32_t timeout_ms)
{
    ESP_LOGI(TAG,"%s",cmd);

    if(strcmp(cmd,"AT+HTTPTERM")==0)
    {
        modem_send_at(cmd);

        modem_wait_for("OK",1000);

        return ESP_OK;
    }

    return modem_send_wait(
                cmd,
                "OK",
                timeout_ms);
}

/*==============================================================
                        Construir JSON
==============================================================*/

static void firebase_build_json(void)
{
    snprintf(
        json_buffer,
        sizeof(json_buffer),

        "{"

        "\"id\":\"%s\","

        "\"timestamp\":%lu,"

        "\"temperature\":%.2f,"

        "\"tds\":%.2f,"

        "\"wave_height\":%.3f,"

        "\"wave_period\":%.3f,"

        "\"latitude\":%.7f,"

        "\"longitude\":%.7f,"

        "\"altitude\":%.2f,"

        "\"speed\":%.2f"

        "}",

        buoy_data.buoy_id,

        (unsigned long)buoy_data.timestamp,

        buoy_data.temperature.value,

        buoy_data.tds.value,

        buoy_data.wave_height.value,

        buoy_data.wave_period.value,

        buoy_data.latitude.value,

        buoy_data.longitude.value,

        buoy_data.altitude.value,

        buoy_data.speed.value);

    ESP_LOGI(TAG,"%s",json_buffer);
}

/*==============================================================
                        Enviar datos a Firebase
==============================================================*/

esp_err_t firebase_send(void)  // ACTUALIZADO + mutex
{

    if (!modem_lock(15000))
    {
        ESP_LOGE(TAG, "Cannot lock modem");
        return ESP_FAIL;
    }

    // INICIO

    if(!modem_is_ready())
    {
        modem_unlock();
        return ESP_FAIL;
    }

    sprintf(
         buoy_data.buoy_id,
         "%s",
         BOYA_ID);

    firebase_build_json();

    /*
     * Cerrar cualquier sesión HTTP anterior
     */

    firebase_http_command(
        "AT+HTTPTERM",
        3000);

    /*
     * Inicializar HTTP
     */

    if(firebase_http_command(
            "AT+HTTPINIT",
            3000)!=ESP_OK)
    {    
        modem_unlock();    
        return ESP_FAIL;
    }

    if(firebase_http_command(
            "AT+HTTPPARA=\"SSLCFG\",0",
            3000)!=ESP_OK)
    {
        ESP_LOGE(TAG,"Cannot set SSL config");

        modem_unlock();
        return ESP_FAIL;
    } 

    // Mandar comandos para depuracion:

/*    sprintf(
        firebase_cmd,
        "AT+HTTPINIT");

    ESP_LOGI(TAG,"%s",firebase_cmd);
   
    if(modem_send_raw(
            firebase_cmd,
            firebase_rx,
            sizeof(firebase_rx),
            10000)!=ESP_OK)
    {
        ESP_LOGE(TAG,"%s command made no response", firebase_cmd);

        firebase_http_command(
                "AT+HTTPTERM",
                3000);

        modem_unlock();
        return ESP_FAIL;
    }
    
    // Mostrar exactamente todo lo que respondió el módem

    ESP_LOGI(TAG,
            "RAW RESPONSE:\n%s",
            firebase_rx); */

    /*
     * URL
     */

    if(strlen(FIREBASE_AUTH)==0)
    {
        sprintf(
            firebase_cmd,
            "AT+HTTPPARA=\"URL\",\"%s\"",
            FIREBASE_URL);
    }
    else
    {
        sprintf(
            firebase_cmd,
            "AT+HTTPPARA=\"URL\",\"%s?auth=%s\"",
            FIREBASE_URL,
            FIREBASE_AUTH);
    }

    if(firebase_http_command(firebase_cmd,3000)!=ESP_OK)
    {
        modem_unlock();
        return ESP_FAIL;
    }

    /*
     * Content Type
     */

    if(firebase_http_command(
        "AT+HTTPPARA=\"CONTENT\",\"application/json\"",
        3000)!=ESP_OK)
    {
        modem_unlock();
        return ESP_FAIL;
    }

    /*
     * Tamaño del JSON: Sustuido por modem_send_raw()
     */

    /*
    * Solicitar modo HTTPDATA
    */

    sprintf(
        firebase_cmd,
        "AT+HTTPDATA=%d,10000",
        (int)strlen(json_buffer));

    ESP_LOGI(TAG,"%s",firebase_cmd);

    if(modem_send_raw(
            firebase_cmd,
            firebase_rx,
            sizeof(firebase_rx),
            15000)!=ESP_OK)
    {
        ESP_LOGE(TAG,"HTTPDATA command failed");

        firebase_http_command(
                "AT+HTTPTERM",
                3000);

        modem_unlock();
        return ESP_FAIL;
    }

    // Mostrar exactamente todo lo que respondió el módem

    ESP_LOGI(TAG,
            "HTTPDATA RAW RESPONSE:\n%s",
            firebase_rx);

    // Verificar manualmente si apareció DOWNLOAD

    if(strstr(firebase_rx,"DOWNLOAD")==NULL)
    {
        ESP_LOGE(TAG,"DOWNLOAD not found");

        firebase_http_command(
                "AT+HTTPTERM",
                3000);

        modem_unlock();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,"DOWNLOAD detected");

    /*
     * Enviar JSON
     */

    uart_write_bytes(
        MODEM_UART_NUM,
        json_buffer,
        strlen(json_buffer));

    uart_wait_tx_done(
        MODEM_UART_NUM,
        pdMS_TO_TICKS(3000));

    if(modem_wait_for(
        "OK",
        10000)!=ESP_OK)
    {
        ESP_LOGE(TAG,"JSON upload failed");

        firebase_http_command(
                "AT+HTTPTERM",
                3000);

        modem_unlock();
        return ESP_FAIL;
    } else {
        ESP_LOGI(TAG,"Payload accepted");
    }

    /*
     * Ejecutar HTTP PUT
     */

    sprintf(
        firebase_cmd,
        "AT+HTTPACTION=4");

    ESP_LOGI(TAG,"%s",firebase_cmd);
   
    if(modem_send_raw(
            firebase_cmd,
            firebase_rx,
            sizeof(firebase_rx),
            10000)!=ESP_OK)
    {
        ESP_LOGE(TAG,"%s command made no response", firebase_cmd);

        firebase_http_command(
                "AT+HTTPTERM",
                3000);

        modem_unlock();
        return ESP_FAIL;
    }
    
    // Mostrar exactamente todo lo que respondió el módem

    ESP_LOGI(TAG,
            "RAW RESPONSE:\n%s",
            firebase_rx);

    /*
     * Finalizar sesión
     */

    firebase_http_command(
            "AT+HTTPTERM",
            3000);

    ESP_LOGI(TAG,"Firebase upload finished");

    modem_unlock();
    return ESP_OK;

}
