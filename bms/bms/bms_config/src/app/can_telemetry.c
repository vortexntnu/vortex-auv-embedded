#include "app/can_telemetry.h"
#include "app/can_facade.h"
#include "ic_bms/bms_spi.h"


#include <stdbool.h>
#include <stdint.h>

extern volatile bool rxReady;
extern uint32_t rx_messageID;
extern uint8_t  rx_message[64];
extern uint8_t  rx_messageLength;






static volatile bool flag_vol_tx = false;
static volatile bool flag_temp_tx = false;

void CAN_telemetry_init(void)
{
    flag_vol_tx = false;
}

void CAN_telemetry_tickISR(void)
{
    flag_vol_tx = true;
    flag_temp_tx = true;
}

void CAN_voltage_send(void)
{
    uint16_t cell_mV[CELLS_COUNT] = {0};
    uint8_t payload[CELLS_PAYLOAD_LEN];
    uint8_t i;
    bool ok;

    if (!flag_vol_tx)
        return;

    flag_vol_tx = false;

    ok = read_cells_1to6(cell_mV);
    if (!ok)
    {
        //handle read error
    }

    for (i = 0; i < CELLS_COUNT; i++)
    {
        payload[2 * i] = (uint8_t)(cell_mV[i] & 0xFF);
        payload[2 * i + 1] = (uint8_t)((cell_mV[i] >> 8) & 0xFF);
    }

    ok = CAN_Send(CAN_VOLTAGE_ID, payload, CELLS_PAYLOAD_LEN);
    if (!ok)
    {
        //handle send error
    }
}
void CAN_temp_send(void)
{
        int16_t t1_dC, t2_dC, t3_dC;
        uint8_t payload[6];
        bool ok;
    
        if (!flag_temp_tx)
            return;
    
        flag_temp_tx = false;
    
        bms_read_ts_temp(TS1_TEMP, &t1_dC);
        bms_read_ts_temp(TS2_TEMP, &t2_dC);
        bms_read_ts_temp(TS3_TEMP, &t3_dC);
    
        payload[0] = (uint8_t)(t1_dC & 0xFF);
        payload[1] = (uint8_t)((t1_dC >> 8) & 0xFF);
        payload[2] = (uint8_t)(t2_dC & 0xFF);
        payload[3] = (uint8_t)((t2_dC >> 8) & 0xFF);
        payload[4] = (uint8_t)(t3_dC & 0xFF);
        payload[5] = (uint8_t)((t3_dC >> 8) & 0xFF);
    
        ok = CAN_Send(CAN_TEMP_ID, payload, sizeof(payload)); 
        if (!ok)
        {
            //handle send error
        }

    
}

void CAN_bothoff_rx(void)
{

    if(!rxReady)
        return;
    rxReady = false;

    if ((rx_messageID == CAN_ID_BOTHOFF_CMD) && (rx_messageLength > 0) && rx_message[0] == BOTHOFF_CMD_BYTE)
    {
        
        bothoff_init();
        bothoff_high();
    }
    
    
}

void CAN_stb_mode(void){
    
    if(!rxReady)
        return;
    rxReady = false;

    if(rx_messageID == CAN_STANDBYMODE_ID && rx_messageLength > 0){
        
        uint8_t standby_mode = rx_message[0];
        
        
        switch (standby_mode) {
            case 0x00:
                // normal operation mode
                break;
            case 0x01:
                // low power standby mode
                break;
            case 0x02:
                // deep sleep mode
                break;
            default:
                // Handle invalid standby mode value
                break;
        }
    }
}

void CAN_rst(void){
    if(!rxReady)
        return;
    rxReady = false;
    
    if(rx_messageID == CAN_RST_MCU && rx_messageLength > 0){
        
        uint8_t rst_cmd = rx_message[0];
        
        if(rst_cmd == CAN_RST_MCU){
            NVIC_SystemReset();
        }
    }
}