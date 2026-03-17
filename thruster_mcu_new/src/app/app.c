#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include "definitions.h"
#include "app.h"

#define WRITE_ID(id) (id << 18)
#define READ_ID(id) (id >> 18)

#define TRANSFER_SIZE 16

/* --- Constants --- */
static const uint32_t TCC0_PERIOD               = 59500;
static const uint32_t TCC1_PERIOD               = 59500;
static const uint32_t TCC2_PERIOD               = 59550;
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
static volatile bool adc_dma_done = false;
static uint16_t adc_result_array[TRANSFER_SIZE];


/* Application */
static const struct pwm_output thrusters[8] = {
    {PWM_TCC, 1, 0, TCC1_PERIOD, 1000, 2000, 1500, THRUSTER_PWM_PERIOD_US}, // TCC1_CC0
    {PWM_TCC, 1, 1, TCC1_PERIOD, 1000, 2000, 1500, THRUSTER_PWM_PERIOD_US}, // TCC1_CC1
    {PWM_TCC, 0, 2, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US}, // TCC0_CC2
    {PWM_TCC, 0, 3, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US}, // TCC0_CC3
    {PWM_TCC, 0, 0, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US}, // TCC0_CC0
    {PWM_TCC, 0, 1, TCC0_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US}, // TCC0_CC1
    {PWM_TCC, 2, 0, TCC2_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US}, // TCC2_CC0
    {PWM_TCC, 2, 1, TCC2_PERIOD, 1000 ,2000, 1500, THRUSTER_PWM_PERIOD_US}  // TCC2_CC1
};

static const struct {
    uint8_t ain;
    uint8_t thruster;
} imon_map[8] = {
    { 0, 3 },   /* slot 0: AIN0  ? Thruster 3 */  
    { 1, 4 },   /* slot 1: AIN1  ? Thruster 4 */
    { 2, 1 },   /* slot 2: AIN2  ? Thruster 1 */
    { 4, 2 },   /* slot 3: AIN4  ? Thruster 2 */
    { 5, 5 },   /* slot 4: AIN5  ? Thruster 5 */
    { 6, 6 },   /* slot 5: AIN6  ? Thruster 6 */
    { 7, 7 },   /* slot 6: AIN7  ? Thruster 7 */
    { 9, 8 },   /* slot 7: AIN9  ? Thruster 8 */
};

static const struct pwm_output lights[1] = {{MPWM_TC, 3, 1, TC3_PERIOD, 1100, 1900, 1100, LIGHT_PWM_PERIOD_US}}; // TC3_CC1. For MPWM TOP = CC0 and duty cycle is determined by CC1

// FOR TESTING
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
static void set_pwm_outputs(const uint8_t *data, const struct pwm_output *outputs, size_t count);

/**
 * @brief Handles incoming CAN messages and dispatches them to their corresponding action.
 */
static void message_handler(void);

/**
 * @brief Logs thruster current readings from the IMON pins for all 8 channels.
 * 
 * Reads ADC samples collected via DMA sleepwalking, converts to current using
 * the efuse IMON transfer function: I_out = V_Imon / (G_Imon * R_Imon)
 * where G_Imon = 18.31 uA/A and R_Imon = 4.6 kOhm for thrusters.
 */
static void log_current(void);

/**
 * @brief Sends a fault message over CAN when an EIC FLT interrupt fires.
 * 
 * Constructs and transmits a CAN FD fault message identifying which thruster
 * triggered a hardware fault via the FLT pin.
 * 
 * @param thruster_id Thruster identifier (0-7)
 * @return true if message was transmitted successfully, false if not
 */
static bool send_thruster_fault(uint8_t thruster_id);

/**
 * @brief Sets PWM outputs to their neutral/off position
 * 
 * @param outputs Pointer to array of pwm_output structs
 * @param count Number of outputs to set
 */
static void set_pwm_neutral(const struct pwm_output *outputs, size_t count);

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
static void adc_dma_callback(DMAC_TRANSFER_EVENT returned_event, uintptr_t MyDmacContext);
static void eic_pin_flt_thruster(uintptr_t context);
static void eic_pin_pg_thruster(uintptr_t context);
static void eic_pin_killswitch(uintptr_t context);

