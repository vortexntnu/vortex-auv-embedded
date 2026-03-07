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

/* USER CODE END Includes */

/* Exported types ------------------------------------------------------------*/
/* USER CODE BEGIN ET */

/* USER CODE END ET */

/* Exported constants --------------------------------------------------------*/
/* USER CODE BEGIN EC */

/* USER CODE END EC */

/* Exported macro ------------------------------------------------------------*/
/* USER CODE BEGIN EM */
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

/* USER CODE END EM */

/* Exported functions prototypes ---------------------------------------------*/
void Error_Handler(void);

/* USER CODE BEGIN EFP */
int _write(int file, char *ptr, int len);
/* USER CODE END EFP */

/* Private defines -----------------------------------------------------------*/

/* USER CODE BEGIN Private defines */

/* USER CODE END Private defines */

#ifdef __cplusplus
}
#endif

#endif /* __MAIN_H */
