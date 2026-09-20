#include "definitions.h"
#include "can_facade.h"

/* ===== RX variables (same as before, now owned by CAN module) ===== */
volatile bool rxReady = false;

uint32_t rx_messageID = 0;
uint8_t  rx_message[64] = {0};
uint8_t  rx_messageLength = 0;
uint16_t timestamp = 0;

static CAN_MSG_RX_FRAME_ATTRIBUTE msgFrameAttr = CAN_MSG_RX_DATA_FRAME;

/* ===== CAN RAM ===== */
static bool s_ram_bound = false;
static uint8_t s_can_msg_ram[CAN0_MESSAGE_RAM_CONFIG_SIZE];

/* ===== ISR callback ===== */
void APP_CAN_Callback(uintptr_t context)
{
    (void)context;

    rxReady = true;
    
    printf("Callback");

    /* Re-arm RX */
    CAN0_MessageReceive(&rx_messageID,
                        &rx_messageLength,
                        rx_message,
                        &timestamp,
                        CAN_MSG_ATTR_RX_FIFO0,
                        &msgFrameAttr);
}

/* ===== Init ===== */
void CAN_Init(void)
{
    if (!s_ram_bound)
    {
        CAN0_MessageRAMConfigSet(s_can_msg_ram);
        s_ram_bound = true;
    }

    CAN0_RxCallbackRegister(APP_CAN_Callback, (uintptr_t)NULL, CAN_MSG_ATTR_RX_FIFO0);

    CAN0_MessageReceive(&rx_messageID,
                        &rx_messageLength,
                        rx_message,
                        &timestamp,
                        CAN_MSG_ATTR_RX_FIFO0,
                        &msgFrameAttr);
}

/* ===== Send ===== */
bool CAN_Send(uint32_t id, uint8_t *data, uint8_t len)
{
    const CAN_MODE mode = CAN_MODE_FD_WITHOUT_BRS;
    const CAN_MSG_TX_ATTRIBUTE attr = CAN_MSG_ATTR_TX_FIFO_DATA_FRAME;

    return CAN0_MessageTransmit(id, len, data, mode, attr);
}
