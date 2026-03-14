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

// Exported Variables
//__attribute__((section(".SRAM4")))
extern const uint16_t EXIT_REGISTER_MODE;
extern const uint16_t EXIT_ADC_MODE ;

// Exported Defines
#define READ_CONVST EXIT_REGISTER_MODE

// Exported Functions
uint16_t acoustics_construct_SPI_frame(uint8_t read_enable, uint8_t read_write, uint8_t adc_register_address, uint8_t data);

bool all_dma_complete(void);
bool all_dma_idle(void);
bool dma_error(void);
bool dma_busy(void);

int _write(int file, char *ptr, int len);

void start_convst(void);

void update_buffer_idx(void);
uint8_t fast_get_detection_block_pos(void);

void read_hydrophone_buffers_at_idx(q15_t data_array[N_HYDROPHONES], uint16_t idx);
void read_hydrophone_block_at_idx(q15_t data_array[N_HYDROPHONES],uint16_t block, uint16_t idx);
void read_newest_hydrophone_data(q15_t data_array[N_HYDROPHONES]);

void init_hyrdophone_buffers(void);

void acoustics_init_from_arrays(SPI_HandleTypeDef* hspi_master);
void acoustics_init_from_arrays_debug(SPI_HandleTypeDef* hspi_master_send,SPI_HandleTypeDef* hspi_master_receive);

void acoustics_DOUT_read_adc(int16_t received_data[8], int dout_n);
void acoustics_DOUT8_read_adc(int16_t received_data[8]);
void acoustics_DOUT4_read_adc(int16_t received_data[8]);
void acoustics_DOUT2_read_adc(int16_t received_data[8]);
void acoustics_DOUT1_read_adc(int16_t received_data[8]);
void acoustics_read_registers(SPI_HandleTypeDef* hspi_master_send, SPI_HandleTypeDef* hspi_master_receive);

#endif /* INC_ACOUSTICS_H_ */
