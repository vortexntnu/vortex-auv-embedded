/*
 * File:   i2c.c
 * Author: nathaniel
 * Updated for ATSAME54P20A
 */

#include "i2c.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "component/sercom.h"
#include "sam.h"
#include "same54p20a.h"

#define F_GCLK 120000000
#define F_SCL 100000

#define I2C_ERROR() \
    (I2C_SERCOM->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_BUSERR_Msk)
#define I2C_CLKHOLD() \
    (I2C_SERCOM->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_CLKHOLD_Msk)
#define I2C_ARBLOST() \
    (I2C_SERCOM->I2CM.SERCOM.STATUS & SERCOM_I2CM_STATUS_ARBLOST_Msk)

#define I2C_SYNC()                                                      \
    (I2C_SERCOM->I2CM.SERCOM_SYNCBUSY &                                 \
     (SERCOM_I2CM_SYNCBUSY_SWRST_Msk | SERCOM_I2CM_SYNCBUSY_SYSOP_Msk | \
      SERCOM_I2CM_SYNCBUSY_ENABLE_Msk))
#define I2C_SYNC_CHECK                                                       \
    0U /*~(SERCOM_I2CM_SYNCBUSY_SWRST_Msk | SERCOM_I2CM_SYNCBUSY_SYSOP_Msk | \
         SERCOM_I2CM_SYNCBUSY_ENABLE_Msk) */
#define I2C_BAUD() (F_GCLK / F_SCL - 10 - F_GCLK) / 2

volatile static SERCOM_I2C_OBJ sercom3;

void sercom3_i2c_configure_pins(void) {
    PORT_REGS->GROUP[0].PORT_DIRSET = (1 << 23);
    PORT_REGS->GROUP[0].PORT_DIRSET = (1 << 22);
    // Configure SDA and SCL pins for SERCOM3
    PORT_REGS->GROUP[0].PORT_PINCFG[22] |=
        PORT_PINCFG_PMUXEN_Msk | PORT_PINCFG_PULLEN_Msk;  // PA22: SDA
    PORT_REGS->GROUP[0].PORT_PINCFG[23] |=
        PORT_PINCFG_PMUXEN_Msk | PORT_PINCFG_PULLEN_Msk;  // PA23: SCL

    // Set multiplexing to I2C function (MUX C for SERCOM3)
    PORT_REGS->GROUP[0].PORT_PMUX[11] =
        PORT_PMUX_PMUXE(0x2) | PORT_PMUX_PMUXO(0x2);  // MUX D for sercom3
}

void debug_pins(void) {
    // Debug pins on PB1, PB6, PB14 and PB15
    PORT_REGS->GROUP[1].PORT_DIRSET =
        (1 << 1) | (1 << 6) | (1 << 14) | (1 << 15);
}

void sercom3_i2c_configure_master(void) {
    // Enable sercom3 clock
    MCLK_REGS->MCLK_APBBMASK |= MCLK_APBBMASK_SERCOM3_Msk;

    // Configure the GCLK for sercom3
    GCLK_REGS->GCLK_PCHCTRL[SERCOM3_GCLK_ID_CORE] =
        GCLK_PCHCTRL_GEN_GCLK0 | GCLK_PCHCTRL_CHEN_Msk;

    // Software reset sercom3 I2C
    I2C_SERCOM->I2CM.SERCOM_CTRLA = SERCOM_I2CM_CTRLA_SWRST_Msk;
    while (I2C_SYNC() != 0U)
        ;

    I2C_SERCOM->I2CM.SERCOM_BAUD = I2C_BAUD();
    // Configure I2C Master mode, with SDA hold and fast mode
    I2C_SERCOM->I2CM.SERCOM_CTRLA =
        SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
        SERCOM_I2CM_CTRLA_SPEED(0x1) |  // Fast mode (up to 400 kHz)
        SERCOM_I2CM_CTRLA_SDAHOLD(0x2) | SERCOM_I2CM_CTRLA_ENABLE_Msk;

    // Configure I2C baud rate

    while (I2C_SYNC() != I2C_SYNC_CHECK)
        ;
    // Enable sercom3 I2C

    // Force bus into idle state
    I2C_SERCOM->I2CM.SERCOM_STATUS = SERCOM_I2CM_STATUS_BUSSTATE(1);
    while (I2C_SYNC() != I2C_SYNC_CHECK)
        ;

    sercom3.error = SERCOM_I2C_ERROR_NONE;
    sercom3.state = SERCOM_I2C_STATE_IDLE;

    I2C_SERCOM->I2CM.SERCOM_INTENSET |= SERCOM_I2CM_INTENSET_Msk;
}

