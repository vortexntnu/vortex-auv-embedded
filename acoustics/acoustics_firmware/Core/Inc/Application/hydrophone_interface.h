/*
 * adc_management.h
 *
 *  Created on: 24. apr. 2026
 *      Author: vikin
 */

#ifndef INC_APPLICATION_HYDROPHONE_INTERFACE_H_
#define INC_APPLICATION_HYDROPHONE_INTERFACE_H_

#include "main.h"

extern q15_t hydrophone_buffers[N_HYDROPHONES][N_BLOCKS][BLOCK_LEN];

extern uint8_t hydrophone_valid[N_HYDROPHONES];
extern float32_t hydrophone_positions[N_HYDROPHONES][3];

extern volatile uint8_t mdma_half;
extern volatile bool mdma_done_flag;

extern uint16_t max_idx_difference;

void hydrophone_interface_update_temp(void);
void hydrophone_interface_init(float32_t new_hydrophone_positions[N_HYDROPHONES][3]);
void hydrophone_interface_restart_spi_and_buffers(void);
void hydrophone_buffers_circular_unwrap_to_f32(q15_t *src, float32_t *dst, uint32_t data_len, uint32_t buffer_len, uint32_t start_idx);
uint8_t hydrophone_buffers_get_detection_block_pos_fast(void);

void hydrophone_interface_stop_datastream(void);
void hydrophone_interface_start_datastream(void);

void hydrophone_interface_wait_for_mdma(void);

#endif /* INC_APPLICATION_HYDROPHONE_INTERFACE_H_ */
