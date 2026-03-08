/**
 * spi6_dma.h
 */

#ifndef SPI6_DMA_H
#define SPI6_DMA_H

#include <stdint.h>

/**
 * @brief  Initialise SPI6, BDMA channels, GPIO, and NVIC.
 *         Call once before any transfer.
 */
void SPI6_DMA_Init(void);

/**
 * @brief  Start a single 16-bit full-duplex transfer.
 *         CS is asserted automatically and released in the RX-complete ISR.
 *         Safe to call from an ISR. Do not call while SPI6_IsBusy() is true.
 *
 * @param  tx_data  16-bit word to transmit
 */
void SPI6_Transfer16(uint16_t tx_data);

/**
 * @brief  Returns 1 if a transfer is currently in progress, 0 if idle.
 */
uint8_t SPI6_IsBusy(void);

/**
 * @brief  User-defined callback — implement this in your application.
 *         Called from the BDMA RX-complete ISR after CS is deasserted.
 *         Keep it short.
 *
 * @param  rx_data  The 16-bit word received during the last transfer
 */
void SPI6_TransferCpltCallback(uint16_t rx_data);

#endif /* SPI6_DMA_H */
