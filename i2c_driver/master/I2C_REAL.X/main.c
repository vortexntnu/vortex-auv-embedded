/*
 * File:   main.c
 * Author: nathaniel
 *
 * Created on November 2, 2024, 6:32 PM
 */

#ifndef __SAME54P20A__
#define __SAME54P20A__
#include "same54p20a.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "clock.h"
#include "i2c.h"
#include "sam.h"

#define ARDUINO_ADDRESS 0x76
#define SEND_LENGTH 1
#define READ_LENGTH 1
#define ACK_LENGTH 1
#define READ_WRITE_LENGTH 1

typedef enum {
    ARDUINO_STATE_STATUS_VERIFY,
    ARDUINO_STATE_WRITE,
    ARDUINO_STATE_WAIT_WRITE_COMPLETE,
    ARDUINO_STATE_CHECK_INTERNAL_WRITE_STATUS,
    ARDUINO_STATE_READ,
    ARDUINO_STATE_WAIT_READ_COMPLETE,
    APP_STATE_VERIFY,
    APP_STATE_IDLE,
    APP_STATE_XFER_SUCCESSFUL,
    APP_STATE_XFER_ERROR

} APP_STATES;

typedef enum {
    APP_TRANSFER_STATUS_IN_PROGRESS,
    APP_TRANSFER_STATUS_SUCCESS,
    APP_TRANSFER_STATUS_ERROR,
    APP_TRANSFER_STATUS_IDLE,

} APP_TRANSFER_STATUS;

void APP_I2CCallback(uintptr_t context) {
    APP_TRANSFER_STATUS* transferStatus = (APP_TRANSFER_STATUS*)context;

    if (sercom3_i2c_error_get() == SERCOM_I2C_ERROR_NONE) {
        if (transferStatus) {
            *transferStatus = APP_TRANSFER_STATUS_SUCCESS;
        }
    } else {
        if (transferStatus) {
            *transferStatus = APP_TRANSFER_STATUS_ERROR;
        }
    }
}
void delay_ms(uint32_t ms) {
    for (volatile uint32_t i = 0; i < (ms * 3000); i++) {
        __asm("NOP");  // "No Operation"
    }
}

void NVIC_Initialize(void) {
    /* Priority 0 to 7 and no sub-priority. 0 is the highest priority */
    NVIC_SetPriorityGrouping(0x00);

    /* Enable NVIC Controller */
    __DMB();
    __enable_irq();
    /* Enable the interrupt sources and configure the priorities as configured
     * from within the "Interrupt Manager" of MHC. */
    NVIC_SetPriority(SERCOM3_0_IRQn, 7);
    NVIC_EnableIRQ(SERCOM3_0_IRQn);
    NVIC_SetPriority(SERCOM3_1_IRQn, 7);
    NVIC_EnableIRQ(SERCOM3_1_IRQn);
    NVIC_SetPriority(SERCOM3_2_IRQn, 7);
    NVIC_EnableIRQ(SERCOM3_2_IRQn);
    NVIC_SetPriority(SERCOM3_OTHER_IRQn, 7);
    NVIC_EnableIRQ(SERCOM3_OTHER_IRQn);

    /* I have no idea what this does */

    /* Enable Usage fault */
    SCB->SHCSR |= (SCB_SHCSR_USGFAULTENA_Msk);
    /* Trap divide by zero */
    SCB->CCR |= SCB_CCR_DIV_0_TRP_Msk;

    /* Enable Bus fault */
    SCB->SHCSR |= (SCB_SHCSR_BUSFAULTENA_Msk);

    /* Enable memory management fault */
    SCB->SHCSR |= (SCB_SHCSR_MEMFAULTENA_Msk);
}

