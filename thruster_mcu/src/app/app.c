#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "definitions.h"
#include "app.h"

/* --- Constants --- */
static const uint32_t TCC0_PERIOD               = 75000U;
static const uint32_t TCC1_PERIOD               = 75000U;
static const uint32_t TCC2_PERIOD               = 18500U;
static const uint32_t THRUSTER_PWM_PERIOD_US    = 20000U; // 50Hz
static const uint32_t LIGHT_PWM_PERIOD_US       = 20000U; // 50Hz
static const float    ADC_VREF                  = 3.3f;
static const float    G_IMON                    = 18.18e-6f; // Amplifier gain 18.18 uA/A -> in A/A
static const float    R_IMON                    = 2550.0f;   // 2.55 kilo ohms resistor
static const uint8_t  THRUSTER_RATED_CURRENT    = 15U;    // From TSD7 datasheet  

/* --- Types --- */
struct thruster {
    uint8_t  instance;
    uint8_t  channel;
    uint32_t period_ticks;
};

struct light {
    uint8_t instance;
    uint8_t channel;
    uint32_t period_ticks;
};

enum can_events {
    TURN_THRUSTERS_OFF = 0x369,
    TURN_LIGHTS_OFF    = 0x36A,
    RESET              = 0x36B,
    SET_THRUSTER_PWM   = 0x36C,
    SET_LIGHT_PWM      = 0x36D
};

/* --- Private states --- */
/* CAN */
static uint8_t Can1MessageRAM[CAN1_MESSAGE_RAM_CONFIG_SIZE] __attribute__((aligned(32)));

#define WRITE_ID(id) (id << 18)
#define READ_ID(id) (id >> 18)

static volatile uint32_t can_status = 0;
static volatile bool can_message_received = false;

static uint8_t txFiFo[CAN1_TX_FIFO_BUFFER_SIZE];
static uint8_t rxFiFo0[CAN1_RX_FIFO0_SIZE];

/* ADC */
static const uint32_t adc_seq_regs[8] = {0x1801, 0x1802, 0x1803, 0x1805, 0x1806, 0x1807, 0x1812, 0x1813};

static volatile uint16_t adc_res[8] = {0};
static volatile bool adc_dma_done = false;

/* Application */
static struct thruster thrusters[8] = {
    {0, 0, TCC0_PERIOD}, // TCC0_CHANNEL0
    {0, 1, TCC0_PERIOD}, // TCC0_CHANNEL1
    {0, 2, TCC0_PERIOD}, // TCC0_CHANNEL2
    {0, 3, TCC0_PERIOD}, // TCC0_CHANNEL3
    {0, 4, TCC0_PERIOD}, // TCC0_CHANNEL4
    {0, 5, TCC0_PERIOD}, // TCC0_CHANNEL5
    {1, 0, TCC1_PERIOD}, // TCC1_CHANNEL0
    {1, 1, TCC1_PERIOD}  // TCC1_CHANNEL1
};

static struct light lights = {1, 2, TCC1_PERIOD}; // TCC1_CHANNEL2

/* --- Private function prototypes --- */
static void set_thruster_pwm(const uint8_t *data);
static void set_light_pwm(const uint8_t *data);
static void message_handler(void);
static void check_overcurrent(void);
static void turn_thrusters_off(void);
static void turn_lights_off(void);


static inline uint16_t clamp(uint16_t value, uint16_t low, uint16_t high);
static inline void tcc_write(uint8_t instance, uint8_t channel, uint32_t ticks);
static inline uint32_t us_to_ticks(uint32_t period_ticks, uint16_t us, uint32_t frame_us);

/* Callbacks */
static void CAN_Receive_Callback(uint8_t numberOfMessage, uintptr_t context);
static void CAN_Transmit_Callback(uintptr_t context);

static void adc_sram_dma_callback(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle);

/* --- Public functions --- */

void App_Init(void) {
    // Configure CAN RAM & callbacks 
    CAN1_MessageRAMConfigSet(Can1MessageRAM);
    CAN1_RxFifoCallbackRegister(CAN_RX_FIFO_0, CAN_Receive_Callback, (uintptr_t)NULL);
    CAN1_TxFifoCallbackRegister(CAN_Transmit_Callback, (uintptr_t)NULL);
    
    // Configure DMA
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_1, adc_sram_dma_callback, 0);
    DMAC_ChannelTransfer(DMAC_CHANNEL_1, (const void *)&ADC0_REGS->ADC_RESULT, (const void *)adc_res, 16); // Each adc result is 16 bits=2 bytes. 8*2=16
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *)adc_seq_regs, (const void *)&ADC0_REGS->ADC_DSEQDATA, 32); // DSEQDATA is 32 bits=4 bytes. 8 * 4 = 32
    
    TCC0_PWMStart();
    TCC1_PWMStart();
    //TCC2_PWMStart();
    
    // Set all thrusters and lights to neutral on startup
    turn_thrusters_off(); 
    turn_lights_off();
    
    ADC0_Enable(); // TODO: Remember to manually configure sample averaging in plib_adc0 before testing
    
    TC0_TimerStart();
    
    // Enable watchdog
    WDT_Enable();
}