// I2C start function
static void sercom3_i2c_send_address(uint8_t address, bool direction) {
    sercom3.writeCount = 0U;
    sercom3.readCount = 0U;

    if (direction) {
        sercom3.state = SERCOM_I2C_STATE_TRANSFER_READ;

    } else {
        sercom3.state = SERCOM_I2C_STATE_TRANSFER_WRITE;
    }
    I2C_SERCOM->I2CM.SERCOM_INTFLAG = SERCOM_I2CM_INTFLAG_MB_Msk;
    while (I2C_SYNC() != I2C_SYNC_CHECK)
        ;
    I2C_SERCOM->I2CM.SERCOM_ADDR = (address << 1) | direction;

    while (I2C_SYNC() != I2C_SYNC_CHECK)
        ;
}

static bool sercom3_start(uint8_t address,
                          uint8_t* wrData,
                          uint32_t wrLength,
                          uint8_t* rdData,
                          uint32_t rdLength,
                          bool direction) {
    if (sercom3.state != SERCOM_I2C_STATE_IDLE) {
        return false;
    } else {
        sercom3.address = address;
        sercom3.readBuffer = rdData;
        sercom3.readSize = rdLength;
        sercom3.writeBuffer = wrData;
        sercom3.writeSize = wrLength;
        sercom3.transferDir = direction;
        sercom3.error = SERCOM_I2C_ERROR_NONE;
    }
    sercom3_i2c_send_address(address, direction);
    return true;
}

bool sercom3_i2c_write(uint8_t address, uint8_t* wrData, uint32_t wrLength) {
    return sercom3_start(address, wrData, wrLength, NULL, 0, false);
}
bool sercom3_i2c_read(uint8_t address, uint8_t* rdData, uint32_t rdLength) {
    return sercom3_start(address, NULL, 0, rdData, rdLength, true);
}

bool sercom3_i2c_write_read(uint16_t address,
                            uint8_t* wrData,
                            uint32_t wrLength,
                            uint8_t* rdData,
                            uint32_t rdLength) {
    return sercom3_start(address, wrData, wrLength, rdData, rdLength, false);
}

bool i2c_ping(uint8_t address) {
    I2C_SERCOM->I2CM.SERCOM_ADDR = (address << 1);
    while (!(I2C_SERCOM->I2CM.SERCOM_INTFLAG & SERCOM_I2CM_INTFLAG_MB_Msk)) {
        if (I2C_SERCOM->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_RXNACK_Msk) {
            return 0;  // NACK received, no device at this address
        }
    }
    return 1;  // Device acknowledged
}

void SERCOM3_I2C_CallbackRegister(SERCOM_I2C_CALLBACK callback,
                                  uintptr_t contextHandle) {
    sercom3.callback = callback;

    sercom3.context = contextHandle;
}

SERCOM_I2C_ERROR sercom3_i2c_error_get(void) {
    return sercom3.error;
}

void i2c_scanner() {
    uint8_t address;
    uint8_t ackData = 0;

    for (address = 0x00; address < 0x7F; address++) {
        sercom3_i2c_write(address, &ackData, 1);
        if (sercom3_i2c_error_get() == SERCOM_I2C_ERROR_NONE) {
            printf("I2C device found at address 0x%02X\r\n", address);
        }
    }
    printf("I2C scan complete\r\n");
}

