

// --------------------- AUTO AMBU ------------------------
//      Codigo v3.0
//
//      Maquina de estados
//      Sensor laser para posicion HOME
//
//      Selector:
//      OFF - INFANTE - ADULTO - AJUSTABLE - OFF
//
//      Selector activo LOW con pull-up
// --------------------------------------------------------

#include <stdio.h>
#include <stdbool.h>
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
// CONFIGURACION PWM
// ============================================================

#define PWM_FREQUENCY_HZ       10000
#define PWM_RESOLUTION         LEDC_TIMER_10_BIT
#define PWM_MAX                1023


// ============================================================
// TIEMPOS DE DESCOMPRESION
// ============================================================

// Tiempo maximo permitido para que el sensor detecte HOME
// durante una descompresion normal.
#define TIEMPO_DESCOMPRESION_MAX_S       0.3f

// Tiempo maximo permitido para recuperar HOME durante
// el estado RECUPERACION_HOME.
#define TIEMPO_RECUPERACION_HOME_MAX_S   0.02f

// Tiempo maximo permitido para buscar HOME durante
// el arranque del sistema.
#define TIEMPO_INICIO_HOME_MAX_S         0.50f


// ============================================================
// TIEMPO DE COMPROBACION
// ============================================================

// Periodo de actualizacion de la maquina de estados.
#define ESTADO_TICK_MS                   10


// ============================================================
// ESTADOS DE LA MAQUINA
// ============================================================

typedef enum
{
    ESTADO_INICIO = 0,

    ESTADO_OFF,

    ESTADO_INFANTE,

    ESTADO_ADULTO,

    ESTADO_AJUSTABLE,

    ESTADO_RECUPERACION_HOME,

    ESTADO_ERROR

} estado_t;


// ============================================================
// VARIABLES GLOBALES
// ============================================================

static estado_t estado_actual = ESTADO_INICIO;


// ============================================================
// PROTOTIPOS
// ============================================================

// ---------------- Motor ----------------

static void motor_detener(void);

static void motor_comprimir(void);

static void motor_descomprimir(void);

static void motor_pwm_set(float porcentaje);


// ---------------- Selector ----------------

static void leer_selector(
    bool *adulto,
    bool *infante,
    bool *ajustable
);

static bool selector_off(void);

static bool selector_valido(void);


// ---------------- Sensores ----------------

static bool sensor_home(void);

static float leer_frecuencia_ajustable(void);

static float leer_voltaje_bateria(void);


// ---------------- Ciclo ----------------

static bool ejecutar_compresion(void);

static bool ejecutar_descompresion(void);

static bool ejecutar_ciclo(float frecuencia);


// ---------------- Estados ----------------

static void entrar_estado(estado_t nuevo_estado);

static bool buscar_home_inicio(void);


// ============================================================
// CONFIGURACION GPIO
// ============================================================

