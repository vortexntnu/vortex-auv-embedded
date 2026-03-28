
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "definitions.h"
#include "app.h"

#define WRITE_ID(id) (id << 18)
#define READ_ID(id) (id >> 18)

#define TRANSFER_SIZE 16

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
static const uint32_t TCC0_PERIOD               = 38320;
static const uint32_t TCC1_PERIOD               = 38320;
static const uint32_t TCC2_PERIOD               = 38320;
static const uint32_t TC3_PERIOD                = 65535; 
static const uint32_t THRUSTER_PWM_PERIOD_US    = 20000U; // 50Hz
static const uint32_t LIGHT_PWM_PERIOD_US       = 20000U; // 50Hz

enum operating_mode {
    PWM_TCC,
    MPWM_TC,
};

/* --- Types --- */
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
};

enum can_events {
    TURN_THRUSTERS_OFF = 0x369,
    TURN_LIGHTS_OFF    = 0x36A,
    RESET              = 0x36B,
    SET_THRUSTER_PWM   = 0x36C,
    SET_LIGHT_PWM      = 0x36D
};

/* UART receive state machine */
typedef enum {
    UART_STATE_WAIT_HEADER,   /* Waiting for 3-byte header */
    UART_STATE_WAIT_PAYLOAD,  /* Waiting for payload + checksum byte */
} uart_rx_state_t;

static uart_rx_state_t  uart_rx_state = UART_STATE_WAIT_HEADER;
static uint8_t          uart_header[UART_HEADER_SIZE];
uint8_t          uart_payload[UART_MAX_PAYLOAD + 1U]; /* +1 for checksum */
volatile bool    uart_message_ready = false; uint8_t          uart_msg_id        = 0U; uint8_t          uart_msg_len       = 0U;

/* Shared TX frame buffer */
static uint8_t uart_tx_frame[UART_MAX_TX_FRAME];

/* --- Private states --- */
/* CAN */
static uint8_t Can1MessageRAM[CAN1_MESSAGE_RAM_CONFIG_SIZE] __attribute__((aligned(32)));

static volatile uint32_t can_status = 0;
static volatile bool can_message_received = false;

static uint8_t txFiFo[CAN1_TX_FIFO_BUFFER_SIZE];
static uint8_t rxFiFo0[CAN1_RX_FIFO0_SIZE];

/* ADC */
static volatile bool adc_dma_done = false;
static uint16_t adc_result_array[TRANSFER_SIZE];


/* Application */
static struct pwm_output thrusters[8] = {
    {PWM_TCC, 2, 0, TCC2_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500}, // TH1 -> TCC2_CC0
    {PWM_TCC, 2, 1, TCC2_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500}, // TH2 -> TCC2_CC1
    {PWM_TCC, 1, 0, TCC1_PERIOD, 1000, 2000, 1500, THRUSTER_PWM_PERIOD_US, 1500}, // TH3 -> TCC1_CC0
    {PWM_TCC, 1, 1, TCC1_PERIOD, 1000, 2000, 1500, THRUSTER_PWM_PERIOD_US, 1500}, // TH4 -> TCC1_CC1
    {PWM_TCC, 0, 1, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500}, // TH5 -> TCC0_CC1
    {PWM_TCC, 0, 0, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500}, // TH6 -> TCC0_CC0
    {PWM_TCC, 0, 3, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500}, // TH7 -> TCC0_CC3
    {PWM_TCC, 0, 2, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US, 1500}  // TH8 -> TCC0_CC2
};

static struct pwm_output lights[1] = {{MPWM_TC, 3, 1, TC3_PERIOD, 1100, 1900, 1100, LIGHT_PWM_PERIOD_US, 1100}}; // TC3_CC1

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

