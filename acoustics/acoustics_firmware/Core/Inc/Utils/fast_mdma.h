/*
 * fast_mdma.h
 *
 *  Created on: 23. mar. 2026
 *      Author: vikin
 */

#ifndef INC_UTILS_FAST_MDMA_H_
#define INC_UTILS_FAST_MDMA_H_

#include "stm32h753xx.h"
#include "stm32h7xx_hal.h"
#include "arm_math_types.h"

#ifndef MDMA_BLOCK_SIZE
#define MDMA_BLOCK_SIZE 64
#endif

HAL_StatusTypeDef fast_MDMA_copy_block(q15_t *src, q15_t *dst, MDMA_HandleTypeDef* hdma);

void fast_MDMA_wait_complete(void);

#endif /* INC_UTILS_FAST_MDMA_H_ */
