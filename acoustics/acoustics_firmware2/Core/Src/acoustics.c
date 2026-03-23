#include "acoustics.h"
#include "main.h"

#include <stm32h753xx.h>
#include <stm32h7xx_hal_gpio.h>
#include <stm32h7xx_hal_spi.h>
#include <stdint.h>
#include <stdio.h>


PLACE_IN_D3_SRAM const uint16_t EXIT_REGISTER_MODE = 0x0000;
PLACE_IN_D3_SRAM const uint16_t EXIT_ADC_MODE = 0x4100;

// copy from register tool start
const uint8_t ad7606_reg_table[] =
{
    0x02, 0x18,
    0x03, 0x44,
    0x04, 0x44,
    0x05, 0x44,
    0x06, 0x44,
    0x07, 0xFF,
    0x08, 0x03,
    0x09, 0x00,
    0x0A, 0x00,
    0x0B, 0x00,
    0x0C, 0x00,
    0x0D, 0x00,
    0x0E, 0x00,
    0x0F, 0x00,
    0x10, 0x00,
    0x11, 0x80,
    0x12, 0x80,
    0x13, 0x80,
    0x14, 0x80,
    0x15, 0x80,
    0x16, 0x80,
    0x17, 0x80,
    0x18, 0x80,
    0x19, 0x00,
    0x1A, 0x00,
    0x1B, 0x00,
    0x1C, 0x00,
    0x1D, 0x00,
    0x1E, 0x00,
    0x1F, 0x00,
    0x20, 0x00,
    0x21, 0x01,
    0x22, 0x00,
    0x23, 0x00,
    0x24, 0x00,
    0x28, 0x00,
    0x29, 0x00,
    0x2A, 0x00,
    0x2B, 0x00,
    0x2C, 0x00,
};
#define N_REGS (sizeof(ad7606_reg_table)/2)

// copy from register tool end

// Interrupts and shit


// Functions and shit
uint16_t acoustics_construct_SPI_frame(uint8_t read_enable, uint8_t read_write, uint8_t adc_register_address, uint8_t data){

	// read_enable set true to read and false to enable write
	// read_write set true to read and false to write
	uint16_t data_frame = 0x00;
	data_frame |= ((read_enable << 15) | (read_write << 14));
	data_frame |= ((adc_register_address & 0x3F) << 8);
	data_frame |= ((data & 0xFF) << 0);

	return data_frame;
}

int _write(int file, char *ptr, int len)
{
    // Only send if ITM is enabled and debugger connected
    if ((CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk) &&
        (ITM->TCR & ITM_TCR_ITMENA_Msk))
    {
        for (int i = 0; i < len; i++)
            ITM_SendChar((uint8_t)ptr[i]);
    }
    return len;
}

void start_convst(void){
	HAL_GPIO_WritePin(CONVST, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CONVST, GPIO_PIN_RESET);
}

uint8_t fast_get_detection_block_pos(void) {
    uint16_t remaining     = (uint16_t)__HAL_DMA_GET_COUNTER(hspi2.hdmarx);
    uint16_t current_idx = (BUFFER_LEN - remaining) % BUFFER_LEN; // claude might be wrong here
    uint8_t  current_block = current_idx / BLOCK_LEN;

    return (uint8_t)((current_block + N_BLOCKS - WORKSPACE_OFFSET)%N_BLOCKS);
}

void init_hyrdophone_buffers(void){
	for(int i = 0; i < N_HYDROPHONES; i++){
		if(HAL_SPI_GetState(dout_channel_handles[i]) != HAL_SPI_STATE_READY){
		    HAL_SPI_DMAStop(dout_channel_handles[i]);
		}
		dout_channel_handles[i]->hdmarx->Init.Mode = DMA_CIRCULAR;
		HAL_DMA_Init(dout_channel_handles[i]->hdmarx);

		HAL_SPI_Receive_DMA(dout_channel_handles[i], (uint8_t*)&hydrophone_buffers[i], BUFFER_LEN);
		dma_channel_state[i] = DMA_SPI_CIRCULAR;
	}
}

bool all_dma_complete(void){
	bool all_complete = true;
	for(int i = 0; i < N_HYDROPHONES; i++){
		all_complete &= (dma_channel_state[i] == DMA_SPI_COMPLETE);
	}
	return all_complete;
}

bool all_dma_idle(void){
	bool all_idle = true;
	for(int i = 0; i < N_HYDROPHONES; i++){
		all_idle &= (dma_channel_state[i] == DMA_SPI_IDLE);
	}
	return all_idle;
}

bool dma_busy(void){
	bool busy = false;
	for(int i = 0; i < N_HYDROPHONES; i++){
		busy |= (dma_channel_state[i] == DMA_SPI_RUNNING);
	}
	return busy;
}

bool dma_error(void){
	bool error = false;
	for(int i = 0; i < N_HYDROPHONES; i++){
		error |= (dma_channel_state[i] == DMA_SPI_ERROR);
	}
	return error;
}

