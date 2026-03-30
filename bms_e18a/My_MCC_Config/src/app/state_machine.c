#include "app/state_machine.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "app/can_facade.h"
#include "app/can_telemetry.h"
#include "app/pwr_mode.h"
#include "definitions.h"
#include "ic_bms/bms_spi.h"
#include "peripheral/port/plib_port.h"


typedef enum
{
    CAN_STATE_OK = 0,
    CAN_STATE_RECOVERING,
    CAN_STATE_OFFLINE
} CAN_STATE;

static CAN_STATE can0State = CAN_STATE_OK;
static uint8_t can0RecoveryAttempts = 0;

extern volatile bool rxReady;
extern uint32_t rx_messageID;
extern uint8_t rx_message[64];
extern uint8_t rx_messageLength;

static volatile bool s_can_wake_irq = false;
static volatile uint32_t s_rtc_tick = 0U;
static uint32_t s_last_can_tick = 0U;

volatile bool can_tx_avaliable = false;

#define STBY_TO_TICKS 6000U

void sm_init(void) {
    s_rtc_tick = 0U;
    s_last_can_tick = 0U;
    s_can_wake_irq = false;
}

void sm_on_can_wake(void) {
    s_can_wake_irq = true;
}

void sm_on_rtc_tick(void) {
    s_rtc_tick++;
}

void state_machine(void) {
    switch (pwr_get_state()) {
        case PWR_MODE_ACTIVE: {
            uint32_t now;
            bool can_alive = s_can_wake_irq;

            CAN_voltage_send();
            CAN_temp_send();
            now = s_rtc_tick;

            if (rxReady) {
                uint32_t id = rx_messageID;
                uint8_t len = rx_messageLength;
                uint8_t b0 = (len > 0U) ? rx_message[0] : 0U;

                rxReady = false;
                can_alive = true;

                switch (id) {
                    case CAN_ID_BOTHOFF_CMD:
                        if ((len > 0U) && (b0 == BOTHOFF_CMD_BYTE)) {
                            bothoff_high();
                        }
                        break;

                    case CAN_RST_MCU:
                        NVIC_SystemReset();
                        break;

                    default:
                        break;
                }
            }

            if (can_alive) {
                s_last_can_tick = now;
                s_can_wake_irq = false;
            }

            if ((uint32_t)(now - s_last_can_tick) >=
                STBY_TO_TICKS)  // Check if the time since the last CAN activity
                                // exceeds the threshold
            {
                STB_Set();
                pwr_set_stb_lowpower();
                pwr_set_state(PWR_MODE_STANDBY);
            }
            break;
        }

        case PWR_MODE_STANDBY:
            if (s_can_wake_irq) {
                s_can_wake_irq = false;
                s_last_can_tick = s_rtc_tick;

                STB_Clear();
                pwr_set_idle0();
                CAN_Init();
                pwr_set_state(PWR_MODE_ACTIVE);
            }
            break;

        default:
            s_last_can_tick = s_rtc_tick;
            STB_Clear();
            pwr_set_idle0();
            pwr_set_state(PWR_MODE_ACTIVE);
            break;
    }

    pwr_enter_sleep();
}

static bool CAN0_IsBusOff(void)
{
    CAN_ERROR err = CAN0_ErrorGet();

    if (err == CAN_ERROR_INVALID)
        return true;

    return ((err & CAN_ERROR_BUS_OFF) != 0U);
}

static bool CAN0_IsDegraded(void)
{
    CAN_ERROR err = CAN0_ErrorGet();

    if (err == CAN_ERROR_INVALID)
        return true;

    if ((err & CAN_ERROR_PASSIVE) != 0U)
        return true;

    if ((err & CAN_ERROR_WARNING_STATUS) != 0U)
        return true;

    return false;
}

static void CAN0_Recover(void)
{
    can0State = CAN_STATE_RECOVERING;

    // Optional for TCAN3414:
    // TCAN_SHDN_Set();
    // SYSTICK_DelayMs(2);
    // TCAN_SHDN_Clear();
    // SYSTICK_DelayMs(2);

    // Let supply / transceiver settle
    SYSTICK_DelayMs(10);

    CAN0_Initialize();

    // Clear interrupt flags your app uses
    CAN0_InterruptClear(0xFFFFFFFFU);

    can0State = CAN_STATE_OK;
}

static void CAN0_HandleError(void)
{
    CAN_ERROR err = CAN0_ErrorGet();
    uint8_t txe = 0U;
    uint8_t rxe = 0U;

    CAN0_ErrorCountGet(&txe, &rxe);

    if (err == CAN_ERROR_INVALID)
    {
        can0State = CAN_STATE_OFFLINE;
        CAN0_Recover();
        return;
    }

    if ((err & CAN_ERROR_BUS_OFF) != 0U)
    {
        can0State = CAN_STATE_OFFLINE;
        CAN0_Recover();
        return;
    }

    if ((err & CAN_ERROR_PASSIVE) != 0U)
    {
        can0State = CAN_STATE_RECOVERING;

        // Brief backoff first
        SYSTICK_DelayMs(2);

        CAN0_ErrorCountGet(&txe, &rxe);

        // Only escalate if still bad
        if ((txe > 200U) || (rxe > 200U))
        {
            CAN0_Recover();
        }
        else
        {
            can0State = CAN_STATE_OK;
        }
    }
}

void state_machine_simple(void) {
    if (rxReady) {
        uint32_t id = rx_messageID;
        uint8_t len = rx_messageLength;
        uint8_t b0 = (len > 0U) ? rx_message[0] : 0U;

        rxReady = false;

        switch (id) {
            case CAN_ID_BOTHOFF_CMD:
                printf("BOTHOFF\r\n");
                // if ((len > 0U) && (b0 == BOTHOFF_CMD_BYTE)) {
                //     bothoff_high();
                // }
                break;

            case CAN_RST_MCU:
                printf("RESET\r\n");
                NVIC_SystemReset();
                break;
            case CAN_START_TRANSMIT:
                printf("CAN enabled\r\n");
                can_tx_avaliable = true;
                break;
            case CAN_STOP_TRANSMIT:
                printf("CAN disabled\r\n");
                can_tx_avaliable = false;
                break;
            default:
                break;
        }
    }

    if (CAN0_IsBusOff())
    {
        CAN0_HandleError();
        can_tx_avaliable = false;
    }
    else if (CAN0_IsDegraded())
    {
        CAN0_HandleError();
    }
    // pwr_enter_sleep();
}
