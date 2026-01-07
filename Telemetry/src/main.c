/*******************************************************************************
  Main Source File

  Company:
    Vortex NTNU.

 Author:
    Markus Sandvik

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all generated modules in the system. It also needs to call
    led_init() to initialize the LED facade before using any LED functions.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "definitions.h"                
#include "ws2812_spi_enc.h"      
#include "led_facade.h"
#include "ws2812_port_sercom0_harmony.h"

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

static uint32_t rx_messageID = 0;
static uint8_t rx_message[64] = {0};
static uint8_t rx_messageLength = 0;
static uint16_t timestamp = 0;
static CAN_MSG_RX_FRAME_ATTRIBUTE msgFrameAttr = CAN_MSG_RX_DATA_FRAME;

volatile bool rxReady = false;
static bool s_ram_bound = false;
static uint8_t s_can_msg_ram[CAN0_MESSAGE_RAM_CONFIG_SIZE];

void APP_CAN_Callback(uintptr_t context)
{
    (void)context;

    rxReady = true;

    // Re-arm for next message
    CAN0_MessageReceive(&rx_messageID, &rx_messageLength, rx_message, &timestamp, CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
}

/* --- Initialize CAN0 and bind message RAM --- */
void CAN_Init(void)
{
    if (!s_ram_bound)
    {
        CAN0_MessageRAMConfigSet(s_can_msg_ram);
        s_ram_bound = true;
    }
    
    CAN0_RxCallbackRegister(APP_CAN_Callback,(uintptr_t)NULL,CAN_MSG_ATTR_RX_FIFO0);
    CAN0_MessageReceive(&rx_messageID, &rx_messageLength, rx_message, &timestamp, CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
}

/* --- Non-blocking send using Tx FIFO --- */
bool CAN_Send(uint32_t id, uint8_t* data, uint8_t len)
{
    const CAN_MODE mode = CAN_MODE_FD_WITHOUT_BRS;
    const CAN_MSG_TX_ATTRIBUTE attr = CAN_MSG_ATTR_TX_FIFO_DATA_FRAME;

    // Try once, return false if FIFO full
    return CAN0_MessageTransmit(id, len, data, mode, attr);
}

int main(void)
{
    SYS_Initialize(NULL);
    
    CAN_Init();
    led_init();
    
    printf("\r\n----------------------------------------\r\n");
    printf(" SAMC21 CAN RX (LED Controller) \r\n");
    printf(" DLC=4: [idx R G B], idx=255 -> ALL \r\n");
    printf("----------------------------------------\r\n");
    

    while (1) {
        SYS_Tasks();
        
        if (rxReady)
        {
            rxReady = false;

            printf("New Message Received\r\n");
            printf("Timestamp: 0x%x ID: 0x%lx Length: 0x%x\r\n",
                   (unsigned)timestamp, (unsigned long)rx_messageID, (unsigned)rx_messageLength);

            printf("Message: ");
            for (uint8_t i = 0; i < rx_messageLength; i++)
                printf("0x%02x ", rx_message[i]);
            printf("\r\n");
        }
        /*
        if(!led_busy()){
            led_clear_all();
            led_set(2,255,255,255);
            led_commit_async();
        }*/
    }
    return (EXIT_FAILURE);
}



/*******************************************************************************
 End of File
*/


