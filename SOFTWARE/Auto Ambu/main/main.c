// --------------------- AUTO AMBU ------------------------
//      Codigo v2.1
//      Incluye calibracion HOME
// --------------------------------------------------------

#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/adc.h"

#include "esp_log.h"

#include "board.h"


// ============================================================
// TAG
// ============================================================

static const char *TAG = "AUTO_AMBU";


// ============================================================
// CONFIGURACION DEL PWM
// ============================================================

#define PWM_FREQUENCY_HZ       10000
#define PWM_RESOLUTION         LEDC_TIMER_10_BIT

#define PWM_MAX                1023


// ============================================================
// ESTADOS DEL SISTEMA
// ============================================================

typedef enum
{
    MODO_NINGUNO = 0,
    MODO_ADULTO,
    MODO_NINO,
    MODO_AJUSTABLE

} modo_t;


// ============================================================
// PROTOTIPOS
// ============================================================

static void motor_detener(void);
static void motor_comprimir(void);
static void motor_descomprimir(void);

static void motor_pwm_set(float porcentaje);

static modo_t leer_modo(void);
static float leer_frecuencia_ajustable(void);

static float leer_voltaje_bateria(void);

static bool calibrar_home(void);

static void ejecutar_ciclo(float frecuencia);


// ============================================================
// CONFIGURACION GPIO
// ============================================================

static void configurar_gpio(void)
{
    // --------------------------------------------------------
    // Selector
    // --------------------------------------------------------

    gpio_config_t selector_config = {
        .pin_bit_mask =
            (1ULL << PIN_MODO_ADULTO) |
            (1ULL << PIN_MODO_NINO) |
            (1ULL << PIN_MODO_AJUSTABLE),

        .mode = GPIO_MODE_INPUT,

        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&selector_config);


    // --------------------------------------------------------
    // Puente H
    // --------------------------------------------------------

    gpio_config_t motor_config = {
        .pin_bit_mask =
            (1ULL << PIN_MOTOR_INB1) |
            (1ULL << PIN_MOTOR_INB2),

        .mode = GPIO_MODE_OUTPUT,

        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&motor_config);


    // --------------------------------------------------------
    // Sensor HOME
    // Activo en LOW
    // --------------------------------------------------------

    gpio_config_t home_config = {
        .pin_bit_mask = (1ULL << PIN_SENSOR_HOME),

        .mode = GPIO_MODE_INPUT,

        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&home_config);


    // --------------------------------------------------------
    // LED
    // --------------------------------------------------------

    gpio_config_t led_config = {
        .pin_bit_mask = (1ULL << PIN_LED),

        .mode = GPIO_MODE_OUTPUT,

        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE
    };

    gpio_config(&led_config);


    // --------------------------------------------------------
    // Estado inicial seguro
    // --------------------------------------------------------

    gpio_set_level(PIN_MOTOR_INB1, 0);
    gpio_set_level(PIN_MOTOR_INB2, 0);

    // LED apagado
    gpio_set_level(PIN_LED, 1);
}


// ============================================================
// CONFIGURACION PWM
// ============================================================

static void configurar_pwm(void)
{
    ledc_timer_config_t timer_config = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };

    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));


    ledc_channel_config_t channel_config = {
        .gpio_num = PIN_MOTOR_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0
    };

    ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}


// ============================================================
// CONFIGURACION ADC
// ============================================================

static void configurar_adc(void)
{
    // GPIO36 VP = ADC1_CHANNEL_0

    adc1_config_width(ADC_WIDTH_BIT_12);

    adc1_config_channel_atten(
        ADC1_CHANNEL_0,
        ADC_ATTEN_DB_11
    );


    // GPIO39 VN = ADC1_CHANNEL_3

    adc1_config_channel_atten(
        ADC1_CHANNEL_3,
        ADC_ATTEN_DB_11
    );
}


// ============================================================
// CONTROL PWM
// ============================================================

static void motor_pwm_set(float porcentaje)
{
    if (porcentaje < 0.0f)
        porcentaje = 0.0f;

    if (porcentaje > 100.0f)
        porcentaje = 100.0f;


    uint32_t duty =
        (uint32_t)((porcentaje / 100.0f) * PWM_MAX);


    ledc_set_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0,
        duty
    );

    ledc_update_duty(
        LEDC_LOW_SPEED_MODE,
        LEDC_CHANNEL_0
    );
}


// ============================================================
// MOTOR DETENIDO
// ============================================================

static void motor_detener(void)
{
    motor_pwm_set(0.0f);

    gpio_set_level(PIN_MOTOR_INB1, 0);
    gpio_set_level(PIN_MOTOR_INB2, 0);
}


// ============================================================
// MOTOR EN DIRECCION DE COMPRESION
// ============================================================

static void motor_comprimir(void)
{
    /*
     * Direccion de compresion.
     */

    gpio_set_level(PIN_MOTOR_INB1, 1);
    gpio_set_level(PIN_MOTOR_INB2, 0);

    motor_pwm_set(PWM_MOTOR_INICIAL);
}


