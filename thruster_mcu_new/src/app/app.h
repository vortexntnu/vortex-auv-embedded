#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

#define WRITE_ID(id) (id << 18)
#define READ_ID(id) (id >> 18)

#define THRUSTER_TIMEOUT_TICKS 50
#define TRANSFER_SIZE 16
#define PWM_MAX_STEP_US  25U

typedef enum {
    CAN_ID_TURN_THRUSTERS_OFF = 0x369U,
    CAN_ID_TURN_LIGHTS_OFF,
    CAN_ID_RESET,
    CAN_ID_SET_THRUSTER_PWM,
    CAN_ID_SET_LIGHT_PWM,

    CAN_ID_FLT_EVENT,
    CAN_ID_PGOOD_EVENT,
    CAN_ID_KILLSWITCH_EVENT,
    CAN_ID_CURRENT_MEASUREMENTS,
    CAN_ID_THRUSTER_TIMEOUT_EVENT,
} can_msg_id_t;


typedef struct {
    volatile uint8_t flt_pending_mask;
    volatile uint8_t pgood_pending_mask;
    volatile uint8_t killswitch_pending_mask;
} hw_event_flags_t;

void app_init(void);
void app_task(void);

#ifdef __cplusplus
}
#endif

#endif
