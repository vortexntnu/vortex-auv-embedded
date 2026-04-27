/*
 * interrupts.c
 *
 *  Created on: 7. mar. 2026
 *      Author: vikin
 */

#include "main.h"
#include "ad7606_driver.h"
#include "stm_temp_driver.h"

#include "acoustics.h"


// SPI Interrupts start
//void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi) {
//
//}

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi) {
	Error_Handler();
}
// SPI Interrupts end

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == BUSY_INT)
    {
        // Kick next transfer — non-blocking, returns in ~5 cycles
    	ad7606_trigger_burst(MASTER_SPI);
    }
}

void HAL_ADC_LevelOutOfWindowCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC3)
    {
    	float32_t temp = stm_temp_get_latest();
        // Temperature out of range — take action
        // e.g. reduce clock, shut down peripherals, set a flag
    	if((temp > 90.0) && (temp < -40.0)){
        	Error_Handler();
    	}
    }
}

// Optional, makes the temperature readily available
//void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
//{
//	stm_temp_sensor_callback(hadc);
//}



