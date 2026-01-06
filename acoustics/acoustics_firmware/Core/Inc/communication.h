#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include <stddef.h>
#include "stm32h7xx_hal.h"
#include <stm32h7xx.h>

#ifdef __cplusplus
extern "C"{
#endif

void send_data_can(const void* data, size_t len, FDCAN_HandleTypeDef* can);
void send_data_usart(const void* data, size_t len, UART_HandleTypeDef* usart);

#ifdef __cplusplus
}
#endif

#endif
