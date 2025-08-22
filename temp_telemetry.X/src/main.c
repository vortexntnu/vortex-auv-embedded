

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/types.h>
#include "clock.h"
#include "sercom0_i2c.h"
#include "sercom3_i2c.h"
#include "system_init.h"
#include "systick.h"

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

static int start_conversion(uint16_t config);
static int wait_for_conversion_complete(void);
static int read_psm(uint8_t* output);
bool sercom3_i2c_callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                          uintptr_t contextHandle);

int main(void) {
    system_init();

    SERCOM3_I2C_CallbackRegister(sercom3_i2c_callback, 0);

    SYSTICK_TimerStart();

    while (1) {
    }

    return EXIT_FAILURE;
}

static int start_conversion(uint16_t config) {
    config |= CFG_OS_SINGLE;
    uint8_t i2c_data[3];
    i2c_data[0] = REG_CFG;
    i2c_data[1] = (config >> 8) & 0xFF;
    i2c_data[2] = config & 0xFF;

    return SERCOM0_I2C_Write(PSM_ADDRESS, i2c_data, 3);
}

static int wait_for_conversion_complete(void) {
    uint8_t cfg_high;
    uint8_t reg = REG_CFG;
    do {
        if (SERCOM0_I2C_WriteRead(PSM_ADDRESS, &reg, 1,
                                  &cfg_high, 1)) {
            return -1;  // I²C error
        }
    } while ((cfg_high & 0x80) == 0);
    return 0;
}

static int read_psm(uint8_t* output) {
    const uint16_t base_cfg = CFG_OS_SINGLE | CFG_PGA_6_144V | CFG_MODE_SINGLE |
                              CFG_DR_128SPS | CFG_COMP_MODE | CFG_COMP_POL |
                              CFG_COMP_LAT | CFG_COMP_QUE_DIS;

    const uint16_t cfg_list[2] = {(uint16_t)(base_cfg | CFG_MUX_DIFF_0_1),
                                  (uint16_t)(base_cfg | CFG_MUX_DIFF_2_3)};

    uint8_t read_buffer[2];
    uint8_t reg_conv = REG_CONV;

    for (uint8_t i = 0; i < 2; i++) {
        if (start_conversion(cfg_list[i])) {
            return -1;
        }
        if (wait_for_conversion_complete()) {
            return -2;
        }
        if (SERCOM0_I2C_WriteRead(PSM_ADDRESS, &reg_conv, 1, read_buffer, 2)) {
            return -3;
        }
        output[2 * i] = read_buffer[0];
        output[2 * i + 1] = read_buffer[1];
    }
    return 0;
}

bool sercom3_i2c_callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                          uintptr_t contextHandle) {
    bool isSuccess = true;
    static uint8_t index = 0;

    static uint8_t tx_data[4] = {0};
    static uint8_t rx_data[2] = {0};

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
            if (read_psm(tx_data)) {
            };

            break;
        default:
            break;
    }

    return isSuccess;
}
