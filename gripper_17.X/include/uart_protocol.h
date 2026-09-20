#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define UART_PROTO_SOF 0xAAu

#define GRIPPER_STOP_ID          0x0469u
#define GRIPPER_START_ID         0x046Au
#define GRIPPER_PWM_ID           0x046Bu
#define ENCODER_ANGLES_ID        0x046Du

#define UART_PROTO_MAX_PAYLOAD_SIZE 32u

typedef struct
{
    uint16_t id;
    uint8_t len;
    uint8_t payload[UART_PROTO_MAX_PAYLOAD_SIZE];
} uart_packet_t;

void uart_proto_init(void);
void uart_proto_task(void);

bool uart_proto_send_packet(uint16_t id, const uint8_t *payload, uint8_t len);

bool uart_proto_packet_available(void);
bool uart_proto_pop_packet(uart_packet_t *packet);