// FOR TESTING
void test_thrusters(const uint8_t *thruster_indices, size_t count, uint16_t max_us, uint16_t min_us, uint16_t step_us, uint32_t step_delay_ms);
void test_thrusters_seq(const uint8_t *thruster_indices, size_t count, uint16_t max_us, uint16_t min_us, uint16_t step_us, uint16_t step_delay_ms);
void test_neutral_to_max(const uint8_t *thruster_indices, size_t count, uint16_t max_us, uint32_t hold_ms);
void test_thrusters_split(const uint8_t *thruster_indices, size_t count, uint16_t max_us, uint16_t min_us, uint16_t step_us, uint32_t step_delay_ms);


void generate_pwm_signals();
void test_can_rx();
void test_can_tx();

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
 */
static void set_pwm_outputs(const uint8_t *data, struct pwm_output *outputs, size_t count);

/**
 * @brief Handles incoming CAN messages and dispatches them to their corresponding action.
 */
static void message_handler(void);

static bool send_flt_event(uint8_t context);

static bool send_pgood_event(uint8_t context);

static bool send_killswitch_event(uint8_t context);

static bool send_current_measurements(float I_arr[8]);

static void dispatch_hw_event(volatile uint8_t *mask, bool (*send)(uint8_t channel));

/* UART */
static uint8_t compute_checksum(uint8_t msg_id, uint8_t length, const uint8_t *payload);
bool    uart_send_frame(uint8_t msg_id, const uint8_t *payload, uint8_t length);

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
static void can_receive_callback(uint8_t numberOfMessage, uintptr_t context);
static void can_transmit_callback(uintptr_t context);
static void adc_dma_callback(DMAC_TRANSFER_EVENT returned_event, uintptr_t MyDmacContext);
static void eic_pin_flt_thruster(uintptr_t context);
static void eic_pin_pg_thruster(uintptr_t context);
static void eic_pin_killswitch(uintptr_t context);

/* --- Public functions --- */

void app_init(void) {
    /* Register UART receive callback and arm the first header read.
     * From this point the receive is self-sustaining: the callback
     * always re-arms itself before returning. */
    SERCOM2_USART_Enable();
    SERCOM2_USART_ReadCallbackRegister(uart_receive_callback, (uintptr_t)NULL);
    SERCOM2_USART_Read(uart_header, UART_HEADER_SIZE);
    
    // Configure CAN RAM & callbacks 
    CAN1_MessageRAMConfigSet(Can1MessageRAM);
    CAN1_RxFifoCallbackRegister(CAN_RX_FIFO_0, can_receive_callback, (uintptr_t)NULL);
    CAN1_TxFifoCallbackRegister(can_transmit_callback, (uintptr_t)NULL);
    
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
    RTC_Timer32Start();
    RTC_Timer32CompareSet(50);
    
    // Configure SysTick Timer
    // SYSTICK_TimerStart();
    
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

//    for (int i = 0; i < 1000000; i++) {
//        __NOP();
//    }
    
    // Enable TC
    TC0_TimerStart();
    TC3_CompareStart();
    
    ADC0_ConversionStart();
    
    // Enable watchdog
    //WDT_Enable();
}

void app_task(void) {
    
//    if (ADC0_ConversionSequenceIsFinished()) {
//            ADC0_ConversionStart();
//        }
//    
//    if (adc_dma_done) {
//        adc_dma_done = false;
//        log_current();
//    }
//    
//    if (hw_events.flt_pending_mask) {
//        dispatch_hw_event(&hw_events.flt_pending_mask, send_flt_event);
//    }
//    
//    if (hw_events.pgood_pending_mask) {
//        dispatch_hw_event(&hw_events.pgood_pending_mask, send_pgood_event);
//    }
//    
//    if (hw_events.killswitch_pending_mask) {
//        dispatch_hw_event(&hw_events.killswitch_pending_mask, send_killswitch_event);
//    }
        
    
   if (can_message_received) {
       can_message_received = false;
       test_can_rx();
   }
    
    // if (uart_message_ready) {
    //     uart_message_ready = false;
    //     message_handler();
    // }
    
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
        printf("(uint16_t)length + 4U > UART_MAX_TX_FRAME is the fault");
        return false;
    }

    uint8_t checksum = compute_checksum(msg_id, length, payload);
    printf("checksum = %u\n", (unsigned int)checksum);

    uart_tx_frame[0] = UART_START_BYTE;
    uart_tx_frame[1] = msg_id;
    uart_tx_frame[2] = length;

    if (length > 0U && payload != NULL) {
        memcpy(&uart_tx_frame[3], payload, length);
    }

    uart_tx_frame[3U + length] = checksum;

    return SERCOM2_USART_Write(uart_tx_frame, (size_t)(4U + length));
}

