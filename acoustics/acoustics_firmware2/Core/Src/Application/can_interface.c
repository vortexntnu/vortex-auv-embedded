/*
 * can_interface.c
 *
 *  Created on: 24. apr. 2026
 *      Author: vikin
 */


#include "can_interface.h"
#include "main.h"
#include <string.h>

#include "hydrophone_interface.h"
#include "acoustics.h"

/* ── Latest received message, written in ISR, read in handler ─*/
static volatile FDCAN_RxHeaderTypeDef _rx_header;
static volatile uint8_t               _rx_data[8];
static volatile uint8_t               _rx_pending = 0;

/*
 * Private function declarations
 */
void can_handle_restart(void);
void can_handle_stop(void);
void can_handle_start(void);
void can_handle_checkup(void);
void can_handle_change_lower_n(const uint8_t *data);
void can_handle_change_upper_n(const uint8_t *data);
void can_handle_change_snr_threshold(const uint8_t *data);
void can_handle_change_lerp_threshold(const uint8_t *data);

/* ═══════════════════════════════════════════════════════════════
 * INIT
 * ═══════════════════════════════════════════════════════════════*/
void can_init(void)
{
    /* ── Hardware filter: classic range filter 0x330–0x33F ───── */
    FDCAN_FilterTypeDef filter = {0};
    filter.IdType       = FDCAN_STANDARD_ID;
    filter.FilterIndex  = 0;
    filter.FilterType   = FDCAN_FILTER_RANGE;          /* accept ID1..ID2  */
    filter.FilterConfig = FDCAN_FILTER_TO_RXFIFO0;
    filter.FilterID1    = CAN_RX_FILTER_ID_LOW;
    filter.FilterID2    = CAN_RX_FILTER_ID_HIGH;

    if (HAL_FDCAN_ConfigFilter(CAN_HANDLE, &filter) != HAL_OK)
        Error_Handler();

    /* Reject everything that doesn't match a filter */
    if (HAL_FDCAN_ConfigGlobalFilter(CAN_HANDLE,
                                     FDCAN_REJECT,
                                     FDCAN_REJECT,
                                     FDCAN_FILTER_REMOTE,
                                     FDCAN_FILTER_REMOTE) != HAL_OK)
        Error_Handler();

    HAL_NVIC_SetPriority(FDCAN1_IT0_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(FDCAN1_IT0_IRQn);

    HAL_FDCAN_ConfigInterruptLines(CAN_HANDLE,
                                   FDCAN_IT_RX_FIFO0_NEW_MESSAGE,
                                   FDCAN_INTERRUPT_LINE0);

    /* Enable RX FIFO 0 new-message interrupt */
    if (HAL_FDCAN_ActivateNotification(CAN_HANDLE,
                                       FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0) != HAL_OK)
        Error_Handler();

    HAL_FDCAN_Start(CAN_HANDLE);
    can_ok();
}

/* ═══════════════════════════════════════════════════════════════
 * INTERRUPT CALLBACK  (called by HAL from ISR context)
 * ═══════════════════════════════════════════════════════════════*/
void HAL_FDCAN_RxFifo0Callback(FDCAN_HandleTypeDef *hfdcan, uint32_t RxFifo0ITs)
{
    if (hfdcan->Instance != hfdcan1.Instance)
        return;

    if (RxFifo0ITs & FDCAN_IT_RX_FIFO0_NEW_MESSAGE)
    {
        /* Read into shadow buffers — keep ISR short */
        HAL_FDCAN_GetRxMessage(hfdcan,
                               FDCAN_RX_FIFO0,
                               (FDCAN_RxHeaderTypeDef *)&_rx_header,
                               (uint8_t *)_rx_data);

        _rx_pending     	= 1;
        prev_program_state  = program_state;
        program_state		= STATE_CAN_COMMUNICATE;
    }
}

/* ═══════════════════════════════════════════════════════════════
 * REQUEST HANDLER  (call this from STATE_CAN_COMMUNICATE in your
 *                   state machine, then restore the previous state)
 * ═══════════════════════════════════════════════════════════════*/
void can_handle_requests(void)
{
    if (!_rx_pending)
        return;

    /* Snapshot and clear the pending flag immediately */
    FDCAN_RxHeaderTypeDef hdr;
    uint8_t data[8];

    __disable_irq();
    memcpy(&hdr,  (void *)&_rx_header, sizeof(hdr));
    memcpy(data,  (void *)_rx_data,    sizeof(data));
    _rx_pending = 0;
    __enable_irq();

    uint32_t id = hdr.Identifier;

    /* ── Route by received ID ─────────────────────────────── */
    switch (id)
    {
        case CAN_RESTART_ID:
            can_handle_restart();
            break;

        case CAN_STOP_ID:
            can_handle_stop();
            break;

        case CAN_START_ID:
            can_handle_start();
            break;

        case CAN_CHECKUP_ID:
            can_handle_checkup();
            break;

        case CAN_CHANGE_LOWER_N_ID:
            can_handle_change_lower_n(data);
            break;

        case CAN_CHANGE_UPPER_N_ID:
            can_handle_change_upper_n(data);
            break;

        case CAN_CHANGE_SNR_THRESHOLD_ID:
            can_handle_change_snr_threshold(data);
            break;

        case CAN_CHANGE_LERP_THRESHOLD_ID:
            can_handle_change_lerp_threshold(data);
            break;

        default:
            can_unrecognized();
            break;
    }
}

/* ═══════════════════════════════════════════════════════════════
 * THE ACTUAL DIRECTION
 * ═══════════════════════════════════════════════════════════════*/

void can_send_direction(float32_t vec[3], float32_t weight)
{
    FDCAN_TxHeaderTypeDef txHeader;
    uint8_t txData[16];  // 4 x float32 = 16 bytes

    memcpy(txData, vec, 12);
    memcpy(txData + 12, &weight, 4);

    txHeader.Identifier          = CAN_DIRECTION_ID;
    txHeader.IdType              = FDCAN_STANDARD_ID;
    txHeader.TxFrameType         = FDCAN_DATA_FRAME;
    txHeader.DataLength          = FDCAN_DLC_BYTES_16;
    txHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    txHeader.BitRateSwitch       = FDCAN_BRS_OFF;
    txHeader.FDFormat            = FDCAN_FD_CAN;
    txHeader.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    txHeader.MessageMarker       = 0;

    uint16_t timouter = GENERAL_TIMEOUT;

    while ((HAL_FDCAN_GetTxFifoFreeLevel(CAN_HANDLE) == 0) && timouter--);

    HAL_StatusTypeDef status = HAL_FDCAN_AddMessageToTxFifoQ(CAN_HANDLE, &txHeader, txData);
    __NOP();
}

/* ═══════════════════════════════════════════════════════════════
 * INTERNAL TX HELPER
 * ═══════════════════════════════════════════════════════════════*/
static void _can_send_classic(uint32_t id, const uint8_t *data, uint32_t dlc)
{
    FDCAN_TxHeaderTypeDef hdr = {0};
    hdr.Identifier          = id;
    hdr.IdType              = FDCAN_STANDARD_ID;
    hdr.TxFrameType         = FDCAN_DATA_FRAME;
    hdr.DataLength          = dlc;
    hdr.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    hdr.BitRateSwitch       = FDCAN_BRS_OFF;
    hdr.FDFormat            = FDCAN_CLASSIC_CAN;
    hdr.TxEventFifoControl  = FDCAN_NO_TX_EVENTS;
    hdr.MessageMarker       = 0;

    uint16_t timeout = GENERAL_TIMEOUT;
    while ((HAL_FDCAN_GetTxFifoFreeLevel(CAN_HANDLE) == 0) && timeout--)
        ;

    if (HAL_FDCAN_AddMessageToTxFifoQ(CAN_HANDLE, &hdr, (uint8_t *)data) != HAL_OK)
        Error_Handler();
}

/* ═══════════════════════════════════════════════════════════════
 * STATUS TX RESPONSES
 * ═══════════════════════════════════════════════════════════════*/
void can_ok(void)
{
    uint8_t d[8] = { 'S', 'O', 'U', 'N', 'D', ' ', 'O', 'K' };
    _can_send_classic(CAN_OK_ID, d, FDCAN_DLC_BYTES_8);
}

void can_stopped(void)
{
    uint8_t d[8] = { 'S', 'T', 'O', 'P', 'E', 'D', 0, 0 };
    _can_send_classic(CAN_STOPPED_ID, d, FDCAN_DLC_BYTES_8);
}

void can_errored(void)
{
    uint8_t d[8] = { 'E', 'R', 'R', 'O', 'R', 0, 0 ,0 };
    _can_send_classic(CAN_ERRORED_ID, d, FDCAN_DLC_BYTES_8);
}

void can_unrecognized(void)
{
    uint8_t d[8] = { '?', 0, 0, 0, 0, 0, 0, 0 };
    _can_send_classic(CAN_UNRECOGNIZED_REQUEST_ID, d, FDCAN_DLC_BYTES_8);
}

/* ═══════════════════════════════════════════════════════════════
 * REQUEST HANDLERS  — fill in your application logic here
 * ═══════════════════════════════════════════════════════════════*/
void can_handle_restart(void)
{
	HAL_NVIC_SystemReset();
}

void can_handle_stop(void)
{
	prev_program_state = STATE_STOPPED;
	HAL_GPIO_WritePin(GREEN_LED, GPIO_PIN_RESET);
	hydrophone_interface_stop_datastream();
    can_stopped();
}

void can_handle_start(void)
{
	prev_program_state = STATE_SEARCHING;
	HAL_GPIO_WritePin(GREEN_LED, GPIO_PIN_SET);
	hydrophone_interface_restart_spi_and_buffers();
	hydrophone_interface_start_datastream();
    can_ok();
}

void can_handle_checkup(void)
{
    can_ok();
}

void can_handle_change_lower_n(const uint8_t *data)
{
    /* Payload: 4-byte little-endian float or uint32 — adapt as needed */
	uint16_t value;
    memcpy(&value, data, sizeof(uint16_t));
    n_lower_average = value;
    can_ok();
}

void can_handle_change_upper_n(const uint8_t *data)
{
	uint16_t value;
    memcpy(&value, data, sizeof(uint16_t));
    n_upper_average = value;
    can_ok();
}

void can_handle_change_snr_threshold(const uint8_t *data)
{
    float32_t value;
    memcpy(&value, data, sizeof(float32_t));
    snr_threshold = value;
    can_ok();
}

void can_handle_change_lerp_threshold(const uint8_t *data)
{
    float32_t value;
    memcpy(&value, data, sizeof(float32_t));
    lerp_threshold = value;
    can_ok();
}
