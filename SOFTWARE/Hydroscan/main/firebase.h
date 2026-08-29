
#ifndef FIREBASE_H
#define FIREBASE_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/*==============================================================
                    Configuración
==============================================================*/

#define FIREBASE_SEND_PERIOD_MS    (120000UL)

/*==============================================================
                    API pública
==============================================================*/

esp_err_t firebase_init(void);

esp_err_t firebase_send(void);

#ifdef __cplusplus
}
#endif

#endif