void __attribute__((used)) SERCOM3_0_Handler(void) {
    if (I2C_SERCOM->I2CM.SERCOM_INTENSET != 0U) {
        uintptr_t context = sercom3.context;
        /* Checks if the arbitration lost in multi-master scenario */
        if ((I2C_SERCOM->I2CM.SERCOM_STATUS & SERCOM_I2CM_STATUS_ARBLOST_Msk) ==
            SERCOM_I2CM_STATUS_ARBLOST_Msk) {
            /* Set Error status */
            sercom3.state = SERCOM_I2C_STATE_ERROR;
            sercom3.error = SERCOM_I2C_ERROR_BUS;

            PORT_REGS->GROUP[1].PORT_OUTSET = (1 << 14);  // red

        }
        /* Check for Bus Error during transmission */
        else if ((I2C_SERCOM->I2CM.SERCOM_STATUS &
                  SERCOM_I2CM_STATUS_BUSERR_Msk) ==
                 SERCOM_I2CM_STATUS_BUSERR_Msk) {
            /* Set Error status */
            sercom3.state = SERCOM_I2C_STATE_ERROR;
            sercom3.error = SERCOM_I2C_ERROR_BUS;

            PORT_REGS->GROUP[1].PORT_OUTSET = (1 << 15);  // yellow
        }
        /* Checks slave acknowledge for address or data */
        else if ((I2C_SERCOM->I2CM.SERCOM_STATUS &
                  SERCOM_I2CM_STATUS_RXNACK_Msk) ==
                 SERCOM_I2CM_STATUS_RXNACK_Msk) {
            sercom3.state = SERCOM_I2C_STATE_ERROR;
            sercom3.error = SERCOM_I2C_ERROR_NAK;

            PORT_REGS->GROUP[1].PORT_OUTSET = (1 << 6);  // green
        } else {
            switch (sercom3.state) {
                case SERCOM_I2C_REINITIATE_TRANSFER:

                    if (sercom3.writeSize != 0U) {
                        /* Initiate Write transfer */
                        sercom3_i2c_send_address(sercom3.address, false);
                    } else {
                        /* Initiate Read transfer */
                        sercom3_i2c_send_address(sercom3.address, true);
                    }

                    break;

                case SERCOM_I2C_STATE_IDLE:

                    break;

                case SERCOM_I2C_STATE_TRANSFER_WRITE: {
                    size_t writeCount = sercom3.writeCount;

                    if (writeCount == (sercom3.writeSize)) {
                        if (sercom3.readSize != 0U) {
                            /* Write 7bit address with direction (ADDR.ADDR[0])
                             * equal to 1*/
                            I2C_SERCOM->I2CM.SERCOM_ADDR =
                                ((uint32_t)(sercom3.address) << 1U) |
                                (uint32_t)I2C_TRANSFER_READ;

                            /* Wait for synchronization */
                            while ((I2C_SERCOM->I2CM.SERCOM_SYNCBUSY) !=
                                   I2C_SYNC_CHECK) {
                                /* Do nothing */
                            }

                            sercom3.state = SERCOM_I2C_STATE_TRANSFER_READ;

                        } else {
                            I2C_SERCOM->I2CM.SERCOM_CTRLB |=
                                SERCOM_I2CM_CTRLB_CMD(3UL);

                            /* Wait for synchronization */
                            while ((I2C_SERCOM->I2CM.SERCOM_SYNCBUSY) != 0U) {
                                /* Do nothing */
                            }

                            sercom3.state = SERCOM_I2C_STATE_TRANSFER_DONE;
                        }
                    }
                    /* Write next byte */
                    else {
                        I2C_SERCOM->I2CM.SERCOM_DATA =
                            sercom3.writeBuffer[writeCount];
                        writeCount++;
                        /* Wait for synchronization */
                        while ((I2C_SERCOM->I2CM.SERCOM_SYNCBUSY) != 0U) {
                            /* Do nothing */
                        }
                        sercom3.writeCount = writeCount;
                    }
                }

                break;

                case SERCOM_I2C_STATE_TRANSFER_READ: {
                    size_t readCount = sercom3.readCount;

                    if (readCount == (sercom3.readSize - 1U)) {
                        /* Set NACK and send stop condition to the slave from
                         * master */
                        I2C_SERCOM->I2CM.SERCOM_CTRLB |=
                            SERCOM_I2CM_CTRLB_ACKACT_Msk |
                            SERCOM_I2CM_CTRLB_CMD(3UL);

                        /* Wait for synchronization */
                        while ((I2C_SERCOM->I2CM.SERCOM_SYNCBUSY) !=
                               I2C_SYNC_CHECK) {
                            /* Do nothing */
                        }

                        sercom3.state = SERCOM_I2C_STATE_TRANSFER_DONE;
                    }

                    /* Wait for synchronization */
                    while ((I2C_SERCOM->I2CM.SERCOM_SYNCBUSY) != 0U) {
                        /* Do nothing */
                    }

                    /* Read the received data */
                    sercom3.readBuffer[readCount] =
                        (uint8_t)I2C_SERCOM->I2CM.SERCOM_DATA;
                    readCount++;

                    sercom3.readCount = readCount;
                }

                break;

                default:

                    /* Do nothing */
                    break;
            }
        }

        /* Error Status */
        if (sercom3.state == SERCOM_I2C_STATE_ERROR) {
            /* Reset the PLib objects and Interrupts */
            sercom3.state = SERCOM_I2C_STATE_IDLE;

            /* Generate STOP condition */
            I2C_SERCOM->I2CM.SERCOM_CTRLB |= SERCOM_I2CM_CTRLB_CMD(3UL);

            /* Wait for synchronization */
            while ((I2C_SERCOM->I2CM.SERCOM_SYNCBUSY) != 0U) {
                /* Do nothing */
            }

            I2C_SERCOM->I2CM.SERCOM_INTFLAG = (uint8_t)SERCOM_I2CM_INTFLAG_Msk;

            if (sercom3.callback != NULL) {
                sercom3.callback(context);
            }

        }
        /* Transfer Complete */
        else if (sercom3.state == SERCOM_I2C_STATE_TRANSFER_DONE) {
            /* Reset the PLib objects and interrupts */
            sercom3.state = SERCOM_I2C_STATE_IDLE;
            sercom3.error = SERCOM_I2C_ERROR_NONE;

            I2C_SERCOM->I2CM.SERCOM_INTFLAG = (uint8_t)SERCOM_I2CM_INTFLAG_Msk;

            /* Wait for the NAK and STOP bit to be transmitted out and I2C state
             * machine to rest in IDLE state */
            while ((I2C_SERCOM->I2CM.SERCOM_STATUS &
                    SERCOM_I2CM_STATUS_BUSSTATE_Msk) !=
                   SERCOM_I2CM_STATUS_BUSSTATE(0x01U)) {
                /* Do nothing */
            }
            if (sercom3.callback != NULL) {
                sercom3.callback(context);
            }

        } else {
            /* Do nothing */
        }
    }

    return;
}
