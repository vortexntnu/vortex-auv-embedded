#include "acoustics.h"
#include "main.h"

#include <stm32h753xx.h>
#include <stm32h7xx_hal_gpio.h>
#include <stm32h7xx_hal_spi.h>
#include <stdint.h>
#include <stdio.h>

#include "utils.h"
#include "dsp.h"
#include "cwt.h"
#include "TDOA.h"

#include "hydrophone_interface.h"
#include "can_interface.h"

PLACE_IN_DTCM arm_rfft_fast_instance_f32 detection_fft_instance_f32;
PLACE_IN_DTCM arm_rfft_instance_q15 detection_fft_instance_q15;
MDMA_BUF_DTCM(ALIGN_DMA_BURST_8_WORD) q15_t detection_buffer[2][BLOCK_LEN];
PLACE_IN_DTCM float32_t fft_input_f32[DETECTION_FFT_SIZE];
PLACE_IN_DTCM float32_t fft_output_f32[DETECTION_FFT_SIZE * 2];
PLACE_IN_DTCM float32_t magnitude_output_f32[DETECTION_FFT_SIZE / 2];

PLACE_IN_DTCM q15_t fft_output_q15[DETECTION_FFT_SIZE * 2];
PLACE_IN_DTCM q15_t magnitude_output_q15[DETECTION_FFT_SIZE / 2];

PLACE_IN_DTCM float32_t SNR = 1;

PLACE_IN_DTCM arm_rfft_instance_q15 processing_fft_instance;
PLACE_IN_DTCM arm_rfft_instance_q15 processing_ifft_instance;

PLACE_IN_DTCM float32_t processing_workspace[N_HYDROPHONES][PROCESSING_FFT_SIZE];

PLACE_IN_DTCM uint16_t idxs[N_HYDROPHONES] = {0};
PLACE_IN_DTCM float32_t times_of_arrival[N_HYDROPHONES] = {0};
PLACE_IN_DTCM float32_t direction_of_arrival[3] = {0};

PLACE_IN_DTCM float32_t snr_threshold;
PLACE_IN_DTCM float32_t lerp_threshold;

PLACE_IN_DTCM uint16_t n_upper_average;
PLACE_IN_DTCM uint16_t n_lower_average;

void acoustics_init(void){
	SNR = 1;

	arm_rfft_fast_init_f32(&detection_fft_instance_f32, DETECTION_FFT_SIZE);
	arm_rfft_init_q15(&detection_fft_instance_q15, DETECTION_FFT_SIZE, 0, 1);
	cwt_init_f32(TARGET_FREQUENCY, SAMPLING_FREQUENCY, 0.5);

	direction_of_arrival[0] = 1.0;
	direction_of_arrival[1] = 0.0;
	direction_of_arrival[2] = 0.0;

	snr_threshold = LINEAR_THRESHOLD;
	lerp_threshold = 0.01;

	n_upper_average = 15;
	n_lower_average = WORKSPACE_LEN/2;

	acoustics_clear_detection_buffer();
}


bool acoustics_signal_present(uint8_t half_idx) {

    arm_q15_to_float(detection_buffer[half_idx], fft_input_f32, DETECTION_FFT_SIZE);
    arm_rfft_fast_f32(&detection_fft_instance_f32, fft_input_f32, fft_output_f32, 0);
    arm_cmplx_mag_squared_f32(fft_output_f32, magnitude_output_f32, DETECTION_FFT_SIZE/2);

    const uint32_t SIGNAL_BIN_LOW = 14;
    const uint32_t SIGNAL_BIN_HIGH = 17;
    const uint32_t SIGNAL_BIN_COUNT = SIGNAL_BIN_HIGH-SIGNAL_BIN_LOW;
    const uint32_t NOISE_BIN_COUNT = DETECTION_FFT_SIZE/2 - 1 - SIGNAL_BIN_COUNT;

    float32_t noise_power = 0.0f;
    for (int i = 1; i < SIGNAL_BIN_LOW; i++)
        noise_power += magnitude_output_f32[i];
    for (int i = SIGNAL_BIN_HIGH; i < DETECTION_FFT_SIZE / 2; i++)
        noise_power += magnitude_output_f32[i];

    bool present = false;
    if (unlikely(noise_power <= 0.0f)){
    	return false;
    }

    float32_t signal_power = magnitude_output_f32[14] + magnitude_output_f32[15] + magnitude_output_f32[16];
    if (likely(signal_power < SIGNAL_MIN_POWER)){
    	return false;
    }

    // signal_power/2 > (noise_power/NOISE_BIN_COUNT) * LINEAR_THRESHOLD
    present = (signal_power * NOISE_BIN_COUNT) > (noise_power * SIGNAL_BIN_COUNT * snr_threshold);
    return present;
}