void change_buffers_to_normal(void){
    for(int i = 0; i < N_HYDROPHONES; i++){
        HAL_SPI_DMAStop(dout_channel_handles[i]);
        dout_channel_handles[i]->hdmarx->Init.Mode = DMA_NORMAL;
        HAL_DMA_Init(dout_channel_handles[i]->hdmarx);
        dma_channel_state[i] = DMA_SPI_IDLE;
    }
}

// should really just be used for debugging and testing as it is not optimal
void acoustics_init_from_arrays(SPI_HandleTypeDef* hspi_master) {

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_SET); // Yellow LED On
	uint16_t data_frames_16[N_REGS];

	for(int i = 0; i < N_REGS; i++){
		uint8_t address = ad7606_reg_table[i*2];
		uint8_t register_data = ad7606_reg_table[i*2+1];
		uint16_t data_frame = acoustics_construct_SPI_frame(0, 0, address, register_data);
		data_frames_16[i] = data_frame;
	}

	HAL_SPI_Transmit(hspi_master, (const uint8_t*)data_frames_16, N_REGS, 10);

	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_RESET); // Yellow LED Off
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS High
}

void acoustics_init_from_arrays_debug(SPI_HandleTypeDef* hspi_master_send,SPI_HandleTypeDef* hspi_master_receive) {

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_SET); // Yellow LED On
	uint8_t data_frames[N_REGS*2];

	printf("\r\n");
	{
		uint16_t data_frame = acoustics_construct_SPI_frame(0, 1, 0x01, 0x00);
		HAL_SPI_Transmit(hspi_master_send, (const uint8_t*)&data_frame,  1, 10);
	}


	for(int i = 0; i < N_REGS; i++){
		uint8_t address = ad7606_reg_table[i*2];
		uint8_t register_data = ad7606_reg_table[i*2+1];
		uint16_t data_frame = acoustics_construct_SPI_frame(0, 0, address, register_data);
        uint16_t data_frame_received;
		data_frames[2*i] =  (data_frame >> 8) & 0xFF;
		data_frames[2*i + 1] =  (data_frame) & 0xFF;

		HAL_SPI_Receive_DMA(hspi_master_receive, (uint8_t*)&data_frame_received,  1);
		HAL_SPI_Transmit(hspi_master_send, (const uint8_t*)&data_frame,  1, 10);
		printf("Received: 0x%04X\r\n", data_frame_received);
		printf("Sending Address: %02X, Data: %02X\r\n", data_frames[2*i], data_frames[2*i + 1]);
	}

	uint8_t address = 0x00;
	uint8_t register_data = 0x00;
	uint16_t data_frame = acoustics_construct_SPI_frame(0, 0, address, register_data);
	uint16_t data_frame_received;

	HAL_SPI_Receive_DMA(hspi_master_receive, (uint8_t*)&data_frame_received,  1);
	HAL_SPI_Transmit(hspi_master_send, (const uint8_t*)&data_frame,  1, 10);
	printf("Received: 0x%04X\r\n", data_frame_received);
	printf("Sending Address: %02X, Data: %02X\r\n", address, register_data);

	printf("\r\n");

	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_RESET); // Yellow LED Off
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS High
}

void acoustics_DOUT_read_adc(int16_t received_data[8],const int dout_n){

	if(8 % dout_n){
		Error_Handler(); //dout_n should only be either 8, 4, 2 or 1
	}

	uint16_t data_frames[8/dout_n];
	for(int i = 0; i < 8/dout_n; i++){
		 data_frames[i] = 0x0000;
	}

	HAL_StatusTypeDef status_array[5];
	for(int i = 0; i < 5; i++){
		 data_frames[i] = HAL_OK;
	}

	SPI_HandleTypeDef* dout_channels[6] = {
			DOUTA,
			DOUTB,
			DOUTC,
			DOUTD,
			DOUTE,
			DOUTH
	};

	int n = dout_n;
	if(dout_n == 8){
		n = 5;
	}

	for(int i = 0; i < n; i++){
		status_array[i] = HAL_SPI_Receive_DMA(dout_channels[i], (uint8_t*)&received_data[i*8/dout_n], 8/dout_n);
	}

	HAL_GPIO_WritePin(CONVST, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CONVST, GPIO_PIN_RESET);
	while(HAL_GPIO_ReadPin(BUSY));

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
	if(dout_n == 8){
		HAL_SPI_TransmitReceive(MASTER_SPI, (const uint8_t*)&data_frames[0], (uint8_t*)&received_data[7], 1, 10);
	}else{
		HAL_SPI_Transmit(MASTER_SPI, (const uint8_t*)&data_frames[0], 8/dout_n, 10);
	}
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);

	bool busy = true;
	while(busy){
		busy = false;
		for(int i = 0; i < 5; i++){
			busy |= (status_array[i] == HAL_BUSY);
		}
	}

}