static void configurar_gpio(void)
{
    // --------------------------------------------------------
    // SELECTOR
    // Activo LOW + pull-up
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

    ESP_ERROR_CHECK(
        gpio_config(&selector_config)
    );


    // --------------------------------------------------------
    // PUENTE H
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

    ESP_ERROR_CHECK(
        gpio_config(&motor_config)
    );


    // --------------------------------------------------------
    // SENSOR LASER
    // Activo LOW + pull-up
    // --------------------------------------------------------

    gpio_config_t laser_config = {
        .pin_bit_mask =
            (1ULL << PIN_SENSOR_LASER),

        .mode = GPIO_MODE_INPUT,

        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(
        gpio_config(&laser_config)
    );


    // --------------------------------------------------------
    // LED
    // --------------------------------------------------------

    gpio_config_t led_config = {
        .pin_bit_mask =
            (1ULL << PIN_LED),

        .mode = GPIO_MODE_OUTPUT,

        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,

        .intr_type = GPIO_INTR_DISABLE
    };

    ESP_ERROR_CHECK(
        gpio_config(&led_config)
    );


    // --------------------------------------------------------
    // ESTADO INICIAL SEGURO
    // --------------------------------------------------------

    gpio_set_level(
        PIN_MOTOR_INB1,
        0
    );

    gpio_set_level(
        PIN_MOTOR_INB2,
        0
    );

    gpio_set_level(
        PIN_LED,
        1
    );
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

    ESP_ERROR_CHECK(
        ledc_timer_config(&timer_config)
    );


    ledc_channel_config_t channel_config = {

        .gpio_num = PIN_MOTOR_PWM,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0

    };

    ESP_ERROR_CHECK(
        ledc_channel_config(&channel_config)
    );
}


// ============================================================
// CONFIGURACION ADC
// ============================================================

static void configurar_adc(void)
{
    // --------------------------------------------------------
    // ADC1
    // --------------------------------------------------------

    adc1_config_width(
        ADC_WIDTH_BIT_12
    );


    // --------------------------------------------------------
    // GPIO36 / VP
    // ADC1_CHANNEL_0
    // POTENCIOMETRO
    // --------------------------------------------------------

    ESP_ERROR_CHECK(
        adc1_config_channel_atten(
            ADC1_CHANNEL_0,
            ADC_ATTEN_DB_11
        )
    );


    // --------------------------------------------------------
    // GPIO39 / VN
    // ADC1_CHANNEL_3
    // SENSOR DE BATERIA
    // --------------------------------------------------------

    ESP_ERROR_CHECK(
        adc1_config_channel_atten(
            ADC1_CHANNEL_3,
            ADC_ATTEN_DB_11
        )
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
     */

    gpio_set_level(PIN_MOTOR_INB1, 0);
    gpio_set_level(PIN_MOTOR_INB2, 1);

    motor_pwm_set(PWM_MOTOR_INICIAL);
}

// ============================================================
// LEER SELECTOR
// ============================================================

static void leer_selector(
    bool *adulto,
    bool *infante,
    bool *ajustable
)
{
    /*
     * Selector activo LOW con pull-up:
     *
     * LOW  = seleccionado
     * HIGH = no seleccionado
     */

    *adulto =
        (gpio_get_level(PIN_MODO_ADULTO) == 0);

    *infante =
        (gpio_get_level(PIN_MODO_NINO) == 0);

    *ajustable =
        (gpio_get_level(PIN_MODO_AJUSTABLE) == 0);
}

// ============================================================
// SELECTOR EN OFF
// ============================================================

static bool selector_off(void)
{
    bool adulto;
    bool infante;
    bool ajustable;


    leer_selector(
        &adulto,
        &infante,
        &ajustable
    );


    return
        !adulto &&
        !infante &&
        !ajustable;
}


// ============================================================
// SELECTOR VALIDO
// ============================================================

static bool selector_valido(void)
{
    bool adulto;
    bool infante;
    bool ajustable;


    leer_selector(
        &adulto,
        &infante,
        &ajustable
    );


    int seleccionados =
        (adulto ? 1 : 0) +
        (infante ? 1 : 0) +
        (ajustable ? 1 : 0);


    return seleccionados <= 1;
}


// ============================================================
// SENSOR HOME
// ============================================================

static bool sensor_home(void)
{
    /*
     * Sensor laser activo LOW.
     *
     * LOW  = brazo en HOME
     * HIGH = brazo fuera de HOME
     */

    return (
        gpio_get_level(PIN_SENSOR_LASER) == 0
    );
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
        adc1_get_raw(
            ADC1_CHANNEL_3
        );


    /*
     * Conversion actual:
     *
     * 0 - 4095 ADC
     *        ↓
     * 0 - 16 V bateria
     *
     * Se conserva la correccion de -0.3 V
     * utilizada en la version anterior.
     */

    float bateria =
        (
            ((float)raw / 4095.0f)
            * 16.0f
        )
        - 0.3f;


    return bateria;
}


// ============================================================
// EJECUTAR COMPRESION
// ============================================================

static bool ejecutar_compresion(void)
{
    /*
     * La compresion se controla por tiempo.
     *
     * El sensor HOME NO interviene durante
     * esta fase.
     */

    motor_comprimir();


    vTaskDelay(
        pdMS_TO_TICKS(
            TIEMPO_COMPRESION_S * 1000.0f
        )
    );


    motor_detener();


    return true;
}


// ============================================================
// EJECUTAR DESCOMPRESION
// ============================================================

static bool ejecutar_descompresion(void)
{
    /*
     * La descompresion comienza normalmente.
     *
     * El sensor laser determina cuando se
     * alcanza HOME.
     *
     * TIEMPO_DESCOMPRESION_MAX_S es el
     * timeout de seguridad.
     */

    TickType_t inicio =
        xTaskGetTickCount();


    TickType_t timeout =
        pdMS_TO_TICKS(
            TIEMPO_DESCOMPRESION_MAX_S
            * 1000.0f
        );


    motor_descomprimir();


    while (1)
    {
        // ----------------------------------------------------
        // HOME detectado
        // ----------------------------------------------------

        if (sensor_home())
        {
            motor_detener();

            ESP_LOGI(
                TAG,
                "HOME detectado durante descompresion"
            );

            return true;
        }


        // ----------------------------------------------------
        // Timeout
        // ----------------------------------------------------

        TickType_t transcurrido =
            xTaskGetTickCount() - inicio;


        if (transcurrido >= timeout)
        {
            motor_detener();

            ESP_LOGE(
                TAG,
                "ERROR: Timeout de descompresion, HOME no detectado"
            );

            return false;
        }


        vTaskDelay(
            pdMS_TO_TICKS(ESTADO_TICK_MS)
        );
    }
}


// ============================================================
// EJECUTAR CICLO COMPLETO
// ============================================================

static bool ejecutar_ciclo(float frecuencia)
{
    if (frecuencia <= 0.0f)
        return false;

    /*
     * Periodo total entre comienzos de
     * compresiones.
     */

    float periodo =
        60.0f / frecuencia;


    /*
     * Tiempo utilizado por compresion
     * y mantenimiento.
     *
     * La descompresion NO se suma aqui
     * como tiempo fijo porque ahora termina
     * cuando el sensor detecta HOME.
     */

    float tiempo_espera =
        periodo -
        TIEMPO_COMPRESION_S -
        TIEMPO_MANTENIMIENTO_S;


    if (tiempo_espera < 0.0f)
        tiempo_espera = 0.0f;


    // --------------------------------------------------------
    // COMPRESION
    // --------------------------------------------------------

    ejecutar_compresion();

    // --------------------------------------------------------
    // MANTENIMIENTO
    // --------------------------------------------------------

    motor_detener();

    if (TIEMPO_MANTENIMIENTO_S > 0.0f)
    {
        vTaskDelay(
            pdMS_TO_TICKS(
                TIEMPO_MANTENIMIENTO_S * 1000.0f
            )
        );
    }

    // --------------------------------------------------------
    // DESCOMPRESION
    // --------------------------------------------------------

    if (!ejecutar_descompresion())
    {
        return false;
    }

    // --------------------------------------------------------
    // ESPERA
    // --------------------------------------------------------

    /*
     * El tiempo de espera se calcula desde
     * el inicio del ciclo.
     *
     * Para mantener la frecuencia exacta,
     * necesitamos considerar tambien el tiempo
     * real utilizado por la descompresion.
     *
     * En esta primera implementacion se utiliza
     * el tiempo restante despues de compresion
     * y mantenimiento.
     *
     * Esto sera refinado posteriormente con
     * medicion precisa del ciclo.
     */

    if (tiempo_espera > 0.0f)
    {
        vTaskDelay(
            pdMS_TO_TICKS(
                tiempo_espera * 1000.0f
            )
        );
    }

    return true;
}


// ============================================================
// CAMBIO DE ESTADO
// ============================================================

static void entrar_estado(estado_t nuevo_estado)
{
    if (estado_actual == nuevo_estado)
        return;

    estado_actual = nuevo_estado;

    switch (estado_actual)
    {
        case ESTADO_INICIO:

            ESP_LOGI(
                TAG,
                "ESTADO -> INICIO"
            );

            break;


        case ESTADO_OFF:

            ESP_LOGI(
                TAG,
                "ESTADO -> OFF"
            );

            motor_detener();

            gpio_set_level(
                PIN_LED,
                1
            );

            break;


        case ESTADO_INFANTE:

            ESP_LOGI(
                TAG,
                "ESTADO -> INFANTE"
            );

            gpio_set_level(
                PIN_LED,
                0
            );

            break;


        case ESTADO_ADULTO:

            ESP_LOGI(
                TAG,
                "ESTADO -> ADULTO"
            );

            gpio_set_level(
                PIN_LED,
                0
            );

            break;


        case ESTADO_AJUSTABLE:

            ESP_LOGI(
                TAG,
                "ESTADO -> AJUSTABLE"
            );

            gpio_set_level(
                PIN_LED,
                0
            );

            break;


        case ESTADO_RECUPERACION_HOME:

            ESP_LOGW(
                TAG,
                "ESTADO -> RECUPERACION_HOME"
            );

            gpio_set_level(
                PIN_LED,
                0
            );

            break;


        case ESTADO_ERROR:

            ESP_LOGE(
                TAG,
                "ESTADO -> ERROR"
            );

            motor_detener();

            gpio_set_level(
                PIN_LED,
                1
            );

            break;
    }
}


// ============================================================
// RECUPERACION DE HOME
// ============================================================

static bool recuperar_home(void)
{
    ESP_LOGW(
        TAG,
        "Iniciando recuperacion de HOME"
    );


    // --------------------------------------------------------
    // Si ya estamos en HOME no mover el motor
    // --------------------------------------------------------

    if (sensor_home())
    {
        motor_detener();

        ESP_LOGI(
            TAG,
            "Brazo ya se encuentra en HOME"
        );

        return true;
    }


    // --------------------------------------------------------
    // Iniciar descompresion
    // --------------------------------------------------------

    motor_descomprimir();


    TickType_t inicio =
        xTaskGetTickCount();


    TickType_t timeout =
        pdMS_TO_TICKS(
            TIEMPO_RECUPERACION_HOME_MAX_S
            * 1000.0f
        );


    while (1)
    {
        // ----------------------------------------------------
        // HOME detectado
        // ----------------------------------------------------

        if (sensor_home())
        {
            motor_detener();

            ESP_LOGI(
                TAG,
                "HOME recuperado correctamente"
            );

            return true;
        }


        // ----------------------------------------------------
        // Timeout
        // ----------------------------------------------------

        TickType_t transcurrido =
            xTaskGetTickCount() - inicio;


        if (transcurrido >= timeout)
        {
            motor_detener();

            ESP_LOGE(
                TAG,
                "ERROR: No fue posible recuperar HOME"
            );

            return false;
        }


        vTaskDelay(
            pdMS_TO_TICKS(
                ESTADO_TICK_MS
            )
        );
    }
}

// ============================================================
// BUSCAR HOME DURANTE EL ARRANQUE
// ============================================================

static bool buscar_home_inicio(void)
{
    ESP_LOGI(
        TAG,
        "INICIO: verificando posicion HOME"
    );

    // --------------------------------------------------------
    // Si ya estamos en HOME, no mover el mecanismo
    // --------------------------------------------------------

    if (sensor_home())
    {
        motor_detener();

        ESP_LOGI(
            TAG,
            "INICIO: HOME ya detectado"
        );

        return true;
    }


    // --------------------------------------------------------
    // HOME no detectado.
    //
    // Hacemos una pequeña COMPRESION para llevar
    // nuevamente el brazo hacia HOME.
    // --------------------------------------------------------

    ESP_LOGW(
        TAG,
        "INICIO: HOME no detectado, buscando HOME mediante compresion"
    );

    motor_comprimir();


    TickType_t inicio =
        xTaskGetTickCount();

    TickType_t timeout =
        pdMS_TO_TICKS(
            TIEMPO_INICIO_HOME_MAX_S * 1000.0f
        );


    while (1)
    {
        // ----------------------------------------------------
        // HOME detectado
        // ----------------------------------------------------

        if (sensor_home())
        {
            motor_detener();

            ESP_LOGI(
                TAG,
                "INICIO: HOME recuperado correctamente"
            );

            return true;
        }


        // ----------------------------------------------------
        // Timeout
        // ----------------------------------------------------

        TickType_t transcurrido =
            xTaskGetTickCount() - inicio;


        if (transcurrido >= timeout)
        {
            motor_detener();

            ESP_LOGE(
                TAG,
                "INICIO: no fue posible encontrar HOME"
            );

            return false;
        }


        vTaskDelay(
            pdMS_TO_TICKS(ESTADO_TICK_MS)
        );
    }
}

// ============================================================
// MAIN
// ============================================================

void app_main(void)
{
    ESP_LOGI(
        TAG,
        "======================================"
    );

    ESP_LOGI(
        TAG,
        "       AUTO AMBU - VERSION 3.0"
    );

    ESP_LOGI(
        TAG,
        "======================================"
    );


    // --------------------------------------------------------
    // CONFIGURAR HARDWARE
    // --------------------------------------------------------

    configurar_gpio();

    configurar_pwm();

    configurar_adc();


    // --------------------------------------------------------
    // ESTADO SEGURO
    // --------------------------------------------------------

    motor_detener();

    ESP_LOGI(
        TAG,
        "Sistema inicializado"
    );


    // ========================================================
    // MAQUINA DE ESTADOS
    // ========================================================

    while (1)
    {
        switch (estado_actual)
        {
            // =================================================
            // INICIO
            // =================================================

            case ESTADO_INICIO:
            {
                bool adulto;
                bool infante;
                bool ajustable;


                leer_selector(
                    &adulto,
                    &infante,
                    &ajustable
                );


                // ---------------------------------------------
                // Selector invalido
                // ---------------------------------------------

                if (
                    (adulto && infante) ||
                    (adulto && ajustable) ||
                    (infante && ajustable)
                )
                {
                    entrar_estado(
                        ESTADO_ERROR
                    );

                    break;
                }


                // ---------------------------------------------
                // BUSCAR HOME DURANTE EL ARRANQUE
                // ---------------------------------------------

                if (!buscar_home_inicio())
                {
                    entrar_estado(
                        ESTADO_ERROR
                    );

                    break;
                }


                // ---------------------------------------------
                // HOME confirmado.
                //
                // Ahora debemos esperar que el selector
                // este en OFF antes de habilitar los modos.
                // ---------------------------------------------

                if (selector_off())
                {
                    entrar_estado(
                        ESTADO_OFF
                    );

                    break;
                }


                // ---------------------------------------------------------
                // Si el selector sigue en algun modo, permanecemos en
                // INICIO. Esto obliga a pasar por OFF antes de habilitar
                // cualquier modo.
                // ---------------------------------------------------------

                vTaskDelay(
                    pdMS_TO_TICKS(
                        ESTADO_TICK_MS
                    )
                );

                break;
            }


            // =================================================
            // OFF
            // =================================================

            case ESTADO_OFF:
            {
                bool adulto;
                bool infante;
                bool ajustable;


                motor_detener();


                leer_selector(
                    &adulto,
                    &infante,
                    &ajustable
                );


                // ---------------------------------------------
                // Combinacion imposible
                // ---------------------------------------------

                if (
                    (adulto && infante) ||
                    (adulto && ajustable) ||
                    (infante && ajustable)
                )
                {
                    entrar_estado(
                        ESTADO_ERROR
                    );

                    break;
                }


                // ---------------------------------------------
                // Seleccionar INFANTE
                // ---------------------------------------------

                if (infante)
                {
                    entrar_estado(
                        ESTADO_INFANTE
                    );

                    break;
                }


                // ---------------------------------------------
                // Seleccionar ADULTO
                // ---------------------------------------------

                if (adulto)
                {
                    entrar_estado(
                        ESTADO_ADULTO
                    );

                    break;
                }


                // ---------------------------------------------
                // Seleccionar AJUSTABLE
                // ---------------------------------------------

                if (ajustable)
                {
                    entrar_estado(
                        ESTADO_AJUSTABLE
                    );

                    break;
                }


                vTaskDelay(
                    pdMS_TO_TICKS(
                        ESTADO_TICK_MS
                    )
                );

                break;
            }


            // =================================================
            // INFANTE
            // =================================================

            case ESTADO_INFANTE:
            {
                float frecuencia =
                    FRECUENCIA_NINO;


                ESP_LOGI(
                    TAG,
                    "INFANTE: %.1f compresiones/min",
                    frecuencia
                );


                float bateria =
                    leer_voltaje_bateria();


                ESP_LOGI(
                    TAG,
                    "Bateria: %.2f V",
                    bateria
                );


                // ---------------------------------------------
                // Ejecutar ciclo
                // ---------------------------------------------

                if (!ejecutar_ciclo(frecuencia))
                {
                    entrar_estado(
                        ESTADO_ERROR
                    );

                    break;
                }


                // ---------------------------------------------
                // Al terminar el ciclo se verifica selector
                // ---------------------------------------------

                bool adulto;
                bool infante;
                bool ajustable;


                leer_selector(
                    &adulto,
                    &infante,
                    &ajustable
                );


                if (
                    (adulto && infante) ||
                    (adulto && ajustable) ||
                    (infante && ajustable)
                )
                {
                    entrar_estado(
                        ESTADO_ERROR
                    );

                    break;
                }


                if (selector_off())
                {
                    entrar_estado(
                        ESTADO_OFF
                    );

                    break;
                }


                if (adulto)
                {
                    entrar_estado(
                        ESTADO_ADULTO
                    );

                    break;
                }


                if (ajustable)
                {
                    entrar_estado(
                        ESTADO_AJUSTABLE
                    );

                    break;
                }


                break;
            }


            // =================================================
            // ADULTO
            // =================================================

            case ESTADO_ADULTO:
            {
                float frecuencia =
                    FRECUENCIA_ADULTO;


                ESP_LOGI(
                    TAG,
                    "ADULTO: %.1f compresiones/min",
                    frecuencia
                );


                float bateria =
                    leer_voltaje_bateria();


                ESP_LOGI(
                    TAG,
                    "Bateria: %.2f V",
                    bateria
                );


                // ---------------------------------------------
                // Ejecutar ciclo
                // ---------------------------------------------

                if (!ejecutar_ciclo(frecuencia))
                {
                    entrar_estado(
                        ESTADO_ERROR
                    );

                    break;
                }


                // ---------------------------------------------
                // Revisar selector al terminar ciclo
                // ---------------------------------------------

                bool adulto;
                bool infante;
                bool ajustable;


                leer_selector(
                    &adulto,
                    &infante,
                    &ajustable
                );


                if (
                    (adulto && infante) ||
                    (adulto && ajustable) ||
                    (infante && ajustable)
                )
                {
                    entrar_estado(
                        ESTADO_ERROR
                    );

                    break;
                }


                if (selector_off())
                {
                    entrar_estado(
                        ESTADO_OFF
                    );

                    break;
                }


                if (infante)
                {
                    entrar_estado(
                        ESTADO_INFANTE
                    );

                    break;
                }


                if (ajustable)
                {
                    entrar_estado(
                        ESTADO_AJUSTABLE
                    );

                    break;
                }


                break;
            }


            // =================================================
            // AJUSTABLE
            // =================================================

            case ESTADO_AJUSTABLE:
            {
                float frecuencia =
                    leer_frecuencia_ajustable();


                ESP_LOGI(
                    TAG,
                    "AJUSTABLE: %.2f compresiones/min",
                    frecuencia
                );


                float bateria =
                    leer_voltaje_bateria();


                ESP_LOGI(
                    TAG,
                    "Bateria: %.2f V",
                    bateria
                );


                // ---------------------------------------------
                // Ejecutar ciclo
                // ---------------------------------------------

                if (!ejecutar_ciclo(frecuencia))
                {
                    entrar_estado(
                        ESTADO_ERROR
                    );

                    break;
                }


                // ---------------------------------------------
                // Revisar selector al terminar ciclo
                // ---------------------------------------------

                bool adulto;
                bool infante;
                bool ajustable;


                leer_selector(
                    &adulto,
                    &infante,
                    &ajustable
                );


                if (
                    (adulto && infante) ||
                    (adulto && ajustable) ||
                    (infante && ajustable)
                )
                {
                    entrar_estado(
                        ESTADO_ERROR
                    );

                    break;
                }


                if (selector_off())
                {
                    entrar_estado(
                        ESTADO_OFF
                    );

                    break;
                }


                if (adulto)
                {
                    entrar_estado(
                        ESTADO_ADULTO
                    );

                    break;
                }


                if (infante)
                {
                    entrar_estado(
                        ESTADO_INFANTE
                    );

                    break;
                }


                break;
            }


            // =================================================
            // RECUPERACION HOME
            // =================================================

            case ESTADO_RECUPERACION_HOME:
            {
                /*
                 * El selector NO tiene autoridad durante
                 * esta recuperacion.
                 *
                 * Primero recuperamos HOME.
                 */

                if (recuperar_home())
                {
                    /*
                     * HOME recuperado.
                     *
                     * Pasamos a OFF independientemente
                     * de la posicion anterior del selector.
                     */

                    entrar_estado(
                        ESTADO_OFF
                    );
                }
                else
                {
                    entrar_estado(
                        ESTADO_ERROR
                    );
                }

                break;
            }


            // =================================================
            // ERROR
            // =================================================

            case ESTADO_ERROR:
            {
                motor_detener();


                bool adulto;
                bool infante;
                bool ajustable;


                leer_selector(
                    &adulto,
                    &infante,
                    &ajustable
                );


                // ---------------------------------------------
                // Mientras exista una combinacion invalida,
                // permanecer en ERROR.
                // ---------------------------------------------

                if (
                    (adulto && infante) ||
                    (adulto && ajustable) ||
                    (infante && ajustable)
                )
                {
                    ESP_LOGE(
                        TAG,
                        "ERROR: multiples modos seleccionados"
                    );


                    vTaskDelay(
                        pdMS_TO_TICKS(
                            ESTADO_TICK_MS
                        )
                    );

                    break;
                }


                /*
                 * Si la causa del ERROR ya desaparecio,
                 * NO vamos directamente a un modo.
                 *
                 * Primero vamos a OFF.
                 */

                if (selector_off())
                {
                    ESP_LOGI(
                        TAG,
                        "Condicion de ERROR corregida -> OFF"
                    );


                    /*
                     * Verificamos nuevamente HOME.
                     */

                    if (sensor_home())
                    {
                        entrar_estado(
                            ESTADO_OFF
                        );
                    }
                    else
                    {
                        /*
                         * Si no estamos en HOME, primero
                         * debemos recuperarlo.
                         */

                        entrar_estado(
                            ESTADO_RECUPERACION_HOME
                        );
                    }

                    break;
                }


                /*
                 * Si hay un unico modo seleccionado,
                 * permanecemos en ERROR hasta que el
                 * usuario coloque el selector en OFF.
                 */

                vTaskDelay(
                    pdMS_TO_TICKS(
                        ESTADO_TICK_MS
                    )
                );

                break;
            }


            // =================================================
            // ESTADO DESCONOCIDO
            // =================================================

            default:

                motor_detener();

                estado_actual =
                    ESTADO_ERROR;

                break;
        }
    }
}

