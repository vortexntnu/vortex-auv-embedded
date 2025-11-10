#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "definitions.h"
#include "app.h"

#define WRITE_ID(id) (id << 18)
#define READ_ID(id) (id >> 18)

/* --- Constants --- */
static const uint32_t TCC0_PERIOD               = 75000U;
static const uint32_t TCC1_PERIOD               = 75000U;
static const uint32_t TCC2_PERIOD               = 18500U;
static const uint32_t THRUSTER_PWM_PERIOD_US    = 20000U; // 50Hz
static const uint32_t LIGHT_PWM_PERIOD_US       = 20000U; // 50Hz

/* --- Types --- */
struct pwm_output {
    uint8_t  instance;
    uint8_t  channel;
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

static volatile uint32_t can_status = 0;
static volatile bool can_message_received = false;

static uint8_t txFiFo[CAN1_TX_FIFO_BUFFER_SIZE];
static uint8_t rxFiFo0[CAN1_RX_FIFO0_SIZE];

/* ADC */
static const uint32_t adc_seq_regs[8] = {0x1801, 0x1802, 0x1803, 0x1805, 0x1806, 0x1807, 0x1812, 0x1813};

static volatile uint16_t adc_res[8] = {0};
static volatile bool adc_dma_done = false;

/* Application */
static struct pwm_output thrusters[8] = {
    {0, 0, TCC0_PERIOD}, // TCC0_CHANNEL0
    {0, 1, TCC0_PERIOD}, // TCC0_CHANNEL1
    {0, 2, TCC0_PERIOD}, // TCC0_CHANNEL2
    {0, 3, TCC0_PERIOD}, // TCC0_CHANNEL3
    {0, 4, TCC0_PERIOD}, // TCC0_CHANNEL4
    {0, 5, TCC0_PERIOD}, // TCC0_CHANNEL5
    {1, 0, TCC1_PERIOD}, // TCC1_CHANNEL0
    {1, 1, TCC1_PERIOD}  // TCC1_CHANNEL1
};

static struct pwm_output lights[1] = {{1, 2, TCC1_PERIOD}}; // TCC1_CHANNEL2

/* --- Private function prototypes --- */

/**
 * @brief Sets PWM pulse widths for multiple PWM outputs (e.g Thrusters and/or lights).
 * 
 * Parses the data buffer containing pulse width values,
 * clamps each to the valid range, and updates the corresponding PWM channels.
 * 
 * @param data Pointer to buffer containing pulse widths in microseconds (format: [MSB, LSB] per output)
 * @param outputs Pointer to array of pwm_output structs
 * @param count Number of outputs to set
 * @param min_us Minimum pulse width in microseconds
 * @param max_us Maximum pulse width in microseconds
 * @param frame_us Total PWM frame duration in microseconds
 */
static void set_pwm_outputs(const uint8_t *data, struct pwm_output *outputs, size_t count, uint16_t min_us, uint16_t max_us, uint32_t frame_us);

/**
 * @brief Handles incoming CAN messages and dispatches them to their corresponding action.
 */
static void message_handler(void);

/**
 * @brief Monitors thruster current draw and shuts down on overcurrent condition.
 * 
 * Reads ADC samples for all 8 thruster channels, calculates output current from
 * the voltage, and disables all thrusters if any channel exceeds the rated current limit.
 */
static void check_overcurrent(void);

/**
 * @brief Sends an overcurrent fault message over CAN
 * 
 * Constructs and transmits a 7-byte CAN FD fault message containing diagnostic information about the fault condition. 
 * 
 * @param thruster_id Thruster identifier (0-7)
 * @param current Measured current value in Amperes
 * @param adc_raw Raw ADC reading 
 * @return true if message was transmitted successfully, false if not
 */
static bool send_thruster_fault(uint8_t thruster_id, float current, uint16_t adc_raw);

/**
 * @brief Sets PWM outputs to their neutral/off position
 * 
 * @param outputs Pointer to array of pwm_output structs
 * @param count Number of outputs to set
 * @param neutral_us Neutral pulse width in microseconds
 * @param frame_us Total PWM frame duration in microseconds
 */
static void set_pwm_neutral(struct pwm_output *outputs, size_t count, uint16_t neutral_us, uint32_t frame_us);

/**
 * @brief Clamps a value between a minimum and maximum bound.
 * 
 * @param value The value to clamp
 * @param low The lower bound (inclusive)
 * @param high The higher bound (inclusive)
 * @return The clamped value: low if value < low, high if value > high, otherwise value
 */
static inline uint16_t clamp(uint16_t value, uint16_t low, uint16_t high);

/**
 * @brief Write PWM duty cycle to the specified TCC instance and channel.
 * 
 * Routes the PWM write to the appropriate TCC peripheral based on instance number.
 * 
 * @param instance TCC instance number (0, 1 or 2)
 * @param channel PWM channel number within the instance
 * @param ticks Duty cycle value in timer ticks
 */
static inline void tcc_write(uint8_t instance, uint8_t channel, uint32_t ticks);

/**
 * @brief Converts a pulse width in microseconds to timer ticks.
 * 
 * Calculates the timer tick count needed to produce a specific pulse width
 * based on the timer's period and the PWM frame duration.
 * 
 * @param period_ticks Timer period in ticks 
 * @param pulse_us Desired pulse width in microseconds
 * @param frame_us Total PWM frame duration in microseconds
 * @return Number of timer ticks corresponding to the pulse width
 */
static inline uint32_t us_to_ticks(uint32_t period_ticks, uint16_t pulse_us, uint32_t frame_us);

/* Callbacks */
static void can_receive_callback(uint8_t numberOfMessage, uintptr_t context);
static void can_transmit_callback(uintptr_t context);

static void adc_sram_dma_callback(DMAC_TRANSFER_EVENT event, uintptr_t contextHandle);

/* --- Public functions --- */

void app_init(void) {
    // Configure CAN RAM & callbacks 
    CAN1_MessageRAMConfigSet(Can1MessageRAM);
    CAN1_RxFifoCallbackRegister(CAN_RX_FIFO_0, can_receive_callback, (uintptr_t)NULL);
    CAN1_TxFifoCallbackRegister(can_transmit_callback, (uintptr_t)NULL);
    
    // Configure DMA
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_1, adc_sram_dma_callback, 0);
    DMAC_ChannelTransfer(DMAC_CHANNEL_1, (const void *)&ADC0_REGS->ADC_RESULT, (const void *)adc_res, 16); // Each adc result is 16 bits=2 bytes. 8*2=16
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *)adc_seq_regs, (const void *)&ADC0_REGS->ADC_DSEQDATA, 32); // DSEQDATA is 32 bits=4 bytes. 8 * 4 = 32
    
    TCC0_PWMStart();
    TCC1_PWMStart();
    //TCC2_PWMStart();
    
    // Set all thrusters and lights to neutral on startup
    set_pwm_neutral(thrusters, 8, 1500, THRUSTER_PWM_PERIOD_US);
    set_pwm_neutral(lights, 1, 1100, LIGHT_PWM_PERIOD_US);

    
    ADC0_Enable(); // TODO: Remember to manually configure sample averaging in plib_adc0 before testing
    
    TC0_TimerStart();
    
    // Enable watchdog
    WDT_Enable();
}

