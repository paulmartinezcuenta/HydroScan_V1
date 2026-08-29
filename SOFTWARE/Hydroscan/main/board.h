
#ifndef BOARD_H
#define BOARD_H

/*
 * ============================================================
 *                HYDROSCAN - CONFIGURACIÓN DE HARDWARE
 * ============================================================
 *
 * Cambiar únicamente este archivo cuando se migre
 * del ESP32 DevKit al LILYGO T-SIM7670G-S3.
 *
 */

/**********************
 * GPIO DS18B20
 **********************/
#define PIN_DS18B20           2

/**********************
 * GPIO Sensor TDS
 **********************/
#define PIN_TDS_ADC           10

/**********************
 * LED de estado
 **********************/
#define PIN_STATUS_LED        

/**********************
 * MPU6050 (I2C)
 **********************/
#define PIN_I2C_SDA           9
#define PIN_I2C_SCL           8
#define I2C_PORT             I2C_NUM_0
#define I2C_FREQ_HZ          400000      // 400 kHz

#endif