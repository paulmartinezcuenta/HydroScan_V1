
#ifndef MODEM_H
#define MODEM_H

#include <stdbool.h>
#include "esp_err.h"

esp_err_t modem_init(void);

bool modem_is_ready(void);

int modem_read_response(char *buffer,
                        size_t max_len,
                        uint32_t timeout_ms);

esp_err_t modem_send_at(const char *cmd);

/*==============================================================
                    Utilidades AT
==============================================================*/

esp_err_t modem_wait_for(
        const char *expected,
        uint32_t timeout_ms);

esp_err_t modem_send_wait(
        const char *cmd,
        const char *expected,
        uint32_t timeout_ms);

void modem_flush_uart(void);

bool modem_response_contains(
        const char *buffer,
        const char *text);

        
esp_err_t modem_set_apn(const char *apn);

esp_err_t modem_activate_pdp(void);

esp_err_t modem_get_ip(char *ip, size_t len);

bool modem_has_ip(void);

esp_err_t modem_send_raw(
        const char *cmd,
        char *response,
        size_t response_size,
        uint32_t timeout_ms);

//==============================================================
//                  Funciones de bloqueo

bool modem_lock(uint32_t timeout_ms);

void modem_unlock(void);

#endif