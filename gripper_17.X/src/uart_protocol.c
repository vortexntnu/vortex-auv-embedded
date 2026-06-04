#include "uart_protocol.h"

#include "usart.h"

#include <string.h>

typedef enum
{
    RX_WAIT_SOF = 0,
    RX_ID_L,
    RX_ID_H,
    RX_LEN,
    RX_PAYLOAD,
    RX_CHECKSUM
} uart_rx_state_t;

static volatile uart_rx_state_t rx_state = RX_WAIT_SOF;

static uint8_t rx_byte = 0;

static uart_packet_t rx_packet;
static uint8_t rx_payload_index = 0;

static volatile bool packet_ready = false;
static uart_packet_t completed_packet;

static uint8_t checksum_xor(uint16_t id, const uint8_t *payload, uint8_t len)
{
    uint8_t cs = 0;

    cs ^= (uint8_t)(id & 0xFFu);
    cs ^= (uint8_t)((id >> 8) & 0xFFu);
    cs ^= len;

    for (uint8_t i = 0; i < len; ++i)
    {
        cs ^= payload[i];
    }

    return cs;
}

static void rx_reset(void)
{
    rx_state = RX_WAIT_SOF;
    rx_payload_index = 0;
    rx_packet.id = 0;
    rx_packet.len = 0;
}

static void process_rx_byte(uint8_t byte)
{
    switch (rx_state)
    {
        case RX_WAIT_SOF:
            if (byte == UART_PROTO_SOF)
            {
                rx_reset();
                rx_state = RX_ID_L;
            }
            break;

        case RX_ID_L:
            rx_packet.id = byte;
            rx_state = RX_ID_H;
            break;

        case RX_ID_H:
            rx_packet.id |= ((uint16_t)byte << 8);
            rx_state = RX_LEN;
            break;

        case RX_LEN:
            rx_packet.len = byte;

            if (rx_packet.len > UART_PROTO_MAX_PAYLOAD_SIZE)
            {
                rx_reset();
            }
            else if (rx_packet.len == 0)
            {
                rx_state = RX_CHECKSUM;
            }
            else
            {
                rx_payload_index = 0;
                rx_state = RX_PAYLOAD;
            }
            break;

        case RX_PAYLOAD:
            rx_packet.payload[rx_payload_index++] = byte;

            if (rx_payload_index >= rx_packet.len)
            {
                rx_state = RX_CHECKSUM;
            }
            break;

        case RX_CHECKSUM:
        {
            const uint8_t calculated =
                checksum_xor(rx_packet.id, rx_packet.payload, rx_packet.len);

            if (byte == calculated)
            {
                if (!packet_ready)
                {
                    completed_packet = rx_packet;
                    packet_ready = true;
                }
            }

            rx_reset();
            break;
        }

        default:
            rx_reset();
            break;
    }
}

static void usart_read_callback(uintptr_t context)
{
    (void)context;

    USART_ERROR error = SERCOM0_USART_ErrorGet();

    if (error == USART_ERROR_NONE)
    {
        process_rx_byte(rx_byte);
    }
    else
    {
        rx_reset();
    }

    SERCOM0_USART_Read(&rx_byte, 1);
}

void uart_proto_init(void)
{
    rx_reset();

    SERCOM0_USART_ReadCallbackRegister(usart_read_callback, 0);

    SERCOM0_USART_ReceiverEnable();
    SERCOM0_USART_TransmitterEnable();

    SERCOM0_USART_Read(&rx_byte, 1);
}

bool uart_proto_packet_available(void)
{
    return packet_ready;
}

bool uart_proto_pop_packet(uart_packet_t *packet)
{
    if (!packet_ready || packet == NULL)
    {
        return false;
    }

    __disable_irq();

    *packet = completed_packet;
    packet_ready = false;

    __enable_irq();

    return true;
}

bool uart_proto_send_packet(uint16_t id, const uint8_t *payload, uint8_t len)
{
    if (len > UART_PROTO_MAX_PAYLOAD_SIZE)
    {
        return false;
    }

    static uint8_t tx_buf[1u + 2u + 1u + UART_PROTO_MAX_PAYLOAD_SIZE + 1u];

    size_t index = 0;

    tx_buf[index++] = UART_PROTO_SOF;
    tx_buf[index++] = (uint8_t)(id & 0xFFu);
    tx_buf[index++] = (uint8_t)((id >> 8) & 0xFFu);
    tx_buf[index++] = len;

    for (uint8_t i = 0; i < len; ++i)
    {
        tx_buf[index++] = payload[i];
    }

    tx_buf[index++] = checksum_xor(id, payload, len);

    if (SERCOM0_USART_WriteIsBusy())
    {
        return false;
    }

    return SERCOM0_USART_Write(tx_buf, index);
}

void uart_proto_task(void)
{
    /*
     * Optional placeholder.
     *
     * The actual receive work happens in the USART callback.
     * You can remove this if you do not need a task function.
     */
}
