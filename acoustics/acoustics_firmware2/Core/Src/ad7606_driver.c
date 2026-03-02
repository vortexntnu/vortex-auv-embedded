#include "ad7606_driver.h"
#include <string.h>
#include "stm32h7xx_hal_spi.h"
#include <stdio.h>
#include "stm32h7xx.h"
#include "main.h"



static inline void set_config(struct ad7606_config* cfg, uint8_t* config) {
    *config |= (cfg->status_header << 6) & (1 << 6);
    *config |= (cfg->external_oversampling_clock << 5) & (1 << 5);
    *config |= (cfg->dout_format << 3) & (0x4 << 3);
    *config |= (cfg->operation_mode) & (0x4);
}

void ad7606_init(struct ad7606_device* dev,
                 struct ad7606_register* reg,
                 struct ad7606_config* cfg,
                 struct ad7606_channel* channels,
                 SPI_HandleTypeDef* hspi_master) {
    ad7606_set_registers(reg, cfg, channels, 8);
    dev->registers = reg;
    

    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);  // CS LOW

    HAL_SPI_Transmit(hspi_master, (void*)reg, sizeof(*reg), 10);
}

void ad7606_set_registers(struct ad7606_register* registers,
                          struct ad7606_config* config,
                          struct ad7606_channel* channels,
                          uint8_t num_channels) {
    registers->config_address = AD7606_CONFIG_ADDRESS;
    set_config(config, &registers->config);

    for (int i = 0; i < num_channels; i += 2) {
        registers->channel_range[i] =
            (channels[2 * i + 1].range << 4) | (channels[2 * i].range & 0xFF);
    }

    for (int i = 0; i < num_channels; i++) {
        registers->channel_gain[i] = channels->gain;
        registers->channel_offset[i] = channels->offset;
        registers->channel_phase[i] = channels->phase;
    }
}

void ad7606_init_from_arrays(SPI_HandleTypeDef* hspi_master) {

	// copy from register tool start
	const uint8_t ad7606_reg_table[] =
	{
	    0x02, 0x1A,
	    0x03, 0x44,
	    0x04, 0x44,
	    0x05, 0x44,
	    0x06, 0x64,
	    0x07, 0xFF,
	    0x08, 0x03,
	    0x2B, 0x10,
	    0x2C, 0x05,
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
	};

	int len = 40;

	// copy from register tool end

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET); // CS LOW
	HAL_GPIO_WritePin(GREEN_LED, GPIO_PIN_RESET); // Green LED Off
	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_SET); // Yellow LED On
	uint16_t data_frames_16[len];

	for(int i = 0; i < len; i++){
		uint8_t address = ad7606_reg_table[i*2];
		uint8_t register_data = ad7606_reg_table[i*2+1];
		uint16_t data_frame = ad7606_construct_SPI_frame(0, 0, address, register_data);
		data_frames_16[i] = data_frame;
	}

	HAL_SPI_Transmit(hspi_master, (const uint8_t*)data_frames_16, len, 10);

	HAL_GPIO_WritePin(GREEN_LED, GPIO_PIN_SET); // Green LED On
	HAL_GPIO_WritePin(YELLOW_LED, GPIO_PIN_RESET); // Yellow LED Off
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET); // CS High
}

uint16_t ad7606_construct_SPI_frame(uint8_t read_enable, uint8_t read_write, uint8_t adc_register_address, uint8_t data){
	// read_enable set true to read and false to enable write
	// read_write set true to read and false to write
	uint16_t data_frame = 0x00;
	data_frame |= ((read_enable << 15) | (read_write << 14));
	data_frame |= ((adc_register_address & 0x3F) << 8);
	data_frame |= ((data & 0xFF) << 0);

	return data_frame;
}

void ad7606_DOUT8_read_adc(int16_t received_data[6],SPI_HandleTypeDef* spi_handle_array[6]){
	uint16_t data_frame = ad7606_construct_SPI_frame(1, 1, 0x00, 0);
	HAL_GPIO_WritePin(CONVST, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CONVST, GPIO_PIN_RESET);
	while(HAL_GPIO_ReadPin(BUSY));
	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);


	HAL_SPI_Receive_DMA(DOUTA, (uint8_t*)&received_data[0], 1);
	HAL_SPI_Receive_DMA(DOUTB, (uint8_t*)&received_data[1], 1);
	HAL_SPI_Receive_DMA(DOUTC, (uint8_t*)&received_data[2], 1);
	HAL_SPI_Receive_DMA(DOUTD, (uint8_t*)&received_data[3], 1);
	HAL_SPI_Receive_DMA(DOUTE, (uint8_t*)&received_data[4], 1);
	HAL_SPI_TransmitReceive(MASTER_SPI, (const uint8_t*)&data_frame, (uint8_t*)&received_data[5], 1, 10);

	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);
}


// I have no idea why both DOUT8 and DOUT1 work with the same ADC config
void ad7606_DOUT1_read_adc(int16_t received_data[6],SPI_HandleTypeDef* spi_handle_array[6]){
	uint16_t garbage = 0;
	uint16_t data_frame = ad7606_construct_SPI_frame(1, 1, 0x00, 0);
	HAL_GPIO_WritePin(CONVST, GPIO_PIN_SET);
	HAL_GPIO_WritePin(CONVST, GPIO_PIN_RESET);
	while(HAL_GPIO_ReadPin(BUSY));
	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);

	HAL_SPI_Receive_DMA(DOUTA, (uint8_t*)&received_data[0], 1);
	HAL_SPI_Transmit(MASTER_SPI, (const uint8_t*)&data_frame, 1, 10);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
	HAL_SPI_Receive_DMA(DOUTA, (uint8_t*)&received_data[1], 1);
	HAL_SPI_Transmit(MASTER_SPI, (const uint8_t*)&data_frame, 1, 10);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
	HAL_SPI_Receive_DMA(DOUTA, (uint8_t*)&received_data[2], 1);
	HAL_SPI_Transmit(MASTER_SPI, (const uint8_t*)&data_frame, 1, 10);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
	HAL_SPI_Receive_DMA(DOUTA, (uint8_t*)&received_data[3], 1);
	HAL_SPI_Transmit(MASTER_SPI, (const uint8_t*)&data_frame, 1, 10);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
	HAL_SPI_Receive_DMA(DOUTA, (uint8_t*)&received_data[4], 1);
	HAL_SPI_Transmit(MASTER_SPI, (const uint8_t*)&data_frame, 1, 10);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
	HAL_SPI_Receive_DMA(DOUTA, (uint8_t*)&garbage, 1);
	HAL_SPI_Transmit(MASTER_SPI, (const uint8_t*)&data_frame, 1, 10);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
	HAL_SPI_Receive_DMA(DOUTA, (uint8_t*)&garbage, 1);
	HAL_SPI_Transmit(MASTER_SPI, (const uint8_t*)&data_frame, 1, 10);
	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);

	HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
	HAL_SPI_Receive_DMA(DOUTA, (uint8_t*)&received_data[5], 1);
	HAL_SPI_Transmit(MASTER_SPI, (const uint8_t*)&data_frame, 1, 10);

	HAL_GPIO_WritePin(CS, GPIO_PIN_SET);
}

