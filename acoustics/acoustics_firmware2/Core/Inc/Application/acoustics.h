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

extern arm_rfft_fast_instance_f32 detection_fft_instance_f32;
extern arm_rfft_instance_q15 detection_fft_instance_q15;
extern q15_t detection_buffer[2][BLOCK_LEN];
extern float32_t fft_input_f32[DETECTION_FFT_SIZE];
extern float32_t fft_output_f32[DETECTION_FFT_SIZE * 2];
extern float32_t magnitude_output_f32[DETECTION_FFT_SIZE / 2];

extern arm_rfft_instance_q15 processing_fft_instance;

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

bool acoustics_signal_present(uint8_t half_idx);
bool acoustics_signal_present_in_array(float32_t* signal);
void acoustics_prepare_data(uint8_t target_block);
void acoustics_process_data(void);
void acoustics_clean_data(void);

bool acoustics_tdoa_is_valid(float32_t vec[3]);

float32_t acoustics_estimate_SNR(void);

void acoustics_clear_detection_buffer(void);
#endif /* INC_ACOUSTICS_H_ */
