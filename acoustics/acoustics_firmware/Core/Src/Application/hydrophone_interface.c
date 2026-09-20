/*
 * adc_management.c
 *
 *  Created on: 24. apr. 2026
 *      Author: vikin
 */

#include <hydrophone_interface.h>
#include "main.h"

#include "ad7606_driver.h"
#include "utils.h"

static struct ad7606_device my_ADC;
static union ad7606_registers ADC_regs;
static struct ad7606_settings ADC_settings;

PLACE_IN_D2_SRAM q15_t hydrophone_buffers[N_HYDROPHONES][N_BLOCKS][BLOCK_LEN];

PLACE_IN_DTCM volatile uint8_t mdma_half = 0;
PLACE_IN_DTCM volatile bool mdma_done_flag = false;

PLACE_IN_D3_SRAM uint8_t hydrophone_valid[N_HYDROPHONES] = {0};
PLACE_IN_DTCM float32_t hydrophone_positions[N_HYDROPHONES][3] = {
		{0.0,0.0,0.0},
		{0.0,0.0,0.0},
		{0.5,0.0,0.0},
		{0.25,0.25,0.354},
		{0.25,-0.25,0.354}
};

PLACE_IN_D3_SRAM q15_t diagnostics_buffer[BLOCK_LEN];
volatile uint16_t diagnostics_sample;
float32_t diagnostics_temp;

PLACE_IN_DTCM uint16_t max_idx_difference;

static void MyMDMA_TransferCompleteCallback(MDMA_HandleTypeDef *hmdma);
static void MyMDMA_ErrorCallback(MDMA_HandleTypeDef *hmdma);

void hydrophone_interface_update_temp(void){
	diagnostics_temp = (float32_t)ad7606_voltage_to_temp(ad7606_reading_to_voltage(&my_ADC,7,diagnostics_sample));
}

uint8_t hydrophone_buffers_get_detection_block_pos_fast(void) {
    uint16_t remaining     = (uint16_t)__HAL_DMA_GET_COUNTER(hspi2.hdmarx);
    uint16_t current_idx = (BUFFER_LEN - remaining) % BUFFER_LEN;
    uint8_t  current_block = current_idx / BLOCK_LEN;

    return (uint8_t)((current_block + N_BLOCKS - WORKSPACE_OFFSET - 1)%N_BLOCKS);
}


/**
 * @brief Unwraps a circular Q15 buffer into a linear float32 array.
 *
 * @param src        Pointer to the circular Q15 buffer
 * @param dst        Pointer to the output float32 array (must be at least data_len long)
 * @param data_len   Number of samples to copy and convert
 * @param buffer_len Total length of the circular buffer
 * @param start_idx  Index of the oldest sample (read head)
 */
void hydrophone_buffers_circular_unwrap_to_f32(q15_t *src, float32_t *dst, uint32_t data_len, uint32_t buffer_len, uint32_t start_idx){
    /* How many samples from start_idx to the end of the buffer */
    uint32_t chunk1 = buffer_len - start_idx;

    if (chunk1 >= data_len) {
        /* No wrap-around: all data sits in one contiguous block */
        arm_q15_to_float(src + start_idx, dst, data_len);
    } else {
        /* Two chunks: tail of buffer, then beginning of buffer */
        uint32_t chunk2 = data_len - chunk1;
        arm_q15_to_float(src + start_idx, dst,          chunk1);
        arm_q15_to_float(src,             dst + chunk1, chunk2);
    }
}

void hydrophone_interface_restart_spi_and_buffers(void){
	int lengths[8] = {
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			0,
			0,
			0,
	};
	int16_t* buffers[8] = {NULL};
	for(int i = 0; i < 5; i++) buffers[i] = (int16_t*)&hydrophone_buffers[i][0][0];

	ad7606_enter_adc_mode(&my_ADC);

	ad7606_init_output_buffers_DMA(&my_ADC, buffers, lengths);
	HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_CPLT_CB_ID,  MyMDMA_TransferCompleteCallback);
	HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_ERROR_CB_ID, MyMDMA_ErrorCallback);
	ad7606_dma_spi_init(&my_ADC, &hdma_spi6_rx, diagnostics_buffer, BLOCK_LEN);
}

