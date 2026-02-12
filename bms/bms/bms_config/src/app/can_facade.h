#ifndef CAN_FACADE_H
#define CAN_FACADE_H

#include <stdint.h>
#include <stdbool.h>
#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Suggested arbitration IDs (11-bit standard). Lower numeric = higher priority. */
#define CAN_ID_SHUTDOWN_CMD   0x050U  /* Highest priority: killswitch/leak */
#define CAN_ID_LEAK_ALERT     0x060U  /* Telemetry alert to BMS */
#define CAN_ID_BMS_STATUS     0x180U  /* Periodic pack/FET status */
#define CAN_ID_CELL_V_BASE    0x200U  /* Cell voltages: CAN_ID_CELL_V_BASE + cell_index (0..15) */

void CAN_Init(void);
bool CAN_Send(uint32_t id, const uint8_t *data, uint8_t len);
void APP_CAN_RxCallback(uint8_t numberOfMessage, uintptr_t context);

#ifdef __cplusplus
}
#endif

#endif /* CAN_FACADE_H */
