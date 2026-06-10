#include "can_handler.h"

bool can_send_frame(uint16_t can_id, const uint8_t* payload, uint8_t length) {
    if (length > 64U) {
        return false;
    }

    if (CAN1_TxFifoFreeLevelGet() == 0U) {
        return false;
    }

    CAN_TX_BUFFER tx = {0};

    tx.id = can_id;
    tx.dlc = length;

    tx.xtd = 0U;
    tx.rtr = 0U;

    tx.fdf = 1U;
    tx.brs = 0U;

    if (payload != NULL && length > 0U) {
        memcpy(tx.data, payload, length);
    }

    return CAN1_MessageTransmitFifo(1U, &tx);
}

bool send_flt_event(uint8_t channel) {
    uint8_t payload[2] = {channel, 0x01U};
    return can_send_frame(CAN_ID_FLT_EVENT, payload, 2U);
}

bool send_pgood_event(uint8_t channel) {
    uint8_t payload[2] = {channel, 0x02U};
    return can_send_frame(CAN_ID_PGOOD_EVENT, payload, 2U);
}

bool send_killswitch_event(uint8_t channel) {
    (void)channel;
    return can_send_frame(CAN_ID_KILLSWITCH_EVENT, NULL, 0U);
}

bool send_current_measurements(float I_arr[8]) {
    uint8_t payload[32];

    for (size_t i = 0U; i < 8U; i++) {
        memcpy(&payload[i * sizeof(float)], &I_arr[i], sizeof(float));
    }

    return can_send_frame(CAN_ID_CURRENT_MEASUREMENTS, payload, 32U);
}

void dispatch_hw_events(hw_event_flags_t* hw) {
    uint8_t snapshot;

    snapshot = hw->flt_pending_mask;
    hw->flt_pending_mask = 0U;

    for (uint8_t i = 0U; i < 8U; i++) {
        if ((snapshot & (1U << i)) != 0U) {
            send_flt_event(i);
        }
    }

    snapshot = hw->pgood_pending_mask;
    hw->pgood_pending_mask = 0U;

    for (uint8_t i = 0U; i < 8U; i++) {
        if ((snapshot & (1U << i)) != 0U) {
            send_pgood_event(i);
        }
    }

    if (hw->killswitch_pending_mask != 0U) {
        hw->killswitch_pending_mask = 0U;
        send_killswitch_event(0U);
    }
}
