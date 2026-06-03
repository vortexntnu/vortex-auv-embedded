/*
 * SAMC21 Receiver - Static WS2812 LED Controller
 */

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "definitions.h"
#include "led_facade.h"

// Advanced logic kept for later:
// #include "led_logic.h"
// #include "node_watchdog.h"

#include "led_static_logic.h"
#include "led_can_decode.h"     // Used for CAN_ID_PI, CAN_ID_ORIN, CAN_ID_SOFTWARE_MODE

#include "can_facade.h"
#include "interrupts.h"
#include "ws2812_port_sercom0_harmony.h"
#include "pressure_calc.h"
#include "bmp280_service.h"

#define WATCHDOG_DISABLE_ID     (0x666u)
#define INT_PT_SENSOR_ID        (0x480u)
#define FAST_LEAK_ALARM_ID      (0x334u)
#define SLOW_LEAK_ALARM_ID      (0x335u)

/* RX variables defined in CAN_facade.c */
extern volatile bool rxReady;
extern uint32_t rx_messageID;
extern uint8_t  rx_message[64];
extern uint8_t  rx_messageLength;
extern uint16_t timestamp;

volatile uint32_t ms = 0;

static uint32_t last_gpio_toggle = 0;
static bool can_activity_seen = false;

void SysTick_Handler(void)
{
    ms++;
}

static inline uint32_t millis(void)
{
    return ms;
}

/* Watchdog TX binding - kept for later */
/*
static void watchdog_can_send(uint32_t can_id, const uint8_t *data, uint8_t len)
{
    CAN_Send(can_id, data, len);
}
*/

int main(void)
{
    SYS_Initialize(NULL);
    CAN_Init();
    led_init();

    /*
     * Advanced rotating alarm logic bypassed for now.
     * Restore later by replacing led_static_logic_init()
     * with led_logic_init().
     */
    // led_logic_init();
    led_static_logic_init();

    GND_Clear();

    int rslt = bmp280_init_device();
    (void)rslt;

    struct leak_det leak_detector;
    leakdet_init(&leak_detector, NULL);

    SysTick_Config(CPU_CLOCK_FREQUENCY / 1000u);

    DBG_LED_Clear();

    printf("\r\n----------------------------------------\r\n");
    printf(" SAMC21 Static LED Controller\r\n");
    printf(" LED0 = PI WiFi\r\n");
    printf(" LED1 = ORIN WiFi\r\n");
    printf(" LED2 = Temperature\r\n");
    printf(" LED3 = Pressure\r\n");
    printf(" LED4 = Software mode\r\n");
    printf(" Advanced LED/watchdog logic bypassed\r\n");
    printf("----------------------------------------\r\n");

    /*
     * Watchdog kept for later.
     */
    // led_can_watchdog_set_send_cb(watchdog_can_send);
    // led_can_watchdog_init(millis());

    float temperature = 0.0f;
    float pressure = 0.0f;
    bool bmp_sample_pipeline_started = false;

    timing_tc1_init_5hz();

    while (true)
    {
        SYS_Tasks();

        const uint32_t now = millis();

        /*
         * Advanced rotating LED tick bypassed for now.
         */
        // led_logic_tick(now);

        /*
         * Watchdog tick bypassed for now.
         */
        /*
        if (can_activity_seen)
        {
            led_can_watchdog_tick(now);
        }
        */

        if ((uint32_t)(now - last_gpio_toggle) >= 1000u)
        {
            last_gpio_toggle = now;
            DBG_LED_Toggle();
        }

        if (rxReady)
        {
            rxReady = false;

            if (!can_activity_seen)
            {
                can_activity_seen = true;
                printf("CAN activity detected.\r\n");
            }

            if (rx_messageID == WATCHDOG_DISABLE_ID)
            {
                can_activity_seen = false;
                bmp_sample_pipeline_started = false;

                /*
                 * Watchdog reset bypassed for now.
                 */
                // led_can_watchdog_init(now);

                printf("Watchdog disable command received, watchdog is bypassed.\r\n");
                continue;
            }

            /*
             * Watchdog RX feed bypassed for now.
             */
            /*
            if (can_activity_seen)
            {
                led_can_watchdog_on_can_rx(
                    rx_messageID,
                    rx_message,
                    rx_messageLength,
                    now
                );
            }
            */

            /*
             * Static PI WiFi indication.
             * 0x11 = connected = blue
             * 0x10 = disconnected = yellow
             */
            if ((rx_messageID == CAN_ID_PI) && (rx_messageLength >= 1u))
            {
                if (rx_message[0] == 0x11u)
                {
                    led_static_set_pi_wifi(true);
                }
                else if (rx_message[0] == 0x10u)
                {
                    led_static_set_pi_wifi(false);
                }
            }

            /*
             * Static ORIN WiFi indication.
             * 0x11 = connected = blue
             * 0x10 = disconnected = yellow
             */
            if ((rx_messageID == CAN_ID_ORIN) && (rx_messageLength >= 1u))
            {
                if (rx_message[0] == 0x11u)
                {
                    led_static_set_orin_wifi(true);
                }
                else if (rx_message[0] == 0x10u)
                {
                    led_static_set_orin_wifi(false);
                }
            }

            /*
             * Static software mode indication on LED4.
             * 0x00 = killswitch = red
             * 0x01 = autonomous = blue
             * 0x02 = manual = yellow
             * 0x03 = reference = green
             */
            if ((rx_messageID == CAN_ID_SOFTWARE_MODE) && (rx_messageLength >= 1u))
            {
                led_static_set_software_mode(
                    (led_static_software_mode_t)rx_message[0]
                );
            }

            /*
             * Advanced CAN-to-alarm decoder bypassed for now.
             */
            /*
            if ((rx_messageID == CAN_ID_SOFTWARE_MODE) ||
                (rx_messageID >= 0x101u && rx_messageID <= 0x10Bu))
            {
                led_can_decode_and_update(
                    rx_messageID,
                    rx_message,
                    rx_messageLength
                );
            }
            */
        }

        if (leakdet_tick)
        {
            leakdet_tick = false;

            if (!can_activity_seen)
            {
                bmp_sample_pipeline_started = false;
            }
            else
            {
                int res = bmp280_try_read_sample(now, &temperature, &pressure);

                if (bmp_sample_pipeline_started && (res == 0))
                {
                    uint8_t can_payload[8] = {0};

                    memcpy(can_payload, &temperature, 4);
                    memcpy(can_payload + 4, &pressure, 4);

                    CAN_Send(INT_PT_SENSOR_ID, can_payload, sizeof(can_payload));

                    /*
                     * BMP280 pressure is usually Pa.
                     * 0.9 bar = 90000 Pa.
                     */
                    const bool pressure_ok = (pressure < 90000.0f);
                    const bool temperature_ok = (temperature < 50.0f);

                    led_static_set_pressure_ok(pressure_ok);
                    led_static_set_temperature_ok(temperature_ok);

                    bool fast = false;
                    bool slow = false;

                    // leakdet_update(&leak_detector, pressure, temperature, &slow);

                    if (fast)
                    {
                        uint8_t payload[8] = {0};
                        CAN_Send(FAST_LEAK_ALARM_ID, payload, sizeof(payload));
                    }

                    if (slow)
                    {
                        uint8_t payload[8] = {0};
                        CAN_Send(SLOW_LEAK_ALARM_ID, payload, sizeof(payload));
                    }
                }

                (void)bmp280_start_sample(now);
                bmp_sample_pipeline_started = true;
            }
        }
    }
}