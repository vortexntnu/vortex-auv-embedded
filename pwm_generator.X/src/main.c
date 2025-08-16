#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include "pm.h"
#include "sam.h"
#include "samc21e17a.h"
#include "system_init.h"
#include "tcc.h"
#include "tcc2.h"
#include "wdt.h"

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));

// CAN
static uint32_t can_status = 0;
static uint32_t xferContext = 0;
static uint32_t messageID = 0x169;
static uint32_t rx_id = 0;
static uint8_t rx_buf[64] = {0};
static uint8_t rx_len = 0;
static uint16_t timestamp = 0;
static CAN_MSG_RX_FRAME_ATTRIBUTE msgFrameAttr = CAN_MSG_RX_DATA_FRAME;

static bool usesCan = true;

typedef struct {
    uint8_t tcc_num;
    uint8_t channel;
    uint32_t period;
} ThrusterTable;

static const ThrusterTable thr_table[8] = {
    {0, 0, TCC_PERIOD},  {0, 1, TCC_PERIOD}, {0, 2, TCC_PERIOD},
    {0, 3, TCC_PERIOD},  {1, 0, TCC_PERIOD}, {1, 1, TCC_PERIOD},
    {2, 0, TCC2_PERIOD}, {2, 1, TCC2_PERIOD}};

static void set_thruster_pwm(uint8_t* pData) {
    for (size_t thr = 0; thr < 8; thr++) {
        uint8_t tcc_num = thr_table[thr].tcc_num;
        uint8_t channel = thr_table[thr].channel;
        uint32_t period = thr_table[thr].period;
      
        uint16_t dutyCycle =
            pData[2 * thr] << 8 | pData[2 * thr + 1];

        uint32_t tccValue = (dutyCycle * (period + 1)) /
                            PWM_PERIOD_MICROSECONDS;
        switch (tcc_num) {
            case 0:
                TCC0_PWM24bitDutySet(channel, tccValue);
                break;
            case 1:
                TCC1_PWM24bitDutySet(channel, tccValue) ;
                break;
            case 2:
                TCC2_PWM16bitDutySet(channel, (uint16_t) tccValue);
                break;
            default:
                break;
        }
    }
    WDT_Clear();
}

bool SERCOM_I2C_Callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle) {
    static uint8_t dataIndex = 0;
    usesCan = false;

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
    xferContext = context;

    /* Check CAN Status */
    can_status = CAN0_ErrorGet();
    /*printf("Entering callback\n");*/

    if (((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
        ((can_status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
        /* Only used for debugging */
        /*printf(" New Message Received\r\n");*/
        /*uint8_t length = rx_messageLength;*/
        /*printf(*/
        /*    " Message - Timestamp : 0x%x ID : 0x%x Length "*/
        /*    ":0x%x",*/
        /*    (unsigned int)timestamp, (unsigned int)rx_messageID,*/
        /*    (unsigned int)rx_messageLength);*/
        /*printf("Message : ");*/
        /*while (length) {*/
        /*    printf("0x%x ", rx_message[rx_messageLength - length--]);*/
        /*}*/
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

// used to test thrusters
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

static void message_handler() {
    uint8_t event;
    uint8_t* pData;
    if (usesCan) {
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
    if (usesCan) {
        CAN0_MessageReceive(&rx_id, &rx_len, rx_buf, &timestamp,
                            CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
    }
}

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
                        CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
    /*printf("Initialize complete\n");*/

    WDT_Enable();
    while (true) {
        PM_IdleModeEnter();
        message_handler();
    }

    return EXIT_FAILURE;
}
