#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "definitions.h"
#include "app.h"
#include <stdio.h>


static volatile bool can_message_ready = false;
static CAN_RX_BUFFER can_rx_buffer;
static uint8_t Can1MessageRAM[CAN1_MESSAGE_RAM_CONFIG_SIZE] __attribute__((aligned(32)));

static volatile bool slew_tick = false;
static volatile bool adc_dma_done = false;
static volatile uint16_t adc_result_array[TRANSFER_SIZE];


static const struct {
    uint8_t ain;
    uint8_t thruster;
} imon_map[8] = {
    { 0, 3 },   /* AIN0 -> Thruster 3 */  
    { 1, 4 },   /* AIN1 -> Thruster 4 */
    { 2, 1 },   /* AIN2 -> Thruster 1 */
    { 4, 2 },   /* AIN4 -> Thruster 2 */
    { 5, 5 },   /* AIN5 -> Thruster 5 */
    { 6, 6 },   /* AIN6 -> Thruster 6 */
    { 7, 7 },   /* AIN7 -> Thruster 7 */
    { 9, 8 },   /* AIN9 -> Thruster 8 */
};

typedef struct {
    volatile uint8_t flt_pending_mask;   // bit 0-7 = FLT channels 0-7
    volatile uint8_t pgood_pending_mask; // bit 0-7 = PGOOD channels 0-7
    volatile uint8_t killswitch_pending_mask;
} hw_event_flags_t;

static hw_event_flags_t hw_events = {0};


/**
 * @brief Handles incoming CAN messages and dispatches them to their corresponding action.
 */
static void message_handler(void);
static bool send_flt_event(uint8_t context);
static bool send_pgood_event(uint8_t context);
static bool send_killswitch_event(uint8_t context);
static bool send_current_measurements(float I_arr[8]);
static void dispatch_hw_event(volatile uint8_t *mask, bool (*send)(uint8_t channel));


static void log_current(void);
/* Callbacks */
static void adc_dma_callback(DMAC_TRANSFER_EVENT returned_event, uintptr_t MyDmacContext);
static void eic_pin_flt_thruster(uintptr_t context);
static void eic_pin_pg_thruster(uintptr_t context);
static void eic_pin_killswitch(uintptr_t context);
static void rtc_callback(RTC_TIMER32_INT_MASK intCause, uintptr_t context);

/* --- Public functions --- */

void app_init(void)
{
    CAN1_MessageRAMConfigSet(Can1MessageRAM);

    CAN1_RxFifoCallbackRegister(CAN_RX_FIFO_0, can_receive_callback, (uintptr_t)NULL);
    CAN1_TxFifoCallbackRegister(can_transmit_callback, (uintptr_t)NULL);

    EIC_NMICallbackRegister(eic_pin_killswitch, 0);

    EIC_CallbackRegister(EIC_PIN_0, eic_pin_pg_thruster, 4);
    EIC_CallbackRegister(EIC_PIN_1, eic_pin_flt_thruster, 4);

    EIC_CallbackRegister(EIC_PIN_2, eic_pin_flt_thruster, 5);
    EIC_CallbackRegister(EIC_PIN_3, eic_pin_pg_thruster, 5);

    EIC_CallbackRegister(EIC_PIN_4, eic_pin_pg_thruster, 3);
    EIC_CallbackRegister(EIC_PIN_5, eic_pin_flt_thruster, 3);

    EIC_CallbackRegister(EIC_PIN_6, eic_pin_pg_thruster, 2);
    EIC_CallbackRegister(EIC_PIN_7, eic_pin_flt_thruster, 2);

    EIC_CallbackRegister(EIC_PIN_8, eic_pin_pg_thruster, 1);
    EIC_CallbackRegister(EIC_PIN_9, eic_pin_flt_thruster, 1);

    EIC_CallbackRegister(EIC_PIN_10, eic_pin_pg_thruster, 7);
    EIC_CallbackRegister(EIC_PIN_11, eic_pin_flt_thruster, 7);

    EIC_CallbackRegister(EIC_PIN_12, eic_pin_pg_thruster, 6);
    EIC_CallbackRegister(EIC_PIN_13, eic_pin_flt_thruster, 6);

    EIC_CallbackRegister(EIC_PIN_14, eic_pin_flt_thruster, 0);
    EIC_CallbackRegister(EIC_PIN_15, eic_pin_pg_thruster, 0);

    ADC0_Enable();

    RTC_Timer32CompareSet(51);
    RTC_Timer32CallbackRegister(rtc_callback, 0);
    RTC_Timer32InterruptEnable(RTC_TIMER32_INT_MASK_CMP0);
    RTC_Timer32Start();

    SYSTICK_TimerStart();

    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, adc_dma_callback, 0);
    DMAC_ChannelTransfer(
        DMAC_CHANNEL_0,
        (const void *)&ADC0_REGS->ADC_RESULT,
        (const void *)adc_result_array,
        sizeof(adc_result_array)
    );

    TCC0_PWMStart();
    TCC1_PWMStart();
    TCC2_PWMStart();

    set_pwm_neutral(thrusters, 8);
    set_pwm_neutral(lights, 1);

    TC0_TimerStart();
    TC3_CompareStart();

    // WDT_Enable();
}

void app_task(void)
{
    if (slew_tick) {
        slew_tick = false;
        slew_pwm_outputs();
    }

    if (adc_dma_done) {
        adc_dma_done = false;
        log_current();
    }

    dispatch_hw_events();

    if (can_message_ready) {
        can_message_ready = false;
        message_handler();
    }
}

