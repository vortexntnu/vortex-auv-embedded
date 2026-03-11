/*
 * fast_spi.c
 *
 *  Created on: 9. mar. 2026
 *      Author: vikin
 */

#include "fast_spi.h"
#include <stdio.h>

volatile uint16_t spi6_tx_buffer = 0x0000;
volatile uint16_t spi6_rx_buffer = 0x0000;

extern SPI_HandleTypeDef hspi6;

void SPI6_DirectInit(void)
{
    HAL_SPI_Abort(MASTER_SPI);  // ensure clean state

    SPI6->CR1 &= ~SPI_CR1_SPE;
    SPI6->CR2  =  1U;       // TSIZE = 1 frame

    // Enable EOT interrupt
    SPI6->IER |= SPI_IER_EOTIE;

    HAL_GPIO_WritePin(CS, GPIO_PIN_RESET);
}

// Called at 125kHz — fire and forget
void SPI6_Kick(void)
{
    SPI6->CR1 &= ~SPI_CR1_SPE;
    SPI6->CR2  =  1U;           // reload TSIZE
    SPI6->CR1 |=  SPI_CR1_SPE;

    *(volatile uint16_t*)&SPI6->TXDR = 0x0000;
    SPI6->CR1 |= SPI_CR1_CSTART;
}
