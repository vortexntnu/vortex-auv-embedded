/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for bootloader project.

  Description:
    This file contains the "main" function for bootloader project.  The
    "main" function calls the "SYS_Initialize" function to initialize
    all modules in the system.
    It calls "bootloader_start" once system is initialized.
 *******************************************************************************/

// DOM-IGNORE-BEGIN
/*******************************************************************************
* Copyright (C) 2023 Microchip Technology Inc. and its subsidiaries.
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
// DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stddef.h>                     // Defines NULL
#include <stdbool.h>                    // Defines true
#include <stdlib.h>                     // Defines EXIT_FAILURE
#include "definitions.h"                // SYS function prototypes
#include "samc21j18a.h"

/*** Macros for SWITCH pin ***/
#define SWITCH_Set()               (PORT_REGS->GROUP[1].PORT_OUTSET = ((uint32_t)1U << 26U))
#define SWITCH_Clear()             (PORT_REGS->GROUP[1].PORT_OUTCLR = ((uint32_t)1U << 26U))
#define SWITCH_Toggle()            (PORT_REGS->GROUP[1].PORT_OUTTGL = ((uint32_t)1U << 26U))
#define SWITCH_OutputEnable()      (PORT_REGS->GROUP[1].PORT_DIRSET = ((uint32_t)1U << 26U))
#define SWITCH_InputEnable()       (PORT_REGS->GROUP[1].PORT_DIRCLR = ((uint32_t)1U << 26U))
#define SWITCH_Get()               (((PORT_REGS->GROUP[1].PORT_IN >> 26U)) & 0x01U)
#define SWITCH_PIN                  PORT_PIN_PB26

/*** Macros for LED pin ***/
#define LED_Set()               (PORT_REGS->GROUP[3].PORT_OUTSET = ((uint32_t)1U << 20U))
#define LED_Clear()             (PORT_REGS->GROUP[3].PORT_OUTCLR = ((uint32_t)1U << 20U))
#define LED_Toggle()            (PORT_REGS->GROUP[3].PORT_OUTTGL = ((uint32_t)1U << 20U))
#define LED_OutputEnable()      (PORT_REGS->GROUP[3].PORT_DIRSET = ((uint32_t)1U << 20U))
#define LED_InputEnable()       (PORT_REGS->GROUP[3].PORT_DIRCLR = ((uint32_t)1U << 20U))
#define LED_Get()               (((PORT_REGS->GROUP[3].PORT_IN >> 20U)) & 0x01U)
#define LED_PIN                  PORT_PIN_PD20

#define BTL_TRIGGER_PATTERN (0x5048434DUL)

static uint32_t *ramStart = (uint32_t *)BTL_TRIGGER_RAM_START;

bool bootloader_Trigger(void)
{
    uint32_t i;

    // Cheap delay. This should give at leat 1 ms delay.
    for (i = 0; i < 2000; i++)
    {
        asm("nop");
    }

    /* Check for Bootloader Trigger Pattern in first 16 Bytes of RAM to enter
     * Bootloader.
     */
    if (BTL_TRIGGER_PATTERN == ramStart[0] && BTL_TRIGGER_PATTERN == ramStart[1] &&
        BTL_TRIGGER_PATTERN == ramStart[2] && BTL_TRIGGER_PATTERN == ramStart[3])
    {
        ramStart[0] = 0;
        return true;
    }

    /* Check for Switch press to enter Bootloader */
    if (SWITCH_Get() == 0)
    {
        return true;
    }

    return false;
}

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************

int main ( void )
{
    /* Initialize all modules */
    SYS_Initialize ( NULL );

    PORT_REGS->GROUP[0].PORT_DIR = (1 << 15);
    PORT_REGS->GROUP[0].PORT_OUTSET = (1 << 15);

    while (true)
    {
        bootloader_CAN_Tasks();
    }

    /* Execution should not come here during normal operation */
    return ( EXIT_FAILURE );
}


/*******************************************************************************
 End of File
*/

