/*
 * acoustics.h
 *
 *  Created on: 5. mar. 2026
 *      Author: vikin
 */

#ifndef INC_ACOUSTICS_H_
#define INC_ACOUSTICS_H_

// Includes
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include <stm32h7xx_hal.h>
#include <stm32h7xx_hal_spi.h>
#include <stm32h7xx.h>

#include <main.h>

extern q15_t detection_buffer[2][BLOCK_LEN];
extern float32_t magnitude_output_f32[DETECTION_FFT_SIZE / 2];

extern float32_t snr_threshold;
extern float32_t lerp_threshold;

extern uint16_t n_upper_average;
extern uint16_t n_lower_average;

typedef enum{
	ACOUSTICS_OK = 0,
	ACOUSTICS_ERROR,
	ACOUSTICS_TIMED_OUT,
} acoustics_return_status;

void acoustics_init(void);

bool acoustics_signal_present(const uint8_t half_idx);
void acoustics_prepare_data(uint8_t target_block);
void acoustics_process_data(void);
void acoustics_clean_data(void);
#endif /* INC_ACOUSTICS_H_ */
