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

//#define LED_CMD_STDID        (0x469u)
#define WATCHDOG_DISABLE_ID  (0x666u)
#define INT_PT_SENSOR_ID (0x780u)

/* RX variables defined in CAN_facade.c */
extern volatile bool rxReady;
extern uint32_t rx_messageID;
extern uint8_t  rx_message[64];
extern uint8_t  rx_messageLength;
extern uint16_t timestamp;

volatile uint32_t ms = 0;

/* GPIO heartbeat blink */
static uint32_t last_gpio_toggle = 0;

/* Watchdog arming state */
static bool can_activity_seen = false;

void SysTick_Handler(void)
{
    ms++;
}

static inline uint32_t millis(void)
{
    return ms;
}

/* Watchdog TX binding */
static void watchdog_can_send(uint32_t can_id, const uint8_t *data, uint8_t len)
{
    CAN_Send(can_id, data, len);
}

int main(void)
{
    SYS_Initialize(NULL);
    CAN_Init();
    led_init();
    led_logic_init();
    bmp280_init_device();

    SysTick_Config(CPU_CLOCK_FREQUENCY / 1000u);

    /* Start GPIO debug LED in known state */
    DBG_LED_Clear();

    /* Start with a known WS2812 state */
    led_clear_all();
    led_set(4, 0, 255, 0);
    led_set(5, 255, 0, 0);
    led_commit_async();

    printf("\r\n----------------------------------------\r\n");
    printf(" SAMC21 CAN RX (LED Controller)\r\n");
    printf(" Watchdog waits for first CAN activity\r\n");
    printf(" 0x999 = disarm watchdog\r\n");
    printf(" Any later CAN frame = arm again\r\n");
    printf("----------------------------------------\r\n");

    /* Configure watchdog, but do not run it until first CAN activity */
    led_can_watchdog_set_send_cb(watchdog_can_send);
    led_can_watchdog_init(millis());

    float temperature;
    float pressure;

    while (true)
    {
        SYS_Tasks();

        const uint32_t now = millis();

        /* Always run LED logic */
        led_logic_tick(now);

        /* Only run watchdog after bus activity has been seen */
        if (can_activity_seen)
        {
            led_can_watchdog_tick(now);
        }

        /* Toggle GPIO LED every 1000 ms */
        if ((uint32_t)(now - last_gpio_toggle) >= 1000u)
        {
            last_gpio_toggle = now;
            DBG_LED_Toggle();
        }

        if (rxReady)
        {
            rxReady = false;

            /* Any other CAN message arms watchdog */
            if (!can_activity_seen)
            {
                can_activity_seen = true;
                printf("CAN activity detected. Watchdog armed.\r\n");
            }
            
            /* 0x999 disarms watchdog again */
            if (rx_messageID == WATCHDOG_DISABLE_ID)
            {
                can_activity_seen = false;

                /* Reset watchdog internal timing/state */
                led_can_watchdog_init(now);

                printf("Watchdog disarmed via CAN (0x999)\r\n");
                continue;
            }

            /* Feed watchdog with every received frame while armed */
            if (can_activity_seen)
            {
                led_can_watchdog_on_can_rx(
                    rx_messageID,
                    rx_message,
                    rx_messageLength,
                    now
                );
            }

            /* LED-system decode */
            if (rx_messageID >= 0x101u && rx_messageID <= 0x10Bu)
            {
                led_can_decode_and_update(
                    rx_messageID,
                    rx_message,
                    rx_messageLength
                );

                /* Optional immediate refresh */
                //led_logic_tick(now);
            }

            /*
            printf("New Message Received\r\n");
            printf("Timestamp: 0x%x ID: 0x%lx Length: 0x%x\r\n",
                   (unsigned)timestamp,
                   (unsigned long)rx_messageID,
                   (unsigned)rx_messageLength);

            printf("Message: ");
            for (uint8_t i = 0; i < rx_messageLength; i++)
            {
                printf("0x%02x ", rx_message[i]);
            }
            printf("\r\n");
            */
        }

        if (bmp280_read_sample(&temperature, &pressure) == 0) {
            uint8_t can_payload[8] = {0};
            can_payload[0] = temperature;
            can_payload[4] = pressure;
            CAN_Send(INT_PT_SENSOR_ID, can_payload, sizeof(can_payload));
        }
    }
}