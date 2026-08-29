
/*
 * ============================================================
 *                      HYDROSCAN
 * ------------------------------------------------------------
 * Archivo      : modem.c
 * Descripción  : Modem de comunicacion A7608 del modulo LILYGO
 *
 * Autor        : Hydroscan Project
 * ============================================================
 */

#include "modem.h"
#include "utilities.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include "esp_log.h"
#include "esp_timer.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <string.h>

#include "freertos/semphr.h"

static SemaphoreHandle_t modem_mutex = NULL;

static const char *TAG = "MODEM";

static bool modem_ready = false;

#define MODEM_RESPONSE_BUFFER_SIZE    1024

static char modem_response[MODEM_RESPONSE_BUFFER_SIZE];

static char modem_ip[32];

#ifndef NETWORK_APN
#define NETWORK_APN "altice"
#endif

/*==============================================================
                        Inicializar UART
==============================================================*/

static esp_err_t modem_uart_init(void)
{
    uart_config_t uart_config =
    {
        .baud_rate = MODEM_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT
    };

    ESP_ERROR_CHECK(
        uart_driver_install(
            MODEM_UART_NUM,
            4096,
            4096,
            0,
            NULL,
            0));

    ESP_ERROR_CHECK(
        uart_param_config(
            MODEM_UART_NUM,
            &uart_config));

    ESP_ERROR_CHECK(
        uart_set_pin(
            MODEM_UART_NUM,
            MODEM_TX_PIN,
            MODEM_RX_PIN,
            UART_PIN_NO_CHANGE,
            UART_PIN_NO_CHANGE));

    return ESP_OK;
}

/*==============================================================
                        LIMPIAR UART
==============================================================*/

void modem_flush_uart(void)
{
    uint8_t dummy[64];

    while (uart_read_bytes(
                MODEM_UART_NUM,
                dummy,
                sizeof(dummy),
                pdMS_TO_TICKS(10)) > 0)
    {
        ;
    }
}

/*==============================================================
                ENVIAR COMANDO Y ESPERAR RESPUESTA
==============================================================*/

bool modem_response_contains(
        const char *buffer,
        const char *text)
{
    if(buffer == NULL || text == NULL)
        return false;

    return strstr(buffer, text) != NULL;
}

/*==============================================================
                ENVIAR COMANDO Y ESPERAR RESPUESTA
==============================================================*/

esp_err_t modem_wait_for(
        const char *expected,
        uint32_t timeout_ms)
{
    int64_t start = esp_timer_get_time() / 1000;

    modem_response[0] = 0;

    while ((esp_timer_get_time()/1000 - start) < timeout_ms)
    {
        int len =
            modem_read_response(
                modem_response,
                sizeof(modem_response),
                500);

        if(len <= 0)
            continue;

        ESP_LOGI(TAG,"%s",modem_response);

        if(strstr(modem_response, expected))
            return ESP_OK;

        /* DOWNLOAD es una respuesta parcial válida */
        if (strstr(modem_response, "DOWNLOAD"))
        {
            if (strcmp(expected, "DOWNLOAD") == 0)
                return ESP_OK;
        }

        if(strstr(modem_response,"ERROR"))
        {
            ESP_LOGW(TAG,"MODEM ERROR: %s",modem_response);

            return ESP_FAIL;
        }
    }

    return ESP_ERR_TIMEOUT;
}

esp_err_t modem_send_wait(
        const char *cmd,
        const char *expected,
        uint32_t timeout_ms)
{
    modem_flush_uart();

    modem_send_at(cmd);

    return modem_wait_for(
                expected,
                timeout_ms);
}

/*==============================================================
                        APN 
==============================================================*/

esp_err_t modem_set_apn(const char *apn)
{
    char cmd[128];

    snprintf(cmd,
            sizeof(cmd),
            "AT+CGDCONT=1,\"IP\",\"%s\"",
            apn);

    if(modem_send_wait(cmd,"OK",5000)!=ESP_OK)
    {
        ESP_LOGE(TAG,"Cannot configure PDP context");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG,"APN configured");

    return ESP_OK;

    return modem_send_wait(cmd,"OK",5000);
}

/*==============================================================
                        ENCENDER EL MODEM
==============================================================*/

static void modem_power_on(void)
{
    gpio_config_t io =
    {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask =
            (1ULL<<BOARD_PWRKEY_PIN) |
            (1ULL<<MODEM_RESET_PIN) |
            (1ULL<<MODEM_DTR_PIN)
    };

    gpio_config(&io);

    gpio_set_level(MODEM_DTR_PIN,0);

    gpio_set_level(MODEM_RESET_PIN,!MODEM_RESET_LEVEL);
    vTaskDelay(pdMS_TO_TICKS(100));

    gpio_set_level(MODEM_RESET_PIN,MODEM_RESET_LEVEL);
    vTaskDelay(pdMS_TO_TICKS(2600));

    gpio_set_level(MODEM_RESET_PIN,!MODEM_RESET_LEVEL);

    gpio_set_level(BOARD_PWRKEY_PIN,0);
    vTaskDelay(pdMS_TO_TICKS(100));

    gpio_set_level(BOARD_PWRKEY_PIN,1);
    vTaskDelay(pdMS_TO_TICKS(MODEM_POWERON_PULSE_WIDTH_MS));

    gpio_set_level(BOARD_PWRKEY_PIN,0);

    ESP_LOGI(TAG,"Power sequence complete");
}

