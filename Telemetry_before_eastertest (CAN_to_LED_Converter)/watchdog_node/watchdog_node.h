/* Eksempel implementation in main

#include "watchdog_node.h"

int main(void)
{
    SYS_Initialize(NULL);
    CAN_Init();

    (void)watchdog_node_init(WD_NODE_THRUSTER);

    while (true)
    {
        CAN_RecoverIfNeeded();

        if (rxReady)
        {
            rxReady = false;

            if (rx_messageID == 0x120u)
            {
                (void)watchdog_node_handle_probe(rx_message, rx_messageLength);
            }
        }
    }
}
*/



#ifndef WATCHDOG_NODE_H
#define WATCHDOG_NODE_H

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    WD_NODE_NONE      = 0,
    WD_NODE_GRIPPER   = 1,
    WD_NODE_BMS       = 2,
    WD_NODE_THRUSTER  = 3,
    WD_NODE_ACOUSTICS = 4,
    WD_NODE_PI        = 5,
    WD_NODE_ORIN      = 6
} watchdog_node_id_t;

/*
 * Must be called once before watchdog_node_handle_probe().
 * Returns false if node_id is invalid.
 */
bool watchdog_node_init(watchdog_node_id_t node_id);

/*
 * Handle payload of an already-filtered alive probe message.
 *
 * Intended usage:
 *   if (rx_messageID == 0x120u)
 *   {
 *       (void)watchdog_node_handle_probe(rx_message, rx_messageLength);
 *   }
 *
 * Returns true only if this node recognized the probe as targeted to itself
 * and attempted to send the IM_GOOD response.
 */
bool watchdog_node_handle_probe(const uint8_t *data, uint8_t len);

#ifdef __cplusplus
}
#endif

#endif /* WATCHDOG_NODE_H */