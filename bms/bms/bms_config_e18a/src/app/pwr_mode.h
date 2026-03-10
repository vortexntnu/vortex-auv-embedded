#ifndef PWR_MODE_H
#define PWR_MODE_H

typedef enum{
    PWR_MODE_ACTIVE=0,
    PWR_MODE_STANDBY=1
} pwr_mode_state_t;

void pwr_mode_init(void);
void pwr_set_idle0(void);
void pwr_set_stb_lowpower(void);
void pwr_enter_sleep(void);

void pwr_set_state(pwr_mode_state_t state);
pwr_mode_state_t pwr_get_state(void);

#endif