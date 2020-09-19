/**
 * fad_adc.c
 * Author: Corey Bean,
 * Organization: Messiah Collaboratory
 * Date: 9/17/2020
 *
 * Description:
 * This file handles ADC input for the user's voice audio data.
 * It uses an ESP32 hardware timer with an interrupt
 */

#include <stdbool.h>
#include <stdlib.h>
#include "driver/adc.h"
#include "driver/timer.h"
#include "esp_intr_alloc.h"
#include "esp_log.h"
#include "fad_adc.h"
#include "fad_app_core.h"
#include "fad_dac.h"
#include "fad_defs.h"
#include "fad_algorithms/algo_white.c"


/**
 * Event enumeration for ADC-related events
 */
enum {
	ADC_BUFFER_READY_EVT,
} adc_evt;

typedef union {

	/**
	 * @brief ADC_BUFFER_READY_EVT
	 */
	struct adc_buffer_rdy_param {
		uint16_t adc_pos;
		uint16_t dac_pos;
	} buff_pos;

} adc_evt_params;

#define TIMER_GROUP TIMER_GROUP_0
#define TIMER_NUMBER TIMER_0

static const char *ADC_TAG = "ADC";

/**
 * 	Need a circular buffer for the adc input, as well as a tracker for the
 * 	current position in the queue
 */
uint16_t *adc_buffer;
uint8_t *dac_buffer;
uint16_t adc_buffer_pos;
uint16_t dac_buffer_pos;
algo_func_t algo_function;

/**
 * @brief	Event handler for ADC-related events
 * @param 	evt An event for the adc
 * @param 	params Optional event parameters if necessary
 */
void adc_hdl_evt(uint16_t evt, void *params) {

	switch (evt) {
	case (ADC_BUFFER_READY_EVT): {
		ESP_LOGI(ADC_TAG, "Buffer ready");
		adc_evt_params *data = (adc_evt_params *) params;
		uint16_t *adc_buff = adc_buffer + data->buff_pos.adc_pos;
		uint8_t *dac_buff = dac_buffer + data->buff_pos.dac_pos;
		algo_function(adc_buff, dac_buff);
	}
	}

}

/**
 * @brief	Initialize input buffer for ADC data, as well as DAC buffer for output
 * @return
 * 		-ESP_OK if successful
 * 		-ESP_ERR_NO_MEM if buffers cannot be allocated memory
 */
static esp_err_t adc_buffer_init(void) {
	adc_buffer = (uint16_t*) calloc(ADC_BUFFER_SIZE, sizeof(uint16_t));
	if (adc_buffer == NULL) {
		return ESP_ERR_NO_MEM;
	}

	dac_buffer = (uint8_t*) calloc(ADC_BUFFER_SIZE, sizeof(uint8_t));
	if (dac_buffer == NULL) {
		return ESP_ERR_NO_MEM;
	}

	adc_buffer_pos = 0;
	dac_buffer_pos = ADC_BUFFER_SIZE - adc_algo_size;

	return ESP_OK;
}

/**
 * @brief Interrupt that is called every time the timer reaches the alarm value. Its purpose
 * is to perform an ADC reading and warn the ADC event handler when the buffer fills up,
 * so that the desired algo function will operate on the data.
 * @param arg Optional arg to be passed to interrupt from timer.
 * @return
 * 		-true if a higher priority task has awoken from handler execution
 * 		-false if no task was awoken
 */
bool adc_timer_intr_handler(void *arg) {

	bool yield = false;

	//Need to call ADC API to gather data input and place it in the buffer. Should add
	//"n" amount of ADC data to queue and

	if (adc_buffer_pos % adc_algo_size == 0) {

		adc_evt_params *params = malloc(sizeof(adc_evt_params));
		params->buff_pos.adc_pos = adc_buffer_pos;
		params->buff_pos.dac_pos = dac_buffer_pos;
		fad_app_work_dispatch(adc_hdl_evt, ADC_BUFFER_READY_EVT, (void *) params, sizeof(adc_evt_params), NULL);
		free(params);
	}

	adc_buffer[adc_buffer_pos] = adc1_get_raw(ADC1_CHANNEL_0);

	dac_output(dac_buffer, dac_buffer_pos);

	//advance buffer, resetting to zero at max buffer position
	adc_buffer_pos = (adc_buffer_pos + 1) % ADC_BUFFER_SIZE;
	dac_buffer_pos = (dac_buffer_pos + 1) % ADC_BUFFER_SIZE;

	return yield;
}

/**
 * @brief 	Initializes the timer for the ADC input. Uses API found in driver/timer.h
 * @return
 * 		-ESP_OK if successful
 * 		-Non-zero if unsuccessful
 */
esp_err_t adc_timer_init(void) {
	//API struct from driver/timer.h
	timer_config_t adc_timer =
			{ .alarm_en = TIMER_ALARM_EN,
				.counter_en = TIMER_PAUSE,
				.intr_type = TIMER_INTR_LEVEL,
				.counter_dir = TIMER_COUNT_UP,
				.auto_reload = TIMER_AUTORELOAD_EN,
				.divider = 20000, //80 MHz / 20000 = 4 kHz
			};

	esp_err_t err;

	//API functions from driver/timer.h
	err = timer_init(TIMER_GROUP, TIMER_NUMBER, &adc_timer);
	err = timer_set_counter_value(TIMER_GROUP, TIMER_NUMBER, 0x00000000ULL);
	err = timer_enable_intr(TIMER_GROUP, TIMER_NUMBER);
	err = timer_set_alarm_value(TIMER_GROUP, TIMER_NUMBER, 10000);
	err = timer_isr_callback_add(TIMER_GROUP, TIMER_NUMBER, adc_timer_intr_handler, 0, ESP_INTR_FLAG_LOWMED);

	return err;

}
/**
 * @brief	Initializes ADC settings and calls function to initiate timer and buffer
 * @return
 * 		-ESP_OK if successful
 */
esp_err_t adc_init(void) {

	//bit width of ADC input
	adc1_config_width(ADC_WIDTH_BIT_12);

	//attenuation
	adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN_DB_0);

	esp_err_t ret;

	ret = adc_buffer_init();

	ret = adc_timer_init();

	algo_white_init(&algo_function);

	return ret;

}

/**
 * @brief	Function to start the timer, meaning interrupts will start occurring
 * @return
 * 		-ESP_OK if successful
 */
esp_err_t adc_timer_start(void) {
	esp_err_t ret;
	ret = timer_start(TIMER_GROUP, TIMER_NUMBER);

	return ret;
}