static bool can_send_frame(uint16_t can_id, const uint8_t *payload, uint8_t length)
{
    if (length > 64U) {
        return false;
    }

    if (CAN1_TxFifoFreeLevelGet() == 0U) {
        return false;
    }

    CAN_TX_BUFFER tx = {0};

    tx.id  = can_id;
    tx.dlc = length;

    tx.xtd = 0U;
    tx.rtr = 0U;  

    tx.fdf = 1U;
    tx.brs = 0U;

    if (payload != NULL && length > 0U) {
        memcpy(tx.data, payload, length);
    }

    return CAN1_MessageTransmitFifo(1U, &tx);
}


static void message_handler(void)
{
    uint16_t can_id = (uint16_t)can_rx_buffer.id;
    uint8_t *data = can_rx_buffer.data;
    uint8_t length = can_rx_buffer.dlc;

    switch (can_id) {
        case CAN_ID_TURN_THRUSTERS_OFF:
            set_pwm_neutral(thrusters, 8);
            break;

        case CAN_ID_TURN_LIGHTS_OFF:
            set_pwm_neutral(lights, 1);
            break;

        case CAN_ID_RESET:
            NVIC_SystemReset();
            break;

        case CAN_ID_SET_THRUSTER_PWM:
            if (length >= 16U) {
                set_pwm_outputs(data, thrusters, 8);
            }
            break;

        case CAN_ID_SET_LIGHT_PWM:
            if (length >= 2U) {
                set_light_output(data, lights, 1);
            }
            break;

        default:
            break;
    }
}

static void dispatch_hw_event(volatile uint8_t *mask, bool (*send)(uint8_t channel)) {
    uint8_t snapshot = *mask;
    *mask = 0;
    for (uint8_t i = 0; i < 8; i++) {
        if (snapshot & (1U << i)) {
            send(i);
        }
    }
}

static void dispatch_hw_events(void)
{
    uint8_t snapshot;

    snapshot = hw_events.flt_pending_mask;
    hw_events.flt_pending_mask = 0U;

    for (uint8_t i = 0U; i < 8U; i++) {
        if ((snapshot & (1U << i)) != 0U) {
            send_flt_event(i);
        }
    }

    snapshot = hw_events.pgood_pending_mask;
    hw_events.pgood_pending_mask = 0U;

    for (uint8_t i = 0U; i < 8U; i++) {
        if ((snapshot & (1U << i)) != 0U) {
            send_pgood_event(i);
        }
    }

    if (hw_events.killswitch_pending_mask != 0U) {
        hw_events.killswitch_pending_mask = 0U;
        send_killswitch_event(0U);
    }
}


static bool send_flt_event(uint8_t channel)
{
    uint8_t payload[2] = { channel, 0x01U };
    return can_send_frame(CAN_ID_FLT_EVENT, payload, 2U);
}

static bool send_pgood_event(uint8_t channel)
{
    uint8_t payload[2] = { channel, 0x02U };
    return can_send_frame(CAN_ID_PGOOD_EVENT, payload, 2U);
}

static bool send_killswitch_event(uint8_t channel)
{
    (void)channel;
    return can_send_frame(CAN_ID_KILLSWITCH_EVENT, NULL, 0U);
}

static void log_current(void) {
    const float ADC_VREF   = 5.0f;
    const float G_IMON     = 18.31e-6f;  
    const float R_IMON     = 2697.0f;
    
    float I_array[8] = {0};
    
    for (size_t i = 0; i < 8; i++) {
        float V_Imon = ((float)adc_result_array[i] * ADC_VREF) / 4095.0f;
        float I_out  = V_Imon / (G_IMON * R_IMON);
        
        I_array[i] = I_out;
    }
    
    send_current_measurements(I_array);
}

static bool send_current_measurements(float I_arr[8])
{
    uint8_t payload[32];

    for (size_t i = 0U; i < 8U; i++) {
        memcpy(&payload[i * sizeof(float)], &I_arr[i], sizeof(float));
    }

    return can_send_frame(CAN_ID_CURRENT_MEASUREMENTS, payload, 32U);
}

static void adc_dma_callback(DMAC_TRANSFER_EVENT returned_event, uintptr_t MyDmacContext) {
    if (returned_event == DMAC_TRANSFER_EVENT_COMPLETE) {
        adc_dma_done = true;
        // Re-arm DMA for next conversion
        DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *)&ADC0_REGS->ADC_RESULT, (const void *)adc_result_array, sizeof(adc_result_array));
    } 
}

static void rtc_callback(RTC_TIMER32_INT_MASK intCause, uintptr_t context) {
    (void)intCause;
    (void)context;
    slew_tick = true;
    
    if (ADC0_ConversionSequenceIsFinished()) {
           ADC0_ConversionStart();
    }
}

static void eic_pin_flt_thruster(uintptr_t context) {
    uint8_t channel = (uint8_t)context;
    hw_events.flt_pending_mask |= (1U << channel);  
}

static void eic_pin_pg_thruster(uintptr_t context) {
    uint8_t channel = (uint8_t)context;
    hw_events.pgood_pending_mask |= (1U << channel);
}

static void eic_pin_killswitch(uintptr_t context) {
    hw_events.killswitch_pending_mask |= 1U;
}


