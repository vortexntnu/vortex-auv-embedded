#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Node Watchdog / Liveness Monitor (MCUs + PI + ORIN)
 *
 * Rules implemented:
 *  - Boot/default = ALARM (fail-safe)
 *  - Only IM_GOOD can transition ALARM -> GOOD
 *  - Traffic can maintain GOOD, never clears ALARM
 *  - If traffic was missing and returns, watchdog forces a new probe (ALIVE_REQ)
 *
 * This module is transport-agnostic:
 *  - You bind callbacks from main() for sending ALIVE_REQ and setting/clearing alarms.
 *  - main() also forwards "traffic seen" and "im good" events into this module.
 */

typedef enum {
    NODE_GRIPPER   = 1,
    NODE_BMS       = 2,
    NODE_THRUSTER  = 3,
    NODE_ACOUSTICS = 4,
    NODE_PI        = 5,
    NODE_ORIN      = 6,
    NODE_COUNT     = 6
} node_id_t;

typedef enum {
    NODE_STATE_ALARM = 0,
    NODE_STATE_GOOD  = 1
} node_watch_state_t;

typedef struct {
    // Silence window before probing (default: 5000 ms)
    uint32_t silence_ms;

    // How long to wait for IM_GOOD after ALIVE_REQ (default: 800 ms)
    uint32_t probe_timeout_ms;

    // Minimum time between probes to the same node (default: 500 ms)
    uint32_t min_probe_interval_ms;
} node_watchdog_config_t;

/**
 * Callback: send an ALIVE request to a node (your code packs CAN payload and transmits).
 * - node: which target to probe
 * - seq: monotonically increasing per node (provided by watchdog)
 */
typedef void (*node_watchdog_send_alive_req_cb)(node_id_t node, uint8_t seq);

/**
 * Callback: set/clear the alarm flag for a node.
 * Typical use: map node -> detail bit(s) in your LED logic.
 */
typedef void (*node_watchdog_set_alarm_cb)(node_id_t node, bool alarm);

/**
 * Optional callback: notify about state changes (for logging).
 */
typedef void (*node_watchdog_state_changed_cb)(node_id_t node,
                                              node_watch_state_t new_state);

typedef struct {
    node_watchdog_send_alive_req_cb  send_alive_req;
    node_watchdog_set_alarm_cb       set_alarm;
    node_watchdog_state_changed_cb   on_state_changed; // may be NULL
} node_watchdog_callbacks_t;

/**
 * Bind callbacks. Call once from main during startup.
 * Required if you want probes and alarm updates to work.
 */
void node_watchdog_set_callbacks(const node_watchdog_callbacks_t *cbs);

/**
 * Optional: override defaults.
 * If you never call this, defaults are used.
 */
void node_watchdog_set_config(const node_watchdog_config_t *cfg);

/**
 * Initialize watchdog using currently stored config + callbacks.
 * All nodes start in ALARM by design and set_alarm(node,true) is called if provided.
 *
 * Minimal main usage:
 *   node_watchdog_set_callbacks(&cbs);
 *   node_watchdog_init_now(millis());
 */
void node_watchdog_init_now(uint32_t now_ms);

/**
 * Inform watchdog that we observed regular traffic from a given node
 * (i.e. we received a frame on that node's "monitor ID").
 */
void node_watchdog_on_traffic_seen(node_id_t node, uint32_t now_ms);

/**
 * Inform watchdog that we received an explicit "IM_GOOD" from a node.
 * This is the ONLY thing that can clear ALARM and set GOOD.
 */
void node_watchdog_on_im_good(node_id_t node, uint32_t now_ms);

/**
 * Periodic tick. Call from main loop with current time (millis).
 */
void node_watchdog_tick(uint32_t now_ms);

/**
 * Read current state (GOOD/ALARM) for a node.
 */
node_watch_state_t node_watchdog_get_state(node_id_t node);

#ifdef __cplusplus
}
#endif
