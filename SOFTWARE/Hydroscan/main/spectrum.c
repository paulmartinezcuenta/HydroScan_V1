
/*
 * ============================================================
 *                      HYDROSCAN
 * ------------------------------------------------------------
 * Archivo      : spectrum.c
 * Descripción  : Procesamiento espectral del oleaje
 * ============================================================
 */

#include "spectrum.h"

#include <math.h>
#include <string.h>
#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/*==============================================================
                        CONFIGURACIÓN
==============================================================*/

static const char *TAG = "SPECTRUM";

/*
 * Frecuencias de interés para oleaje.
 * Se pueden modificar posteriormente.
 */

#define WAVE_MIN_HZ      0.1f      // 10.0 s maximo de periodo (Filtro fuera de rango probado)
#define WAVE_MAX_HZ      1.00f      // 1.0 s minimo de periodo
#define WINDOW_LENGHT    512        // Tamaño de ventana para DFT

// Debug 
#define SPECTRUM_DEBUG   1

#if SPECTRUM_DEBUG
#define SPECTRUM_PRINTF(...) printf(__VA_ARGS__)
#else
#define SPECTRUM_PRINTF(...)
#endif


/*==============================================================
                VARIABLES DE DEBUG
==============================================================*/

static float debug_freq[WINDOW_LENGHT];
static float debug_peta[WINDOW_LENGHT];
static int debug_bins = 0;

/*==============================================================
                ELIMINAR MEDIA
==============================================================*/

static void remove_mean(float *x, int n)
{
    double sum = 0.0;

    for(int i=0;i<n;i++)
        sum += x[i];

    float mean = sum / n;

    for(int i=0;i<n;i++)
        x[i] -= mean;
}

/*==============================================================
                DETREND LINEAL
==============================================================*/

static void detrend_linear(float *x, int n)
{
    double sx = 0.0;
    double sy = 0.0;
    double sxx = 0.0;
    double sxy = 0.0;

    for(int i=0;i<n;i++)
    {
        sx += i;
        sy += x[i];
        sxx += (double)i*i;
        sxy += (double)i*x[i];
    }

    double den =
        n*sxx-sx*sx;

    if(fabs(den)<1e-12)
        return;

    double a =
        (n*sxy-sx*sy)/den;

    double b =
        (sy-a*sx)/n;

    for(int i=0;i<n;i++)
        x[i]-=(float)(a*i+b);
}

/*==============================================================
                MEDIA CUADRÁTICA
==============================================================*/

static float mean_square(
        const float *x,
        int n)
{
    double sum = 0.0;

    for(int i=0;i<n;i++)
        sum += (double)x[i]*x[i];

    return sum/n;
}

/*==============================================================
                DESVIACIÓN ESTÁNDAR
==============================================================*/

static float standard_deviation( // No se ha usado 
        const float *x,
        int n)
{
    double s = 0.0;
    double ss = 0.0;

    for(int i=0;i<n;i++)
    {
        s += x[i];
        ss += (double)x[i]*x[i];
    }

    double mean = s/n;

    double var =
        (ss/n)-(mean*mean);

    if(var<0.0)
        var=0.0;

    return sqrt(var);
}

/*==============================================================
            NUMERO DE SEGMENTOS WELCH
==============================================================*/

static int welch_segment_count(
        int samples,
        int segment_length,
        int step)
{
    if(samples < segment_length)
        return 0;

    int count = 0;

    for(int start = 0;
        start + segment_length <= samples;
        start += step)
    {
        count++;
    }

    return count;
}

/*==============================================================
        DETECCIÓN DE OLEAJE FUERA DEL RANGO MEDIBLE
==============================================================*/

