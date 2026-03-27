#include "app/can_telemetry.h"
#include "app/can_facade.h"
#include "ic_bms/bms_spi.h"
#include "samc21e18a.h"
#include <stdbool.h>
#include <stdint.h>



static volatile bool flag_vol_tx = false;
static volatile bool flag_temp_tx = false;

static inline void pack_u16_le(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static inline void pack_s16_le(uint8_t *dst, int16_t value)
{
    dst[0] = (uint8_t)(value & 0xFF);
    dst[1] = (uint8_t)((value >> 8) & 0xFF);
}

static void pack_s32_le(uint8_t *dst, int32_t value)
{
    dst[0] = (uint8_t)((uint32_t)value & 0xFFU);
    dst[1] = (uint8_t)(((uint32_t)value >> 8) & 0xFFU);
    dst[2] = (uint8_t)(((uint32_t)value >> 16) & 0xFFU);
    dst[3] = (uint8_t)(((uint32_t)value >> 24) & 0xFFU);
}


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

    // if (!flag_vol_tx)
    //     return;
    //
    // flag_vol_tx = false;

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

#define CAN_ALERT_SSA_ID   0x200u

void CAN_alert_ssa_send(void)
{
    uint16_t alarm = 0U;
    uint16_t ssa   = 0U;
    uint16_t ssb   = 0U;
    uint16_t ssc   = 0U;
    uint8_t payload[8];
    bool ok;

    if (!bq_direct_command(AlarmStatus, &alarm, R))
        return;

    if (!bq_direct_command(SafetyStatusA, &ssa, R))
        return;

    if (!bq_direct_command(SafetyStatusB, &ssb, R))
        return;

    if (!bq_direct_command(SafetyStatusC, &ssc, R))
        return;

    pack_u16_le(&payload[0], alarm);
    pack_u16_le(&payload[2], ssa);
    pack_u16_le(&payload[4], ssb);
    pack_u16_le(&payload[6], ssc);

    ok = CAN_Send(CAN_ALERT_SSA_ID, payload, sizeof(payload));
    if (!ok)
    {
        // handle send error
    }
}


#define CAN_ALERT_PFA_1_ID 0x201u
#define CAN_ALERT_PFA_2_ID 0x202u

void CAN_alert_pfa_send(void)
{
    uint16_t pfa = 0U;
    uint16_t pfb = 0U;
    uint16_t pfc = 0U;
    uint16_t pfd = 0U;
    uint16_t fet = 0U;

    uint8_t payload1[8];
    uint8_t payload2[2];
    bool ok;

    if (!bq_direct_command(PFStatusA, &pfa, R))
        return;

    if (!bq_direct_command(PFStatusB, &pfb, R))
        return;

    if (!bq_direct_command(PFStatusC, &pfc, R))
        return;

    if (!bq_direct_command(PFStatusD, &pfd, R))
        return;

    if (!bq_direct_command(FETStatus, &fet, R))
        return;

    pack_u16_le(&payload1[0], pfa);
    pack_u16_le(&payload1[2], pfb);
    pack_u16_le(&payload1[4], pfc);
    pack_u16_le(&payload1[6], pfd);

    pack_u16_le(&payload2[0], fet);

    ok = CAN_Send(CAN_ALERT_PFA_1_ID, payload1, sizeof(payload1));
    if (!ok)
    {
        // handle send error
        return;
    }

    ok = CAN_Send(CAN_ALERT_PFA_2_ID, payload2, sizeof(payload2));
    if (!ok)
    {
        // handle send error
    }
}

#define CAN_CURRENT_ID     0x203u
//
// void CAN_current_send(void)
// {
//     int16_t current = 0;
//     uint8_t payload[2];
//     bool ok;
//
//     if (!bq_direct_command(CC2Current, (uint16_t *)&current, R))
//     {
//         return;
//     }
//
//     if (!read_data_memory)
//
//     pack_s16_le(&payload[0], current);
//
//     ok = CAN_Send(CAN_CURRENT_ID, payload, sizeof(payload));
//     if (!ok)
//     {
//         // handle send error
//     }
// }

void CAN_current_send(void)
{
    int16_t current = 0;
    int32_t current_counts = 0;
    uint8_t da_status5[32];
    uint8_t payload[6];
    bool ok;

    /* Read 16-bit CC2 current */
    if (!bq_direct_command(CC2Current, (uint16_t *)&current, R))
    {
        return;
    }

    /* Read DASTATUS5 (0x0075) and extract raw CC2 counts at offset 24 */
    if (!read_data_memory(DASTATUS5, da_status5, sizeof(da_status5)))
    {
        return;
    }

    current_counts =
        (int32_t)(
            ((uint32_t)da_status5[24]      ) |
            ((uint32_t)da_status5[25] <<  8) |
            ((uint32_t)da_status5[26] << 16) |
            ((uint32_t)da_status5[27] << 24)
        );

    /* Pack into CAN payload:
       bytes 0..1 = CC2Current (int16_t)
       bytes 2..5 = raw CC2 counts (int32_t)
    */
    pack_s16_le(&payload[0], current);
    pack_s32_le(&payload[2], current_counts);

    ok = CAN_Send(CAN_CURRENT_ID, payload, sizeof(payload));
    if (!ok)
    {
        /* handle send error */
    }
}
