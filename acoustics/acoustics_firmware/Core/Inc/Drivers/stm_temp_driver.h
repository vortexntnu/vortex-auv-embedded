/*
 * stm_temp_driver.h
 *
 *  Created on: 23. mar. 2026
 *      Author: vikin
 */

#ifndef INC_DRIVERS_STM_TEMP_DRIVER_H_
#define INC_DRIVERS_STM_TEMP_DRIVER_H_

#include "stm32h753xx.h"
#include "stm32h7xx_hal.h"

float stm_temp_get_latest(void);
float stm_temp_sensor_convert(uint16_t raw_adc);
void stm_temp_sensor_init(ADC_HandleTypeDef* adc, TIM_HandleTypeDef* timer);
void stm_temp_sensor_callback(ADC_HandleTypeDef *hadc);

#endif /* INC_DRIVERS_STM_TEMP_DRIVER_H_ */
