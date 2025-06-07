
#include "sam.h"
#include "samc21e17a.h"
/*#include "sercom0_i2c.h"*/
#include "system_init.h"
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

uint8_t Can0MessageRAM[CAN0_MESSAGE_RAM_CONFIG_SIZE]
    __attribute__((aligned(32)));

/*CAN*/
static uint32_t status = 0;
static uint32_t xferContext = 0;
const static uint32_t messageID = 0x469;
static uint32_t rx_id = 0;
static uint8_t rx_buf[64] = {0};
static uint8_t rx_len = 0;
static uint16_t timestamp = 0;
static CAN_MSG_RX_FRAME_ATTRIBUTE msgFrameAttr = CAN_MSG_RX_DATA_FRAME;


// ADC Variables
static uint8_t servo = SERVO_1;
static uint16_t adc_result_array[TRANSFER_SIZE];

static uint8_t encoder_angles[7] = {0};

static const uint8_t encoder_addresses[NUM_ENCODERS] = {SHOULDER_ADDR,
                                                        WRIST_ADDR, GRIP_ADDR};
volatile bool usesCan = true;

/**
 *@brief reads encoder
 *@param data txbuffer for encoder angles
 *@param reg which encoder register to read from
 *@return 0 on success
 *        -1 on failure
 */
static int encoder_read(uint8_t *data, uint8_t reg) {
  uint32_t timeout;

  for (uint8_t i = 0; i < NUM_ENCODERS; i++) {
    uint8_t buf[2] = {0xFF};
    if (!SERCOM1_I2C_WriteRead(encoder_addresses[i], &reg, 1, buf, 2)) {
      return -1;
    }
    timeout = I2C_TIMEOUT;
    while (SERCOM1_I2C_IsBusy()) {
      if (--timeout == 0) {
        break;
      }
    }
    if (buf[0] == 0xFF && buf[1] == 0xFF) {
      data[2 * i] = 0xFF;
      data[2 * i + 1] = 0xFF;
      continue;
    }
    uint16_t rawAngle = (buf[0] << 6) | (buf[1] & 0x3F);
    data[2 * i] = rawAngle >> 8;
    data[2 * i + 1] = rawAngle & 0xFF;
  }
  return 0;
}