/* --- Public functions --- */

void app_init(void) {
    // Configure CAN RAM & callbacks 
    CAN1_MessageRAMConfigSet(Can1MessageRAM);
    CAN1_RxFifoCallbackRegister(CAN_RX_FIFO_0, can_receive_callback, (uintptr_t)NULL);
    CAN1_TxFifoCallbackRegister(can_transmit_callback, (uintptr_t)NULL);
    
    // Configure callback for killswitch
    EIC_NMICallbackRegister(eic_pin_killswitch, 0);
    
    // Configure callbacks for FLT pins
    EIC_CallbackRegister(EIC_PIN_0, eic_pin_pg_thruster, 5);
    EIC_CallbackRegister(EIC_PIN_1, eic_pin_flt_thruster, 5);
    
    EIC_CallbackRegister(EIC_PIN_2, eic_pin_flt_thruster, 6);
    EIC_CallbackRegister(EIC_PIN_3, eic_pin_pg_thruster, 6);
    
    EIC_CallbackRegister(EIC_PIN_4, eic_pin_pg_thruster, 4);
    EIC_CallbackRegister(EIC_PIN_5, eic_pin_flt_thruster, 4);
    
    EIC_CallbackRegister(EIC_PIN_6, eic_pin_pg_thruster, 3);
    EIC_CallbackRegister(EIC_PIN_7, eic_pin_flt_thruster, 3); 
    
    EIC_CallbackRegister(EIC_PIN_8, eic_pin_pg_thruster, 2);
    EIC_CallbackRegister(EIC_PIN_9, eic_pin_flt_thruster, 2);
    
    EIC_CallbackRegister(EIC_PIN_10, eic_pin_pg_thruster, 8);
    EIC_CallbackRegister(EIC_PIN_11, eic_pin_flt_thruster, 8);
    
    EIC_CallbackRegister(EIC_PIN_12, eic_pin_pg_thruster, 7);
    EIC_CallbackRegister(EIC_PIN_13, eic_pin_flt_thruster, 7);
    
    EIC_CallbackRegister(EIC_PIN_14, eic_pin_flt_thruster, 1);
    EIC_CallbackRegister(EIC_PIN_15, eic_pin_pg_thruster, 1);
    
    
    // Enable ADC
    ADC0_Enable();
    
    // Configure RTC
    RTC_Timer32Start();
    RTC_Timer32CompareSet(50);
    // Configure DMA
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, adc_dma_callback, 0);
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *)&ADC0_REGS->ADC_RESULT, (const void *)adc_result_array, sizeof(adc_result_array));
    
    // Enable TCC 
    TCC0_PWMStart();
    TCC1_PWMStart();
    TCC2_PWMStart();
    
    // Set all thrusters and lights to neutral on startup
    set_pwm_neutral(thrusters, 8);
    //set_pwm_neutral(lights, 1);
    
    for (int i = 0; i < 100000000; i++) {
        __NOP();
    }

    
    // Enable TC
    TC0_TimerStart();
    TC3_CompareStart();
    
    ADC0_ConversionStart();

    
    // Enable watchdog
    //WDT_Enable();
}

void app_task(void) {
    if (ADC0_ConversionSequenceIsFinished()) {
            ADC0_ConversionStart();
        }
    
    if (adc_dma_done) {
        adc_dma_done = false;
        log_current();
    }
    
    //if (can_message_received) {
    //    can_message_received = false;
        //message_handler();
    //    test_can_rx();
    //}
}

/* --- Private helpers --- */

static void message_handler(void) {
    // Interpret event from CAN frame id
    CAN_RX_BUFFER *rxBuf = (CAN_RX_BUFFER *)rxFiFo0;
    
    uint32_t id = rxBuf->xtd ? rxBuf->id : READ_ID(rxBuf->id);
    const uint8_t *pData = rxBuf->data;

    switch (id) {
        case TURN_THRUSTERS_OFF:
            set_pwm_neutral(thrusters, 8);
            break;

        case TURN_LIGHTS_OFF:
            set_pwm_neutral(lights, 1);
            break;

        case RESET:
            /* Force a system reset */
            NVIC_SystemReset();
            break;

        case SET_THRUSTER_PWM:
            set_pwm_outputs(pData, thrusters, 8);
            break;

        case SET_LIGHT_PWM:
            set_pwm_outputs(pData, lights, 1);
            break;
            
        default:
            /* Unknown event: ignore */
            break;
    }
}