void App_Task(void) {
    if (adc_dma_done) {
        adc_dma_done = false;
        check_overcurrent();
    }
    
    if (can_message_received) {
        can_message_received = false;
        message_handler();
    }
}

/* --- Private helpers --- */

static void message_handler(void) {
    // Interpret event from CAN frame id
    CAN_RX_BUFFER *rxBuf = (CAN_RX_BUFFER *)rxFiFo0;
    
    uint32_t id = rxBuf->xtd ? rxBuf->id : READ_ID(rxBuf->id);
    const uint8_t *pData = rxBuf->data;

    switch (id) {
        case TURN_THRUSTERS_OFF:
            turn_thrusters_off();
            break;

        case TURN_LIGHTS_OFF:
            turn_lights_off();
            break;

        case RESET:
            /* Force a system reset */
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
}

static void check_overcurrent(void) {
    for (size_t sample = 0; sample < 8; sample++) {
        float V_Imon = (float)adc_res[sample] * ADC_VREF / 4095.0f;
        float I_out = V_Imon / (G_IMON * R_IMON);

        //printf("raw=%u  V_Imon=%.4f V  I_out=%.3f A\r\n",(unsigned)adc_res[sample], (double)((float)adc_res[sample]*ADC_VREF/4095.0f), (double)I_out);
        if (I_out > THRUSTER_RATED_CURRENT) {
            turn_thrusters_off();
            break;
        }
    }
}

static void set_thruster_pwm(const uint8_t *data) {
    for (size_t thr = 0; thr < 8; thr++) {
        // data layout is uint16 per thruster
        uint16_t pulse_us = ((uint16_t)data[2U * thr] << 8) | (uint16_t)data[2U * thr + 1U];
        
        // Thrusters take duty cycles in range 1000 - 2000 us
        pulse_us = clamp(pulse_us, 1000, 2000);
        
        uint32_t ticks = us_to_ticks(thrusters[thr].period_ticks, pulse_us, THRUSTER_PWM_PERIOD_US);
        
        tcc_write(thrusters[thr].instance, thrusters[thr].channel, ticks);
    }

    // Pet the watchdog after applying updates 
    WDT_Clear();
}

static void set_light_pwm(const uint8_t *data) {
    // data layout is uint16 for light
    uint16_t pulse_us = ((uint16_t)data[0] << 8) | (uint16_t)data[1U];
    
    // Lights takes duty cycle in range 1100 - 1900 us
    pulse_us = clamp(pulse_us, 1100, 1900);
    
    uint32_t ticks = us_to_ticks(lights.period_ticks, pulse_us, LIGHT_PWM_PERIOD_US);
    
    tcc_write(lights.instance, lights.channel, ticks);
    
    // Pet the watchdog after applying updates
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

static void turn_lights_off(void) {
    // Write neutral (1100us) to the lights
    uint32_t ticks = us_to_ticks(lights.period_ticks, 1100, LIGHT_PWM_PERIOD_US);
    tcc_write(lights.instance, lights.channel, ticks);
    
    WDT_Clear();
}

static inline uint16_t clamp(uint16_t value, uint16_t low, uint16_t high) {
    if (value < low) {
        return low;
    } else if (value > high) {
        return high;
    } else {
        return value;
    }
    
}

static inline void tcc_write(uint8_t instance, uint8_t channel, uint32_t ticks) {
    switch (instance) {
        case 0: 
            TCC0_PWM24bitDutySet(channel, ticks);
            break;
            
        case 1: 
            TCC1_PWM24bitDutySet(channel, ticks);
            break;
            
        case 2: 
            TCC2_PWM16bitDutySet(channel, (uint16_t)ticks);
            break; // Not used with current mapping
            
        default: 
            break;
    }
}

static inline uint32_t us_to_ticks(uint32_t period_ticks, uint16_t pulse_us, uint32_t frame_us) {
    return ((uint32_t)pulse_us * (period_ticks + 1U)) / frame_us;
}

static void CAN_Receive_Callback(uint8_t numberOfMessage, uintptr_t context) {
    // Check CAN Status
    can_status = CAN1_ErrorGet();

    // If no new error, handle CAN frame
    if (((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        
        memset(rxFiFo0, 0x00, (numberOfMessage * CAN1_RX_FIFO0_ELEMENT_SIZE));
        if (CAN1_MessageReceiveFifo(CAN_RX_FIFO_0, numberOfMessage, (CAN_RX_BUFFER *)rxFiFo0) == true) {
            can_message_received = true;
            // Optionally print can frame
        } 
    } 
}

static void CAN_Transmit_Callback(uintptr_t context) {
    // Check CAN Status
    can_status = CAN1_ErrorGet();

    if (((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        //printf("CAN TX successful\r\n");
    } 
}

static void adc_sram_dma_callback(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle) {
    
    if (event == DMAC_TRANSFER_EVENT_COMPLETE) {
        adc_dma_done = true;
    }
}


