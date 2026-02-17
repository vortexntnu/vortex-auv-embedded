#ifndef CAN_TELEMETRY_H
#define CAN_TELEMETRY_H

void CAN_telemetry_init(void);
void CAN_telemetry_tickISR(void);   // call from timer/RTC callback
void CAN_voltage_send(void);        // call from main loop

#endif
