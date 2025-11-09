#include "leak_update_tc2.h"
#include <stdint.h>
// #include "plib_tc2.h"
#include "definitions.h"

volatile bool leakdet_tick = false;

static void tc2_cb(TC_TIMER_STATUS status, uintptr_t context) {
    (void)context;
    leakdet_tick = true;
}

void timing_tc2_init_5hz(void) {
    TC2_TimerCallbackRegister(tc2_cb, 0);

    TC2_TimerStart();
}
