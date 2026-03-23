/*
 * interrupts.h
 *
 *  Created on: 23. mar. 2026
 *      Author: vikin
 */

#ifndef INC_INTERRUPTS_H_
#define INC_INTERRUPTS_H_

#include "main.h"

// SPI Interrupts start
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);

void HAL_SPI_RxCpltCallback(SPI_HandleTypeDef *hspi);

void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi);
// SPI Interrupts end

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin);

#endif /* INC_INTERRUPTS_H_ */
