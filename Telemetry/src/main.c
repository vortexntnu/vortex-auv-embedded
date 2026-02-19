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
#include <string.h>

#include "definitions.h"
#include "led_facade.h"
#include "can_facade.h"
#include "interrupts.h"
#include "ws2812_port_sercom0_harmony.h"

#include "leak_sensor_eic.h"
#include "pressure_calc.h"
#include "wsen_pads_port_sercom3.h"

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

#define LED_CMD_STDID  (0x469u)
#define PRESSURE_TEMP_STDID (0x200u) // CAN ID (TEMP VALUE) used for sending pressure and temperature data
#define SLOW_LEAK_ALARM_STDID (0x101u) // CAN ID (TEMP VALUE) used for sending slow leakage alarms
#define FAST_LEAK_ALARM_STDID (0x102u) // TEMP VALUE

/* RX variables defined in CAN_facade.c */
extern volatile bool rxReady;
extern uint32_t rx_messageID;
extern uint8_t  rx_message[64];
extern uint8_t  rx_messageLength;
extern uint16_t timestamp;

static volatile bool uncommitted_changes = false;

uint8_t  tx_counter = 0;
uint32_t last_spin   = 0;

struct leak_det leak_detector;

int main(void)
{
    SYS_Initialize(NULL);
    CAN_Init();
    led_init();
    
    /* Start with a known state */
    led_clear_all();
    //led_set(1, 255, 0, 0);
    //led_set(3, 0, 255, 0);
    //led_set(5, 0, 0, 20);
    //led_set(5, 0, 0, 0);
    led_commit_async();

    printf("\r\n----------------------------------------\r\n");
    printf(" SAMC21 CAN RX (LED Controller) \r\n");
    printf(" Listening on STD ID 0x%03x \r\n", LED_CMD_STDID);
    printf(" DLC=4: [idx R G B], idx=255 -> ALL \r\n");
    printf("----------------------------------------\r\n");
    
    spi_init();
    wsen_init();
    drdy_init();
    leak_sensor_init();


    leakdet_init(&leak_detector, NULL);

    float pressure = 0.0f;  // kPa
    float temp = 0.0f;      // °C
    float pressure_sum = 0.0f;
    float temp_sum = 0.0f;
    uint32_t samples = 0;

    #define MAX_SAMPLES 64
    static float pressure_buf[MAX_SAMPLES];
    static float temp_buf[MAX_SAMPLES];
    
    timing_tc2_init_5hz(); 
    
    
    while (true)
    {
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
            
            uint8_t can_test_payload[8] = {
            tx_counter++, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77
            };
            
            CAN_Send(LED_CMD_STDID, can_test_payload, sizeof(can_test_payload));
        }
        
        if (rxReady)
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
        }

        /* Background commit of pending LED updates */
        if (uncommitted_changes)
        {
            printf("\r\nCommitting LED changes\r\n");
            led_commit_async();

            if (!led_busy())
            {
                // NOTE: If you intend to mark changes as committed, consider setting false here. 
                uncommitted_changes = true;
            }
            else
            {
                uncommitted_changes = false;
            }
        }

        wsen_cycle_tick();
        if (wsen_cycle_done_ok(&pressure, &temp)) {
            wsen_reset();

            pressure_sum += pressure;
            temp_sum += temp;
            samples++;
            if (samples < MAX_SAMPLES) {
                pressure_buf[samples] = pressure;
                temp_buf[samples] = temp;
                samples++;
            } else {
                /* Buffer full; drop the newest sample */
            }
        } else {
            wsen_reset();
        }
        if (leakdet_tick) {
            leakdet_tick = false;

            float pressure_avg = pressure, temp_avg = temp;
            if (samples > 0) {
                pressure_avg = pressure_sum / samples;
                temp_avg = temp_sum / samples;
            }


            bool fast = false, slow = false;
            leakdet_update(&leak_detector, pressure_avg, temp_avg, &fast,
                           &slow);
            if (fast) {
                uint8_t can_payload[8] = {0};
                CAN_Send(FAST_LEAK_ALARM_STDID, can_payload, sizeof(can_payload));
            }
            if (slow) {
                uint8_t can_payload[8] = {0};
                CAN_Send(SLOW_LEAK_ALARM_STDID, can_payload, sizeof(can_payload));
            }        
                           
            /* Send all buffered samples over CAN at 5Hz (one CAN frame per sample)
             * Each frame payload (8 bytes): [pressure(float,4)] [temp(float,4)]
             */
            if (samples > 0) {
                for (uint32_t i = 0; i < samples; i++) {
                    uint8_t can_payload[8];
                    memcpy(&can_payload[0], &pressure_buf[i], sizeof(float));
                    memcpy(&can_payload[4], &temp_buf[i], sizeof(float));
                    CAN_Send(PRESSURE_TEMP_STDID, can_payload, sizeof(can_payload));
                }
                printf("Sent %u CAN telemetry frames ID 0x%03x (pressure/temp)\r\n",
                       (unsigned)samples, (unsigned)PRESSURE_TEMP_STDID);

            pressure_sum = 0.0f;
            temp_sum = 0.0f;
            samples = 0;

            }
        }
    return (EXIT_FAILURE);

    }
}

/*******************************************************************************
 End of File
*/