bool acoustics_signal_present_in_array(float32_t* signal){

	arm_copy_f32(signal, fft_input_f32, DETECTION_FFT_SIZE);
    arm_rfft_fast_f32(&detection_fft_instance_f32, fft_input_f32, fft_output_f32, 0);
    arm_cmplx_mag_f32(fft_output_f32, magnitude_output_f32, DETECTION_FFT_SIZE/2);

    const uint32_t SIGNAL_BIN_LOW = 14;
    const uint32_t SIGNAL_BIN_HIGH = 17;
    const uint32_t SIGNAL_BIN_COUNT = SIGNAL_BIN_HIGH-SIGNAL_BIN_LOW;
    const uint32_t NOISE_BIN_COUNT = DETECTION_FFT_SIZE/2 - 1 - SIGNAL_BIN_COUNT;

    float32_t noise_power = 0.0f;
    for (int i = 1; i < SIGNAL_BIN_LOW; i++)
        noise_power += magnitude_output_f32[i];
    for (int i = SIGNAL_BIN_HIGH; i < DETECTION_FFT_SIZE / 2; i++)
        noise_power += magnitude_output_f32[i];

    if (unlikely(noise_power <= 0.0f)){
    	return false;
    }

    float32_t signal_power = magnitude_output_f32[14] + magnitude_output_f32[15] + magnitude_output_f32[16];
    if (likely(signal_power < SIGNAL_MIN_POWER)){
    	return false;
    }

    // signal_power/2 > (noise_power/NOISE_BIN_COUNT) * LINEAR_THRESHOLD
    return (signal_power * NOISE_BIN_COUNT) > (noise_power * SIGNAL_BIN_COUNT * snr_threshold);
}

void acoustics_prepare_data(uint8_t target_block){
	uint16_t workspace_idx = (target_block*BLOCK_LEN+BUFFER_LEN-(WORKSPACE_LEN-WORKSPACE_OFFSET*BLOCK_LEN))%BUFFER_LEN;

	for(int i = 0; i < N_HYDROPHONES; i++){
		q15_t* buffer_flat = (q15_t*)hydrophone_buffers[i];
		hydrophone_buffers_circular_unwrap_to_f32(buffer_flat ,processing_workspace[i], WORKSPACE_LEN, BUFFER_LEN, (uint32_t)workspace_idx);
		//normalize
		float32_t scalar = 0;
		arm_mean_f32(processing_workspace[i], WORKSPACE_LEN, &scalar);   // Step 1: compute mean
		arm_offset_f32(processing_workspace[i],-scalar,processing_workspace[i],WORKSPACE_LEN); // Step 2: subtract it
		scalar = 0;
		arm_rms_f32(processing_workspace[i], WORKSPACE_LEN, &scalar);
		scalar = 1/scalar;
		arm_scale_f32(processing_workspace[i],scalar,processing_workspace[i],WORKSPACE_LEN);
		cwt_morlet_magnitude_f32(processing_workspace[i], processing_workspace[i]);
	}
}

