

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

static uint8_t tx_data[4] = {0};
static uint8_t rx_data[2] = {0};

/**
 *@brief Starts PSM conversion
 @param cfg_idx 0 for voltage reading
                1 for current reading
 */
static int start_conversion(uint8_t cfg_idx);
/**
 *@brief Polling PSM until conversion is complete
 */
static int wait_for_conversion_complete(void);
/**
 *@brief Read the PSM for both voltage and current readings
 *@param[out] output array which contains voltage and current reading in raw
 *format
 */
static int read_psm(uint8_t* output);
void message_handler(void);
bool sercom3_i2c_callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                          uintptr_t contextHandle);

int main(void) {
    system_init();

    SERCOM3_I2C_CallbackRegister(sercom3_i2c_callback, 0);

    SYSTICK_TimerStart();

    while (1) {
        message_handler();
    }

    return EXIT_FAILURE;
}

static int start_conversion(uint8_t cfg_idx) {
    static const uint16_t base_cfg =
        CFG_OS_SINGLE | CFG_PGA_6_144V | CFG_MODE_SINGLE | CFG_DR_128SPS |
        CFG_COMP_MODE | CFG_COMP_POL | CFG_COMP_LAT | CFG_COMP_QUE_DIS;
    static const uint16_t cfg_list[2] = {
        (uint16_t)(base_cfg | CFG_MUX_DIFF_0_1),
        (uint16_t)(base_cfg | CFG_MUX_DIFF_2_3)};

    uint16_t config = cfg_list[cfg_idx] | CFG_OS_SINGLE;

    uint8_t i2c_data[3];
    i2c_data[0] = REG_CFG;
    i2c_data[1] = (uint8_t)((config >> 8) & 0xFF);
    i2c_data[2] = (uint8_t)(config & 0xFF);

    return SERCOM0_I2C_Write(PSM_ADDRESS, i2c_data, 3);
}

static int wait_for_conversion_complete(void) {
    uint8_t cfg_high;
    uint8_t reg = REG_CFG;
    do {
        if (SERCOM0_I2C_WriteRead(PSM_ADDRESS, &reg, 1, &cfg_high, 1)) {
            return -1;  // I²C error
        }
    } while ((cfg_high & 0x80) == 0);
    return 0;
}

static int read_psm(uint8_t* output) {
    uint8_t rx_buf[2];
    uint8_t reg_conv = REG_CONV;

    for (uint8_t i = 0; i < 2; i++) {
        if (start_conversion(i)) {
            return -1;
        }
        if (wait_for_conversion_complete()) {
            return -2;
        }
        if (SERCOM0_I2C_WriteRead(PSM_ADDRESS, &reg_conv, 1, rx_buf, 2)) {
            return -3;
        }
        output[2 * i] = rx_buf[0];
        output[2 * i + 1] = rx_buf[1];
    }
    return 0;
}

void message_handler(void) {
    if (read_psm(tx_data)) {
    };
}

bool sercom3_i2c_callback(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
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
            // if (rx_data[0]) {
            //     break;
            // }
            break;
        default:
            break;
    }

    return isSuccess;
}
