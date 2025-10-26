#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "definitions.h"
#include "app.h"

/* --- Constants --- */
static const uint32_t TCC0_PERIOD               = 74000U;
static const uint32_t TCC1_PERIOD               = 74000U;
static const uint32_t TCC2_PERIOD               = 18500U;
static const uint32_t THRUSTER_PWM_PERIOD_US    = 20000U; // 50Hz
static const uint32_t LIGHT_PWM_PERIOD_US       = 20000U; // 50Hz
static const uint32_t CAN_EVENT_ID_BASE         = 0x369U;
static const uint8_t  MESSAGES_TO_READ          = 1U;
static const float    ADC_VREF                  = 3.3f;
static const float    R_SHUNT_OHMS              = 0.005f; // Placeholder for the actual shunt data
static const float    INA_GAIN                  = 50.0f;  // Placeholder for the actual INA gain
static const uint8_t  THRUSTER_RATED_CURRENT    = 15U;    // From TSD7 datasheet  

/* --- Types --- */
typedef struct {
    uint8_t  instance;
    uint8_t  channel;
    uint32_t period_ticks;
} Thruster;

typedef struct {
    uint8_t instance;
    uint8_t channel;
    uint32_t period_ticks;
} Light;

typedef enum {
    TURN_THRUSTERS_OFF,
    TURN_LIGHTS_OFF,
    RESET,
    SET_THRUSTER_PWM,
    SET_LIGHT_PWM
} STATES;

/* --- Private states --- */
/* CAN */
static uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE] __attribute__((aligned(32)));
static CAN_RX_BUFFER rx_buf;
static volatile uint32_t can_status = 0;

/* ADC */
static const uint32_t adc_seq_regs[8] = {0x1800, 0x1801, 0x1802, 0x1803, 0x1804, 0x1805, 0x1806, 0x1807};

static volatile uint16_t adc_res[8] = {0};
static volatile bool adc_dma_done = false;
static volatile bool overcurrent_fault = false;
static float input_voltage;

/* Application */
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

static const Light lights = {1, 2, TCC1_PERIOD}; // TCC1_CHANNEL2

/* --- Private function prototypes --- */
static void set_thruster_pwm(const uint8_t *data);
static void set_light_pwm(const uint8_t *data);
static void message_handler(void);
static void check_overcurrent(void);
static void turn_thrusters_off(void);
static void turn_thrusters_on(void);
static void turn_lights_off(void);
static void turn_lights_on(void);

static inline uint16_t clamp(uint16_t value, uint16_t low, uint16_t high);
static inline void tcc_write(uint8_t instance, uint8_t channel, uint32_t ticks);
static inline uint32_t us_to_ticks(uint32_t period_ticks, uint16_t us, uint32_t frame_us);

/* Callbacks */
static void CAN_Receive_Callback(uintptr_t context);
static void CAN_Transmit_Callback(uintptr_t context);

static void adc_sram_dma_callback(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle);

/* --- Public functions --- */

void App_Init(void) {
    /* Configure CAN RAM & callbacks */
    CAN0_MessageRAMConfigSet(Can0MessageRAM);
    CAN0_RxFifoCallbackRegister(CAN_RX_FIFO_0, CAN_Receive_Callback, (uintptr_t)NULL);
    CAN0_TxFifoCallbackRegister(CAN_Transmit_Callback, (uintptr_t)NULL);
    
    /* Configure DMA */
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_1, adc_sram_dma_callback, 0);
    DMAC_ChannelTransfer(DMAC_CHANNEL_1, (const void *)&ADC0_REGS->ADC_RESULT, (const void *)adc_res, 16); // Each adc result is 16 bits=2 bytes. 8*2=16
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *)adc_seq_regs, (const void *)&ADC0_REGS->ADC_DSEQDATA, 32); // DSEQDATA is 32 bits=4 bytes. 8 * 4 = 32
    
    /* Clear RX buffer and prime the first receive */
    memset(&rx_buf, 0x00, sizeof(rx_buf));
    CAN0_MessageReceiveFifo(CAN_RX_FIFO_0, MESSAGES_TO_READ, &rx_buf);

    ADC0_Enable(); // TODO: Remember to manually configure sample averaging in plib_adc0 before testing
    TC0_TimerStart();
    
    /* Enable watchdog */
    WDT_Enable();
}

void App_Task(void) {   
    /* Check for overcurrent */
    check_overcurrent();
    /* Handle any message that arrived since last time */
    message_handler();
}

/* --- Private helpers --- */

