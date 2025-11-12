#ifndef CAN_FACADE_H
#define CAN_FACADE_H

#include <stdint.h>
#include <stdbool.h>
#include "definitions.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bind message RAM
void CAN_Init(void);

// Non-blocking send: returns true if message queued successfully
bool CAN_Send(uint32_t id, uint8_t* data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif // CAN_FACADE_H
    