int main(void) {
    APP_STATES state = ARDUINO_STATE_STATUS_VERIFY;
    volatile APP_TRANSFER_STATUS transferStatus = APP_TRANSFER_STATUS_ERROR;
    uint8_t ackData = 1;
    uint8_t testTxData[1] = {21};
    uint8_t testRxData[1];
    uint8_t send = 25;

    sercom3_i2c_configure_pins();
    CLOCK_Initialize();
    debug_pins();
    sercom3_i2c_configure_master();
    NVIC_Initialize();
    PORT_REGS->GROUP[2].PORT_DIRSET = (1 << 18);
    // sercom3_i2c_scanner();

    while (1) {
        delay_ms(10);
        switch (state) {
            case ARDUINO_STATE_STATUS_VERIFY:

                /* Register the TWIHS Callback with transfer status as context
                 */
                SERCOM3_I2C_CallbackRegister(APP_I2CCallback,
                                             (uintptr_t)&transferStatus);
                /* Verify if EEPROM is ready to accept new requests */
                transferStatus = APP_TRANSFER_STATUS_IN_PROGRESS;
                sercom3_i2c_write(ARDUINO_ADDRESS, &send, ACK_LENGTH);

                state = ARDUINO_STATE_WRITE;
                break;

            case ARDUINO_STATE_WRITE:

                if (transferStatus == APP_TRANSFER_STATUS_SUCCESS) {
                    /* Write data  */
                    transferStatus = APP_TRANSFER_STATUS_IN_PROGRESS;
                    // sercom3_i2c_write(ARDUINO_ADDRESS, &testTxData[0],
                    //                   SEND_LENGTH);
                    sercom3_i2c_write(ARDUINO_ADDRESS, &send, SEND_LENGTH);

                    state = ARDUINO_STATE_WAIT_WRITE_COMPLETE;
                } else if (transferStatus == APP_TRANSFER_STATUS_ERROR) {
                    /* not ready to accept new requests.
                     * Keep checking until the becomes ready. */
                    state = ARDUINO_STATE_STATUS_VERIFY;
                }
                break;

            case ARDUINO_STATE_WAIT_WRITE_COMPLETE:

                if (transferStatus == APP_TRANSFER_STATUS_SUCCESS) {
                    /* Read the status of internal write cycle */
                    transferStatus = APP_TRANSFER_STATUS_IN_PROGRESS;
                    sercom3_i2c_write(ARDUINO_ADDRESS, &ackData, ACK_LENGTH);
                    state = ARDUINO_STATE_CHECK_INTERNAL_WRITE_STATUS;
                } else if (transferStatus == APP_TRANSFER_STATUS_ERROR) {
                    state = APP_STATE_XFER_ERROR;
                    // PORT_REGS->GROUP[2].PORT_OUTTGL = (1 << 18);
                }
                break;

            case ARDUINO_STATE_CHECK_INTERNAL_WRITE_STATUS:

                if (transferStatus == APP_TRANSFER_STATUS_SUCCESS) {
                    state = ARDUINO_STATE_READ;
                    // state = ARDUINO_STATE_WAIT_READ_COMPLETE;
                } else if (transferStatus == APP_TRANSFER_STATUS_ERROR) {
                    /* EEPROM's internal write cycle is not complete. Keep
                     * checking. */
                    transferStatus = APP_TRANSFER_STATUS_IN_PROGRESS;
                    sercom3_i2c_write(ARDUINO_ADDRESS, &ackData, ACK_LENGTH);
                }
                break;

            case ARDUINO_STATE_READ:

                transferStatus = APP_TRANSFER_STATUS_IN_PROGRESS;
                /* Read the data from the page written earlier */
                sercom3_i2c_write_read(ARDUINO_ADDRESS, &testTxData[0],
                                       READ_WRITE_LENGTH, &testRxData[0],
                                       READ_LENGTH);
                state = ARDUINO_STATE_WAIT_READ_COMPLETE;
                break;

            case ARDUINO_STATE_WAIT_READ_COMPLETE:

                if (transferStatus == APP_TRANSFER_STATUS_SUCCESS) {
                    state = APP_STATE_VERIFY;
                } else if (transferStatus == APP_TRANSFER_STATUS_ERROR) {
                    state = APP_STATE_XFER_ERROR;
                }
                break;

            case APP_STATE_VERIFY:

                if (testRxData[0] == 0) {
                    /* It means received data is not same as transmitted data */
                    state = APP_STATE_XFER_ERROR;
                } else {
                    /* It means received data is same as transmitted data */
                    state = APP_STATE_XFER_SUCCESSFUL;
                }
                break;

            case APP_STATE_XFER_SUCCESSFUL: {
                PORT_REGS->GROUP[2].PORT_OUTSET = (1 << 18);

                PORT_REGS->GROUP[1].PORT_OUTSET = (1 << 1);

                // state = ARDUINO_STATE_STATUS_VERIFY;
                break;
            }
            case APP_STATE_XFER_ERROR: {
                PORT_REGS->GROUP[2].PORT_OUTCLR = (1 << 18);
                PORT_REGS->GROUP[1].PORT_OUTSET = (1 << 15);
                state = ARDUINO_STATE_STATUS_VERIFY;

                break;
            }
            default:
                break;
        }
    }
}
