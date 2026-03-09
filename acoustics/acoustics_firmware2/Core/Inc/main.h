/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.h
  * @brief          : Header for main.c file.
  *                   This file contains the common defines of the application.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __MAIN_H
#define __MAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "stm32h7xx_hal.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "arm_math_types.h"
#include <stdbool.h>
#include <stdint.h>
/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */
typedef enum {
    DMA_SPI_IDLE,
	DMA_SPI_RUNNING,
	DMA_SPI_COMPLETE,
	DMA_SPI_ERROR,
	DMA_SPI_CIRCULAR,
} DMA_SPI_ChannelState;
/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */
#define MASTER_SPI spi_handle_array[5]
#define DOUTH spi_handle_array[5]
#define DOUTA spi_handle_array[1]
#define DOUTB spi_handle_array[4]
#define DOUTC spi_handle_array[3]
#define DOUTD spi_handle_array[0]
#define DOUTE spi_handle_array[2]

#define FRSTDATA GPIOE, GPIO_PIN_7
#define BUSY GPIOE, GPIO_PIN_8
#define BUSY_INT GPIO_PIN_8
#define CS GPIOE, GPIO_PIN_9
#define CONVST GPIOE, GPIO_PIN_14

#define GREEN_LED GPIOD, GPIO_PIN_11
#define YELLOW_LED GPIOD, GPIO_PIN_12

#define BLOCK_LEN 			64
#define N_BLOCKS 			5

#define BUFFER_LEN 			(N_BLOCKS * BLOCK_LEN)
#define N_SACRIFICAL_BLOCKS 2
#define WORKSPACE_LEN 		((N_BLOCKS - N_SACRIFICAL_BLOCKS) * BLOCK_LEN)
#define N_HYDROPHONES 		5

//#define BDMA_RAM __attribute__((section(".SRAM4")))
#define TCM __attribute__((section(".DTCM")))

extern SPI_HandleTypeDef* const spi_handle_array[6];
extern SPI_HandleTypeDef* const dout_channels_array[6];

extern volatile DMA_SPI_ChannelState dma_channel_state[(N_HYDROPHONES) + 1];
extern q15_t hydrophone_buffers[N_HYDROPHONES][N_BLOCKS][BLOCK_LEN];
extern int16_t diagnostics_buffer[N_BLOCKS][BLOCK_LEN];

extern uint16_t buffer_remaining;
extern uint16_t buffer_current_idx;
extern uint16_t buffer_latest_idx;
extern uint16_t buffer_current_block;
extern uint16_t buffer_latest_block;
extern uint16_t buffer_current_block_idx;
extern uint16_t buffer_latest_block_idx;

extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi2;
extern SPI_HandleTypeDef hspi3;
extern SPI_HandleTypeDef hspi4;
extern SPI_HandleTypeDef hspi5;
extern SPI_HandleTypeDef hspi6;
extern DMA_HandleTypeDef hdma_spi1_rx;
extern DMA_HandleTypeDef hdma_spi2_rx;
extern DMA_HandleTypeDef hdma_spi3_rx;
extern DMA_HandleTypeDef hdma_spi4_rx;
extern DMA_HandleTypeDef hdma_spi5_rx;
extern DMA_HandleTypeDef hdma_spi6_rx;
extern DMA_HandleTypeDef hdma_spi6_tx;

extern TIM_HandleTypeDef htim1;

extern UART_HandleTypeDef huart1;
/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */


/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
int _write(int file, char *ptr, int len);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/
#define FRSTDATA_Pin GPIO_PIN_7
#define FRSTDATA_GPIO_Port GPIOE
#define BUSY_Pin GPIO_PIN_8
#define BUSY_GPIO_Port GPIOE
#define BUSY_EXTI_IRQn EXTI9_5_IRQn
#define CS_Pin GPIO_PIN_9
#define CS_GPIO_Port GPIOE
#define CONVST_Pin GPIO_PIN_14
#define CONVST_GPIO_Port GPIOE
#define LEDG_Pin GPIO_PIN_11
#define LEDG_GPIO_Port GPIOD
#define LEDY_Pin GPIO_PIN_12
#define LEDY_GPIO_Port GPIOD
#define LEDR_Pin GPIO_PIN_13
#define LEDR_GPIO_Port GPIOD
#define BDMA_RAM __attribute__((section(".SRAM4")))
/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
