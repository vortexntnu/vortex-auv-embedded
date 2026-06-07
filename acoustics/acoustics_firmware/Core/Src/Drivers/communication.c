
#include "communication.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef CAN_MAX_PAYLOAD
#define CAN_MAX_PAYLOAD 64  // set 64 if using CAN FD
#endif

// Pick an arbitration ID suitable for your app
#ifndef CAN_DEFAULT_STD_ID
#define CAN_DEFAULT_STD_ID 0x720
#endif

static uint32_t len_to_dlc(uint32_t n) {
    if (n <= 8)
        return n;
    if (n <= 12)
        return 9;
    if (n <= 16)
        return 10;
    if (n <= 20)
        return 11;
    if (n <= 24)
        return 12;
    if (n <= 32)
        return 13;
    if (n <= 48)
        return 14;
    return 15;
}

static uint32_t dlc_to_len(uint32_t dlc) {
    if (dlc <= 8)
        return dlc;
    static const uint8_t fd_lut[16] = {0, 1,  2,  3,  4,  5,  6,  7,
                                       8, 12, 16, 20, 24, 32, 48, 64};
    return fd_lut[dlc & 0x0F];
}

static bool fdcan_wait_tx_space(FDCAN_HandleTypeDef* h) {
    for (volatile uint32_t i = 0; i < 1000000UL; ++i) {
        if (HAL_FDCAN_GetTxFifoFreeLevel(h) > 0)
            return true;
    }
    return false;
}

void send_data_can(const void* data, size_t len, FDCAN_HandleTypeDef* can) {
    if (!data || len == 0)
        return;

    uint8_t* p = (uint8_t*)data;

    FDCAN_TxHeaderTypeDef hdr = {0};
    hdr.Identifier = CAN_DEFAULT_STD_ID;
    hdr.IdType = FDCAN_STANDARD_ID;
    hdr.TxFrameType = FDCAN_DATA_FRAME;
    hdr.FDFormat = FDCAN_FD_CAN;
    hdr.BitRateSwitch = FDCAN_BRS_OFF;
    hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    hdr.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    hdr.MessageMarker = 0;

    while (len > 0) {
        uint32_t chunk =
            (len < CAN_MAX_PAYLOAD) ? (uint32_t)len : (uint32_t)CAN_MAX_PAYLOAD;
        uint32_t dlc = len_to_dlc(chunk);
        uint32_t dl_sz = dlc_to_len(dlc);
        uint8_t buf[64] = {0};

        memcpy(buf, p, chunk);

        hdr.DataLength = dlc << 16;

        if (!fdcan_wait_tx_space(can)) {
            break;
        }

        if (HAL_FDCAN_AddMessageToTxFifoQ(can, &hdr, buf) != HAL_OK) {
            break;
        }

        p += chunk;
        len -= chunk;
    }
}

void send_data_usart(const void* data, size_t len, UART_HandleTypeDef* usart) {
    if (!data || len == 0)
        return;
#if (__DCACHE_PRESENT == 1U) && (__DCACHE_USED == 1U)
    // Clean D-Cache for the TX buffer so DMA sees the latest data
    SCB_CleanDCache_by_Addr((uint32_t*)(((uintptr_t)data) & ~0x1F),
                            (int32_t)(len + 32));
#endif
    HAL_UART_Transmit_DMA(usart, (uint8_t*)data, (uint16_t)len);
}
