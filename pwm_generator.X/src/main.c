#include "system_init.h"

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));

// CAN
static uint32_t can_status = 0;
static uint32_t rx_id = 0;
static uint8_t rx_buf[64] = {0};
static uint8_t rx_len = 0;

static uint16_t timestamp = 0;
static CAN_MSG_RX_FRAME_ATTRIBUTE msg_frame_atr = CAN_MSG_RX_DATA_FRAME;
static bool use_can = true;

static const struct ThrusterTable thr_table[8] = {
    {0, 0, TCC_PERIOD},  {0, 1, TCC_PERIOD}, {0, 2, TCC_PERIOD},
    {0, 3, TCC_PERIOD},  {1, 0, TCC_PERIOD}, {1, 1, TCC_PERIOD},
    {2, 0, TCC2_PERIOD}, {2, 1, TCC2_PERIOD}};

/**
 *@brief set thruster pwm dutyCycle and reset watchdog timer
 *@param pData pointer to array containing dutycycle values
 */
static void set_thruster_pwm(uint8_t* pData);
/**
 *@brief function called everytime the CPU has awoke from sleep
 */
static void message_handler(void);
static void stop_thrusters(void);
static void start_thrusters(void);
bool SERCOM_I2C_Callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle);
void CAN_Recieve_Callback(uintptr_t context);
void CAN_Transmit_Callback(uintptr_t context);
void TCC_PeriodEventHandler(uint32_t status, uintptr_t context);
void print_can_frame(void);

int main(void) {
    system_init();
    start_thrusters();

    TC4_CompareStart();  // led

    CAN0_MessageRAMConfigSet(Can0MessageRAM);

    // SERCOM3_I2C_CallbackRegister(SERCOM_I2C_Callback, 0);

    // TCC0_PWMCallbackRegister(TCC_PeriodEventHandler, (uintptr_t)NULL);

    CAN0_RxCallbackRegister(CAN_Recieve_Callback, (uintptr_t)STATE_CAN_RECEIVE,
                            CAN_MSG_ATTR_RX_FIFO0);
    CAN0_TxCallbackRegister(CAN_Transmit_Callback,
                            (uintptr_t)STATE_CAN_TRANSMIT);
    memset(rx_buf, 0x00, sizeof(rx_buf));
    CAN0_MessageReceive(&rx_id, &rx_len, rx_buf, &timestamp,
                        CAN_MSG_ATTR_RX_FIFO0, &msg_frame_atr);
    // printf("Initialize complete\n");

    WDT_Enable();
    while (true) {
        PM_IdleModeEnter();
        message_handler();
    }

    return EXIT_FAILURE;
}

static void set_thruster_pwm(uint8_t* data) {
    for (size_t thr = 0; thr < 8; thr++) {
        uint16_t duty_cycle = data[2 * thr] << 8 | data[2 * thr + 1];
        uint32_t tcc_value =
            (duty_cycle * (thr_table[thr].period + 1)) / PWM_PERIOD_MICROSECONDS;

        switch (thr_table[thr].tcc_num) {
            case 0:
                TCC0_PWM24bitDutySet(thr_table[thr].channel, tcc_value);
                break;
            case 1:
                TCC1_PWM24bitDutySet(thr_table[thr].channel, tcc_value);
                break;
            case 2:
                TCC2_PWM16bitDutySet(thr_table[thr].channel,
                                     (uint16_t)tcc_value);
                break;
            default:
                break;
        }
    }
    WDT_Clear();
}

static void message_handler(void) {
    uint8_t event;
    uint8_t* pData;
    if (use_can) {
        event = rx_id - 0x369;
        pData = rx_buf;
        if (can_status) {
            return;
        }
    } else {
        event = rx_buf[0];
        pData = rx_buf + 1;
    }
    switch (event) {
        case STOP_GENERATOR:
            stop_thrusters();
            break;
        case START_GENERATOR:
            start_thrusters();
            break;
        case SET_PWM:
            set_thruster_pwm(pData);
            break;
        case LED:
            TC4_Compare16bitCounterSet(
                (uint16_t)((rx_buf[0] << 8) | rx_buf[1]));
            break;
        case RESET_MCU:
            WDT_REGS->WDT_CLEAR = 0x0;
            break;
        default:
            break;
    }
    if (use_can) {
        CAN0_MessageReceive(&rx_id, &rx_len, rx_buf, &timestamp,
                            CAN_MSG_ATTR_RX_FIFO0, &msg_frame_atr);
    }
}
static void stop_thrusters(void) {
    TCC0_PWMStop();
    TCC1_PWMStop();
    TCC2_PWMStop();
}

static void start_thrusters(void) {
    TCC0_PWMStart();
    TCC1_PWMStart();
    TCC2_PWMStart();
}

bool SERCOM_I2C_Callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle) {
    static uint8_t dataIndex = 0;
    use_can = false;

    switch (event) {
        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_ADDR_MATCH:
            dataIndex = 0;
            break;

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_RX_READY:
            rx_buf[dataIndex++] = SERCOM3_I2C_ReadByte();
            break;

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_TX_READY: {
            break;
        }

        case SERCOM_I2C_SLAVE_TRANSFER_EVENT_STOP_BIT_RECEIVED:
            if (SERCOM3_I2C_TransferDirGet() ==
                SERCOM_I2C_SLAVE_TRANSFER_DIR_WRITE) {
                /* Only used for debugging */
                // printf("Message recieved\n");
                // for (int i = 0; i < 7; i++) {
                //     printf("%x\n", dataBuffer[i]);
                // }
            }
            break;
        default:
            break;
    }

    return true;
}

void CAN_Recieve_Callback(uintptr_t context) {
    /* Check CAN Status */
    can_status = CAN0_ErrorGet();

    if (((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        // print_can_frame();
    }
}

void CAN_Transmit_Callback(uintptr_t context) {
    /* Check CAN Status */
    can_status = CAN0_ErrorGet();

    if (((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        // Sending encoder data
    }
}

void TCC_PeriodEventHandler(uint32_t status, uintptr_t context) {
    static int8_t increment1 = 10;
    static uint32_t duty1 = 0;

    for (int i = 0; i < 4; i++) {
        TCC0_PWM24bitDutySet(i, duty1);
        TCC1_PWM24bitDutySet(i, duty1);
    }

    duty1 += increment1;

    if (duty1 > PWM_MAX) {
        duty1 = PWM_MAX;
        increment1 *= -1;
    } else if (duty1 < PWM_MIN) {
        duty1 = PWM_MIN;
        increment1 *= -1;
    }
}

void print_can_frame(void) {
    printf(" New Message Received\r\n");
    uint8_t length = rx_len;
    printf(
        " Message - Timestamp : 0x%x ID : 0x%x Length "
        ":0x%x",
        (unsigned int)timestamp, (unsigned int)rx_id, (unsigned int)rx_len);
    printf("Message : ");
    while (length) {
        printf("0x%x ", rx_buf[rx_len - length--]);
    }
}
