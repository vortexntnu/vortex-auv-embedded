/*
 Platform:
    ATSAMC21

 Company:
    Vortex NTNU.

 Author:
    Markus Sandvik

 File Name:
    can_facade.c
 */

#include "definitions.h"
#include "can_facade.h"

/* ===== RX variables ===== */
volatile bool rxReady = false;

uint32_t rx_messageID = 0;
uint8_t  rx_message[64] = {0};
uint8_t  rx_messageLength = 0;
uint16_t timestamp = 0;

static CAN_MSG_RX_FRAME_ATTRIBUTE msgFrameAttr = CAN_MSG_RX_DATA_FRAME;

/* ===== CAN RAM ===== */
static bool s_ram_bound = false;
static uint8_t s_can_msg_ram[CAN0_MESSAGE_RAM_CONFIG_SIZE];

/* ===== Recovery state ===== */
static bool s_can_initialized = false;
static uint8_t s_busoff_seen_count = 0;

/* ===== ISR callback ===== */
void APP_CAN_Callback(uintptr_t context)
{
    (void)context;

    rxReady = true;

    /* Re-arm RX */
    CAN0_MessageReceive(&rx_messageID,
                        &rx_messageLength,
                        rx_message,
                        &timestamp,
                        CAN_MSG_ATTR_RX_FIFO0,
                        &msgFrameAttr);
}

/* ===== Low-level startup ===== */
void CAN_Init(void)
{
    if (!s_ram_bound)
    {
        CAN0_MessageRAMConfigSet(s_can_msg_ram);
        s_ram_bound = true;
    }

    CAN0_Initialize();

    CAN0_RxCallbackRegister(APP_CAN_Callback, (uintptr_t)NULL, CAN_MSG_ATTR_RX_FIFO0);

    CAN0_MessageReceive(&rx_messageID,
                        &rx_messageLength,
                        rx_message,
                        &timestamp,
                        CAN_MSG_ATTR_RX_FIFO0,
                        &msgFrameAttr);

    rxReady = false;
    rx_messageID = 0;
    rx_messageLength = 0;
    timestamp = 0;
    msgFrameAttr = CAN_MSG_RX_DATA_FRAME;

    s_can_initialized = true;
    s_busoff_seen_count = 0;
}

/* ===== Send ===== */
bool CAN_Send(uint32_t id, const uint8_t *data, uint8_t len)
{
    const CAN_MODE mode = CAN_MODE_FD_WITHOUT_BRS;
    const CAN_MSG_TX_ATTRIBUTE attr = CAN_MSG_ATTR_TX_FIFO_DATA_FRAME;

    if (!s_can_initialized)
    {
        return false;
    }

    return CAN0_MessageTransmit(id, len, (uint8_t *)data, mode, attr);
}

/* ===== Error helpers ===== */
bool CAN_IsBusOff(void)
{
    CAN_ERROR err = CAN0_ErrorGet();
    return ((err & CAN_ERROR_BUS_OFF) != 0u);
}

void CAN_ForceRecover(void)
{
    /* Reinitialize controller and re-arm receive path */
    CAN_Init();
}

void CAN_RecoverIfNeeded(void)
{
    uint8_t tx_err = 0;
    uint8_t rx_err = 0;
    CAN_ERROR err = CAN0_ErrorGet();

    CAN0_ErrorCountGet(&tx_err, &rx_err);

    if ((err & CAN_ERROR_BUS_OFF) != 0u)
    {
        s_busoff_seen_count++;

        /* Small debounce so we do not thrash reinit on one transient read */
        if (s_busoff_seen_count >= 2u)
        {
            CAN_ForceRecover();
        }
    }
    else
    {
        s_busoff_seen_count = 0;
    }
}