// ============================================================
// MOTOR EN DIRECCION DE DESCOMPRESION
// ============================================================

static void motor_descomprimir(void)
{
    /*
     * Direccion contraria a la compresion.
     *
     * Esta direccion se utiliza tambien para regresar
     * el mecanismo hasta el punto HOME.
     */

    gpio_set_level(PIN_MOTOR_INB1, 0);
    gpio_set_level(PIN_MOTOR_INB2, 1);

    motor_pwm_set(PWM_MOTOR_INICIAL);
}


// ============================================================
// CALIBRACION DEL PUNTO HOME
// ============================================================

static bool calibrar_home(void)
{
    ESP_LOGI(TAG, "======================================");
    ESP_LOGI(TAG, "       INICIANDO CALIBRACION HOME");
    ESP_LOGI(TAG, "======================================");


    // --------------------------------------------------------
    // Verificar si ya estamos en HOME
    // --------------------------------------------------------

    if (gpio_get_level(PIN_SENSOR_HOME) == 0)
    {
        ESP_LOGI(TAG, "HOME ya detectado");

        motor_detener();

        return true;
    }


    // --------------------------------------------------------
    // No estamos en HOME
    // Mover mecanismo hacia HOME
    // --------------------------------------------------------

    ESP_LOGI(
        TAG,
        "HOME no detectado - buscando posicion..."
    );

    motor_descomprimir();


    // --------------------------------------------------------
    // Buscar HOME
    // --------------------------------------------------------

    const int tiempo_maximo_ms =
        (int)(TIEMPO_MAX_HOME_S * 1000.0f);

    const int intervalo_ms = 10;

    int tiempo_transcurrido_ms = 0;


    while (tiempo_transcurrido_ms < tiempo_maximo_ms)
    {
        // ----------------------------------------------------
        // Sensor HOME activo
        // ----------------------------------------------------

        if (gpio_get_level(PIN_SENSOR_HOME) == 0)
        {
            motor_detener();

            ESP_LOGI(
                TAG,
                "HOME DETECTADO - Calibracion completada"
            );

            return true;
        }


        // ----------------------------------------------------
        // Esperar 10 ms antes de volver a revisar
        // ----------------------------------------------------

        vTaskDelay(pdMS_TO_TICKS(intervalo_ms));

        tiempo_transcurrido_ms += intervalo_ms;
    }


    // --------------------------------------------------------
    // HOME no encontrado
    // --------------------------------------------------------

    motor_detener();

    ESP_LOGE(
        TAG,
        "ERROR: HOME no detectado despues de %.1f segundos",
        TIEMPO_MAX_HOME_S
    );

    return false;
}


// ============================================================
// LEER SELECTOR
// ============================================================

static modo_t leer_modo(void)
{
    bool adulto =
        gpio_get_level(PIN_MODO_ADULTO);

    bool nino =
        gpio_get_level(PIN_MODO_NINO);

    bool ajustable =
        gpio_get_level(PIN_MODO_AJUSTABLE);


    if (!adulto)
        return MODO_ADULTO;

    if (!nino)
        return MODO_NINO;

    if (!ajustable)
        return MODO_AJUSTABLE;


    return MODO_NINGUNO;
}


// ============================================================
// LEER POTENCIOMETRO
// ============================================================

static float leer_frecuencia_ajustable(void)
{
    int raw = adc1_get_raw(ADC1_CHANNEL_0);


    float porcentaje =
        ((float)raw / 4095.0f) * 100.0f;


    float frecuencia =
        FRECUENCIA_MIN +
        (porcentaje / 100.0f) *
        (FRECUENCIA_MAX - FRECUENCIA_MIN);


    return frecuencia;
}


// ============================================================
// LEER VOLTAJE DE BATERIA
// ============================================================

static float leer_voltaje_bateria(void)
{
    int raw =
        adc1_get_raw(ADC1_CHANNEL_3);


    // 0 - 4095 ADC -> 0 - 16 V bateria

    float bateria =
        (((float)raw / 4095.0f) * 16.0f) - 0.3f;


    return bateria;
}


// ============================================================
// EJECUTAR UN CICLO COMPLETO
// ============================================================

