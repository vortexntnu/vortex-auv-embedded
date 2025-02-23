/*
 * File:   i2c.h
 * Author: nathaniel
 *
 * Created on January 19, 2025, 2:41 PM
 */

/*
 * File:   i2c_client.h
 * Author: nathaniel
 *
 * Created on November 10, 2024, 4:26 PM
 */

#ifndef I2C_H
#define I2C_H
#include "samc21e17a.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /* I2C Master is writing to slave */
    SERCOM_I2C_SLAVE_TRANSFER_DIR_WRITE = 0,

    /* I2C Master is reading from slave */
    SERCOM_I2C_SLAVE_TRANSFER_DIR_READ = 1,
} SERCOM_I2C_SLAVE_TRANSFER_DIR;

typedef enum {
    SERCOM_I2C_SLAVE_ACK_ACTION_SEND_ACK = 0,
    SERCOM_I2C_SLAVE_ACK_ACTION_SEND_NAK,
} SERCOM_I2C_SLAVE_ACK_ACTION_SEND;

typedef enum {
    SERCOM_I2C_SLAVE_INTFLAG_PREC = SERCOM_I2CS_INTFLAG_PREC_Msk,
    SERCOM_I2C_SLAVE_INTFLAG_AMATCH = SERCOM_I2CS_INTFLAG_AMATCH_Msk,
    SERCOM_I2C_SLAVE_INTFLAG_DRDY = SERCOM_I2CS_INTFLAG_DRDY_Msk,
    SERCOM_I2C_SLAVE_INTFLAG_ERROR = SERCOM_I2CS_INTFLAG_ERROR_Msk,
} SERCOM_I2C_SLAVE_INTFLAG;

typedef enum {
    SERCOM_I2C_SLAVE_ACK_STATUS_RECEIVED_ACK = 0,
    SERCOM_I2C_SLAVE_ACK_STATUS_RECEIVED_NAK,
} SERCOM_I2C_SLAVE_ACK_STATUS;

typedef enum {
    SERCOM_I2C_SLAVE_TRANSFER_EVENT_NONE = 0,
    SERCOM_I2C_SLAVE_TRANSFER_EVENT_ADDR_MATCH,

    /* Data sent by I2C Master is available */
    SERCOM_I2C_SLAVE_TRANSFER_EVENT_RX_READY,

    /* I2C slave can respond to data read request from I2C Master */
    SERCOM_I2C_SLAVE_TRANSFER_EVENT_TX_READY,

    SERCOM_I2C_SLAVE_TRANSFER_EVENT_STOP_BIT_RECEIVED,
    SERCOM_I2C_SLAVE_TRANSFER_EVENT_ERROR,
} SERCOM_I2C_SLAVE_TRANSFER_EVENT;

typedef enum {
    SERCOM_I2C_SLAVE_COMMAND_SEND_ACK = 0,
    SERCOM_I2C_SLAVE_COMMAND_SEND_NAK,
    SERCOM_I2C_SLAVE_COMMAND_RECEIVE_ACK_NAK,
    SERCOM_I2C_SLAVE_COMMAND_WAIT_FOR_START,
} SERCOM_I2C_SLAVE_COMMAND;

typedef enum {
    SERCOM_I2C_SLAVE_ERROR_BUSERR = SERCOM_I2CS_STATUS_BUSERR_Msk,
    SERCOM_I2C_SLAVE_ERROR_COLL = SERCOM_I2CS_STATUS_COLL_Msk,
    SERCOM_I2C_SLAVE_ERROR_LOWTOUT = SERCOM_I2CS_STATUS_LOWTOUT_Msk,
    SERCOM_I2C_SLAVE_ERROR_SEXTTOUT = SERCOM_I2CS_STATUS_SEXTTOUT_Msk,
    SERCOM_I2C_SLAVE_ERROR_ALL =
        (SERCOM_I2C_SLAVE_ERROR_BUSERR | SERCOM_I2C_SLAVE_ERROR_COLL |
         SERCOM_I2C_SLAVE_ERROR_LOWTOUT | SERCOM_I2C_SLAVE_ERROR_SEXTTOUT)
} SERCOM_I2C_SLAVE_ERROR;

// *****************************************************************************
/* SERCOM I2C Callback

   Summary:
    SERCOM I2C Callback Function Pointer.

   Description:
    This data type defines the SERCOM I2C Callback Function Pointer.

   Remarks:
    None.
*/

typedef bool (*SERCOM_I2C_SLAVE_CALLBACK)(SERCOM_I2C_SLAVE_TRANSFER_EVENT event,
                                          uintptr_t contextHandle);

// *****************************************************************************
/* SERCOM I2C PLib Instance Object

   Summary:
    SERCOM I2C PLib Object structure.

   Description:
    This data structure defines the SERCOM I2C PLib Instance Object.

   Remarks:
    None.
*/

typedef struct {
    bool isBusy;

    bool isRepeatedStart;

    bool isFirstRxAfterAddressPending;

    /* Transfer Event Callback */
    SERCOM_I2C_SLAVE_CALLBACK callback;

    /* Transfer context */
    uintptr_t context;

} SERCOM_I2C_SLAVE_OBJ;

void SERCOM3_SLAVE_I2C_Initialize(void);
void SERCOM3_I2C_CallbackRegister(SERCOM_I2C_SLAVE_CALLBACK callback,
                                  uintptr_t contextHandle);
bool SERCOM3_I2C_IsBusy(void);
uint8_t SERCOM3_I2C_ReadByte(void);
void SERCOM3_I2C_WriteByte(uint8_t wrByte);
SERCOM_I2C_SLAVE_ERROR SERCOM3_SLAVE_I2C_ErrorGet(void);
SERCOM_I2C_SLAVE_TRANSFER_DIR SERCOM3_I2C_TransferDirGet(void);
SERCOM_I2C_SLAVE_ACK_STATUS SERCOM3_I2C_LastByteAckStatusGet(void);
void SERCOM3_I2C_CommandSet(SERCOM_I2C_SLAVE_COMMAND command);

#ifdef __cplusplus
}
#endif

#endif /* I2C_CLIENT_H */