void hydrophone_interface_start_datastream(void){
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_4);
	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
}

void hydrophone_interface_stop_datastream(void){
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_4);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);
}

void hydrophone_interface_wait_for_mdma(void){
	uint16_t waiting_patience = GENERAL_TIMEOUT;
	while((mdma_done_flag == false) && waiting_patience--);
	mdma_done_flag = false;
}



void hydrophone_interface_init(float32_t new_hydrophone_positions[N_HYDROPHONES][3]){

	for(int i = 0; i < N_HYDROPHONES; i++){
		hydrophone_valid[i] = false;
		hydrophone_positions[i][0] = new_hydrophone_positions[i][0];
		hydrophone_positions[i][1] = new_hydrophone_positions[i][1];
		hydrophone_positions[i][2] = new_hydrophone_positions[i][2];
	}

	float32_t biggest_distance = 0;
	for(int i = 1; i < N_HYDROPHONES; i++){
		float32_t dist = utils_distance_3d(hydrophone_positions[0],hydrophone_positions[i]);
		if(dist > biggest_distance){
			biggest_distance = dist;
		}
	}
	{
		float32_t idx_distance = (biggest_distance/(WAVE_SPEED)*SAMPLING_FREQUENCY) + BLOCK_LEN/4;
		uint16_t n = (uint16_t)ceilf(idx_distance);
		n--;
		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n |= n >> 16;
		n++;
		max_idx_difference = n;
	}


	struct ad7606_pins pins = {
			.cs = {CS},
			.busy = {BUSY},
			.frstdata = {FRSTDATA},
			.convst = {CONVST},
	};

	union ad7606_spi spi = {
		.by_name = {
			.douta = DOUTA,
			.doutb = DOUTB,
			.doutc = DOUTC,
			.doutd = DOUTD,
			.doute = DOUTE,
			.doutf = NULL,
			.doutg = NULL,
			.douth = DOUTH,
			.sdi   = MASTER_SPI,
		}
	};

	struct ad7606_config config = {
		.status_header = false,
		.external_oversampling_clock = false,
		.dout_format = AD7606_DOUT_8,
		.operation_mode = AD7606_OPERATION_NORMAL,
	};

	struct ad7606_channel channels[8];
	for(int i = 0; i < 8; i++){
//			AD7606_MUX_CTRL_TEMP,
//			AD7606_MUX_CTRL_2V5_REF,
//			AD7606_MUX_CTRL_1V8_ALDO,
//			AD7606_MUX_CTRL_1V8_DLDO,
//			AD7606_MUX_CTRL_V_DRIVE,
//			AD7606_MUX_CTRL_A_GND,
//			AD7606_MUX_CTRL_AV_CC;
		AD7606_CHANNEL_MUX_CTRL mux_ctrl = AD7606_MUX_CTRL_A_IN; // = (i != 8) ? (AD7606_MUX_CTRL_A_IN) : (AD7606_MUX_CTRL_TEMP);
		AD7606_CHANNEL_RANGE range = AD7606_RANGE_SE_PM_12_5V;
		switch(i){
			case(2):
				range = AD7606_RANGE_SE_PM_2_5V;
			break;
			case(5):
				mux_ctrl = AD7606_MUX_CTRL_A_GND;
				range = AD7606_RANGE_SE_PM_2_5V;
			break;
			case(6):
				mux_ctrl = AD7606_MUX_CTRL_AV_CC;
				range = AD7606_RANGE_SE_0_TO_5V;
			break;
			case(7):
				mux_ctrl = AD7606_MUX_CTRL_TEMP;
				range = AD7606_RANGE_SE_PM_2_5V;
			break;
		}
	    struct ad7606_channel ch = {
	        .open_detect    = false,
	        .high_bandwidth = true,
	        .range          = range,
			.gain 			= 0,
			.phase 			= 0,
			.offset 		= 0x80,
			.mux_ctrl 		= mux_ctrl,
	    };
	    channels[i] = ch;
	}

	struct ad7606_oversampling oversampling = {
			.oversampling_ratio = 3, // 2^N oversampling
			.oversampling_padding = 0,
	};

	struct ad7606_digital_diagnostics digital_diagnostics = {
			.rom_CRC_err_en = true,
			.mm_CRC_err_en = false,
			.int_CRC_err_en = false,
			.spi_write_err_en = false,
			.spi_read_err_en = false,
			.busy_stuck_high_err_en = true,
			.clk_fs_os_en = false,
			.interface_check_en = false,
	};

	ADC_settings.config = config;
	ADC_settings.digital_diagnostics = digital_diagnostics;
	ADC_settings.oversampling = oversampling;

	for(int i = 0; i < 8; i++){
		ADC_settings.channels[i] = channels[i];
	}
	my_ADC.cooked = true;
	ad7606_init(&my_ADC, &ADC_regs, pins, spi, &ADC_settings, &diagnostics_sample);
//	if(verbose){
//		printf("ADC initialized. Status register:\t");
//		print_binary(ad7606_check_status(&my_ADC),8);
//		printf("\r\n");
//
//		printf("Digital diagnostics error register:\t");
//		print_binary(ad7606_check_digital_error(&my_ADC),8);
//		printf("\r\n");
//	}

	uint8_t interface_check_result[8];
	ad7606_check_interface(&my_ADC, interface_check_result);
	if(!hydrophone_valid[0]){
		for(int i = 0; i < N_HYDROPHONES; i++){
			hydrophone_valid[i] = interface_check_result[i];
		}
	}
//	if(verbose){
//		printf("Interface check result:\r\n");
//		for(int i = 0; i < 8; i++){
//			printf("Channel V%d: ",i+1);
//		  switch(interface_check_result[i]){
//		  case 0xFF:
//			  printf("Not configured");
//			  break;
//		  case 0:
//			  printf("Fail");
//			  break;
//		  case 1:
//			  printf("Pass");
//			break;
//		  default:
//			printf("Unknown result");
//			break;
//		  }
//		  printf("\t\t\t");
//		  if(i%4 == 3) printf("\r\n");
//		}
//		printf("\r\n");
//	}

	int lengths[8] = {
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			BUFFER_LEN,
			0,
			0,
			0,
	};
	int16_t* buffers[8] = {NULL};
	for(int i = 0; i < 5; i++) buffers[i] = (int16_t*)&hydrophone_buffers[i][0][0];

	ad7606_enter_adc_mode(&my_ADC);

	for(int i = 0; i < 5; i++){
		utils_clear_array_q15(hydrophone_buffers[i][0], BUFFER_LEN);
	}

	ad7606_init_output_buffers_DMA(&my_ADC, buffers, lengths);
	HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_CPLT_CB_ID,  MyMDMA_TransferCompleteCallback);
	HAL_MDMA_RegisterCallback(&hmdma_mdma_channel0_sw_0, HAL_MDMA_XFER_ERROR_CB_ID, MyMDMA_ErrorCallback);
	ad7606_dma_spi_init(&my_ADC, &hdma_spi6_rx, diagnostics_buffer, BLOCK_LEN);

	mdma_half = 0;
	mdma_done_flag = false;
}

static void MyMDMA_TransferCompleteCallback(MDMA_HandleTypeDef *hmdma) {
    // Handle transfer complete
    mdma_done_flag = 1;
    mdma_half = (mdma_half) ? 0 : 1;
}

static void MyMDMA_ErrorCallback(MDMA_HandleTypeDef *hmdma) {
    // Handle error
	Error_Handler();
}
