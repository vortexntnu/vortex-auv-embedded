/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

/*
 * SAMC21 Receiver - CAN -> WS2812 LED Controller
 *
 * Protocol (classic CAN, STD ID 0x321, DLC = 4):
 *   data[0] = idx (0..N-1, or 255 (0xFF) for ALL)
 *   data[1] = R
 *   data[2] = G
 *   data[3] = B
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "definitions.h"
#include "led_facade.h"
#include "led_logic.h"
#include "led_can_decode.h"
#include "can_facade.h"
#include "node_watchdog.h"
#include "interrupts.h"
#include "ws2812_port_sercom0_harmony.h"

#define LED_CMD_STDID  (0x469u)

/* RX variables defined in CAN_facade.c */
extern volatile bool rxReady;
extern uint32_t rx_messageID;
extern uint8_t  rx_message[64];
extern uint8_t  rx_messageLength;
extern uint16_t timestamp;

static volatile bool uncommitted_changes = false;

uint8_t  tx_counter = 0;
uint32_t last_spin   = 0;

volatile uint32_t ms = 0;

void SysTick_Handler(void)
{
    ms++;
}

static inline uint32_t millis(void)
{
    return ms;
}

// Watchdog binding, just forward CAN_Send via callback.
static void watchdog_can_send(uint32_t can_id, const uint8_t *data, uint8_t len)
{
    CAN_Send(can_id, data, len);
}

// Test ID.
//const uint32_t can_test_id = 0x469;


int main(void)
{
    SYS_Initialize(NULL);
    CAN_Init();
    led_init();
    led_logic_init();
    
    SysTick_Config(CPU_CLOCK_FREQUENCY / 1000u);
    
    /* Start with a known state */
    led_clear_all();
    led_set(1, 255, 0, 0);
    led_set(3, 0, 255, 0);
    //led_set(5, 0, 0, 20);
    //led_set(5, 0, 0, 0);
    led_commit_async();

    printf("\r\n----------------------------------------\r\n");
    printf(" SAMC21 CAN RX (LED Controller) \r\n");
    printf(" Listening on STD ID 0x%03x \r\n", LED_CMD_STDID);
    printf(" DLC=4: [idx R G B], idx=255 -> ALL \r\n");
    printf("----------------------------------------\r\n");
    
    // Enable watchdog (integrated into decoder module)
    // - CAN IDs and default monitor IDs are defined in led_can_decode.h
    // - You can override alive-req CAN ID / monitor IDs later via the led_can_watchdog_set_* API.
    led_can_watchdog_set_send_cb(watchdog_can_send);
    led_can_watchdog_init(millis());
    
    
    
    while (true)
    {
        SYS_Tasks();
        
        const uint32_t ms = millis();
        led_logic_tick(ms);
        led_can_watchdog_tick(ms);
        
        
        if (rxReady)
        {
            rxReady = false;
            
            // Feed watchdog with every received frame (passive monitoring + IM_GOOD decoding)
            led_can_watchdog_on_can_rx(rx_messageID, rx_message, rx_messageLength, millis());

            // Filter for LED-systems addresses
            if (rx_messageID >= 0x101u && rx_messageID <= 0x10Bu){
                led_can_decode_and_update(rx_messageID, rx_message, rx_messageLength);
                led_logic_tick(millis());
            }
            
            /*printf("New Message Received\r\n");
            printf("Timestamp: 0x%x ID: 0x%lx Length: 0x%x\r\n",
                   (unsigned)timestamp, (unsigned long)rx_messageID, (unsigned)rx_messageLength);

            printf("Message: ");
            for (uint8_t i = 0; i < rx_messageLength; i++)
                printf("0x%02x ", rx_message[i]);
            printf("\r\n");
            
            uint8_t can_test_payload[8] = {
            tx_counter++, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
            };*/
            
            //CAN_Send(LED_CMD_STDID, can_test_payload, sizeof(can_test_payload));
        }
        
        /*if (rxReady)
        {
            rxReady = false;
            
            if (rx_messageID == LED_CMD_STDID){
                led_set(rx_message[0], rx_message[1], rx_message[2], rx_message[3]);
                uncommitted_changes = true;
            }

            uint8_t can_test_payload[8] = {
            tx_counter++, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
            };
            
            CAN_Send(LED_CMD_STDID, can_test_payload, sizeof(can_test_payload));
        }*/
    }
}