static void log_current(void) {
    const float ADC_VREF   = 5.0f;
    const float G_IMON     = 18.31e-6f;  // Efuse current monitor gain: 18.31 uA/A
    const float R_IMON     = 4020.0f;    // 4.02 kOhm sense resistor for thrusters
    
    printf("\n");
    for (size_t i = 0; i < 8; i++) {
        float V_Imon = ((float)adc_result_array[i] * ADC_VREF) / 4095.0f;
        float I_out  = V_Imon / (G_IMON * R_IMON);
        
        printf("TH%u (AIN%u) raw=%u  V=%.4f  I=%.3f A\r\n",
               imon_map[i].thruster,
               imon_map[i].ain,
               (unsigned)adc_result_array[i],
               V_Imon,
               I_out);
    }
}


static bool send_thruster_fault(uint8_t thruster_id) {
    CAN_TX_BUFFER *txBuffer = NULL;
    
    memset(txFiFo, 0x00, CAN1_TX_FIFO_BUFFER_SIZE);
    txBuffer = (CAN_TX_BUFFER*)txFiFo;
    
    txBuffer->id = WRITE_ID(0x45A);
    txBuffer->dlc = 8;
    txBuffer->fdf = 1;
    txBuffer->brs = 1;
    
    txBuffer->data[0] = thruster_id;
    txBuffer->data[1] = 0x01; // Fault source: FLT pin (hardware fault)
    // Bytes 2-7 reserved for future use (e.g. IMON reading once conversion is known)
    
    bool result = CAN1_MessageTransmitFifo(1, txBuffer);
    if (!result) {
        printf("ERROR: CAN1_MessageTransmitFifo failed!\r\n");
    }
    
    return result;
}


static void set_pwm_outputs(const uint8_t *data, const struct pwm_output *outputs, size_t count) {
    const uint16_t *pulse_data = (const uint16_t *)data;
    for (size_t i = 0; i < count; i++) {
        
        uint16_t pulse_us = pulse_data[i];
        
        pulse_us = clamp(pulse_us, outputs[i].min_us, outputs[i].max_us);
        
        uint32_t ticks = us_to_ticks(outputs[i].period_ticks, pulse_us, outputs[i].frame_us);
        
        // TODO: Fix this shitty code
        if (outputs[i].mode == PWM_TCC) {
            tcc_write(outputs[i].instance, outputs[i].channel, ticks);
        } else if (outputs[i].mode == MPWM_TC) {
            TC3_Compare16bitPeriodSet(ticks);
        } 
    }
    
    // Pet the watchdog after applying updates 
    WDT_Clear();
}

static void set_pwm_neutral(const struct pwm_output *outputs, size_t count) {
    for (size_t i = 0; i < count; i++) {
        uint32_t ticks = us_to_ticks(outputs[i].period_ticks, outputs[i].neutral_us, outputs[i].frame_us);
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
    
    txBuffer->id = WRITE_ID(0x45A);
    txBuffer->dlc = 1;
    txBuffer->fdf = 1;
    txBuffer->brs = 1;
    
    txBuffer->data[0] = 0x43;
    
    bool result = CAN1_MessageTransmitFifo(1, txBuffer);

    if (!result) {
        printf("ERROR: CAN1_MessageTransmitFifo failed!\r\n");
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
    uint8_t thruster_id = (uint8_t)context;
    
    printf("Fault pin triggered for thruster %u\n", (unsigned int)thruster_id);
    
    set_pwm_neutral(thrusters, 8);
    
    // TODO: Send CAN fault message (I don't have the current available so send_thruster_fault() can't be used)
}

static void eic_pin_pg_thruster(uintptr_t context) {
    uint8_t thruster_id = (uint8_t)context;
    
    printf("PGOOD pin triggered for thruster  %u\n", (unsigned int)thruster_id);
}

static void eic_pin_killswitch(uintptr_t context) {
    printf("LOG: KILLSWITCH TRIGGERED");
}
