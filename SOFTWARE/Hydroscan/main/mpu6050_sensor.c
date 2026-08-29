
/*
 * ============================================================
 *                      HYDROSCAN
 * ------------------------------------------------------------
 * Archivo      : mpu6050_sensor.c
 * Descripción  : Driver del sensor MPU6050
 * ============================================================
 */

#include "mpu6050_sensor.h"

#include <math.h>
#include <string.h>

#include "driver/i2c.h"
#include "esp_log.h"
#include "esp_err.h"

//#include "board.h"

/*==============================================================
                        CONFIGURACIÓN
==============================================================*/

static const char *TAG = "MPU6050";

/* Direccion I2C */

#define MPU6050_ADDR           0x68

/* Registros */

#define MPU_PWR_MGMT_1         0x6B
#define MPU_SMPLRT_DIV         0x19
#define MPU_CONFIG             0x1A
#define MPU_GYRO_CONFIG        0x1B
#define MPU_ACCEL_CONFIG       0x1C
#define MPU_ACCEL_XOUT_H       0x3B

/*==============================================================
                        CONFIGURACIÓN I2C
==============================================================*/

#define MPU6050_I2C_PORT      I2C_NUM_0
#define MPU6050_SDA_PIN       9
#define MPU6050_SCL_PIN       8
#define MPU6050_I2C_FREQ_HZ   100000


/*==============================================================
                        VARIABLES
==============================================================*/

static float gx_bias = 0.0f;
static float gy_bias = 0.0f;
static float gz_bias = 0.0f;

static float ax_offset = 0.0f;
static float ay_offset = 0.0f;
static float az_offset = 0.0f;

/*==============================================================
                    FUNCIONES PRIVADAS
==============================================================*/

static esp_err_t i2c_write_byte(uint8_t reg, uint8_t data)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);

    i2c_master_write_byte(cmd,
                          (MPU6050_ADDR << 1) | I2C_MASTER_WRITE,
                          true);

    i2c_master_write_byte(cmd, reg, true);

    i2c_master_write_byte(cmd, data, true);

    i2c_master_stop(cmd);

    esp_err_t ret =
        i2c_master_cmd_begin(MPU6050_I2C_PORT,
                             cmd,
                             pdMS_TO_TICKS(1000));

    i2c_cmd_link_delete(cmd);

    return ret;
}

/*------------------------------------------------------------*/

static esp_err_t i2c_read_bytes(uint8_t reg,
                                uint8_t *buffer,
                                size_t len)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();

    i2c_master_start(cmd);

    i2c_master_write_byte(cmd,
                          (MPU6050_ADDR << 1) | I2C_MASTER_WRITE,
                          true);

    i2c_master_write_byte(cmd, reg, true);

    i2c_master_start(cmd);

    i2c_master_write_byte(cmd,
                          (MPU6050_ADDR << 1) | I2C_MASTER_READ,
                          true);

    if (len > 1)
    {
        i2c_master_read(cmd,
                        buffer,
                        len - 1,
                        I2C_MASTER_ACK);
    }

    i2c_master_read_byte(cmd,
                         buffer + len - 1,
                         I2C_MASTER_NACK);

    i2c_master_stop(cmd);

    esp_err_t ret =
        i2c_master_cmd_begin(MPU6050_I2C_PORT,
                             cmd,
                             pdMS_TO_TICKS(1000));

    i2c_cmd_link_delete(cmd);

    return ret;
}

/*------------------------------------------------------------
                Inicialización del bus I2C
------------------------------------------------------------*/

static esp_err_t i2c_bus_init(void)
{
    ESP_LOGI(TAG, "Inicializando bus I2C...");

    i2c_config_t config =
    {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = MPU6050_SDA_PIN,
        .scl_io_num = MPU6050_SCL_PIN,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = MPU6050_I2C_FREQ_HZ,
        .clk_flags = 0
    };

    ESP_ERROR_CHECK(
        i2c_param_config(
            MPU6050_I2C_PORT,
            &config));

    esp_err_t ret =
        i2c_driver_install(
            MPU6050_I2C_PORT,
            config.mode,
            0,
            0,
            0);

    if (ret == ESP_ERR_INVALID_STATE)
    {
        ESP_LOGW(TAG, "Driver I2C ya estaba instalado.");
        return ESP_OK;
    }

    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG,
             "Bus I2C inicializado correctamente: SDA=%d SCL=%d",
             MPU6050_SDA_PIN,
             MPU6050_SCL_PIN);

    return ESP_OK;
}

/*==============================================================
                INICIALIZACIÓN DEL SENSOR
==============================================================*/

esp_err_t mpu6050_sensor_init(void)
{
    esp_err_t ret;

        ESP_ERROR_CHECK(
        i2c_bus_init());

    for(uint8_t addr=1; addr<127; addr++)
    {
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();

        i2c_master_start(cmd);

        i2c_master_write_byte(
            cmd,
            (addr << 1) | I2C_MASTER_WRITE,
            true);

        i2c_master_stop(cmd);

        esp_err_t ret =
            i2c_master_cmd_begin(
                MPU6050_I2C_PORT,
                cmd,
                pdMS_TO_TICKS(100));

        i2c_cmd_link_delete(cmd);

        if(ret == ESP_OK)
        {
            ESP_LOGI("I2C", "Device found: 0x%02X", addr);
        }
    }

    ret = i2c_write_byte(MPU_PWR_MGMT_1, 0x00);
    ESP_ERROR_CHECK(ret);

    vTaskDelay(pdMS_TO_TICKS(100));

    /* 100 Hz internos */

    ret = i2c_write_byte(MPU_SMPLRT_DIV, 99);
    ESP_ERROR_CHECK(ret);

    /* DLPF */

    ret = i2c_write_byte(MPU_CONFIG, 0x03);
    ESP_ERROR_CHECK(ret);

    /* ±250 °/s */

    ret = i2c_write_byte(MPU_GYRO_CONFIG, 0x00);
    ESP_ERROR_CHECK(ret);

    /* ±2 g */

    ret = i2c_write_byte(MPU_ACCEL_CONFIG, 0x00);
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, "MPU6050 inicializado");

    return ESP_OK;
}

