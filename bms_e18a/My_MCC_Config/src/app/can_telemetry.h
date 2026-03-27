#ifndef CAN_TELEMETRY_H
#define CAN_TELEMETRY_H

#define CELLS_COUNT        6
#define CELLS_PAYLOAD_LEN  12
#define CAN_ID_BOTHOFF_CMD 0x200  //EXAMPLE VALUE
#define BOTHOFF_CMD_BYTE 0xA5
#define CAN_TEMP_ID 0x100         //EXAMPLE VALUE
#define CAN_VOLTAGE_ID 0x204      //EXAMPLE VALUE
//
#define CAN_START_TRANSMIT 0x215
#define CAN_STOP_TRANSMIT 0x216

//legg til func for current, pressure, standbymode, reset mcu

#define CAN_PRESSURE_ID 0x102      //EXAMPLE VALUE
#define CAN_STANDBYMODE_ID 0x103     //EXAMPLE VALUE
#define CAN_RST_MCU 0x104     //EXAMPLE VALUE
#define CAN_CURRENT_ID 0x105     //EXAMPLE VALUE


void CAN_telemetry_init(void);
void CAN_telemetry_tickISR(void);   // call from timer/RTC callback
void CAN_voltage_send(void);        // call from main loop
void CAN_temp_send(void);

void CAN_current_send(void);
void CAN_alert_pfa_send(void);
void CAN_alert_ssa_send(void);



#endif
