

#ifndef BUOY_DATA_H
#define BUOY_DATA_H

#include <stdbool.h>
#include <stdint.h>

#define BUOY_ID_MAX_LEN 24

/*==============================================================
                Estado de un sensor tipo float
==============================================================*/

typedef struct
{
    float value;

    bool valid;

    uint32_t last_update_ms;

} sensor_float_t;

/*==============================================================
                Estado de un sensor tipo entero
==============================================================*/

typedef struct
{
    int value;

    bool valid;

    uint32_t last_update_ms;

} sensor_int_t;

/*==============================================================
                    Datos completos de la boya
==============================================================*/

typedef struct
{
    /* Tiempo del sistema */
    uint32_t timestamp;

    /* Identificador de la boya */
    char buoy_id[BUOY_ID_MAX_LEN];

    /* Sensores ambientales */
    sensor_float_t temperature;
    sensor_float_t tds;
    sensor_float_t salinity;

    /* Oleaje */
    sensor_float_t wave_height;
    sensor_float_t wave_period;

    /* GPS */
    sensor_float_t latitude;
    sensor_float_t longitude;
    sensor_float_t altitude;
    sensor_float_t speed;
    sensor_float_t accuracy;

    sensor_int_t satellites_visible;
    sensor_int_t satellites_used;
    sensor_int_t gps_fix_mode;

} buoy_data_t;


/*==============================================================
                    Variable global
==============================================================*/

extern buoy_data_t buoy_data;

#endif