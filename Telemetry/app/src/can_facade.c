#include "can_facade.h"
#include "definitions.h"   // if required for PLIBs

static bool s_ram_bound = false;
static uint8_t s_can_msg_ram[CAN0_MESSAGE_RAM_CONFIG_SIZE];

/* --- Initialize CAN0 and bind message RAM --- */
void CAN_Init(void)
{
    if (!s_ram_bound)
    {
        CAN0_MessageRAMConfigSet(s_can_msg_ram);
        s_ram_bound = true;
    }
}

/* --- Non-blocking send using Tx FIFO --- */
bool CAN_Send(uint32_t id, uint8_t* data, uint8_t len)
{
    const CAN_MODE mode = CAN_MODE_FD_WITHOUT_BRS;
    const CAN_MSG_TX_ATTRIBUTE attr = CAN_MSG_ATTR_TX_FIFO_DATA_FRAME;

    // Try once, return false if FIFO full
    return CAN0_MessageTransmit(id, len, data, mode, attr);
}