static bool spectrum_detect_valid_range(
        const float *freq,
        const float *peta,
        int bins)
{
    if(bins < 5)
        return false;

    /*------------------------------------------
            Suavizado del espectro
    ------------------------------------------*/

    float peta_smooth[WINDOW_LENGHT];

    peta_smooth[0] =
        (peta[0] +
         peta[1]) * 0.5f;

    for(int i = 1; i < bins - 1; i++)
    {
        peta_smooth[i] =
            (peta[i-1] +
             peta[i] +
             peta[i+1]) / 3.0f;
    }

    peta_smooth[bins-1] =
        (peta[bins-2] +
         peta[bins-1]) * 0.5f;

    /*------------------------------------------
            Buscar el máximo global
    ------------------------------------------*/

    int peak_bin = 0;

    float peak_power =
        peta_smooth[0];

    float total_energy = 0.0f;

    float weighted_freq = 0.0f;

    for(int i = 0; i < bins; i++)
    {
        total_energy +=
            peta_smooth[i];

        weighted_freq +=
            freq[i] *
            peta_smooth[i];

        if(peta_smooth[i] >
           peak_power)
        {
            peak_power =
                peta_smooth[i];

            peak_bin = i;
        }
    }

    if(total_energy <= 0.0f)
        return false;

    /*------------------------------------------
            Centroide espectral
    ------------------------------------------*/

    float centroid =
        weighted_freq /
        total_energy;

#if SPECTRUM_DEBUG

    SPECTRUM_PRINTF("\n========= SPECTRAL SHAPE =========\n");

    SPECTRUM_PRINTF(
        "Bins utiles        : %d\n",
        bins);

    SPECTRUM_PRINTF(
        "Peak bin           : %d\n",
        peak_bin);

    SPECTRUM_PRINTF(
        "Peak frequency     : %.3f Hz\n",
        freq[peak_bin]);

    SPECTRUM_PRINTF(
        "Peak period        : %.3f s\n",
        1.0f / freq[peak_bin]);

    SPECTRUM_PRINTF(
        "Centroide          : %.3f Hz\n",
        centroid);

#endif   
        
    // Parte II

    /*------------------------------------------
        Analisis de la forma del espectro
    ------------------------------------------*/

    int local_peaks = 0;

    int slope_changes = 0;

    int previous_sign = 0;

    for(int i = 1; i < bins - 1; i++)
    {
        float left =
            peta_smooth[i] -
            peta_smooth[i-1];

        float right =
            peta_smooth[i+1] -
            peta_smooth[i];

        /*----------------------------------
            Buscar máximos locales
        ----------------------------------*/

        float prominence =
            peta_smooth[i] /
            (peak_power + 1e-12f);

        if(left > 0.0f &&
        right < 0.0f &&
        prominence > 0.10f)
        {
            local_peaks++;
        }

        /*----------------------------------
            Analizar pendiente global
        ----------------------------------*/

        int sign = 0;

        if(right > 0.0f)
            sign = 1;
        else if(right < 0.0f)
            sign = -1;

        if(previous_sign != 0 &&
           sign != 0 &&
           sign != previous_sign)
        {
            slope_changes++;
        }

        if(sign != 0)
            previous_sign = sign;
    }

    /*------------------------------------------
        Distancia del pico a los extremos
    ------------------------------------------*/

    int distance_left =
        peak_bin;

    int distance_right =
        (bins - 1) - peak_bin;

#if SPECTRUM_DEBUG

    SPECTRUM_PRINTF(
        "Picos locales      : %d\n",
        local_peaks);

    SPECTRUM_PRINTF(
        "Cambios pendiente  : %d\n",
        slope_changes);

    SPECTRUM_PRINTF(
        "Distancia izquierda: %d\n",
        distance_left);

    SPECTRUM_PRINTF(
        "Distancia derecha  : %d\n",
        distance_right);

#endif

    // Parte III

    /*----------------------------------------------------------
        Detección de oleaje fuera del rango medible
    ----------------------------------------------------------*/

    /*
    * El pico debe estar prácticamente pegado al borde.
    */

    bool below_range =
    (
        (peak_bin == 0) &&
        (distance_left == 0) &&
        (centroid <= (freq[0] + 1.5f * (freq[1] - freq[0]))) &&
        (local_peaks == 0)
    );

    /*
    * Caso equivalente para el extremo superior.
    */

    bool above_range =
    (
        (peak_bin == (bins - 1)) &&
        (distance_right == 0) &&
        (centroid >= (freq[bins - 1] - 1.5f * (freq[1] - freq[0]))) &&
        (local_peaks == 0)
    );


#if SPECTRUM_DEBUG

    SPECTRUM_PRINTF(
        "Below range      : %s\n",
        below_range ? "SI" : "NO");

    SPECTRUM_PRINTF(
        "Above range      : %s\n",
        above_range ? "SI" : "NO");

    SPECTRUM_PRINTF(
        "====================================\n\n");

#endif

    return !(below_range || above_range);

}

static void highpass_filter(
        float *x,
        int n,
        float fs,
        float cutoff_hz)
{
    if(n < 2)
        return;

    const float dt = 1.0f / fs;

    const float RC =
        1.0f /
        (2.0f *
         (float)M_PI *
         cutoff_hz);

    const float alpha =
        RC /
        (RC + dt);

    float y_prev = 0.0f;
    float x_prev = x[0];

    for(int i=1;i<n;i++)
    {
        float y =
            alpha *
            ( y_prev +
              x[i] -
              x_prev );

        x_prev = x[i];
        y_prev = y;

        x[i] = y;
    }

    x[0]=0.0f;
}

/*==============================================================
            CÁLCULO DE MOMENTOS ESPECTRALES
==============================================================*/