/*==============================================================
                        ENVIAR COMANDO AT
==============================================================*/

 esp_err_t modem_send_at(const char *cmd)
{
    modem_flush_uart();

    uart_write_bytes(
        MODEM_UART_NUM,
        cmd,
        strlen(cmd));

    uart_write_bytes(
        MODEM_UART_NUM,
        "\r\n",
        2);

    return ESP_OK;
}

/*==============================================================
                        LEER RESPUESTA
==============================================================*/

static bool modem_wait_response(
    const char *expected,
    uint32_t timeout_ms)
{
    char buffer[512];

    int len =
        uart_read_bytes(
            MODEM_UART_NUM,
            (uint8_t *)buffer,
            sizeof(buffer)-1,
            pdMS_TO_TICKS(timeout_ms));

    if(len<=0)
        return false;

    buffer[len]=0;

    ESP_LOGI(TAG,"%s",buffer);

    return strstr(buffer,expected)!=NULL;
}

/*==============================================================
                    VERIFICAR COMUNICACION AT
==============================================================*/

static bool modem_test_at(void)
{
    modem_send_at("AT");

    return modem_wait_response("OK",1000);
}

/*==============================================================
                    ESPERAR QUE EL MODEM RESPONDA
==============================================================*/

static bool modem_wait_ready(void)
{
    int retry=0;

    while(!modem_test_at())
    {
        ESP_LOGI(TAG,"Waiting modem...");

        retry++;

        if(retry>30)
        {
            modem_power_on();
            retry=0;
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return true;
}

/*==============================================================
                    VERIFICAR SIM
==============================================================*/

static bool modem_wait_sim(void)
{
    while(true)
    {
        modem_send_at("AT+CPIN?");

        if(modem_wait_response("READY",1000))
        {
            ESP_LOGI(TAG,"SIM READY");
            return true;
        }

        ESP_LOGI(TAG,"Waiting SIM...");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*==============================================================
                    REGISTRO EN RED
==============================================================*/

static bool modem_wait_network(void)    // Actualizado
{
    while(true)
    {
        modem_send_at("AT+CEREG?");

        if(modem_read_response(modem_response,
                               sizeof(modem_response),
                               3000) <= 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        ESP_LOGI(TAG,"%s", modem_response);

        if(strstr(modem_response,",1"))
            return true;

        if(strstr(modem_response,",5"))
            return true;

        ESP_LOGI(TAG,"Waiting LTE registration...");

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

/*==============================================================
                        ACTIVAR LTE
==============================================================*/

esp_err_t modem_activate_pdp(void)
{
    if(modem_send_wait("AT+CGATT=1","OK",10000)!=ESP_OK)
        return ESP_FAIL;

    if(modem_send_wait("AT+CGACT=1,1","OK",10000)!=ESP_OK)
        return ESP_FAIL;

    return ESP_OK;
}

/*==============================================================
                        OBTENER IP
==============================================================*/

esp_err_t modem_get_ip(char *ip,size_t len)
{
    char rx[256];

    modem_send_at("AT+CGPADDR=1");

    if(modem_read_response(rx,sizeof(rx),3000)<=0)
        return ESP_FAIL;

    ESP_LOGI(TAG,"%s",rx);

    char temp[32];

    if(sscanf(rx,
              "%*[^:]: 1,%31s",
              temp)!=1)
        return ESP_FAIL;

    strncpy(ip,temp,len);

    return ESP_OK;
}

bool modem_has_ip(void)
{
    modem_ip[0]=0;

    if(modem_get_ip(modem_ip,sizeof(modem_ip))!=ESP_OK)
        return false;

    if(strlen(modem_ip)==0)
        return false;

    if(strcmp(modem_ip,"0.0.0.0")==0)
        return false;

    ESP_LOGI(TAG,"IP: %s",modem_ip);

    return true;
}

/*==============================================================
                    CALIDAD SEÑAL LTE
==============================================================*/

static esp_err_t modem_check_signal(void)
{
    modem_send_at("AT+CSQ");

    if(modem_read_response(modem_response,
                           sizeof(modem_response),
                           3000) <= 0)
        return ESP_FAIL;

    ESP_LOGI(TAG,"%s", modem_response);

    return ESP_OK;
}

/*==============================================================
                    VERIFICAR LTE
==============================================================*/

static esp_err_t modem_check_radio(void)
{
    modem_send_at("AT+CPSI?");

    if(modem_read_response(modem_response,
                           sizeof(modem_response),
                           5000) <= 0)
        return ESP_FAIL;

    ESP_LOGI(TAG,"%s", modem_response);

    return ESP_OK;
}

static esp_err_t modem_ipaddr(void)
{
    modem_send_at("AT+IPADDR");

    if(modem_read_response(modem_response,
                           sizeof(modem_response),
                           5000) <= 0)
        return ESP_FAIL;

    ESP_LOGI(TAG,"%s", modem_response);

    return ESP_OK;
}

static esp_err_t modem_ping(void)   // No usada por ahora
{
    modem_send_at("AT+CPING=\"8.8.8.8\"");

    if(modem_wait_for("OK",15000)!=ESP_OK)
        return ESP_FAIL;

    return ESP_OK;
}

/*==============================================================
                    BLOQUEO DEL MODEM
==============================================================*/

bool modem_lock(uint32_t timeout_ms)
{
    if(modem_mutex == NULL)
        return false;

    return xSemaphoreTake(
            modem_mutex,
            pdMS_TO_TICKS(timeout_ms)) == pdTRUE;
}

void modem_unlock(void)
{
    if(modem_mutex)
        xSemaphoreGive(modem_mutex);
}

/*==============================================================
                        PARA LEER
==============================================================*/

int modem_read_response(char *buffer,
                        size_t max_len,
                        uint32_t timeout_ms)
{
    size_t index = 0;
    uint8_t c;

    int64_t start = esp_timer_get_time() / 1000;

    while ((esp_timer_get_time()/1000 - start) < timeout_ms)
    {
        int len = uart_read_bytes(
                    MODEM_UART_NUM,
                    &c,
                    1,
                    pdMS_TO_TICKS(20));

        if(len > 0)
        {
            if(index < max_len - 1)
                buffer[index++] = c;

            buffer[index] = '\0';

            if(strstr(buffer, "\r\nOK\r\n"))
                return index;

            if(strstr(buffer, "\r\nERROR\r\n"))
                return index;

            if(strstr(buffer, "+CME ERROR"))
                return index;
        }
    }

    return -1;
}

/*==============================================================
                    FUNCION NETOPEN
==============================================================*/

static esp_err_t modem_netopen(void) // version 4
{
    modem_flush_uart();

    modem_send_raw(
        "AT+NETOPEN",
        modem_response,
        sizeof(modem_response),
        15000);

    ESP_LOGI(TAG,"%s",modem_response);

    return ESP_OK;
} 

/*==============================================================
                    FUNCION PRINCIPAL MODEM
==============================================================*/

esp_err_t modem_init(void)
{
    modem_ready=false;

    modem_mutex = xSemaphoreCreateMutex();

    if(modem_mutex == NULL)
    {
        return ESP_FAIL;
    }

    modem_uart_init();

    modem_power_on();

    if(!modem_wait_ready())
        return ESP_FAIL;

    if(!modem_wait_sim())
        return ESP_FAIL;

    if(modem_check_signal()!=ESP_OK)
        return ESP_FAIL;

    if(modem_check_radio()!=ESP_OK)
        return ESP_FAIL;

    if(!modem_wait_network())
        return ESP_FAIL;

    if(modem_set_apn(NETWORK_APN)!=ESP_OK)
        return ESP_FAIL;

    if(modem_activate_pdp()!=ESP_OK)
        return ESP_FAIL;

    if(!modem_has_ip())
        return ESP_FAIL;
    
    if(modem_netopen()!=ESP_OK)     // El modem nunca devuelve +NETOPEN:0
        return ESP_FAIL; 

    if(modem_ipaddr()!=ESP_OK)
        return ESP_FAIL;  

    //if(modem_ping()!=ESP_OK)
      //  return ESP_FAIL; 

    modem_ready = true;

    ESP_LOGI(TAG,"MODEM READY");

    return ESP_OK;
}

/*==============================================================
                    ESTADO
==============================================================*/

bool modem_is_ready(void)
{
    return modem_ready;
}

/*==============================================================
            ENVIAR COMANDO RAW (DEPURACION)
==============================================================*/

int modem_read_all(
        char *buffer,
        size_t max_len,
        uint32_t timeout_ms)
{
    size_t index = 0;
    uint8_t c;

    int64_t start = esp_timer_get_time()/1000;

    while((esp_timer_get_time()/1000-start) < timeout_ms)
    {
        int len = uart_read_bytes(
                    MODEM_UART_NUM,
                    &c,
                    1,
                    pdMS_TO_TICKS(20));

        if(len>0)
        {
            if(index < max_len-1)
                buffer[index++] = c;

            buffer[index]=0;
        }
    }

    return index;
}

esp_err_t modem_send_raw(
        const char *cmd,
        char *response,
        size_t response_size,
        uint32_t timeout_ms)
{
    if(response == NULL || response_size == 0)
        return ESP_ERR_INVALID_ARG;

    modem_send_at(cmd);

    int len = modem_read_all(
                    response,
                    response_size,
                    timeout_ms);

    modem_unlock();

    if(len <= 0)
        return ESP_ERR_TIMEOUT;

    ESP_LOGI(TAG,
             "RAW RESPONSE:\n%s",
             response);

    return ESP_OK;
}