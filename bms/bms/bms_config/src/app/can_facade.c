#include "definitions.h"
#include "can_facade.h"

/* RX state */
static volatile bool rx_ready = false;
static CAN_RX_BUFFER rx_buf;

/* CAN message RAM */
static bool ram_bound = false;
static uint8_t can_msg_ram[CAN0_MESSAGE_RAM_CONFIG_SIZE];

/* RX callback */
void APP_CAN_RxCallback(uint8_t numberOfMessage, uintptr_t context)
{
    (void)context;
    (void)numberOfMessage;

    /* Fetch one message from FIFO0 */
    if (CAN0_MessageReceiveFifo(CAN_RX_FIFO_0, 1, &rx_buf))
    {
        rx_ready = true;
        /* Re-arm is implicit; FIFO is re-used after ack inside plib */
    }
}

/* Init */
void CAN_Init(void)
{
    if (!ram_bound)
    {
        CAN0_MessageRAMConfigSet(can_msg_ram);
        ram_bound = true;
    }

    /* Accept all standard/extended frames into FIFO0 (override MCC default reject). */
    CAN0_REGS->CAN_GFC = CAN_GFC_ANFS_RXF0 | CAN_GFC_ANFE_RXF0;

    /* Register RX callback on FIFO0 */
    CAN0_RxFifoCallbackRegister(CAN_RX_FIFO_0, APP_CAN_RxCallback, (uintptr_t)NULL);

    /* Optional: relax global filters if needed (left as-is for now) */
}

/* Send a standard 11-bit ID data frame */
bool CAN_Send(uint32_t id, const uint8_t *data, uint8_t len)
{
    if (len > 8U || data == NULL)
        return false;

    CAN_TX_BUFFER tx = {0};
    tx.id  = (id & 0x7FFU) << 18;  /* standard ID format */
    tx.xtd = 0;
    tx.rtr = 0;
    tx.dlc = len & 0xFU;
    tx.brs = 0;
    tx.fdf = 0;
    tx.efc = 0;
    tx.mm  = 0;
    for (uint8_t i = 0; i < len; i++)
        tx.data[i] = data[i];

    /* Single-message transmit via FIFO */
    return CAN0_MessageTransmitFifo(1, &tx);
}