static void set_pwm_dutycycle(uint8_t *dutyCycleMicroSeconds) {
  uint16_t shoulderDuty =
      (dutyCycleMicroSeconds[0] << 8) | dutyCycleMicroSeconds[1];
  uint16_t wristDuty =
      (dutyCycleMicroSeconds[2] << 8) | dutyCycleMicroSeconds[3];
  uint16_t gripDuty =
      (dutyCycleMicroSeconds[4] << 8) | dutyCycleMicroSeconds[5];

  uint32_t tccValue =
      (shoulderDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
  TCC0_PWM24bitDutySet(3, tccValue);

  tccValue = (wristDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
  TCC1_PWM24bitDutySet(0, tccValue);

  tccValue = (gripDuty * (TCC_PERIOD + 1)) / PWM_PERIOD_MICROSECONDS;
  TCC1_PWM24bitDutySet(1, tccValue);
}

bool SERCOM_I2C_Callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle) {
  static uint8_t dataBuffer[7];
  static uint8_t dataIndex = 0;
  usesCan = false;

  switch (event) {
  case SERCOM_I2C_SLAVE_TRANSFER_EVENT_ADDR_MATCH:
    dataIndex = 0;
    break;
  case SERCOM_I2C_SLAVE_TRANSFER_EVENT_RX_READY:

    if (dataIndex < sizeof(dataBuffer)) {
      dataBuffer[dataIndex++] = SERCOM3_I2C_ReadByte();
    }

    break;
  case SERCOM_I2C_SLAVE_TRANSFER_EVENT_TX_READY: {
    SERCOM3_I2C_WriteByte(encoder_angles[dataIndex++]);
    break;
  }
  case SERCOM_I2C_SLAVE_TRANSFER_EVENT_STOP_BIT_RECEIVED:
    if (SERCOM3_I2C_TransferDirGet() == SERCOM_I2C_SLAVE_TRANSFER_DIR_WRITE) {
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

  status = CAN0_ErrorGet();

  if (((status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
      ((status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
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
  status = CAN0_ErrorGet();

  if (((status & CAN_PSR_LEC_Msk) == CAN_ERROR_NONE) ||
      ((status & CAN_PSR_LEC_Msk) == CAN_ERROR_LEC_NC)) {
  }
}

// Only used for testing SERVOS
void TCC_PeriodEventHandler(uint32_t status, uintptr_t context) {
  /* duty cycle values */
  static int8_t increment1 = 10;
  static uint32_t duty1 = 0;
  static uint32_t duty2 = 0;
  static uint32_t duty3 = 0U;

  TCC0_PWM24bitDutySet(1, duty1);
  TCC1_PWM24bitDutySet(0, duty2);
  TCC1_PWM24bitDutySet(1, duty3);

  duty1 += increment1;
  duty2 += increment1;
  duty3 += increment1;

  if (duty1 > PWM_MAX) {
    duty1 = PWM_MAX;
    increment1 *= -1;
  } else if (duty2 < PWM_MIN) {
    duty1 = PWM_MIN;
    increment1 *= -1;
  }
  if (duty2 > PWM_MAX) {
    duty2 = PWM_MAX;
    increment1 *= -1;
  } else if (duty2 < PWM_MIN) {
    duty2 = PWM_MIN;
    increment1 *= -1;
  }
  if (duty3 > PWM_MAX) {
    duty3 = PWM_MAX;
    increment1 *= -1;
  } else if (duty3 < PWM_MIN) {
    duty3 = PWM_MIN;
    increment1 *= -1;
  }
}

void Dmac_Channel0_Callback(DMAC_TRANSFER_EVENT returned_evnt,
                            uintptr_t MyDmacContext) {
  if (DMAC_TRANSFER_EVENT_COMPLETE == returned_evnt) {
    bool overCurrent = false;
    uint16_t input_voltage = 0;
    for (size_t sample = 0; sample < TRANSFER_SIZE; sample++) {
      input_voltage += adc_result_array[sample];
      input_voltage = input_voltage >> 1; // Dividing by two

      // 2.5 V == 0 A
      /*input_voltage =*/
      /*    (float)(adc_result_array[sample] * ADC_VREF / 4095U - 2.5) /*/
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
      /*printf("SERVO2\n");*/
      break;
    case SERVO_2:
      if (overCurrent) {
        PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 28);
      }
      ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN4;
      servo = SERVO_3;
      /*printf("SERVO3\n");*/
      break;
    case SERVO_3:
      if (overCurrent) {
        PORT_REGS->GROUP[0].PORT_OUTCLR |= (1 << 0);
      }
      ADC0_REGS->ADC_INPUTCTRL = ADC_POSINPUT_AIN0;
      servo = SERVO_1;
      /*printf("SERVO1\n");*/
      break;
    default:
      break;
    }

    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *)&ADC0_REGS->ADC_RESULT,
                         (const void *)adc_result_array,
                         sizeof(adc_result_array));
  } else if (DMAC_TRANSFER_EVENT_ERROR == returned_evnt) {
    DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *)&ADC0_REGS->ADC_RESULT,
                         (const void *)adc_result_array,
                         sizeof(adc_result_array));
  }
}

static void message_handler() {
  uint8_t event;
  uint8_t *pData;
  if (usesCan) {
    event = rx_id - 0x469;
    pData = rx_buf;
  } else {
    event = rx_buf[0];
    pData = rx_buf + 1;
  }
  switch (event) {
  case STOP_GRIPPER:
    stop_gripper();
    break;
  case START_GRIPPER:
    start_gripper();
    WDT_Enable();
    break;
  case SET_PWM:
    set_pwm_dutycycle(pData);
    encoder_read(encoder_angles, ANGLE_REGISTER);
    if (usesCan) {
      CAN0_MessageTransmit(messageID, 6, encoder_angles,
                           CAN_MODE_FD_WITHOUT_BRS,
                           CAN_MSG_ATTR_TX_FIFO_DATA_FRAME);
    }

    WDT_Clear();
    break;
  case RESET_MCU:
    WDT_REGS->WDT_CLEAR = 0x0;
    break;
  default:
    break;
  }
  if (usesCan) {
    CAN0_MessageReceive(&rx_id, &rx_len, rx_buf,
                        &timestamp, CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);
  }
}

int main(void) {
  system_init();

  // start_gripper();

  CAN0_MessageRAMConfigSet(Can0MessageRAM);

  // SERCOM3_I2C_CallbackRegister(SERCOM_I2C_Callback, 0);

  // TCC0_PWMCallbackRegister(TCC_PeriodEventHandler, (uintptr_t)NULL);

  DMAC_ChannelCallbackRegister(DMAC_CHANNEL_0, Dmac_Channel0_Callback, 0);
  CAN0_RxCallbackRegister(CAN_Recieve_Callback, (uintptr_t)STATE_CAN_RECEIVE,
                          CAN_MSG_ATTR_RX_FIFO0);
  CAN0_TxCallbackRegister(CAN_Transmit_Callback, (uintptr_t)STATE_CAN_TRANSMIT);

  CAN0_MessageReceive(&rx_id, &rx_len, rx_buf, &timestamp,
                      CAN_MSG_ATTR_RX_FIFO0, &msgFrameAttr);

  DMAC_ChannelTransfer(DMAC_CHANNEL_0, (const void *)&ADC0_REGS->ADC_RESULT,
                       (const void *)adc_result_array,
                       sizeof(adc_result_array));

  while (true) {
    PM_IdleModeEnter();
    message_handler();
  }

  return EXIT_FAILURE;
}
