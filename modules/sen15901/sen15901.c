/**
 * @ingroup     drivers_sen15901
 * @{
 *
 * @file
 * @brief       Spark fun meteo station functions 
 *
 * @author      Pieau Maxime <maxime.pieau@etu.univ-grenoble-alpes.fr>
 *		Abelkalon Arthur <arthur.anelkalon@etu.univ-grenoble-alpes.fr>
 *		Lailler Tristan <tristan.lailler@etu.univ-grenoble-alpes.fr>
 *
 * @}
 */

#include "sen15901.h"
#include <string.h>
#include "xtimer.h"
#include "board.h"

uint32_t last_pluviometre_tick = 0;
uint32_t last_anemometre_tick = 0;

uint32_t pluviometre_ticks = 0;
uint32_t anemometre_ticks = 0;

#define DEBOUNCE_INTERVAL_PLUVIOMETRE 10
#define DEBOUNCE_INTERVAL_ANEMOMETRE 10

#define ANEMOMETRE_SPEED_FOR_1_TICK_PER_SEC 2.4
#define PLUVIOMETRE_MM_BY_TICK 0.2794

uint32_t last_anemometre_call = 0;

const float adc_values[] = { 
3143.44186, // 0 °
1624.06276, // 22.5 °
1845.45055,
335.096502,
372.363636,
263.664671,
738.622951,
506.166521,
1149.23741,
978.800609,
2520.61538,
2397.82421,
3780.92308,
3310.12126,
3549.13752,
2811.18193  // 337.5 °
};
const float angle_resolution = 22.5;


static void cb_interrupts(void *arg){ //callback for interrupts
	uint32_t current_time = xtimer_now_usec();
	switch ((int)arg) {
		case PLUVIOMETRE:
			if ((current_time - last_pluviometre_tick) > DEBOUNCE_INTERVAL_PLUVIOMETRE) { // Bounce filtering by checking the time elapsed since the last state change
				last_pluviometre_tick = current_time;
				pluviometre_ticks++;
				printf("INT: external interrupt from Pluviometre pin\n");
				LED_RED_TOGGLE;
			}
			break;
			
		case ANEMOMETRE:
			if ((current_time - last_anemometre_tick) > DEBOUNCE_INTERVAL_ANEMOMETRE) { // Bounce filtering by checking the time elapsed since the last state change
				last_anemometre_tick = current_time;
				anemometre_ticks++;
				printf("INT: external interrupt from Anemometre pin\n");
				LED_GREEN_TOGGLE;
			}
			break;
	}
}
 
 
 
int sen15901_init(sen15901_t *dev, const sen15901_params_t *params){

	memcpy(dev, params, sizeof(sen15901_params_t));

	printf("\r\nConnecting to Meteo Station...\r\n");

	// GIROUETTE init
	if (gpio_init(params->girouette_pin, params->girouette_mode) < 0) {
		printf("error: init_int of GIROUETTE\n");
		return SEN15901_ERROR_GPIO;
	}

	// Sensor GIROUETTE adc init
	if (adc_init(params->adc) < 0) {
		printf("error: initialization of ADC failed\n");
		return SEN15901_ERROR_ADC;
	}
	printf("SENSOR (1/3) init succeed\n");


	//Sensor ANEMOMETRE init
	if (gpio_init_int(params->anemometre_pin, params->anemometre_mode, params->anemometre_flank, &cb_interrupts, (void *)ANEMOMETRE) < 0) {
		printf("error: init_int of ANEMOMETRE\n");
		return SEN15901_ERROR_GPIO;
	}
	gpio_irq_enable(params->anemometre_pin);
	printf("SENSOR (2/3) init succeed\n");


	//Sensor PLUVIOMETRE init
	if (gpio_init_int(params->pluviometre_pin, params->pluviometre_mode, params->pluviometre_flank, &cb_interrupts, (void *)PLUVIOMETRE) < 0) {
		printf("error: init_int of PLUVIOMETRE\n");
		return SEN15901_ERROR_GPIO;
	}
	gpio_irq_enable(params->pluviometre_pin);
	printf("SENSOR (3/3) init succeed\n");

	return SEN15901_OK;
}

/**
 * @brief   Read ADC value and get corresponding wind direction
 *
 * @param[in] dev    device to read
 * @param[out] data  wind direction
 *
 * @return SEN15901_OK on success
 * @return < 0 on error
 */
int sen15901_get_girouette(const sen15901_t *dev, uint16_t *data){
	float mesure;
	mesure = adc_sample(dev->params.adc,dev->params.res);

	//Max diff(2^12-1)
	float min_dif = 4095;
	int index = 0;
	
	
	for(int i=0; i < 16; i++){
		float ADC_dif = adc_values[i] - mesure;

		ADC_dif = ADC_dif < 0 ? -ADC_dif : ADC_dif;

		if(ADC_dif < min_dif){ // Get the closest value from known ADC values
		    min_dif = ADC_dif;
		    index = i;
		}
	}
	
    	*data = index*angle_resolution;	
	return SEN15901_OK;
}

/**
 * @brief   Read wind ticks and compute wind speed
 *
 *
 * @param[in] dev    device to read
 * @param[out] data  mean wind speed since last call
 *
 * @return SEN15901_OK on success
 * @return < 0 on error
 */
int sen15901_get_anemometre(const sen15901_t *dev, uint16_t *data){
	if (dev==NULL) return SEN15901_ERROR_DEV;
	uint32_t current_time = xtimer_now();
	*data=anemometre_ticks*ANEMOMETRE_SPEED_FOR_1_TICK_PER_SEC/(current_time - last_anemometre_call);
	last_anemometre_call=current_time;
	anemometre_ticks=0;
	return SEN15901_OK;
}

/**
 * @brief   Read water ticks and compute mm of rain 
 *
 * @param[in] dev    device to read
 * @param[out] data  mm of rain since last call
 *
 * @return SEN15901_OK on success
 * @return < 0 on error
 */
int sen15901_get_pluviometre(const sen15901_t *dev, uint16_t  *data){
	if (dev==NULL) return SEN15901_ERROR_DEV;
	*data=pluviometre_ticks*PLUVIOMETRE_MM_BY_TICK;
	pluviometre_ticks=0;
	return SEN15901_OK;
}
