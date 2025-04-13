

#include "clock.h"
#include "sercom0_i2c.h"
#include "sercom3_i2c.h"
#include "system_init.h"
#include "systick.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#define CFG_OS_SINGLE 0x8000
#define CFG_MUX_DIFF_0_1 0x0000
#define CFG_MUX_DIFF_2_3 0x3000
#define CFG_PGA_6_144V 0x0000
#define CFG_MODE_SINGLE 0x0100
#define CFG_DR_128SPS 0x0080
#define CFG_COMP_MODE 0x0010
#define CFG_COMP_POL 0x0008
#define CFG_COMP_LAT 0x0004
#define CFG_COMP_QUE_DIS 0x0003

#define PSM_ADDRESS 0x48
#define REG_CONV 0x00
#define REG_CFG 0x01

static uint8_t tx_data[4] = {0};
static uint8_t rx_data[4] = {0};

static inline int start_conversion(uint16_t config) {
  config |= CFG_OS_SINGLE;
  uint8_t i2c_data[3];
  i2c_data[0] = REG_CFG;
  i2c_data[1] = (config >> 8) & 0xFF;
  i2c_data[2] = config & 0xFF;

  return SERCOM0_I2C_Write(PSM_ADDRESS, i2c_data, 3);
}

static uint8_t read_psm() {

  static uint16_t default_config =
      CFG_OS_SINGLE | CFG_MUX_DIFF_0_1 | CFG_PGA_6_144V | CFG_MODE_SINGLE |
      CFG_DR_128SPS | CFG_COMP_MODE | CFG_COMP_POL | CFG_COMP_LAT |
      CFG_COMP_QUE_DIS;
  uint8_t reg_conv = REG_CONV;

  uint16_t config = default_config;
  config &= ~0x7000;
  config |= CFG_MUX_DIFF_0_1;

  if (start_conversion(config))
    return 1;
  
  SYSTICK_DelayMs(10);
  
  if (SERCOM0_I2C_WriteRead(PSM_ADDRESS, &reg_conv, 1, tx_data, 2)) {
    return 1;
  }


  config = default_config;
  config &= ~0x7000;
  config |= CFG_MUX_DIFF_2_3;

  if (start_conversion(config))
    return 1;

  SYSTICK_DelayMs(10);

  if (SERCOM0_I2C_WriteRead(PSM_ADDRESS, &reg_conv, 1, tx_data + 2, 2)) {
    return 1;
  }


  return 0;
}

bool sercom3_i2c_calback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                         uintptr_t contextHandle) {
  bool isSuccess = true;
  static uint8_t index = 0;

  switch (event) {
  case SERCOM_I2C_SLAVE_TRANSFER_EVENT_ADDR_MATCH:
    index = 0;
    break;

  case SERCOM_I2C_SLAVE_TRANSFER_EVENT_RX_READY:

    rx_data[index++] = SERCOM3_I2C_ReadByte();

    break;

  case SERCOM_I2C_SLAVE_TRANSFER_EVENT_TX_READY:

    SERCOM3_I2C_WriteByte(tx_data[index++]);

    break;

  case SERCOM_I2C_SLAVE_TRANSFER_EVENT_STOP_BIT_RECEIVED:
    if (rx_data[0]) {
      break;
    }
    read_psm();

    
    break;
  default:
    break;
  }

  return isSuccess;
}

int main() {

  nvm_ctrl_init();
  pin_init();
  CLOCK_Initialize();
  SERCOM0_I2C_Initialize();
  SERCOM3_I2C_Initialize();
  SYSTICK_TimerInitialize();
  nvic_init();

  SERCOM3_I2C_CallbackRegister(sercom3_i2c_calback, 0);

  SYSTICK_TimerStart();

  while (1) {
  }

  return EXIT_FAILURE;
}