static void message_handler(void) {
    /* Interpret event from CAN frame id */
    uint8_t event = (uint8_t)(rx_buf.id - CAN_EVENT_ID_BASE);
    const uint8_t *pData = rx_buf.data;

    switch (event) {
        case TURN_THRUSTERS_OFF:
            turn_thrusters_off();
            break;

        case TURN_LIGHTS_OFF:
            turn_lights_off();
            break;

        case RESET:
            /* Force a watchdog reset */
            NVIC_SystemReset();
            break;

        case SET_THRUSTER_PWM:
            set_thruster_pwm(pData);
            break;

        case SET_LIGHT_PWM:
            set_light_pwm(pData);
            break;
            
        default:
            /* Unknown event: ignore */
            break;
    }

    /* Re-arm RX FIFO for next frame */
    CAN0_MessageReceiveFifo(CAN_RX_FIFO_0, MESSAGES_TO_READ, &rx_buf);
}

static void check_overcurrent(void) {
    if (adc_dma_done) {
        adc_dma_done = false;

        for (size_t sample = 0; sample < 8; sample++) {
            float input_voltage = (float)adc_res[sample] * ADC_VREF / 4095.0f;
            float amps = (input_voltage / INA_GAIN) / R_SHUNT_OHMS;

            printf("\r\n Measured current = %.2f A \r\n\r\n", (double)amps);
            if (amps > THRUSTER_RATED_CURRENT) {
                overcurrent_fault = true;
                printf("\r\n Overcurrent flagged \r\n\r\n");
                break;
            }
        }
    }
    
    if (overcurrent_fault) {
        overcurrent_fault = false;
        turn_thrusters_off();
    }
}

static void set_thruster_pwm(const uint8_t *data) {
    for (size_t thr = 0; thr < 8; thr++) {
        /* data layout: uint16 per thruster */
        uint16_t pulse_us = ((uint16_t)data[2U * thr] << 8) | (uint16_t)data[2U * thr + 1U];
        
        /* Thrusters take duty cycles in range 1000 - 2000 us*/
        pulse_us = clamp(pulse_us, 1000, 2000);
        
        uint32_t ticks = us_to_ticks(thrusters[thr].period_ticks, pulse_us, THRUSTER_PWM_PERIOD_US);
        
        tcc_write(thrusters[thr].instance, thrusters[thr].channel, ticks);
    }

    /* Pet the watchdog after applying updates */
    WDT_Clear();
}

static void set_light_pwm(const uint8_t *data) {
    /* data layout: uint16 for light */
    uint16_t pulse_us = ((uint16_t)data[0] << 8) | (uint16_t)data[1U];
    
    /* Lights takes duty cycle in range 1100 - 1900 us */
    pulse_us = clamp(pulse_us, 1100, 1900);
    
    uint32_t ticks = us_to_ticks(lights.period_ticks, pulse_us, LIGHT_PWM_PERIOD_US);
    
    tcc_write(lights.instance, lights.channel, ticks);
    
    /* Pet the watchdog after applying updates */
    WDT_Clear();
}

static void turn_thrusters_off(void) {
    for (size_t thr = 0; thr < 8; thr++) {
        // Write neutral (1500us) to each thrusters, keep modules running so ESC's stay armed
        uint32_t ticks = us_to_ticks(thrusters[thr].period_ticks, 1500, THRUSTER_PWM_PERIOD_US);
        tcc_write(thrusters[thr].instance, thrusters[thr].channel, ticks);
    }
    WDT_Clear();
}

static void turn_thrusters_on(void) {
    __NOP(); /* Might remove later */
}

static void turn_lights_off(void) {
    /* Write neutral (1100us) to the lights*/
    uint32_t ticks = us_to_ticks(lights.period_ticks, 1100, LIGHT_PWM_PERIOD_US);
    tcc_write(lights.instance, lights.channel, ticks);
    
    WDT_Clear();
}

static void turn_lights_on(void) {
    __NOP(); /* Might remove later */
}

static inline uint16_t clamp(uint16_t value, uint16_t low, uint16_t high) {
    return (value < low) ? low : (value > high) ? high : value;
}

static inline void tcc_write(uint8_t instance, uint8_t channel, uint32_t ticks) {
    switch (instance) {
        case 0: TCC0_PWM24bitDutySet(channel, ticks); break;
        case 1: TCC1_PWM24bitDutySet(channel, ticks); break;
        case 2: TCC2_PWM16bitDutySet(channel, (uint16_t)ticks); break; /* Not used with current mapping */
        default: break;
    }
}

static inline uint32_t us_to_ticks(uint32_t period_ticks, uint16_t pulse_us, uint32_t frame_us) {
    return ((uint32_t)pulse_us * (period_ticks + 1U)) / frame_us;
}

static void CAN_Receive_Callback(uintptr_t context) {
    /* Check CAN Status */
    can_status = CAN0_ErrorGet();

    // If no new error, handle CAN frame
    if (((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        /* Optionally, debug/log here */
    }
}

static void CAN_Transmit_Callback(uintptr_t context) {
    /* Check CAN Status */
    can_status = CAN0_ErrorGet();

    if (((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        /* Optionally, queue next TX or debug */

    }
}

static void adc_sram_dma_callback(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle) {
    
    if (event == DMAC_TRANSFER_EVENT_COMPLETE) {
        adc_dma_done = true;
    }
}


