#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "definitions.h"
#include "app.h"
#include <stdio.h>

#define WRITE_ID(id) (id << 18)
#define READ_ID(id) (id >> 18)

#define PWM_MAX_STEP_US  25U
#define THRUSTER_TIMEOUT_TICKS 100U // ~5 seconds at 20Hz

/* =============================================================================
 * Serial framing protocol
 * Frame format: [ 0xAA | MSG_ID | LENGTH | PAYLOAD (LENGTH bytes) | CHECKSUM ]
 * Checksum: XOR of MSG_ID ^ LENGTH ^ all payload bytes
 * ============================================================================= */

/* Inbound message IDs */
#define MSG_TURN_THRUSTERS_OFF   0x01U
#define MSG_TURN_LIGHTS_OFF      0x02U
#define MSG_RESET                0x03U
#define MSG_SET_THRUSTER_PWM     0x04U
#define MSG_SET_LIGHT_PWM        0x05U

/* Outbound message IDs */
#define MSG_FLT_EVENT            0x10U
#define MSG_PGOOD_EVENT          0x11U
#define MSG_KILLSWITCH_EVENT     0x12U
#define MSG_CURRENT_MEASUREMENTS 0x13U

/* Framing constants */
#define UART_START_BYTE          0xAAU
#define UART_HEADER_SIZE         3U    // START(1) + MSG_ID(1) + LENGTH(1)
#define UART_MAX_PAYLOAD         16U   // Largest inbound payload: 8x uint16_t
#define UART_MAX_TX_FRAME        36U   // 3 header + 32 payload + 1 checksum

/* --- Constants --- */
static const uint32_t TCC0_PERIOD               = 38275;
static const uint32_t TCC1_PERIOD               = 38250;
static const uint32_t TCC2_PERIOD               = 38250;
static const uint32_t TC3_PERIOD                = 38250; 
static const uint32_t THRUSTER_PWM_PERIOD_US    = 20000U; // 50Hz
static const uint32_t LIGHT_PWM_PERIOD_US       = 20000U; // 50Hz

enum operating_mode {
    PWM_TCC,
    MPWM_TC,
};

struct pwm_output {
    enum operating_mode mode;
    uint8_t  instance;
    uint8_t  channel;
    uint32_t period_ticks;
    uint16_t min_us;
    uint16_t max_us;
    uint16_t neutral_us;
    uint32_t frame_us;
    uint16_t current_pulse_us;
    uint16_t target_pulse_us;
};

typedef enum {
    UART_STATE_WAIT_HEADER,   /* Waiting for 3-byte header */
    UART_STATE_WAIT_PAYLOAD,  /* Waiting for payload + checksum byte */
} uart_rx_state_t;

static uart_rx_state_t  uart_rx_state = UART_STATE_WAIT_HEADER;
static uint8_t          uart_header[UART_HEADER_SIZE];
uint8_t                 uart_payload[UART_MAX_PAYLOAD + 1U]; /* +1 for checksum */
volatile bool           uart_message_ready = false;
uint8_t                 uart_msg_id        = 0U;
uint8_t                 uart_msg_len       = 0U;
static uint8_t          uart_tx_frame[UART_MAX_TX_FRAME];

static volatile bool slew_tick = false;

static volatile bool adc_dma_done = false;
static volatile uint16_t adc_result_array[16];

static volatile uint16_t thruster_cmd_timeout = 0U;
static volatile bool thruster_timeout_flag = false;


static struct pwm_output thrusters[8] = {
    {PWM_TCC, 2, 0, TCC2_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500, 1500}, // TH1 -> TCC2_CC0
    {PWM_TCC, 2, 1, TCC2_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500, 1500}, // TH2 -> TCC2_CC1
    {PWM_TCC, 1, 0, TCC1_PERIOD, 1000, 2000, 1500, THRUSTER_PWM_PERIOD_US, 1500, 1500}, // TH3 -> TCC1_CC0
    {PWM_TCC, 1, 1, TCC1_PERIOD, 1000, 2000, 1500, THRUSTER_PWM_PERIOD_US, 1500, 1500}, // TH4 -> TCC1_CC1
    {PWM_TCC, 0, 1, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500, 1500}, // TH5 -> TCC0_CC1
    {PWM_TCC, 0, 0, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500, 1500}, // TH6 -> TCC0_CC0
    {PWM_TCC, 0, 3, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500, 1500}, // TH7 -> TCC0_CC3
    {PWM_TCC, 0, 2, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500, 1500}  // TH8 -> TCC0_CC2
};

