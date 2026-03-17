#pragma once
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// CAN IDs (single source of truth) - edit these manually when your system changes
// ============================================================================

// ---------------- Node CAN IDs (messages directed to this LED MCU) ----------------
#ifndef CAN_ID_PI
#define CAN_ID_PI                  (0x101u)
#endif
#ifndef CAN_ID_ORIN
#define CAN_ID_ORIN                (0x102u)
#endif
#ifndef CAN_ID_MCU_POWER
#define CAN_ID_MCU_POWER           (0x103u)
#endif
#ifndef CAN_ID_KILLSWITCH
#define CAN_ID_KILLSWITCH          (0x104u)
#endif
#ifndef CAN_ID_PRESSURE
#define CAN_ID_PRESSURE            (0x105u)
#endif
#ifndef CAN_ID_ETHERNET
#define CAN_ID_ETHERNET            (0x106u)
#endif
#ifndef CAN_ID_TEMPERATURE
#define CAN_ID_TEMPERATURE         (0x107u)
#endif
#ifndef CAN_ID_THRUSTERS
#define CAN_ID_THRUSTERS           (0x108u)
#endif
#ifndef CAN_ID_SENSORS
#define CAN_ID_SENSORS             (0x109u)
#endif
#ifndef CAN_ID_POWER_CONSUMPTION
#define CAN_ID_POWER_CONSUMPTION   (0x10Au)
#endif
#ifndef CAN_ID_SOFTWARE_MODE
#define CAN_ID_SOFTWARE_MODE       (0x10Bu)
#endif

// ---------------- Watchdog monitor CAN IDs (periodic traffic / heartbeat) ----------------
// These must be sent REGULARLY by each node (one-directional traffic).
// They should NOT be CAN_ID_PI/CAN_ID_ORIN if those are only sent on state changes.
#ifndef CAN_ID_MON_GRIPPER
#define CAN_ID_MON_GRIPPER         (0x300u)
#endif
#ifndef CAN_ID_MON_BMS
#define CAN_ID_MON_BMS             (0x301u)
#endif
#ifndef CAN_ID_MON_THRUSTER
#define CAN_ID_MON_THRUSTER        (0x302u)
#endif
#ifndef CAN_ID_MON_ACOUSTICS
#define CAN_ID_MON_ACOUSTICS       (0x303u)
#endif
#ifndef CAN_ID_MON_PI
#define CAN_ID_MON_PI              (0x304u)
#endif
#ifndef CAN_ID_MON_ORIN
#define CAN_ID_MON_ORIN            (0x305u)
#endif

// ---------------- Watchdog active probe CAN ID ----------------
// The LED MCU sends a probe on this CAN ID if a node has been silent for too long.
#ifndef CAN_ID_ALIVE_REQ
#define CAN_ID_ALIVE_REQ           (0x120u)
#endif

// ---------------- Watchdog timing (ms) ----------------
#ifndef WATCHDOG_SILENCE_MS
#define WATCHDOG_SILENCE_MS        (5000u)
#endif
#ifndef WATCHDOG_PROBE_TIMEOUT_MS
#define WATCHDOG_PROBE_TIMEOUT_MS  (800u)
#endif
#ifndef WATCHDOG_MIN_PROBE_INTERVAL_MS
#define WATCHDOG_MIN_PROBE_INTERVAL_MS (500u)
#endif

// ============================================================================
// Existing API (verified): decoder -> LED logic updates
// ============================================================================

// Decode a received CAN frame and update LED logic.
// Returns true if the message was recognized/used.
bool led_can_decode_and_update(uint32_t can_id, const uint8_t *data, uint8_t len);

// ============================================================================
// Added functionality (integrated watchdog) - no runtime configuration setters
// ============================================================================

typedef void (*led_can_send_cb_t)(uint32_t can_id, const uint8_t *data, uint8_t len);

// Bind CAN TX callback (required for active probes)
void led_can_watchdog_set_send_cb(led_can_send_cb_t cb);

// Enable watchdog + force initial ALARM (fail-safe). Call once at startup.
void led_can_watchdog_init(uint32_t now_ms);

// Feed watchdog with raw CAN RX (call for every received frame)
void led_can_watchdog_on_can_rx(uint32_t can_id, const uint8_t *data, uint8_t len, uint32_t now_ms);

// Periodic tick (call from main loop)
void led_can_watchdog_tick(uint32_t now_ms);

#ifdef __cplusplus
}
#endif