static void compute_wave_moments(
        const float *acc,
        int samples,
        float fs,
        float *m0,
        float *m1,
        float *m2,
        float *fp,
        bool *valid)
{
    /*----------------------------------------------------------
                    PARAMETROS DE WELCH
    ----------------------------------------------------------*/

    const int segment_length = WINDOW_LENGHT;

    const int overlap = segment_length / 2;

    const int step =
        segment_length - overlap;

    const int kmax =
        segment_length / 2;

    const float df =
        fs / segment_length;

    /*----------------------------------------------------------
                    LIMPIAR RESULTADOS
    ----------------------------------------------------------*/

    *m0 = 0.0f;
    *m1 = 0.0f;
    *m2 = 0.0f;
    *fp = 0.0f;
    *valid = true;

    debug_bins = 0;

    float peak_power = -1.0f;

    /*----------------------------------------------------------
                NUMERO DE SEGMENTOS WELCH
    ----------------------------------------------------------*/

    int segments =
        welch_segment_count(
            samples,
            segment_length,
            step);

    if(segments <= 0)
        return;

    /*----------------------------------------------------------
                PSD PROMEDIO
    ----------------------------------------------------------*/

    float Paa_average[kmax+1];

    memset(
        Paa_average,
        0,
        sizeof(Paa_average));
    
    /*----------------------------------------------------------
        RECORRER SEGMENTOS
    ----------------------------------------------------------*/

    for(int seg=0; seg<segments; seg++)
    {
        int start =
            seg * step;

        /*------------------------------------------
            ENERGIA DE LA VENTANA
        ------------------------------------------*/

        double U = 0.0;

        for(int n=0;n<segment_length;n++)
        {
            float w =
                0.5f *
                (1.0f -
                cosf(
                2.0f*M_PI*n/
                (segment_length-1)));

            U += w*w;
        }

        U /= segment_length;

        /*------------------------------------------
            DFT
        ------------------------------------------*/

        for(int k=0;k<=kmax;k++)
        {
            if((k%10)==0)
                vTaskDelay(1);

            double re = 0.0;
            double im = 0.0;

            for(int n=0;n<segment_length;n++)
            {
                float w =
                    0.5f *
                    (1.0f -
                    cosf(
                    2.0f*M_PI*n/
                    (segment_length-1)));

                float xn =
                    acc[start+n] * w;

                double ang =
                    -2.0 *
                    M_PI *
                    k *
                    n /
                    segment_length;

                re +=
                    xn *
                    cos(ang);

                im +=
                    xn *
                    sin(ang);
            }
        
            /*------------------------------------------
                PSD DEL SEGMENTO
            ------------------------------------------*/

            double mag2 =
                re*re + im*im;

            float Paa;

            if(k==0 || k==kmax)
            {
                Paa =
                    mag2 /
                    (segment_length *
                     segment_length *
                     U);
            }
            else
            {
                Paa =
                    2.0f *
                    mag2 /
                    (segment_length *
                     segment_length *
                     U);
            }

            Paa_average[k] += Paa;

        }   /* fin for(k) */

    }   /* fin for(seg) */

    /*----------------------------------------------------------
        PROMEDIAR PSD
    ----------------------------------------------------------*/

    for(int k=0;k<=kmax;k++)
    {
        Paa_average[k] /= segments;
    }

    /*----------------------------------------------------------
            CALCULAR MOMENTOS ESPECTRALES
    ----------------------------------------------------------*/

    for(int k=0;k<=kmax;k++)
    {
        float f = k * df;

        if(f < WAVE_MIN_HZ)
            continue;

        if(f > WAVE_MAX_HZ)
            continue;

        /*------------------------------------------
            PSD aceleración -> PSD elevación
        ------------------------------------------*/

        float omega =
            2.0f * M_PI * f;

        float omega2 =
            omega * omega;

        float Peta =
            Paa_average[k] /
            (omega2 * omega2 * df);

        /*------------------------------------------
                    DEBUG
        ------------------------------------------*/

        if(debug_bins < WINDOW_LENGHT)
        {
            debug_freq[debug_bins] = f;
            debug_peta[debug_bins] = Peta;
            debug_bins++;
        }

        /*------------------------------------------
                MOMENTOS ESPECTRALES
        ------------------------------------------*/

        *m0 += Peta * df;
        *m1 += f * Peta * df;
        *m2 += f * f * Peta * df;

        /*------------------------------------------
                FRECUENCIA DE PICO
        ------------------------------------------*/

        if(Peta > peak_power)
        {
            peak_power = Peta;
            *fp = f;
        }
    }

    /*------------------------------------------
        Verificar si el espectro es válido
    ------------------------------------------*/
    
    bool validation =
        spectrum_detect_valid_range(
            debug_freq,
            debug_peta,
            debug_bins); 
    
    *valid = validation;

#if SPECTRUM_DEBUG

    SPECTRUM_PRINTF("\n");
    SPECTRUM_PRINTF("========= WELCH =========\n");
    SPECTRUM_PRINTF("Segmentos     : %d\n", segments);
    SPECTRUM_PRINTF("Tam ventana   : %d\n", segment_length);
    SPECTRUM_PRINTF("Solape        : %d\n", overlap);
    SPECTRUM_PRINTF("df            : %.5f Hz\n", df);
    SPECTRUM_PRINTF("=========================\n");

    /*SPECTRUM_PRINTF("========= TOP PICOS ESPECTRALES =========\n");

    for(int i=0;i<debug_bins;i++)
    {
        SPECTRUM_PRINTF("%.3f Hz  -> %e\n",
                        debug_freq[i],
                        debug_peta[i]);
    }

    SPECTRUM_PRINTF("=========================================\n");

    SPECTRUM_PRINTF("m0    : %.8e\n", *m0);
    SPECTRUM_PRINTF("m1    : %.8e\n", *m1);
    SPECTRUM_PRINTF("m2    : %.8e\n", *m2);
    SPECTRUM_PRINTF("fp    : %.3f Hz\n", *fp); */

#endif

}    