/*==============================================================
                CALIBRACIÓN DEL GIROSCOPIO
==============================================================*/

esp_err_t mpu6050_calibrate_gyro(void)
{
    const int samples = 300;

    double sx = 0.0;
    double sy = 0.0;
    double sz = 0.0;

    double sax = 0.0;
    double say = 0.0;
    double saz = 0.0;

    mpu6050_raw_data_t raw;

    ESP_LOGI(TAG,
             "Calibrando giroscopio. Mantenga el sensor inmovil...");

    for(int i=0;i<samples;i++)
    {
        if(mpu6050_read_raw(&raw)==ESP_OK)
        {
            sx += raw.gx;
            sy += raw.gy;
            sz += raw.gz;

            sax += raw.ax;
            say += raw.ay;
            saz += raw.az;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    gx_bias =
        ((sx/samples)/GYRO_LSB_PER_DPS) *
        (float)M_PI/180.0f;

    gy_bias =
        ((sy/samples)/GYRO_LSB_PER_DPS) *
        (float)M_PI/180.0f;

    gz_bias =
        ((sz/samples)/GYRO_LSB_PER_DPS) *
        (float)M_PI/180.0f;

    ESP_LOGI(TAG,
             "Bias: %.6f %.6f %.6f rad/s",
             gx_bias,
             gy_bias,
             gz_bias);
    
    ESP_LOGI(TAG,
         "Accel RAW promedio:");

    ESP_LOGI(TAG,
         "AX = %.1f",
         sax / samples);

    ESP_LOGI(TAG,
         "AY = %.1f",
         say / samples);

    ESP_LOGI(TAG,
         "AZ = %.1f",
         saz / samples);

    return ESP_OK;
}

/*==============================================================
                CALIBRACIÓN DEL ACELERÓMETRO
==============================================================*/

esp_err_t mpu6050_calibrate_acc(void)
{
    const int samples = 300;

    double sx = 0.0;
    double sy = 0.0;
    double sz = 0.0;

    mpu6050_raw_data_t raw;

    ESP_LOGI(TAG,
             "Calibrando acelerometro. Mantenga el sensor inmovil...");

    for(int i = 0; i < samples; i++)
    {
        if(mpu6050_read_raw(&raw) == ESP_OK)
        {
            sx += raw.ax;
            sy += raw.ay;
            sz += raw.az;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }

    float ax_mean = sx / samples;
    float ay_mean = sy / samples;
    float az_mean = sz / samples;

    /*
     * En reposo esperamos:
     *
     * ax = 0
     * ay = 0
     * az = +1 g
     */

    ax_offset = ax_mean;

    ay_offset = ay_mean;

    az_offset = az_mean - ACC_LSB_PER_G;

    ESP_LOGI(TAG,
             "Offsets acelerometro:");

    ESP_LOGI(TAG,
             "AX = %.2f",
             ax_offset);

    ESP_LOGI(TAG,
             "AY = %.2f",
             ay_offset);

    ESP_LOGI(TAG,
             "AZ = %.2f",
             az_offset);

    return ESP_OK;
}

/*==============================================================
                    LECTURA CRUDA
==============================================================*/

esp_err_t mpu6050_read_raw(mpu6050_raw_data_t *raw)
{
    uint8_t buffer[14];

    esp_err_t ret =
        i2c_read_bytes(MPU_ACCEL_XOUT_H,
                       buffer,
                       sizeof(buffer));

    if(ret != ESP_OK)
        return ret;

    raw->ax =
        (int16_t)((buffer[0]<<8)|buffer[1]);

    raw->ay =
        (int16_t)((buffer[2]<<8)|buffer[3]);

    raw->az =
        (int16_t)((buffer[4]<<8)|buffer[5]);

    raw->temperature =
        (int16_t)((buffer[6]<<8)|buffer[7]);

    raw->gx =
        (int16_t)((buffer[8]<<8)|buffer[9]);

    raw->gy =
        (int16_t)((buffer[10]<<8)|buffer[11]);

    raw->gz =
        (int16_t)((buffer[12]<<8)|buffer[13]);

    return ESP_OK;
}

/*==============================================================
            CONVERSIÓN A UNIDADES FÍSICAS
==============================================================*/

esp_err_t mpu6050_read(mpu6050_data_t *imu)
{
    mpu6050_raw_data_t raw;

    esp_err_t ret =
        mpu6050_read_raw(&raw);

    if(ret != ESP_OK)
        return ret;

    imu->ax =
        (((float)raw.ax - ax_offset) / ACC_LSB_PER_G) * G0;

    imu->ay =
        (((float)raw.ay - ay_offset) / ACC_LSB_PER_G) * G0;

    imu->az =
        (((float)raw.az - az_offset) / ACC_LSB_PER_G) * G0;

    imu->gx =
        (((float)raw.gx/GYRO_LSB_PER_DPS)*
        (float)M_PI/180.0f)
        - gx_bias;

    imu->gy =
        (((float)raw.gy/GYRO_LSB_PER_DPS)*
        (float)M_PI/180.0f)
        - gy_bias;

    imu->gz =
        (((float)raw.gz/GYRO_LSB_PER_DPS)*
        (float)M_PI/180.0f)
        - gz_bias;

    imu->temperature =
        (raw.temperature/340.0f)+36.53f;

    return ESP_OK;
}