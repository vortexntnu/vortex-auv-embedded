#include "app/can_telemetry.h"
#include "app/can_facade.h"
#include "ic_bms/bms_spi.h"

#include <stdbool.h>
#include <stdint.h>

#define CAN_ID_CELLS_ALL   0x180
#define CELLS_COUNT        6
#define CELLS_PAYLOAD_LEN  12

static volatile bool telemetry_due = false;

void CAN_telemetry_init(void)
{
    telemetry_due = false;
}

void CAN_telemetry_tickISR(void)
{
    telemetry_due = true;
}

void CAN_voltage_send(void)
{
    uint16_t cell_mV[CELLS_COUNT] = {0};
    uint8_t payload[CELLS_PAYLOAD_LEN];
    uint8_t i;
    bool ok;

    if (!telemetry_due)
        return;

    telemetry_due = false;

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

    ok = CAN_Send(CAN_ID_CELLS_ALL, payload, CELLS_PAYLOAD_LEN);
    if (!ok)
    {
        // optional: handle send error
    }
}