static void dump_bytes(const char *label, const uint8_t *data, size_t len) {
    printf("%s (%u): ", label, (unsigned)len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X ", data[i]);
    }
    printf("\r\n");
}

static void uart_receive_callback(uintptr_t context) {
    (void)context;

    printf("RX callback\r\n");

    switch (uart_rx_state) {

        case UART_STATE_WAIT_HEADER: {
            /* Validate start byte */
            if (uart_header[0] != UART_START_BYTE) {
                /* Bad frame start - discard and wait for the next header */
                SERCOM2_USART_Read(uart_header, UART_HEADER_SIZE);
                return;
            }

            uart_msg_id  = uart_header[1];
            uart_msg_len = uart_header[2];

            printf("%u, %u\r\n", uart_msg_id, uart_msg_len);

            if (uart_msg_len == 0U) {
                /* No payload: checksum is just MSG_ID ^ LENGTH ^ (no bytes) = MSG_ID ^ LENGTH.
                 * We still need to read the single checksum byte before validating. */
                uart_rx_state = UART_STATE_WAIT_PAYLOAD;
                SERCOM2_USART_Read(uart_payload, 1U); /* checksum only */
            } else {
                if (uart_msg_len > UART_MAX_PAYLOAD) {
                    /* LENGTH field is out of range - discard and resync */
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

            printf("%u checksum %u expected\r\n", received_checksum, expected_checksum);

            dump_bytes("PWM", uart_payload, uart_msg_len);

            if (received_checksum == expected_checksum) {
                uart_message_ready = true; /* Signal app_task to process */
            }
            /* On checksum mismatch we silently drop the frame */

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

/* --- Private helpers --- */

static void message_handler(void) {
    printf("Entering message handler\r\n");
    switch (uart_msg_id) {
        case MSG_TURN_THRUSTERS_OFF:
            set_pwm_neutral(thrusters, 8);
            break;

        case MSG_TURN_LIGHTS_OFF:
            set_pwm_neutral(lights, 1);
            break;

        case MSG_RESET:
            NVIC_SystemReset();
            break;

        case MSG_SET_THRUSTER_PWM:
            set_pwm_outputs(uart_payload, thrusters, 8);
            break;

        case MSG_SET_LIGHT_PWM:
            set_pwm_outputs(uart_payload, lights, 1);
            break;

        default:
            /* Unknown message ID - ignore */
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
    const float G_IMON     = 18.31e-6f;  // Efuse current monitor gain: 18.31 uA/A
    const float R_IMON     = 2697.0f;    // 2.697 kOhm sense resistor for thrusters
    
    float I_array[8] = {0};
    
    for (size_t i = 0; i < 8; i++) {
        float V_Imon = ((float)adc_result_array[i] * ADC_VREF) / 4095.0f;
        float I_out  = V_Imon / (G_IMON * R_IMON);
        
        I_array[i] = I_out;
        
        send_current_measurements(I_array);
        
//        printf("\nTH%u (AIN%u) raw=%u  V=%.4f  I=%.3f A PWM=%u us\r\n",
//               imon_map[i].thruster,
//               imon_map[i].ain,
//               (unsigned)adc_result_array[i],
//               V_Imon,
//               I_out, 
//               (unsigned)thrusters[i].current_pulse_us);
        //printf("%u %.3f\n", (unsigned)thrusters[imon_map[i].thruster - 1].current_pulse_us, I_out);
    }
    
    //bool result = send_current_measurements(I_array);
    
    //if (!result) {
      //  printf("CAN Transmission of current measurements failed!\r\n");
    //}
}

static void set_pwm_outputs(const uint8_t *data, struct pwm_output *outputs, size_t count) {
    const uint16_t *pulse_data = (const uint16_t *)data;
    for (size_t i = 0; i < count; i++) {
        
        uint16_t pulse_us = pulse_data[i];
        printf("Thruser %d pwm: %d", i, pulse_us);
        
        pulse_us = clamp(pulse_us, outputs[i].min_us, outputs[i].max_us);
        
        uint32_t ticks = us_to_ticks(outputs[i].period_ticks, pulse_us, outputs[i].frame_us);
        
        if (outputs[i].mode == PWM_TCC) {
            tcc_write(outputs[i].instance, outputs[i].channel, ticks);
        } else if (outputs[i].mode == MPWM_TC) {
            TC3_Compare16bitPeriodSet(ticks);
        } 
        
        outputs[i].current_pulse_us = pulse_us; // Update struct
    }
    
    // Pet the watchdog after applying updates 
    //WDT_Clear();
}

static void set_pwm_neutral(struct pwm_output *outputs, size_t count) {
    for (size_t i = 0; i < count; i++) {
        uint32_t ticks = us_to_ticks(outputs[i].period_ticks, outputs[i].neutral_us, outputs[i].frame_us);
        
        if (outputs[i].mode == PWM_TCC) {
            tcc_write(outputs[i].instance, outputs[i].channel, ticks);
        } else if (outputs[i].mode == MPWM_TC) {
            TC3_Compare16bitPeriodSet(ticks);
        } 
        
        outputs[i].current_pulse_us = outputs[i].neutral_us; // Update struct
        
    }
    //WDT_Clear();
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

void test_can_rx() {
    CAN_RX_BUFFER *rxBuf = (CAN_RX_BUFFER *)rxFiFo0;
    
    uint32_t id = rxBuf->xtd ? rxBuf->id : READ_ID(rxBuf->id);
    const uint8_t *pData = rxBuf->data;
    
    printf("CAN RX | ID: 0x%08lX (%s) | DLC: %u | Data:",
       (unsigned long)id,
       rxBuf->xtd ? "EXT" : "STD",
       (unsigned int)rxBuf->dlc);

    for (uint8_t i = 0; i < rxBuf->dlc; i++) {
        printf(" %02X", pData[i]);
    }
    printf("\n");
    
}

void test_can_tx() {
    CAN_TX_BUFFER *txBuffer = NULL;
    
    memset(txFiFo, 0x00, CAN1_TX_FIFO_BUFFER_SIZE);
    txBuffer = (CAN_TX_BUFFER*)txFiFo;
    
    txBuffer->id = WRITE_ID(0x215);
    txBuffer->dlc = 1;
    txBuffer->fdf = 1;
    txBuffer->brs = 0;
    
    txBuffer->data[0] = 0xAA;
    
    bool result = CAN1_MessageTransmitFifo(1, txBuffer);

    if (!result) {
        printf("ERROR: CAN1_MessageTransmitFifo failed!\r\n");
    }
    
    // SYSTICK_DelayMs(2000);
    
    
}
//
// void test_thrusters(const uint8_t *thruster_indices, size_t count, uint16_t max_us, uint16_t min_us, uint16_t step_us, uint32_t step_delay_ms) {
//     /* Beregn test_max og test_min per thruster */
//     uint16_t test_max[8];
//     uint16_t test_min[8];
//     for (size_t i = 0; i < count; i++) {
//         uint8_t idx = thruster_indices[i];
//         test_max[i] = clamp(max_us, thrusters[idx].neutral_us, thrusters[idx].max_us);
//         test_min[i] = clamp(min_us, thrusters[idx].min_us,     thrusters[idx].neutral_us);
//     }
//
//     /* --- Neutral -> Max --- */
//     for (uint16_t pw = 1500; pw <= max_us; pw += step_us) {
//         for (size_t i = 0; i < count; i++) {
//             uint8_t idx = thruster_indices[i];
//             struct pwm_output *th = &thrusters[idx];
//             uint16_t clamped = clamp(pw, th->neutral_us, test_max[i]);
//             uint32_t ticks = us_to_ticks(th->period_ticks, clamped, th->frame_us);
//             tcc_write(th->instance, th->channel, ticks);
//             th->current_pulse_us = clamped;
//         }
//         SYSTICK_DelayMs(step_delay_ms);
//         if (adc_dma_done) { adc_dma_done = false; log_current(); }
//         //WDT_Clear();
//
//         // Hold 3 seconds at max
//         SYSTICK_DelayMs(6000);
//     }
//
//     /* --- Max -> Neutral --- */
//     for (uint16_t pw = max_us; pw >= 1500; pw -= step_us) {
//         for (size_t i = 0; i < count; i++) {
//             uint8_t idx = thruster_indices[i];
//             struct pwm_output *th = &thrusters[idx];
//             uint16_t clamped = clamp(pw, th->neutral_us, test_max[i]);
//             uint32_t ticks = us_to_ticks(th->period_ticks, clamped, th->frame_us);
//             tcc_write(th->instance, th->channel, ticks);
//             th->current_pulse_us = clamped;
//         }
//         SYSTICK_DelayMs(step_delay_ms);
//         if (adc_dma_done) { adc_dma_done = false; log_current(); }
//         //WDT_Clear();
//         if (pw < step_us) break;
//
//         // Hold 3 seconds at n
//         SYSTICK_DelayMs(3000);
//     }
//
//     /* --- Neutral -> Min --- */
//     for (uint16_t pw = 1500; pw >= min_us; pw -= step_us) {
//         for (size_t i = 0; i < count; i++) {
//             uint8_t idx = thruster_indices[i];
//             struct pwm_output *th = &thrusters[idx];
//             uint16_t clamped = clamp(pw, test_min[i], th->neutral_us);
//             uint32_t ticks = us_to_ticks(th->period_ticks, clamped, th->frame_us);
//             tcc_write(th->instance, th->channel, ticks);
//             th->current_pulse_us = clamped;
//         }
//         SYSTICK_DelayMs(step_delay_ms);
//         if (adc_dma_done) { adc_dma_done = false; log_current(); }
//         //WDT_Clear();
//         if (pw < step_us) break;
//
//         // Hold 6 seconds at min
//         SYSTICK_DelayMs(6000);
//     }
//
//     /* --- Min -> Neutral --- */
//     for (uint16_t pw = min_us; pw <= 1500; pw += step_us) {
//         for (size_t i = 0; i < count; i++) {
//             uint8_t idx = thruster_indices[i];
//             struct pwm_output *th = &thrusters[idx];
//             uint16_t clamped = clamp(pw, test_min[i], th->neutral_us);
//             uint32_t ticks = us_to_ticks(th->period_ticks, clamped, th->frame_us);
//             tcc_write(th->instance, th->channel, ticks);
//             th->current_pulse_us = clamped;
//         }
//         SYSTICK_DelayMs(step_delay_ms);
//         if (adc_dma_done) { adc_dma_done = false; log_current(); }
//         //WDT_Clear();
//         // Hold 3 seconds at n
//         SYSTICK_DelayMs(3000);
//     }
//
//     /* Snap alle til n�ytral */
//     for (size_t i = 0; i < count; i++) {
//         uint8_t idx = thruster_indices[i];
//         struct pwm_output *th = &thrusters[idx];
//         uint32_t ticks = us_to_ticks(th->period_ticks, th->neutral_us, th->frame_us);
//         tcc_write(th->instance, th->channel, ticks);
//         th->current_pulse_us = th->neutral_us;
//     }
// }
// //
// void test_thrusters_seq(const uint8_t *thruster_indices, size_t count, uint16_t max_us, uint16_t min_us, uint16_t step_us, uint16_t step_delay_ms) {
//     for (size_t i = 0; i < count; i++) {
//         uint8_t idx = thruster_indices[i];
//         if (idx >= 8) {
//             continue;
//         }
//
//         struct pwm_output *th = &thrusters[idx];
//
//         uint16_t test_max = clamp(max_us, th->neutral_us, th->max_us);
//         uint16_t test_min = clamp(min_us, th->min_us, th->neutral_us);
//
//         /* Neutral -> Max */
//         for (uint16_t pw = th->neutral_us; pw <= max_us; pw += step_us) {
//             uint32_t ticks = us_to_ticks(th->period_ticks, pw, th->frame_us);
//             tcc_write(th->instance, th->channel, ticks);
//             th->current_pulse_us = pw;
//             SYSTICK_DelayMs(step_delay_ms);
//             if (adc_dma_done) {
//                 adc_dma_done = false;
//                 log_current();
//             }
//             // WDT_Clear();
//         }
//
//         // Hold 3 seconds at max
//         SYSTICK_DelayMs(3000);
//
//         /* Max -> Neutral */
//         for (uint16_t pw = test_max; pw >= th->neutral_us; pw -= step_us) {
//             uint32_t ticks = us_to_ticks(th->period_ticks, pw, th->frame_us);
//             tcc_write(th->instance, th->channel, ticks);
//             th->current_pulse_us = pw;
//             SYSTICK_DelayMs(step_delay_ms);
//             if (adc_dma_done) {
//                 adc_dma_done = false;
//                 log_current();
//             }
//             //WDT_Clear();
//             if (pw < step_us) break; /* Underflow guard */
//         }
//
//         /* Snap to neutral */
//         uint32_t neutral_ticks = us_to_ticks(th->period_ticks, th->neutral_us, th->frame_us);
//         tcc_write(th->instance, th->channel, neutral_ticks);
//         th->current_pulse_us = th->neutral_us;
//
//         /* --- Neutral -> Min --- */
//         for (uint16_t pw = th->neutral_us; pw >= test_min; pw -= step_us) {
//             uint32_t ticks = us_to_ticks(th->period_ticks, pw, th->frame_us);
//             tcc_write(th->instance, th->channel, ticks);
//             th->current_pulse_us = pw;
//             SYSTICK_DelayMs(step_delay_ms);
//             if (adc_dma_done) {
//                 adc_dma_done = false;
//                 log_current();
//             }
//             //WDT_Clear();
//             if (pw < step_us) break; /* Underflow guard */
//         }
//
//         // Hold 3 seconds at min
//         SYSTICK_DelayMs(3000);
//
//         /* --- Min -> Neutral --- */
//         for (uint16_t pw = test_min; pw <= th->neutral_us; pw += step_us) {
//             uint32_t ticks = us_to_ticks(th->period_ticks, pw, th->frame_us);
//             tcc_write(th->instance, th->channel, ticks);
//             th->current_pulse_us = pw;
//             SYSTICK_DelayMs(step_delay_ms);
//             if (adc_dma_done) {
//                 adc_dma_done = false;
//                 log_current();
//             }
//             //WDT_Clear();
//         }
//
//         tcc_write(th->instance, th->channel, neutral_ticks);
//         th->current_pulse_us = th->neutral_us;
//
//         // Hold 3 seconds at neutral
//         SYSTICK_DelayMs(3000);
//     }
// }

void test_thrusters_split(const uint8_t *thruster_indices, size_t count, uint16_t max_us, uint16_t min_us, uint16_t step_us, uint32_t step_delay_ms) {
        size_t first_count = (count < 4) ? count : 4;
        test_thrusters(thruster_indices, first_count, max_us, min_us, step_us, step_delay_ms);
        
        if (count > 4) {
            test_thrusters(thruster_indices + 4, count - 4, max_us, min_us, step_us, step_delay_ms);
        }

        
}

void test_neutral_to_max(const uint8_t *thruster_indices, size_t count, uint16_t max_us, uint32_t hold_ms) {
    for (size_t i = 0; i < count; i++) {
        uint8_t idx = thruster_indices[i];
        struct pwm_output *th = &thrusters[idx];

        /* Neutral -> Spin-up (1750us for 1 second) */
        uint32_t ticks = us_to_ticks(th->period_ticks, 1750, th->frame_us);
        tcc_write(th->instance, th->channel, ticks);
        th->current_pulse_us = 1750;
        SYSTICK_DelayMs(1000);

        /* Spin-up -> Max */
        ticks = us_to_ticks(th->period_ticks, max_us, th->frame_us);
        tcc_write(th->instance, th->channel, ticks);
        th->current_pulse_us = max_us;

        /* Hold */
        uint32_t elapsed = 0;
        while (elapsed < hold_ms) {
            SYSTICK_DelayMs(20);
            elapsed += 20;
            if (adc_dma_done) { adc_dma_done = false; log_current(); }
        }

        /* Back to neutral */
        ticks = us_to_ticks(th->period_ticks, th->neutral_us, th->frame_us);
        tcc_write(th->instance, th->channel, ticks);
        th->current_pulse_us = th->neutral_us;

        SYSTICK_DelayMs(2000);
    }
}

void generate_pwm_signals() {
    // PWM 4 | TCC1_WO2 | Correctly configured
    // PWM 3 | TCC1_WO3 | Correctly configured
    // PWM 8 | TCC0_WO2 | Correctly configured
    // PWM 7 | TCC0_WO3 | Correctly configured
    // PWM 6 | TCC0_WO4 | Correctly configured
    // PWM 5 | TCC0_WO5 | Correctly configured
    // PWM 1 | TCC2_WO0 | Correctly configured
    // PWM 2 | TCC2_WO1 | Correctly configured
    // PWM 9 | TC3_ WO1 | PB01 | Working
    
    uint8_t instance = 1;
    uint8_t channel = 0;
    uint32_t period = TCC1_PERIOD;
    uint32_t frame_period = THRUSTER_PWM_PERIOD_US;
    
    static int increment = 1;
    static uint16_t pulse_us = 1000;
    
    pulse_us = clamp(pulse_us, 1000, 2000);
    
    if (pulse_us >= 2000 || pulse_us <=1000) {
        increment *= -1;
    } 
    
    pulse_us += increment;
    
    
    //uint32_t ticks = us_to_ticks(TC3_PERIOD, pulse_us, LIGHT_PWM_PERIOD_US);
     
    uint32_t ticks = us_to_ticks(period, pulse_us, frame_period);
    //bool ok = TC3_Compare16bitMatch1Set((uint16_t)ticks);
    
    
    tcc_write(instance, channel, ticks);
    
    //TC3_Compare16bitMatch1Set(ticks);
    
    //WDT_Clear();
    
    //TCC1_PWM24bitDutySet(1, 10000);
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

static void can_receive_callback(uint8_t numberOfMessage, uintptr_t context) {
    // Check CAN Status
    can_status = CAN1_ErrorGet();
    printf("CAN interrupt occurred\r\n");

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

static void adc_dma_callback(DMAC_TRANSFER_EVENT returned_event, uintptr_t MyDmacContext) {
    if (returned_event == DMAC_TRANSFER_EVENT_COMPLETE) {
        adc_dma_done = true;
        // Re-arm DMA for next conversion
        DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *)&ADC0_REGS->ADC_RESULT, (const void *)adc_result_array, sizeof(adc_result_array));
    } 
    else if (returned_event == DMAC_TRANSFER_EVENT_ERROR) {
        printf("ERROR: DMAC Transfer Failed!\r\n");
    }
}

static void eic_pin_flt_thruster(uintptr_t context) {
    uint8_t channel = (uint8_t)context;
    hw_events.flt_pending_mask |= (1U << channel);
    
    printf("Fault pin triggered for thruster %u\n", (unsigned int)channel);    
}

static void eic_pin_pg_thruster(uintptr_t context) {
    uint8_t channel = (uint8_t)context;
    hw_events.pgood_pending_mask |= (1U << channel);
    
    printf("PGOOD pin triggered for thruster  %u\n", (unsigned int)channel);
}

static void eic_pin_killswitch(uintptr_t context) {
    hw_events.killswitch_pending_mask |= 1U;
    printf("KILLSWITCH triggered \n");
}

