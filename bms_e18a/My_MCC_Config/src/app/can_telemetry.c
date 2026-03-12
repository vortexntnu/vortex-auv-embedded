#include "app/can_telemetry.h"
#include "app/can_facade.h"
#include "ic_bms/bms_spi.h"
#include "samc21e18a.h"
#include <stdbool.h>
#include <stdint.h>



static volatile bool flag_vol_tx = false;
static volatile bool flag_temp_tx = false;

void CAN_telemetry_init(void) 
{
    flag_vol_tx = false; // Initialize voltage transmission flag
}

void CAN_telemetry_tickISR(void) // Call this function every 100ms from a timer interrupt
{
    flag_vol_tx = true;
    flag_temp_tx = true; // Set temperature transmission flag
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

        //Divides temp data into 6 payloads (2 bytes per temp) and sends over CAN
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