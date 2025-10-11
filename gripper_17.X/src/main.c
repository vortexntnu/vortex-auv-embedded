#include <stdint.h>
#include "samc21e17a.h"
#include "system_init.h"
#include "tc0.h"
#include "tc1.h"
#include "wdt.h"

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));
static struct can_rx_frame rx_frame;
static uint16_t adc_result_array[TRANSFER_SIZE];
static volatile uint32_t events;

#define EVENT_SET_PWM (1 << 3)
#define EVENT_READ_ENCODER (1 << 4)
#define EVENT_START_GRIPPER (1 << 5)
#define EVENT_TRANSMIT_ANGLES (1 << 3)

/**
 * @brief Read encoder angle registers over I2C.
 *
 *
 * @param[in]  reg   Register address to read from each encoder.
 * @param[out] data  Pointer to a buffer that will receive the angle data.
 *                   Must be at least 2 * NUM_ENCODERS bytes long.
 *
 * @return  0  on success,
 *         -1  on failure
 */
static int read_encoders(uint8_t reg, uint8_t* data);

/**
 * @brief Set servo PWM duty cycles.
 *
 * @param[in] pwm_data  Pointer to an array containing the PWM duty values
 *                      for each servo channel. Array length must match
 *                      the number of servos.
 */
static void set_servos_pwm(const uint8_t* pwm_data);
void can_rx_callback(uintptr_t context);
void dmac_channel0_callback(DMAC_TRANSFER_EVENT returned_evnt,
                            uintptr_t MyDmacContext);
void tc0_callback(TC_TIMER_STATUS status, uintptr_t context);
void tc1_callback(TC_TIMER_STATUS status, uintptr_t context);

int main(void) {
    system_init();

    CAN0_MessageRAMConfigSet(Can0MessageRAM);
    CAN0_RxCallbackRegister(can_rx_callback, STATE_CAN_RECEIVE,
                            CAN_MSG_ATTR_RX_FIFO0);
    DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, dmac_channel0_callback, 0);
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
                         (const void*)adc_result_array,
                         sizeof(adc_result_array));
    TC0_TimerCallbackRegister(tc0_callback, (uintptr_t)NULL);
    TC1_TimerCallbackRegister(tc1_callback, (uintptr_t)NULL);
    can_recieve(&rx_frame);


    static struct can_tx_frame angles_tx_frame;

    WDT_Enable();

    while (true) {
        PM_IdleModeEnter();

        uint32_t ev = events;
        events &= ~ev;

        if (ev & EVENT_SET_PWM){
            set_servos_pwm(rx_frame.buf);
            WDT_Clear();
        }

        if (ev & EVENT_READ_ENCODER){
            angles_tx_frame.id = CAN_SEND_ANGLES;
            angles_tx_frame.len = 6;
            read_encoders(ANGLE_REGISTER, angles_tx_frame.buf);
        }

        if (ev & EVENT_TRANSMIT_ANGLES){
            can_transmit(&angles_tx_frame);
        }

        can_recieve(&rx_frame);
    }

    return EXIT_FAILURE;
}

static int read_encoders(uint8_t reg, uint8_t* data) {
    static const uint8_t encoder_addresses[NUM_ENCODERS] = {
        SHOULDER_ADDR, WRIST_ADDR, GRIP_ADDR};
    uint32_t timeout;

    for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
        uint8_t buf[2] = {0xFF, 0xFF};

        if (!SERCOM1_I2C_WriteRead(encoder_addresses[i], &reg, 1, buf, 2)) {
            return -1;
        }

        timeout = I2C_TIMEOUT;

        while (SERCOM1_I2C_IsBusy()) {
            if (--timeout == 0) {
                break;
            }
        }

        if (timeout == 0 || (buf[0] == 0xFF && buf[1] == 0xFF)) {
            data[2 * i] = 0xFF;
            data[2 * i + 1] = 0xFF;
            continue;
        }

        uint16_t raw_angle = (buf[0] << 6) | (buf[1] & 0x3F);
        data[2 * i] = raw_angle >> 8;
        data[2 * i + 1] = raw_angle & 0xFF;
    }
    return 0;
}

static void set_servos_pwm(const uint8_t* pwm_data) {
    uint16_t shoulder_duty = (pwm_data[0] << 8) | pwm_data[1];
    uint16_t wrist_duty = (pwm_data[2] << 8) | pwm_data[3];
    uint16_t grip_duty = (pwm_data[4] << 8) | pwm_data[5];

    uint32_t tcc_val =
        (shoulder_duty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC0_PWM24bitDutySet(3, tcc_val);

    tcc_val = (wrist_duty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC1_PWM24bitDutySet(0, tcc_val);

    tcc_val = (grip_duty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
    TCC1_PWM24bitDutySet(1, tcc_val);
}

void can_rx_callback(uintptr_t context) {
    if (CAN0_ErrorGet()) {
        return;
    }
    switch (rx_frame.id) {
        case STOP_GRIPPER:
            stop_gripper();
            break;
        case START_GRIPPER:
            start_gripper();
            break;
        case SET_PWM:
            set_servos_pwm(rx_frame.buf);
            WDT_Clear();
            break;
        case RESET_MCU:
            NVIC_SystemReset();
            break;
        default:
            break;
    }
}

void tc0_callback(TC_TIMER_STATUS status, uintptr_t context) {
    events |= EVENT_READ_ENCODER;
}

void tc1_callback(TC_TIMER_STATUS status, uintptr_t context) {
    events |= EVENT_TRANSMIT_ANGLES;
}

void dmac_channel0_callback(DMAC_TRANSFER_EVENT returned_evnt,
                            uintptr_t MyDmacContext) {
    static uint8_t servo = SERVO_1;

    if (DMAC_TRANSFER_EVENT_COMPLETE == returned_evnt) {
        bool overCurrent = false;
        uint16_t input_voltage = 0;
        for (size_t sample = 0; sample < TRANSFER_SIZE; sample++) {
            input_voltage += adc_result_array[sample];
            input_voltage = input_voltage >> 1;  // Dividing by two

            // 2.5 V == 0 A
            /*input_voltage =*/
            /*    (float)(adc_result_array[sample] * ADC_VREF / 4095U - 2.5)
             * /*/
            /*    0.4;*/

            /*printf(*/
            /*    "ADC Count = 0x%03x, ADC Input Current = %d.%03d A "*/
            /*    "\n\r",*/
            /*    adc_result_array[sample], (int)input_voltage,*/
            /*    (int)((input_voltage - (int)input_voltage) * 100.0));*/
        }

        if (input_voltage > VOLTAGE_THRESHOLD) {
            overCurrent = true;
        }
        switch (servo) {
            case SERVO_1:
                if (overCurrent) {
                    PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 27);
                }
                ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN1;
                servo = SERVO_2;
                break;
            case SERVO_2:
                if (overCurrent) {
                    PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 28);
                }
                ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN4;
                servo = SERVO_3;
                break;
            case SERVO_3:
                if (overCurrent) {
                    PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 0);
                }
                ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN0;
                servo = SERVO_1;
                break;
            default:
                break;
        }

        DMAC_ChannelTransfer(
            DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
            (const void*)adc_result_array, sizeof(adc_result_array));
    } else if (DMAC_TRANSFER_EVENT_ERROR == returned_evnt) {
        DMAC_ChannelTransfer(
            DMAC_CHANNEL_0, (const void*)&ADC0_REGS->ADC_RESULT,
            (const void*)adc_result_array, sizeof(adc_result_array));
    }
}