static struct pwm_output lights[1] = {{MPWM_TC, 3, 1, TC3_PERIOD, 1100, 1900, 1100, LIGHT_PWM_PERIOD_US, 1100, 1100}}; // TC3_CC1

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
 * @brief Sets PWM pulse widths for multiple PWM outputs (e.g Thrusters and/or lights).
 * 
 * Parses the data buffer containing pulse width values,
 * clamps each to the valid range, and updates the corresponding PWM channels.
 * 
 * @param data Pointer to buffer containing pulse widths in microseconds (format: [MSB, LSB] per output)
 * @param outputs Pointer to array of pwm_output structs
 * @param count Number of outputs to set
 */
static void set_pwm_outputs(const uint8_t *data, struct pwm_output *outputs, size_t count);

static void set_light_output(const uint8_t *data, struct pwm_output *outputs, size_t count);


/**
 * @brief Handles incoming UART messages and dispatches them to their corresponding action.
 */
static void message_handler(void);

static bool send_flt_event(uint8_t context);

static bool send_pgood_event(uint8_t context);

static bool send_killswitch_event(uint8_t context);

static bool send_current_measurements(float I_arr[8]);

static void dispatch_hw_event(volatile uint8_t *mask, bool (*send)(uint8_t channel));

static uint8_t compute_checksum(uint8_t msg_id, uint8_t length, const uint8_t *payload);

bool uart_send_frame(uint8_t msg_id, const uint8_t *payload, uint8_t length);

/**
 * @brief Logs thruster current readings from the IMON pins for all 8 channels.
 * 
 * Reads ADC samples collected via DMA sleepwalking, converts to current using
 * the efuse IMON transfer function: I_out = V_Imon / (G_Imon * R_Imon)
 * where G_Imon = 18.31 uA/A and R_Imon = 2.697 kOhm for thrusters.
 */
static void log_current(void);

/**
 * @brief Sets PWM outputs to their neutral/off position
 * 
 * @param outputs Pointer to array of pwm_output structs
 * @param count Number of outputs to set
 */
static void set_pwm_neutral(struct pwm_output *outputs, size_t count);

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
static void uart_receive_callback(uintptr_t context);
static void adc_dma_callback(DMAC_TRANSFER_EVENT returned_event, uintptr_t MyDmacContext);
static void eic_pin_flt_thruster(uintptr_t context);
static void eic_pin_pg_thruster(uintptr_t context);
static void eic_pin_killswitch(uintptr_t context);
static void rtc_callback(RTC_TIMER32_INT_MASK intCause, uintptr_t context);

static void slew_pwm_outputs(void);

/* --- Public functions --- */

void app_init(void) {
    // Configure USART
    SERCOM2_USART_Enable();
    SERCOM2_USART_ReadCallbackRegister(uart_receive_callback, (uintptr_t)NULL);
    SERCOM2_USART_Read(uart_header, UART_HEADER_SIZE);
    
    
    // Configure callback for killswitch
    EIC_NMICallbackRegister(eic_pin_killswitch, 0);
    
    // Configure callbacks for FLT pins
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
    
    
    // Enable ADC
    ADC0_Enable();
    
    // Configure RTC
    RTC_Timer32CompareSet(51); // ~20Hz
    RTC_Timer32CallbackRegister(rtc_callback, 0);
    RTC_Timer32InterruptEnable(RTC_TIMER32_INT_MASK_CMP0);
    RTC_Timer32Start();
    
    // Configure DMA
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, adc_dma_callback, 0);
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *)&ADC0_REGS->ADC_RESULT, (const void *)adc_result_array, sizeof(adc_result_array));
    
    // Enable TCC 
    TCC0_PWMStart();
    TCC1_PWMStart();
    TCC2_PWMStart();
    
    // Set all thrusters and lights to neutral on startup
    set_pwm_neutral(thrusters, 8);
    set_pwm_neutral(lights, 1);
    
    // Enable TC3
    TC3_CompareStart();
}

