
/*******************************************************************************
  CAN Peripheral Library Interface Header File

  Company:
    Microchip Technology Inc.

  File Name:
    plib_can1.h

  Summary:
    CAN PLIB interface declarations.

  Description:
    The CAN plib provides a simple interface to manage the CAN modules on
    Microchip microcontrollers. This file defines the interface declarations
    for the CAN plib.

  Remarks:
    None.

*******************************************************************************/
//DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2018 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/
//DOM-IGNORE-END

#ifndef CAN1_H
#define CAN1_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

/*
 * This section lists the other files that are included in this file.
 */
#include <stdbool.h>
#include <string.h>
#include "can_common.h"

// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility
    extern "C" {
#endif
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Data Types
// *****************************************************************************
// *****************************************************************************
/* CAN0 Message RAM Configuration Size */
#define CAN0_RX_FIFO0_ELEMENT_SIZE       72
#define CAN0_RX_FIFO0_SIZE               72
#define CAN0_RX_FIFO1_ELEMENT_SIZE       72
#define CAN0_RX_FIFO1_SIZE               72
#define CAN0_RX_BUFFER_ELEMENT_SIZE      72
#define CAN0_RX_BUFFER_SIZE              72
#define CAN0_TX_FIFO_BUFFER_ELEMENT_SIZE 72
#define CAN0_TX_FIFO_BUFFER_SIZE         72
#define CAN0_TX_EVENT_FIFO_SIZE          8
#define CAN0_STD_MSG_ID_FILTER_SIZE      8
#define CAN0_EXT_MSG_ID_FILTER_SIZE      16

/* CAN0_MESSAGE_RAM_CONFIG_SIZE to be used by application or driver
   for allocating buffer from non-cached contiguous memory */
#define CAN0_MESSAGE_RAM_CONFIG_SIZE     320

// *****************************************************************************
// *****************************************************************************
// Section: Interface Routines
// *****************************************************************************
// *****************************************************************************
void CAN0_Initialize (void);
bool CAN0_MessageTransmit(uint32_t id, uint8_t length, uint8_t* data, CAN_MODE mode, CAN_MSG_TX_ATTRIBUTE msgAttr);
bool CAN0_MessageReceive(uint32_t *id, uint8_t *length, uint8_t *data, uint16_t *timestamp,
                                         CAN_MSG_RX_ATTRIBUTE msgAttr, CAN_MSG_RX_FRAME_ATTRIBUTE *msgFrameAttr);
bool CAN0_TransmitEventFIFOElementGet(uint32_t *id, uint8_t *messageMarker, uint16_t *timestamp);
CAN_ERROR CAN0_ErrorGet(void);
void CAN0_ErrorCountGet(uint8_t *txErrorCount, uint8_t *rxErrorCount);
bool CAN0_InterruptGet(CAN_INTERRUPT_MASK interruptMask);
void CAN0_InterruptClear(CAN_INTERRUPT_MASK interruptMask);
bool CAN0_TxFIFOIsFull(void);
void CAN0_MessageRAMConfigSet(uint8_t *msgRAMConfigBaseAddress);
bool CAN0_StandardFilterElementSet(uint8_t filterNumber, can_sidfe_registers_t *stdMsgIDFilterElement);
bool CAN0_StandardFilterElementGet(uint8_t filterNumber, can_sidfe_registers_t *stdMsgIDFilterElement);
bool CAN0_ExtendedFilterElementSet(uint8_t filterNumber, can_xidfe_registers_t *extMsgIDFilterElement);
bool CAN0_ExtendedFilterElementGet(uint8_t filterNumber, can_xidfe_registers_t *extMsgIDFilterElement);
void CAN0_TxCallbackRegister(CAN_CALLBACK callback, uintptr_t contextHandle);
void CAN0_RxCallbackRegister(CAN_CALLBACK callback, uintptr_t contextHandle, CAN_MSG_RX_ATTRIBUTE msgAttr);
// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility
    }
#endif
// DOM-IGNORE-END

#endif // CAN01_H

/*******************************************************************************
 End of File
*/