void acoustics_process_data(void){
	bool valid_result = false;
	uint8_t max_calculation_retries = 3;

	while(!valid_result && max_calculation_retries--){

		uint8_t valid_buffers = 0;
		uint8_t valid_buffers_array[N_HYDROPHONES] = {0};

		for(int i = 0; i < N_HYDROPHONES; i++){
			valid_buffers_array[i] = hydrophone_valid[i];
			uint8_t n_signal_present_blocks = 0;
			for(int j = 0; j < WORKSPACE_LEN/BLOCK_LEN; j++){
				n_signal_present_blocks += acoustics_signal_present_in_array(&processing_workspace[i][j*BLOCK_LEN]);
			}
			bool valid = (n_signal_present_blocks < WORKSPACE_LEN/BLOCK_LEN);
			valid_buffers += valid;
			valid_buffers_array[i] &= valid;
		}
		bool valid_data = (valid_buffers >= MINIMUM_VALID_BUFFERS);

		float32_t linear_threshold = lerp_threshold;
		uint8_t max_retries = 6;
		uint32_t dead_space = n_lower_average;
		idxs[0] = 0;
		while(((idxs[0] < dead_space) || (idxs[0] > (WORKSPACE_LEN-(WORKSPACE_OFFSET-1)*BLOCK_LEN))) && max_retries--){
			float32_t threshold = dsp_min_max_lerp(processing_workspace[0], WORKSPACE_LEN, linear_threshold, n_upper_average, dead_space);
			idxs[0] = dsp_rl_under_threshold_search(processing_workspace[0],WORKSPACE_LEN, threshold , PROCESSING_PATIENCE);
			linear_threshold *= 2;
		}
		times_of_arrival[0] = (float32_t)idxs[0];

		for(int i = 1; i < N_HYDROPHONES; i++){
			float32_t linear_threshold = lerp_threshold;
			uint8_t max_retries = 6;
			uint32_t dead_space = n_lower_average;
			idxs[i] = 0;
			while(((utils_abs_int32(idxs[0] - idxs[i])) > max_idx_difference) && max_retries--){
				float32_t threshold = dsp_min_max_lerp(processing_workspace[i], WORKSPACE_LEN, linear_threshold, n_upper_average, dead_space);
				idxs[i] = dsp_rl_under_threshold_search(processing_workspace[i],WORKSPACE_LEN, threshold , PROCESSING_PATIENCE);
				linear_threshold *= 2;
			}
			times_of_arrival[i] = (float32_t)idxs[i];
		}

		valid_buffers = 0;

		for(int i = 1; i < N_HYDROPHONES; i++){
			bool valid = (max_idx_difference > utils_abs_int32(idxs[0] - idxs[i]));
			valid_buffers += valid;
			valid_buffers_array[i] &= valid;
		}

		bool valid_idxs = (valid_buffers >= (MINIMUM_VALID_BUFFERS - 1));

		int32_t tdoa_status = 0;
		if(valid_data){
			tdoa_status = TDOA_direction_solve_f32(hydrophone_positions,
												  times_of_arrival,
												  valid_buffers_array,
												  N_HYDROPHONES,
												  direction_of_arrival);
		}

		valid_result = valid_data && acoustics_tdoa_is_valid(direction_of_arrival) && valid_idxs && (tdoa_status == 0);
	}

	if(valid_result){
		float32_t snr = acoustics_estimate_SNR();
		can_send_direction(direction_of_arrival, snr);
		utils_DWT_delay_ms(300);
	}else{
		printf("invalid ping\r\n");
	}
}

void acoustics_clean_data(void){
	for(int i = 0; i < 5; i++){
		utils_clear_array_f32(processing_workspace[i], WORKSPACE_LEN);
	}
}


static bool acoustics_tdoa_is_along_axis(float32_t vec[3]){
	float32_t sum = 0;
	for(int i = 0; i < 3; i++) sum += utils_abs_f32(vec[i]);
	return sum <= 1;
}

static bool acoustics_tdoa_is_too_long(float32_t vec[3]){
	float32_t sum = 0;
	for(int i = 0; i < 3; i++) sum += utils_abs_f32(vec[i]);
	return sum > 1.732050807569f;
}

bool acoustics_tdoa_is_valid(float32_t vec[3]){
	if(acoustics_tdoa_is_along_axis(vec)) return false;

	if(acoustics_tdoa_is_too_long(vec)) return false;

	return true;
}

float32_t acoustics_estimate_SNR(void){
    const uint32_t SIGNAL_BIN_LOW = 14;
    const uint32_t SIGNAL_BIN_HIGH = 17;
    const uint32_t SIGNAL_BIN_COUNT = SIGNAL_BIN_HIGH-SIGNAL_BIN_LOW;
    const uint32_t NOISE_BIN_COUNT = DETECTION_FFT_SIZE/2 - 1 - SIGNAL_BIN_COUNT;

    float32_t noise_power = 0.0f;
    for (int i = 1; i < SIGNAL_BIN_LOW; i++)
        noise_power += magnitude_output_f32[i];
    for (int i = SIGNAL_BIN_HIGH; i < DETECTION_FFT_SIZE / 2; i++)
        noise_power += magnitude_output_f32[i];

    float32_t signal_power = magnitude_output_f32[14] + magnitude_output_f32[15] + magnitude_output_f32[16];

    return (signal_power * NOISE_BIN_COUNT) / (noise_power * SIGNAL_BIN_COUNT);
}

void acoustics_clear_detection_buffer(void){
	utils_clear_array_q15(detection_buffer[0], BLOCK_LEN*2);
}