void app_task(void) {
    if (slew_tick) {
        slew_tick = false;
        slew_pwm_outputs();
    }
    
    if (adc_dma_done) {
        adc_dma_done = false;
        log_current();
    }
    
    if (hw_events.flt_pending_mask) {
        dispatch_hw_event(&hw_events.flt_pending_mask, send_flt_event);
    }
    
    if (hw_events.pgood_pending_mask) {
        dispatch_hw_event(&hw_events.pgood_pending_mask, send_pgood_event);
    }
    
    if (hw_events.killswitch_pending_mask) {
        dispatch_hw_event(&hw_events.killswitch_pending_mask, send_killswitch_event);
    }
       
    if (uart_message_ready) {
        uart_message_ready = false;
        message_handler();
    }
    
    if (thruster_timeout_flag) {
        thruster_timeout_flag = false;
        set_pwm_neutral(thrusters, 8);
    }
    
}

static uint8_t compute_checksum(uint8_t msg_id, uint8_t length, const uint8_t *payload) {
    uint8_t csum = msg_id ^ length;
    for (uint8_t i = 0U; i < length; i++) {
        csum ^= payload[i];
    }
    return csum;
}

bool uart_send_frame(uint8_t msg_id, const uint8_t *payload, uint8_t length) {
    /* Sanity check: 3 header bytes + payload + 1 checksum must fit in tx buffer */
    if ((uint16_t)length + 4U > UART_MAX_TX_FRAME) {
        return false;
    }

    uint8_t checksum = compute_checksum(msg_id, length, payload);

    uart_tx_frame[0] = UART_START_BYTE;
    uart_tx_frame[1] = msg_id;
    uart_tx_frame[2] = length;

    if (length > 0U && payload != NULL) {
        memcpy(&uart_tx_frame[3], payload, length);
    }

    uart_tx_frame[3U + length] = checksum;

    return SERCOM2_USART_Write(uart_tx_frame, (size_t)(4U + length));
}

static void uart_receive_callback(uintptr_t context) {
    (void)context;

    switch (uart_rx_state) {

        case UART_STATE_WAIT_HEADER: {
            if (uart_header[0] != UART_START_BYTE) {
                /* Bad frame start, discard and wait for the next header */
                SERCOM2_USART_Read(uart_header, UART_HEADER_SIZE);
                return;
            }

            uart_msg_id  = uart_header[1];
            uart_msg_len = uart_header[2];

            if (uart_msg_len == 0U) {
                /* No payload: checksum is just MSG_ID ^ LENGTH ^ (no bytes) = MSG_ID ^ LENGTH.
                 * Still need to read the single checksum byte before validating. */
                uart_rx_state = UART_STATE_WAIT_PAYLOAD;
                SERCOM2_USART_Read(uart_payload, 1U); /* checksum only */
            } else {
                if (uart_msg_len > UART_MAX_PAYLOAD) {
                    /* LENGTH field is out of range, discard and resync */
                    SERCOM2_USART_Read(uart_header, UART_HEADER_SIZE);
                    return;
                }
                /* Read payload + checksum in one shot */
                uart_rx_state = UART_STATE_WAIT_PAYLOAD;
                SERCOM2_USART_Read(uart_payload, (size_t)(uart_msg_len + 1U));
            }
            break;
        }

        case UART_STATE_WAIT_PAYLOAD: {
            /* Checksum byte is always at uart_payload[uart_msg_len] */
            uint8_t received_checksum = uart_payload[uart_msg_len];
            uint8_t expected_checksum = compute_checksum(uart_msg_id, uart_msg_len, uart_payload);

            if (received_checksum == expected_checksum) {
                uart_message_ready = true; /* Signal app_task to process */
            }

            /* Always return to header state and re-arm */
            uart_rx_state = UART_STATE_WAIT_HEADER;
            SERCOM2_USART_Read(uart_header, UART_HEADER_SIZE);
            break;
        }

        default:
            uart_rx_state = UART_STATE_WAIT_HEADER;
            SERCOM2_USART_Read(uart_header, UART_HEADER_SIZE);
            break;
    }
}

