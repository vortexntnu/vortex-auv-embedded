#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "definitions.h"
#include "app.h"

/* --- Constants --- */
static const uint32_t TCC0_PERIOD              = 74000U;
static const uint32_t TCC1_PERIOD              = 74000U;
static const uint32_t TCC2_PERIOD              = 18500U;
static const uint32_t PWM_PERIOD_MICROSECONDS  = 20000U;
static const uint32_t CAN_EVENT_ID_BASE        = 0x369U;
static const uint8_t  MESSAGES_TO_READ         = 1U;

/* --- Types --- */
typedef struct {
    uint8_t  tcc_num;
    uint8_t  channel;
    uint32_t period;
} Thruster;

typedef enum {
    STOP = 0,
    START,
    RESET,
    SET_PWM
} STATES;

/* --- Private states --- */
static uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE] __attribute__((aligned(32)));
static CAN_RX_BUFFER rx_buf;

static volatile uint32_t can_status = 0;

static const Thruster thrusters[8] = {
    {0, 0, TCC0_PERIOD}, // TCC0_CHANNEL0
    {0, 1, TCC0_PERIOD}, // TCC0_CHANNEL1
    {0, 2, TCC0_PERIOD}, // TCC0_CHANNEL2
    {0, 3, TCC0_PERIOD}, // TCC0_CHANNEL3
    {0, 4, TCC0_PERIOD}, // TCC0_CHANNEL4
    {0, 5, TCC0_PERIOD}, // TCC0_CHANNEL5
    {1, 0, TCC1_PERIOD}, // TCC1_CHANNEL0
    {1, 1, TCC1_PERIOD}  // TCC1_CHANNEL1
};

/* --- Private function prototypes --- */
static void set_thruster_pwm(const uint8_t *data);
static void message_handler(void);
static void stop_thrusters(void);
static void start_thrusters(void);

/* Callbacks */
static void CAN_Receive_Callback(uintptr_t context);
static void CAN_Transmit_Callback(uintptr_t context);


/* --- Public functions --- */

void App_Init(void)
{
    /* Configure CAN RAM & callbacks */
    CAN0_MessageRAMConfigSet(Can0MessageRAM);
    CAN0_RxFifoCallbackRegister(CAN_RX_FIFO_0, CAN_Receive_Callback, (uintptr_t)NULL);
    CAN0_TxFifoCallbackRegister(CAN_Transmit_Callback, (uintptr_t)NULL);

    /* Clear RX buffer and prime the first receive */
    memset(&rx_buf, 0x00, sizeof(rx_buf));
    CAN0_MessageReceiveFifo(CAN_RX_FIFO_0, MESSAGES_TO_READ, &rx_buf);

    /* Enable watchdog */
    WDT_Enable();
}

void App_Task(void)
{
    /* Handle any message that arrived since last time */
    message_handler();
}

/* --- Private helpers --- */

static void message_handler(void)
{
    /* Interpret event from CAN frame id */
    uint8_t event = (uint8_t)(rx_buf.id - CAN_EVENT_ID_BASE);
    const uint8_t *pData = rx_buf.data;

    switch (event)
    {
        case STOP:
            stop_thrusters();
            break;

        case START:
            start_thrusters();
            break;

        case RESET:
            /* Force a watchdog reset */
            WDT_REGS->WDT_CLEAR = 0x0;
            break;

        case SET_PWM:
            set_thruster_pwm(pData);
            break;

        default:
            /* Unknown event: ignore */
            break;
    }

    /* Re-arm RX FIFO for next frame */
    CAN0_MessageReceiveFifo(CAN_RX_FIFO_0, MESSAGES_TO_READ, &rx_buf);
}

static void set_thruster_pwm(const uint8_t *data)
{
    for (size_t thr = 0; thr < 8; thr++)
    {
        /* data layout: uint16 per thruster */
        uint16_t duty_cycle = ((uint16_t)data[2U * thr] << 8) | (uint16_t)data[2U * thr + 1U];

        /* Map microsecond duty to TCC counter domain */
        uint32_t tcc_value =
            (duty_cycle * (thrusters[thr].period + 1U)) / PWM_PERIOD_MICROSECONDS;

        switch (thrusters[thr].tcc_num)
        {
            case 0:
                TCC0_PWM24bitDutySet(thrusters[thr].channel, tcc_value);
                break;

            case 1:
                TCC1_PWM24bitDutySet(thrusters[thr].channel, tcc_value);
                break;

            case 2:
                /* Not used with current mapping */
                TCC2_PWM16bitDutySet(thrusters[thr].channel, (uint16_t)tcc_value);
                break;

            default:
                break;
        }
    }

    /* Pet the watchdog after applying updates */
    WDT_Clear();
}

static void stop_thrusters(void)
{
    TCC0_PWMStop();
    TCC1_PWMStop();
    /* TCC2_PWMStop(); // not used */
}

static void start_thrusters(void)
{
    TCC0_PWMStart();
    TCC1_PWMStart();
    /* TCC2_PWMStart(); // not used */
}

void CAN_Receive_Callback(uintptr_t context) {
    /* Check CAN Status */
    can_status = CAN0_ErrorGet();

    // If no new error, handle CAN frame
    if (((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        /* Optionally, debug/log here */
    }
}

void CAN_Transmit_Callback(uintptr_t context) {
    /* Check CAN Status */
    can_status = CAN0_ErrorGet();

    if (((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        /* Optionally, queue next TX or debug */

    }
}


