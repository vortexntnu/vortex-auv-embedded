/*
 * stm_temp.c
 *
 *  Created on: 23. mar. 2026
 *      Author: vikin
 */

#include "stm_temp_driver.h"

#include "memory_placement.h"
#include "embedded_macros.h"

#include "stm32h753xx.h"
#include "stm32h7xx_hal.h"

#include "arm_math_types.h"


BDMA_BUF_D3(ALIGN_DMA_WORD) volatile uint16_t g_adc3_dma_buf;

static volatile float g_die_temp = 0.0f;

// Just start everything once at init
void stm_temp_sensor_init(ADC_HandleTypeDef* adc, TIM_HandleTypeDef* timer)
{
    HAL_ADC_Start_DMA(adc, (uint32_t*)&g_adc3_dma_buf, 1);
    HAL_TIM_Base_Start(timer);
}

// Read whenever you want - buffer updates at timer rate
float stm_temp_get_latest(void)
{
	int16_t raw = g_adc3_dma_buf;
	return stm_temp_sensor_convert(raw);
}

// Call this whenever you want a fresh conversion of the buffer value
//float stm_temp_get_latest(void)
//{
//    float32_t raw  = (float32_t)g_adc3_dma_buf;  // no shift
//    float32_t cal1 = (float32_t)(*((uint16_t*)0x1FF1E820));  // 30°C
//    float32_t cal2 = (float32_t)(*((uint16_t*)0x1FF1E824));  // 110°C
//
//    return (110.0f - 30.0f) / (cal2 - cal1) * (raw - cal1) + 30.0f;
//}


float stm_temp_sensor_convert(uint16_t raw_adc)
{
    float32_t raw  = (float32_t)raw_adc;
    float32_t cal1 = (float32_t)(*TEMPSENSOR_CAL1_ADDR);
    float32_t cal2 = (float32_t)(*TEMPSENSOR_CAL2_ADDR);

    float temperature =  (float32_t)(TEMPSENSOR_CAL2_TEMP - TEMPSENSOR_CAL1_TEMP)
					     / (cal2 - cal1)
					     * (raw - cal1)
					     + (float32_t)TEMPSENSOR_CAL1_TEMP;
    return temperature;
}

void stm_temp_sensor_callback(ADC_HandleTypeDef *hadc){
    if (hadc->Instance == ADC3)
    {
        g_die_temp = stm_temp_get_latest();
    }
}


