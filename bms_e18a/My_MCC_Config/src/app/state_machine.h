#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

void sm_init(void);
void state_machine(void);
void sm_on_can_wake(void);
void sm_on_rtc_tick(void);

#endif