void app_task(void) {
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
            set_pwm_neutral(thrusters, 8, 1500, THRUSTER_PWM_PERIOD_US);
            break;

        case TURN_LIGHTS_OFF:
            set_pwm_neutral(lights, 1, 1100, LIGHT_PWM_PERIOD_US);
            break;

        case RESET:
            /* Force a system reset */
            NVIC_SystemReset();
            break;

        case SET_THRUSTER_PWM:
            set_pwm_outputs(pData, thrusters, 8, 1000, 2000, THRUSTER_PWM_PERIOD_US);
            break;

        case SET_LIGHT_PWM:
            set_pwm_outputs(pData, lights, 1, 1100, 1900, LIGHT_PWM_PERIOD_US);
            break;
            
        default:
            /* Unknown event: ignore */
            break;
    }
}

static void check_overcurrent(void) {
    const float    ADC_VREF                  = 3.3f;
    const float    G_IMON                    = 18.18e-6f; // Amplifier gain 18.18 uA/A -> in A/A
    const float    R_IMON                    = 2550.0f;   // 2.55 kilo ohms resistor
    const uint8_t  THRUSTER_RATED_CURRENT    = 15U;       // From TSD7 datasheet  
    
    for (size_t sample = 0; sample < 8; sample++) {
        float V_Imon = (float)adc_res[sample] * ADC_VREF / 4095.0f;
        float I_out = V_Imon / (G_IMON * R_IMON);

        //printf("raw=%u  V_Imon=%.4f V  I_out=%.3f A\r\n",(unsigned)adc_res[sample], (double)((float)adc_res[sample]*ADC_VREF/4095.0f), (double)I_out);
        if (I_out > THRUSTER_RATED_CURRENT) {
            set_pwm_neutral(thrusters, 8, 1500, THRUSTER_PWM_PERIOD_US);
            
            if (!send_thruster_fault(sample, I_out, adc_res[sample])) {
                // Handle retransmission?
            }
            break;
        }
    }
}

static bool send_thruster_fault(uint8_t thruster_id, float current, uint16_t adc_raw) {
    CAN_TX_BUFFER *txBuffer = NULL;
    
    memset(txFiFo, 0x00, CAN1_TX_FIFO_BUFFER_SIZE);
    txBuffer = (CAN_TX_BUFFER*)txFiFo;
    
    txBuffer->id = WRITE_ID(0x45A); // Just a random ID
    txBuffer->dlc = 15;           // DLC 15 -> 64 Byte Payload
    txBuffer->fdf = 1;
    txBuffer->brs = 1;
    
    txBuffer->data[0] = thruster_id;
    txBuffer->data[1] = 0x00;
    memcpy(&txBuffer->data[2], &current, sizeof(float)); 
    memcpy(&txBuffer->data[6], &adc_raw, sizeof(uint16_t));
    
    return CAN1_MessageTransmitFifo(1, txBuffer);
}


static void set_pwm_outputs(const uint8_t *data, struct pwm_output *outputs, size_t count, uint16_t min_us, uint16_t max_us, uint32_t frame_us) {
    for (size_t i = 0; i < count; i++) {
        uint16_t pulse_us = ((uint16_t)data[2U * i] << 8) | (uint16_t)data[2U * i + 1U];
        
        pulse_us = clamp(pulse_us, min_us, max_us);
        
        uint32_t ticks = us_to_ticks(outputs[i].period_ticks, pulse_us, frame_us);
        
        tcc_write(outputs[i].instance, outputs[i].channel, ticks);
    }
    
    // Pet the watchdog after applying updates 
    WDT_Clear();
}

static void set_pwm_neutral(struct pwm_output *outputs, size_t count, uint16_t neutral_us, uint32_t frame_us) {
    for (size_t i = 0; i < count; i++) {
        uint32_t ticks = us_to_ticks(outputs[i].period_ticks, neutral_us, frame_us);
        tcc_write(outputs[i].instance, outputs[i].channel, ticks);
        
    }
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

static void can_receive_callback(uint8_t numberOfMessage, uintptr_t context) {
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

static void can_transmit_callback(uintptr_t context) {
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


