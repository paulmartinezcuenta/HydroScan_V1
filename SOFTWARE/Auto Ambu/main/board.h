

#ifndef BOARD_H
#define BOARD_H

// ============================================================
// SELECTOR DE MODOS
// ============================================================

#define PIN_MODO_ADULTO       13
#define PIN_MODO_NINO         23
#define PIN_MODO_AJUSTABLE    22

// ============================================================
// POTENCIOMETRO
// ============================================================

#define PIN_POTENCIOMETRO     36 // Probar tambien pin VP 36

// ============================================================
// PUENTE H
// ============================================================

#define PIN_MOTOR_PWM         26
#define PIN_MOTOR_INB1        14
#define PIN_MOTOR_INB2        27

// ============================================================
// SENSOR DE BATERIA
// ============================================================

#define PIN_BATERIA           39

#define PIN_SENSOR_LASER      33

// ============================================================
// LED
// Activo en LOW
// ============================================================

#define PIN_LED               19



// ============================================================
// MODOS
// ============================================================

#define FRECUENCIA_ADULTO     11.0f     // Resultados: cada 5.5s
#define FRECUENCIA_NINO       16.0f     // Resultados: cada 3.5s


// ============================================================
// TIEMPOS DEL MECANISMO
// ============================================================

// Tiempo que tarda el mecanismo en comprimir el Ambu
#define TIEMPO_COMPRESION_S       2.0f       // Recomendado 2s

// Tiempo que permanece completamente comprimido
#define TIEMPO_MANTENIMIENTO_S    0.0f

// Tiempo que tarda en descomprimir / regresar
#define TIEMPO_DESCOMPRESION_S    0.05f      // Recomendado 0.1s

// ============================================================
// MOTOR
// ============================================================

// PWM inicial para las pruebas.
// Posteriormente será reemplazado por el cálculo dinámico
// basado en el voltaje de batería.
#define PWM_MOTOR_INICIAL          100.0f

// ============================================================
// POTENCIOMETRO
// ============================================================

// Rango de frecuencia para el modo ajustable.
// Se puede modificar posteriormente.
#define FRECUENCIA_MIN             5.0f
#define FRECUENCIA_MAX             16.0f

#endif