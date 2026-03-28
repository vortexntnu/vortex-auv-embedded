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
#include "ad7606_driver.h"
#include "memory_placement.h"

#include "arm_math_types.h"
#include "arm_math.h"

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
#define BLOCK_LEN 			64
#define N_BLOCKS 			  8

#define BUFFER_LEN 			(N_BLOCKS * BLOCK_LEN)
#define N_SACRIFICAL_BLOCKS 3
#define WORKSPACE_OFFSET      3
#define WORKSPACE_LEN 		  ((N_BLOCKS - N_SACRIFICAL_BLOCKS - 1) * BLOCK_LEN)
#define N_HYDROPHONES 		  5

#define DETECTION_FFT_SIZE BLOCK_LEN // match this to your buffer size
#define PROCESSING_FFT_SIZE WORKSPACE_LEN  // match this to your buffer size
#define SAMPLING_FREQUENCY 125000
#define TARGET_FREQUENCY 30000
#define BIN_RESOLUTION ((float)SAMPLE_RATE_HZ / (float)DETECTION_FFT_SIZE)

#define LINEAR_THRESHOLD  3.16 // 5dB => 10^(5/10) ~= 3.16
#define SIGNAL_MIN_POWER 1e-5f

extern SPI_HandleTypeDef* const dout_channel_handles[N_HYDROPHONES];
extern volatile DMA_SPI_ChannelState dma_channel_state[N_HYDROPHONES + 1];

extern q15_t hydrophone_buffers[N_HYDROPHONES][N_BLOCKS][BLOCK_LEN];
extern volatile uint16_t diagnostics_sample;

extern arm_rfft_fast_instance_f32 detection_fft_instance;
extern q15_t detection_buffer[2][BLOCK_LEN];
extern float32_t fft_input_f32[DETECTION_FFT_SIZE];
extern float32_t fft_output_f32[DETECTION_FFT_SIZE * 2];
extern float32_t magnitude_output_f32[DETECTION_FFT_SIZE / 2];

extern arm_rfft_instance_q15 processing_fft_instance;

//extern SPI_HandleTypeDef hspi1;
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
#define MASTER_SPI &hspi6
#define DOUTH &hspi6
#define DOUTA &hspi2
#define DOUTB &hspi5
#define DOUTC &hspi4
#define DOUTD &hspi1
#define DOUTE &hspi3

#define FRSTDATA GPIOE, GPIO_PIN_7
#define BUSY GPIOE, GPIO_PIN_8
#define BUSY_INT GPIO_PIN_8
#define CS GPIOE, GPIO_PIN_9
#define CONVST GPIOE, GPIO_PIN_14


#define GREEN_LED GPIOD, GPIO_PIN_11
#define YELLOW_LED GPIOD, GPIO_PIN_12
#define RED_LED GPIOD, GPIO_PIN_13
/* USER CODE END EM */

void HAL_TIM_MspPostInit(TIM_HandleTypeDef *htim);

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
void MyMDMA_TransferCompleteCallback(MDMA_HandleTypeDef *hmdma);
void dump_python_array(q15_t* arr, int len);
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

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
