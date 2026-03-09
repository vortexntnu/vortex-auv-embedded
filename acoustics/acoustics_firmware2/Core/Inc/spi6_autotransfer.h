/*
 * spi6_autotransfer.h
 *
 *  Created on: 9. mar. 2026
 *      Author: vikin
 */

#ifndef INC_SPI6_AUTOTRANSFER_H_
#define INC_SPI6_AUTOTRANSFER_H_

#include "main.h"

// spi6_autotransfer.h additions:
void SPI6_DirectInit(void);
void SPI6_Kick(void);
// RX result is written here by the EOT ISR
extern volatile uint16_t spi6_tx_buffer;
extern volatile uint16_t spi6_rx_buffer;

#endif /* INC_SPI6_AUTOTRANSFER_H_ */
