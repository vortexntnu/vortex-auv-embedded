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
#include <stm32h7xx.h>
#include <main.h>

// Exported Variables
//__attribute__((section(".SRAM4")))
extern uint16_t EXIT_REGISTER_MODE __attribute__((section(".SRAM4")));
extern uint16_t EXIT_ADC_MODE __attribute__((section(".SRAM4")));

// Exported Defines
#define READ_CONVST EXIT_REGISTER_MODE

// Exported Functions
uint16_t acoustics_construct_SPI_frame(uint8_t read_enable, uint8_t read_write, uint8_t adc_register_address, uint8_t data);

void acoustics_init_from_arrays(SPI_HandleTypeDef* hspi_master);

void acoustics_init_from_arrays_debug(SPI_HandleTypeDef* hspi_master_send,SPI_HandleTypeDef* hspi_master_receive);

void acoustics_DOUT_read_adc(int16_t received_data[8], int dout_n);

void acoustics_DOUT8_read_adc(int16_t received_data[8]);

void acoustics_DOUT4_read_adc(int16_t received_data[8]);

void acoustics_DOUT2_read_adc(int16_t received_data[8]);

void acoustics_DOUT1_read_adc(int16_t received_data[8]);

void acoustics_read_registers(SPI_HandleTypeDef* hspi_master_send, SPI_HandleTypeDef* hspi_master_receive);

#endif /* INC_ACOUSTICS_H_ */