/*==============================================================
                PROCESAMIENTO ESPECTRAL
==============================================================*/

void spectrum_process(
        float *vertical_acc,
        float *roll,
        float *pitch,
        int samples,
        float fs,
        spectrum_result_t *result)
{
    remove_mean(vertical_acc,samples);

    detrend_linear(vertical_acc,samples);

    highpass_filter(
        vertical_acc,
        samples,
        fs,
        0.1f);

    remove_mean(vertical_acc,samples);

    remove_mean(roll,samples);

    remove_mean(pitch,samples);

    compute_wave_moments(
            vertical_acc,
            samples,
            fs,
            &result->m0,
            &result->m1,
            &result->m2,
            &result->fp,
            &result->valid);

    if (!result->valid)
    {
        result->Hs = 0.0f;
        result->sigma = 0.0f;
        result->Tp = 0.0f;
        result->Tm01 = 0.0f;
        result->Tm02 = 0.0f;
        result->direction = 0.0f;
        result->fp = 0.0f;
    }
    else
    {

        result->Hs =
        4.0f*sqrtf(fmaxf(result->m0,0.0f));

        result->sigma =
            sqrtf(fmaxf(result->m0,0.0f));

        result->Tp =
            (result->fp>0.0f)?
            1.0f/result->fp:0.0f;

        result->Tm01 =
            (result->m1>0)?
            result->m0/result->m1:0.0f;

        result->Tm02 =
            (result->m2>0)?
            sqrtf(result->m0/result->m2):0.0f;

        float var_r =
            mean_square(roll,samples);

        float var_p =
            mean_square(pitch,samples);

        float cov = 0.0f;

        for(int i=0;i<samples;i++)
            cov += roll[i]*pitch[i];

        cov /= samples;

        float dir =
            0.5f*atan2f(
                2.0f*cov,
                var_r-var_p);

        result->direction =
            dir*180.0f/M_PI;

        if(result->direction<0)
            result->direction+=180.0f;
    }

#if SPECTRUM_DEBUG

    //SPECTRUM_PRINTF("\n");

    SPECTRUM_PRINTF(
    "========= TOP PICOS ESPECTRALES =========\n");

    for(int i=0;i<debug_bins;i++)
    {
        SPECTRUM_PRINTF(
            "%.3f Hz  -> %e\n",
            debug_freq[i],
            debug_peta[i]);
    }

    SPECTRUM_PRINTF(
    "=========================================\n");

    SPECTRUM_PRINTF(
        "m0    : %.8e\n",
        result->m0);

    SPECTRUM_PRINTF(
        "m1    : %.8e\n",
        result->m1);

    SPECTRUM_PRINTF(
        "m2    : %.8e\n",
        result->m2);

    SPECTRUM_PRINTF(
        "Hs    : %.3f m\n",
        result->Hs);

    SPECTRUM_PRINTF(
        "Tp    : %.3f s\n",
        result->Tp);

    SPECTRUM_PRINTF(
        "Tm01  : %.3f s\n",
        result->Tm01);

    SPECTRUM_PRINTF(
        "Tm02  : %.3f s\n",
        result->Tm02);

    SPECTRUM_PRINTF(
        "fp    : %.3f Hz\n",
        result->fp);

#endif
}

// The end 