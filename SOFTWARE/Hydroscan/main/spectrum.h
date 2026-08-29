
#ifndef SPECTRUM_H
#define SPECTRUM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

/*==============================================================
                RESULTADOS ESPECTRALES
==============================================================*/

typedef struct
{
    float Hs;

    float Tp;

    float Tm01;

    float Tm02;

    float fp;

    float sigma;

    float m0; //

    float m1; //

    float m2; //

    float direction;

    bool valid; //

} spectrum_result_t;

/*==============================================================
                FUNCIÓN PRINCIPAL
==============================================================*/

void spectrum_process(
        float *vertical_acc,
        float *roll,
        float *pitch,
        int samples,
        float fs,
        spectrum_result_t *result);

#ifdef __cplusplus
}
#endif

#endif