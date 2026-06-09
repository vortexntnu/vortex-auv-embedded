#ifndef APP_H
#define APP_H

#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

#define WRITE_ID(id) (id << 18)
#define READ_ID(id) (id >> 18)

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
} can_msg_id_t;



enum operating_mode {
    PWM_TCC,
    MPWM_TC,
};

struct pwm_output {
    enum operating_mode mode;
    uint8_t  instance;
    uint8_t  channel;
    uint32_t period_ticks;
    uint16_t min_us;
    uint16_t max_us;
    uint16_t neutral_us;
    uint32_t frame_us;
    uint16_t current_pulse_us;
    uint16_t target_pulse_us;
};





void app_init(void);
void app_task(void);

#ifdef __cplusplus
}
#endif

#endif
