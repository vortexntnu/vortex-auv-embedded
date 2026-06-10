#ifndef CAN_HANDLER_H
#define CAN_HANDLER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <string.h>
#include "app.h"
#include "definitions.h"

bool send_flt_event(uint8_t context);
bool send_pgood_event(uint8_t context);
bool send_killswitch_event(uint8_t context);
bool send_current_measurements(float I_arr[8]);
void dispatch_hw_events(hw_event_flags_t* hw);
bool can_send_frame(uint16_t can_id, const uint8_t* payload, uint8_t length);

#ifdef __cplusplus
}
#endif

#endif
