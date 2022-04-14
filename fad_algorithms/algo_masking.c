/**
 * algo_template.c
 * Author: Corey Bean,
 * Organization: Messiah Collaboratory
 * Date: 10/12/2020
 *
 * Description:
 * This file demonstrates the template for the algorithm file. It simply outputs a square wave of a varied period.
 */

#include "algo_masking.h"
#include "esp_log.h"
#include "fft.h"
#include "fad_defs.h"

#define ALGO_TAG "ALGO_MASKING"

/* Defines how many values the algorithm will read from the ADC buffer. Should always be at least half of buffer size. */
static int s_algo_template_read_size = 2048; // was 512

/* References the sampling freq of the adc */
static int s_algo_sampling_freq = 40000;

/*Determines the time of which data is captured */
static float s_algo_total_time = s_algo_template_read_size/s_algo_sampling_freq;

/* Stores max_magnitude */
volatile float s_algo_max_magnitude = 0;

/* Stores fundamental frequency */
volatile float s_algo_fundamental_freq = 0;

/* Determines the period of the square wave */
static int s_period = 30;

static int switching_voltage = 2048;

void algo_masking(uint16_t *in_buff, uint8_t *out_buff, uint16_t in_pos, uint16_t out_pos, int multisamples)
{

    /**
     * in_pos and out_pos point to the algorithm half of the buffer. Only ready and write to data in the half
     * of the buffer designated by these positions
     * 
     * The algorithm should have minimum side effects: try not to write to globals defined in other files, etc.
     */

    /* This algorithm simply outputs the input to the ADC */

    //ESP_LOGI(ALGO_TAG, "s_algo_template_read_size = %d", s_algo_template_read_size);

    for (int i = 0; i < s_algo_template_read_size; i++)
    {
        real_fft_plan->input[i] = in_buff[in_pos + i];
        
        //uint8_t val = in_buff[in_pos + i];
        //out_buff[out_pos + i] = val;

    }

    for (int k = 1 ; k < real_fft_plan->size / 2 ; k++)
    {
    /*The real part of a magnitude at a frequency is 
        followed by the corresponding imaginary part in the output*/
    float mag = sqrt(pow(real_fft_plan->output[2*k],2) + pow(real_fft_plan->output[2*k+1],2));
    
    float freq = k*1.0/s_algo_total_time;
        if(mag > s_algo_max_magnitude)
        {
            s_algo_max_magnitude = mag;
            s_algo_fundamental_freq = freq;
        }
    }
    ESP_LOGI(ALGO_TAG, "running algo... %d", freq);
    //ESP_LOGI(ALGO_TAG, "running algo... %d", out_buff[out_pos]);
}

void algo_masking_init(fad_algo_init_params_t *params)
{
    s_algo_template_read_size = params->algo_template_params.read_size;
    s_period = params->algo_template_params.period;
    uint16_t fft_input[s_algo_template_read_size];
    uint16_t fft_output[s_algo_template_read_size];
    fft_config_t *real_fft_plan = fft_init(s_algo_template_read_size, FFT_REAL, FFT_FORWARD, fft_input, fft_output);
}

void algo_masking_deinit()
{
    // Undo any memory allocations here
}