static void ejecutar_ciclo(float frecuencia)
{
    if (frecuencia <= 0.0f)
        return;


    // --------------------------------------------------------
    // Duracion total deseada entre comienzos de compresiones
    // --------------------------------------------------------

    float periodo =
        60.0f / frecuencia;


    // --------------------------------------------------------
    // Tiempo utilizado por el mecanismo
    // --------------------------------------------------------

    float tiempo_mecanico =
        TIEMPO_COMPRESION_S +
        TIEMPO_MANTENIMIENTO_S +
        TIEMPO_DESCOMPRESION_S;


    // --------------------------------------------------------
    // Tiempo de espera restante
    // --------------------------------------------------------

    float tiempo_espera =
        periodo - tiempo_mecanico;


    if (tiempo_espera < 0.0f)
        tiempo_espera = 0.0f;


    // ========================================================
    // COMPRESION
    // ========================================================

    motor_comprimir();

    vTaskDelay(
        pdMS_TO_TICKS(
            TIEMPO_COMPRESION_S * 1000.0f
        )
    );


    // ========================================================
    // MANTENER AMBU COMPRIMIDO
    // ========================================================

    motor_detener();

    vTaskDelay(
        pdMS_TO_TICKS(
            TIEMPO_MANTENIMIENTO_S * 1000.0f
        )
    );


    // ========================================================
    // DESCOMPRESION
    // ========================================================

    motor_descomprimir();

    vTaskDelay(
        pdMS_TO_TICKS(
            TIEMPO_DESCOMPRESION_S * 1000.0f
        )
    );


    // ========================================================
    // DETENER
    // ========================================================

    motor_detener();


    // ========================================================
    // ESPERA HASTA EL SIGUIENTE CICLO
    // ========================================================

    if (tiempo_espera > 0.0f)
    {
        vTaskDelay(
            pdMS_TO_TICKS(
                tiempo_espera * 1000.0f
            )
        );
    }
}


// ============================================================
// MAIN
// ============================================================

void app_main(void)
{
    ESP_LOGI(TAG, "======================================");
    ESP_LOGI(TAG, "       AUTO AMBU - INICIO");
    ESP_LOGI(TAG, "======================================");


    // --------------------------------------------------------
    // Inicializar hardware
    // --------------------------------------------------------

    configurar_gpio();

    configurar_pwm();

    configurar_adc();


    // --------------------------------------------------------
    // Motor detenido al arrancar
    // --------------------------------------------------------

    motor_detener();


    // --------------------------------------------------------
    // CALIBRACION HOME
    // --------------------------------------------------------

    if (!calibrar_home())
    {
        ESP_LOGE(
            TAG,
            "CALIBRACION FALLIDA"
        );

        ESP_LOGE(
            TAG,
            "Sistema bloqueado por seguridad"
        );


        // ----------------------------------------------------
        // Si HOME no se encuentra, el sistema queda detenido
        // ----------------------------------------------------

        while (1)
        {
            motor_detener();

            // LED apagado
            gpio_set_level(PIN_LED, 1);

            vTaskDelay(
                pdMS_TO_TICKS(500)
            );
        }
    }


    // --------------------------------------------------------
    // HOME encontrado
    // Sistema listo
    // --------------------------------------------------------

    ESP_LOGI(TAG, "Sistema inicializado");
    ESP_LOGI(TAG, "HOME confirmado");
    ESP_LOGI(TAG, "Sistema listo para operar");


    // --------------------------------------------------------
    // Bucle principal
    // --------------------------------------------------------

    while (1)
    {
        modo_t modo =
            leer_modo();


        float frecuencia = 0.0f;


        // ====================================================
        // MODO ADULTO
        // ====================================================

        if (modo == MODO_ADULTO)
        {
            frecuencia =
                FRECUENCIA_ADULTO;


            // LED encendido

            gpio_set_level(PIN_LED, 0);


            ESP_LOGI(
                TAG,
                "Modo ADULTO - %.1f compresiones/min",
                frecuencia
            );
        }


        // ====================================================
        // MODO NINO
        // ====================================================

        else if (modo == MODO_NINO)
        {
            frecuencia =
                FRECUENCIA_NINO;


            // LED encendido

            gpio_set_level(PIN_LED, 0);


            ESP_LOGI(
                TAG,
                "Modo NINO - %.1f compresiones/min",
                frecuencia
            );
        }


        // ====================================================
        // MODO AJUSTABLE
        // ====================================================

        else if (modo == MODO_AJUSTABLE)
        {
            frecuencia =
                leer_frecuencia_ajustable();


            // LED encendido

            gpio_set_level(PIN_LED, 0);


            ESP_LOGI(
                TAG,
                "Modo AJUSTABLE - %.2f compresiones/min",
                frecuencia
            );
        }


        // ====================================================
        // NINGUN MODO
        // ====================================================

        else
        {
            motor_detener();

            gpio_set_level(PIN_LED, 1);


            ESP_LOGI(
                TAG,
                "Sin modo seleccionado - MOTOR DETENIDO"
            );


            vTaskDelay(
                pdMS_TO_TICKS(200)
            );

            continue;
        }


        // ====================================================
        // MOSTRAR BATERIA
        // ====================================================

        float bateria =
            leer_voltaje_bateria();


        ESP_LOGI(
            TAG,
            "Bateria: %.2f V",
            bateria
        );


        // ====================================================
        // EJECUTAR CICLO
        // ====================================================

        ejecutar_ciclo(frecuencia);
    }
}