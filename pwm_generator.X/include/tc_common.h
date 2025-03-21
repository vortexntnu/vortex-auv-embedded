

#ifndef PLIB_TC_COMMON_H    // Guards against multiple inclusion
#define PLIB_TC_COMMON_H

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************
/*  This section lists the other files that are included in this file.
*/

#include <stdbool.h>
#include <stddef.h>
#include "samc21e17a.h"

// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility

    extern "C" {

#endif
// DOM-IGNORE-END
// *****************************************************************************
// *****************************************************************************
// Section:Preprocessor macros
// *****************************************************************************
// *****************************************************************************
// *****************************************************************************

// *****************************************************************************
/* Convenience macros for TC capture status */
// *****************************************************************************

#define TC_CAPTURE_STATUS_NONE              0U

/* Capture status overflow */
#define TC_CAPTURE_STATUS_OVERFLOW          TC_INTFLAG_OVF_Msk

/* Capture status error */
#define TC_CAPTURE_STATUS_ERROR             TC_INTFLAG_ERR_Msk

/* Capture status ready for channel 0 */
#define TC_CAPTURE_STATUS_CAPTURE0_READY    TC_INTFLAG_MC0_Msk

/* Capture status ready for channel 1 */
#define TC_CAPTURE_STATUS_CAPTURE1_READY    TC_INTFLAG_MC1_Msk

#define TC_CAPTURE_STATUS_MSK               (TC_CAPTURE_STATUS_OVERFLOW | TC_CAPTURE_STATUS_ERROR | TC_CAPTURE_STATUS_CAPTURE0_READY | TC_CAPTURE_STATUS_CAPTURE1_READY) 

/* Invalid compare status */
#define TC_CAPTURE_STATUS_INVALID           0xFFFFFFFFU

// *****************************************************************************
/* Convenience macros for TC compare status */
// *****************************************************************************

#define TC_COMPARE_STATUS_NONE          0U
/*  overflow */
#define TC_COMPARE_STATUS_OVERFLOW      TC_INTFLAG_OVF_Msk
/* match compare 0 */
#define TC_COMPARE_STATUS_MATCH0        TC_INTFLAG_MC0_Msk
/* match compare 1 */
#define TC_COMPARE_STATUS_MATCH1        TC_INTFLAG_MC1_Msk

#define TC_COMPARE_STATUS_MSK           (TC_COMPARE_STATUS_OVERFLOW | TC_COMPARE_STATUS_MATCH0 | TC_COMPARE_STATUS_MATCH1)

/* Invalid capture status */
#define TC_COMPARE_STATUS_INVALID       0xFFFFFFFFU

// *****************************************************************************
/* Convenience macros for TC timer status */
// *****************************************************************************

#define TC_TIMER_STATUS_NONE        0U
/*  overflow */
#define TC_TIMER_STATUS_OVERFLOW    TC_INTFLAG_OVF_Msk

/* match compare 1 */
#define TC_TIMER_STATUS_MATCH1      TC_INTFLAG_MC1_Msk

#define TC_TIMER_STATUS_MSK         (TC_TIMER_STATUS_OVERFLOW | TC_TIMER_STATUS_MATCH1)

/* Invalid timer status */
#define TC_TIMER_STATUS_INVALID     0xFFFFFFFFU

// *****************************************************************************
// *****************************************************************************
// Section: Data Types
// *****************************************************************************
// *****************************************************************************
/*  The following data type definitions are used by the functions in this
    interface and should be considered part it.
*/

// *****************************************************************************

typedef uint32_t TC_CAPTURE_STATUS;

typedef uint32_t TC_COMPARE_STATUS;

typedef uint32_t TC_TIMER_STATUS;

typedef enum 
{
    TC_COMMAND_NONE,
    TC_COMMAND_START_RETRIGGER,
    TC_COMMAND_STOP,
    TC_COMMAND_FORCE_UPDATE,
    TC_COMMAND_READ_SYNC
}TC_COMMAND;

// *****************************************************************************

typedef void (*TC_TIMER_CALLBACK) (TC_TIMER_STATUS status, uintptr_t context);

typedef void (*TC_COMPARE_CALLBACK) (TC_COMPARE_STATUS status, uintptr_t context);

typedef void (*TC_CAPTURE_CALLBACK) (TC_CAPTURE_STATUS status, uintptr_t context);

// *****************************************************************************
typedef struct
{
    TC_TIMER_CALLBACK callback;

    uintptr_t context;

} TC_TIMER_CALLBACK_OBJ;

typedef struct
{
    TC_COMPARE_CALLBACK callback;
    uintptr_t context;
}TC_COMPARE_CALLBACK_OBJ;

typedef struct
{
    TC_CAPTURE_CALLBACK callback;
    uintptr_t context;
}TC_CAPTURE_CALLBACK_OBJ;


// DOM-IGNORE-BEGIN
#ifdef __cplusplus  // Provide C++ Compatibility

    }

#endif
// DOM-IGNORE-END

#endif /* PLIB_TC_COMMON_H */