static void message_handler(void) {
    switch (uart_msg_id) {
        case MSG_TURN_THRUSTERS_OFF:
            set_pwm_neutral(thrusters, 8);
            thruster_cmd_timeout = 0U;
            thruster_timeout_flag = false;
            break;

        case MSG_TURN_LIGHTS_OFF:
            set_pwm_neutral(lights, 1);
            break;

        case MSG_RESET:
            NVIC_SystemReset();
            break;

        case MSG_SET_THRUSTER_PWM:
            set_pwm_outputs(uart_payload, thrusters, 8);
            thruster_cmd_timeout = 0U;
            thruster_timeout_flag = false;
            break;

        case MSG_SET_LIGHT_PWM:
            set_light_output(uart_payload, lights, 1);
            break;

        default:
            /* Unknown message ID */
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


static bool send_flt_event(uint8_t channel) {
    uint8_t payload[2] = { channel, 0x01U };
    return uart_send_frame(MSG_FLT_EVENT, payload, 2U);
}

static bool send_pgood_event(uint8_t channel) {
    uint8_t payload[2] = { channel, 0x02U };
    return uart_send_frame(MSG_PGOOD_EVENT, payload, 2U);
}

static bool send_killswitch_event(uint8_t channel) {
    (void)channel;
    return uart_send_frame(MSG_KILLSWITCH_EVENT, NULL, 0U);
}

static bool send_current_measurements(float I_arr[8]) {
    uint8_t payload[32];
    for (size_t i = 0U; i < 8U; i++) {
        memcpy(&payload[i * sizeof(float)], &I_arr[i], sizeof(float));
    }
    
    return uart_send_frame(MSG_CURRENT_MEASUREMENTS, payload, 32U);
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

static void set_pwm_outputs(const uint8_t *data, struct pwm_output *outputs, size_t count) {
    const uint16_t *pulse_data = (const uint16_t *)data;
    for (size_t i = 0; i < count; i++) {
        
        uint16_t pulse_us = pulse_data[i];
        
        pulse_us = clamp(pulse_us, outputs[i].min_us, outputs[i].max_us);
        
        outputs[i].target_pulse_us = pulse_us; 
    }
}

static void set_light_output(const uint8_t *data, struct pwm_output *outputs, size_t count) {
    const uint16_t *pulse_data = (const uint16_t *)data;
    for (size_t i = 0; i < count; i++) {
        uint16_t pulse_us = pulse_data[i];
        pulse_us = clamp(pulse_us, outputs[i].min_us, outputs[i].max_us);

        uint32_t ticks = us_to_ticks(outputs[i].period_ticks, pulse_us, outputs[i].frame_us);
        TC3_Compare16bitMatch1Set(ticks);

        outputs[i].current_pulse_us = pulse_us;
        outputs[i].target_pulse_us  = pulse_us;
    }
}

static void slew_pwm_outputs(void) {
    for (size_t i = 0; i < 8U; i++) {
        uint16_t target  = thrusters[i].target_pulse_us;
        uint16_t current = thrusters[i].current_pulse_us;

        if (current < target) {
            uint16_t step = target - current;
            current += (step > PWM_MAX_STEP_US) ? PWM_MAX_STEP_US : step;
        } else if (current > target) {
            uint16_t step = current - target;
            current -= (step > PWM_MAX_STEP_US) ? PWM_MAX_STEP_US : step;
        }

        if (current != thrusters[i].current_pulse_us) {
            thrusters[i].current_pulse_us = current;
            uint32_t ticks = us_to_ticks(thrusters[i].period_ticks,
                                         current,
                                         thrusters[i].frame_us);
            
            tcc_write(thrusters[i].instance, thrusters[i].channel, ticks);
            
        }
    }
}

static void set_pwm_neutral(struct pwm_output *outputs, size_t count) {
    for (size_t i = 0; i < count; i++) {
        uint32_t ticks = us_to_ticks(outputs[i].period_ticks, outputs[i].neutral_us, outputs[i].frame_us);
        
        if (outputs[i].mode == PWM_TCC) {
            tcc_write(outputs[i].instance, outputs[i].channel, ticks);
        } else if (outputs[i].mode == MPWM_TC) {
            TC3_Compare16bitMatch1Set(ticks);
        } 
        
        outputs[i].current_pulse_us = outputs[i].neutral_us; 
        outputs[i].target_pulse_us = outputs[i].neutral_us;
        
    }
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
            TCC2_PWM16bitDutySet(channel, ticks);
            break;
            
        default: 
            break;
    }
}

static inline uint32_t us_to_ticks(uint32_t period_ticks, uint16_t pulse_us, uint32_t frame_us) {
    return ((uint32_t)pulse_us * (period_ticks + 1U)) / frame_us;
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
    
    if (thruster_cmd_timeout < THRUSTER_TIMEOUT_TICKS) {
        thruster_cmd_timeout++;
    } else {
        thruster_timeout_flag = true;
    }
    
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


