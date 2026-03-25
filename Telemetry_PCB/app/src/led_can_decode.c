#include "led_can_decode.h"
#include "led_logic.h"

// ============================================================================
// Decoder (existing, verified behavior) - DO NOT change semantics
// ============================================================================

typedef enum {
    MSG_CLEAR          = 0x00,
    MSG_WARN           = 0x01,
    MSG_FAULT          = 0x02,

    WIFI_NOT_CONNECTED = 0x10,
    WIFI_CONNECTED     = 0x11,
} msg_type_t;

static inline uint8_t mask4(uint8_t v) { return (uint8_t)(v & 0x0Fu); }

static bool map_can_id_to_subsystem(uint32_t can_id, uint8_t *out_subsystem_id)
{
    if (!out_subsystem_id) return false;

    switch (can_id) {
        case CAN_ID_PI:                 *out_subsystem_id = 1;  return true;
        case CAN_ID_ORIN:               *out_subsystem_id = 2;  return true;
        case CAN_ID_MCU_POWER:          *out_subsystem_id = 3;  return true;
        case CAN_ID_KILLSWITCH:         *out_subsystem_id = 4;  return true;
        case CAN_ID_PRESSURE:           *out_subsystem_id = 5;  return true;
        case CAN_ID_ETHERNET:           *out_subsystem_id = 6;  return true;
        case CAN_ID_TEMPERATURE:        *out_subsystem_id = 7;  return true;
        case CAN_ID_THRUSTERS:          *out_subsystem_id = 8;  return true;
        case CAN_ID_SENSORS:            *out_subsystem_id = 9;  return true;
        case CAN_ID_POWER_CONSUMPTION:  *out_subsystem_id = 10; return true;
        case CAN_ID_SOFTWARE_MODE:      *out_subsystem_id = 11; return true;
        default: return false;
    }
}

bool led_can_decode_and_update(uint32_t can_id, const uint8_t *data, uint8_t len)
{
    if (!data || len < 1u) return false;

    const uint8_t type = data[0];

    // -------- Node-level WiFi status --------
    if (type == WIFI_CONNECTED || type == WIFI_NOT_CONNECTED)
    {
        const bool connected = (type == WIFI_CONNECTED);

        if (can_id == CAN_ID_PI) {
            led_logic_set_pi_wifi(connected);
            return true;
        }
        if (can_id == CAN_ID_ORIN) {
            led_logic_set_orin_wifi(connected);
            return true;
        }
        return false;
    }

    // -------- Legacy subsystem status/fault messages --------
    if (len < 2u) return false;

    uint8_t subsystem_id = 0;
    if (!map_can_id_to_subsystem(can_id, &subsystem_id)) return false;

    const uint8_t parameter    = data[1];
    const uint8_t detail_mask4 = mask4(parameter);
    const bool    has_detail   = (detail_mask4 != 0u);

    switch ((msg_type_t)type)
    {
        case MSG_CLEAR:
            if (parameter == 0x00u) {            // clear subsystem + detail
                led_logic_clear_subsystem(subsystem_id);
                if (can_id == CAN_ID_PRESSURE) led_logic_set_pressure_ok(true);
                return true;
            }
            if (parameter == 0x01u) {            // clear detail only
                led_logic_clear_detail(subsystem_id);
                return true;
            }
            return false;

        case MSG_WARN:
            led_logic_set_subsystem(LED_SEV_WARN, subsystem_id, has_detail, detail_mask4);
            if (can_id == CAN_ID_PRESSURE) led_logic_set_pressure_ok(false);
            return true;

        case MSG_FAULT:
            led_logic_set_subsystem(LED_SEV_FAULT, subsystem_id, has_detail, detail_mask4);
            if (can_id == CAN_ID_PRESSURE) led_logic_set_pressure_ok(false);
            return true;

        default:
            return false;
    }
}

// ============================================================================
// Integrated watchdog
// MCU subsystem uses detail values 1,2,3,4 (one active detail shown at a time)
// instead of bitmask 1,2,4,8.
// ============================================================================

typedef enum { WD_ALARM = 0, WD_GOOD = 1 } wd_state_t;

typedef enum {
    LED_NODE_GRIPPER   = 1,
    LED_NODE_BMS       = 2,
    LED_NODE_THRUSTER  = 3,
    LED_NODE_ACOUSTICS = 4,
    LED_NODE_PI        = 5,
    LED_NODE_ORIN      = 6,
    LED_NODE_COUNT     = 6
} led_node_id_t;

typedef struct {
    wd_state_t state;

    uint32_t last_traffic_ms;

    bool     probe_pending;
    uint32_t probe_deadline_ms;

    bool     traffic_missing;            // latched "we were silent / not yet validated"
    uint32_t last_probe_sent_ms;         // anti-spam
    uint8_t  probe_seq;

    uint32_t last_validation_probe_ms;   // periodic re-validation while ALARM
} wd_node_t;

static bool g_wd_enabled = false;
static led_can_send_cb_t g_send_cb = 0;

// Monitor IDs are compile-time configuration (from header)
static const uint32_t g_monitor_can_id[LED_NODE_COUNT + 1] = {
    0,
    CAN_ID_MON_GRIPPER,
    CAN_ID_MON_BMS,
    CAN_ID_MON_THRUSTER,
    CAN_ID_MON_ACOUSTICS,
    CAN_ID_MON_PI,
    CAN_ID_MON_ORIN
};

static wd_node_t g_nodes[LED_NODE_COUNT + 1];

// MCU alarm storage:
// index 1..4 corresponds to detail values 1..4 shown on subsystem 3.
static bool g_mcu_alarm_active[5] = {
    false, // unused index 0
    true,  // Gripper starts in ALARM
    true,  // BMS starts in ALARM
    true,  // Thruster starts in ALARM
    true   // Acoustics starts in ALARM
};

static uint8_t first_active_mcu_detail(void)
{
    for (uint8_t i = 1u; i <= 4u; i++) {
        if (g_mcu_alarm_active[i]) {
            return i;
        }
    }
    return 0u;
}

static void apply_mcu_alarm_to_led(void)
{
    const uint8_t detail = first_active_mcu_detail();

    if (detail != 0u) {
        // Subsystem 3 = MCU_POWER
        // Show one MCU detail at a time using values 1,2,3,4.
        led_logic_set_subsystem(LED_SEV_FAULT, 3u, true, detail);
    } else {
        led_logic_clear_subsystem(3u);
    }
}

static void apply_alarm_to_led(led_node_id_t node, bool alarm)
{
    // PI/ORIN use their own subsystems (1 and 2).
    if (node == LED_NODE_PI) {
        if (alarm) led_logic_set_subsystem(LED_SEV_FAULT, 1u, false, 0u);
        else       led_logic_clear_subsystem(1u);
        return;
    }

    if (node == LED_NODE_ORIN) {
        if (alarm) led_logic_set_subsystem(LED_SEV_FAULT, 2u, false, 0u);
        else       led_logic_clear_subsystem(2u);
        return;
    }

    // MCUs use subsystem 3 and detail values 1..4.
    if (node >= LED_NODE_GRIPPER && node <= LED_NODE_ACOUSTICS) {
        g_mcu_alarm_active[(uint8_t)node] = alarm;
        apply_mcu_alarm_to_led();
    }
}

static void send_probe_if_allowed(led_node_id_t node, uint32_t now_ms)
{
    if (!g_send_cb) return;

    wd_node_t *n = &g_nodes[(int)node];

    if ((uint32_t)(now_ms - n->last_probe_sent_ms) < WATCHDOG_MIN_PROBE_INTERVAL_MS) {
        return;
    }

    n->last_probe_sent_ms = now_ms;
    n->probe_seq++;

    // Payload:
    //   data[0] = 0xA1 (ALIVE_REQ)
    //   data[1] = target node
    //   data[2] = sequence
    uint8_t data[3];
    data[0] = 0xA1u;
    data[1] = (uint8_t)node;
    data[2] = n->probe_seq;

    g_send_cb(CAN_ID_ALIVE_REQ, data, 3u);

    n->probe_pending = true;
    n->probe_deadline_ms = now_ms + WATCHDOG_PROBE_TIMEOUT_MS;
}

void led_can_watchdog_set_send_cb(led_can_send_cb_t cb)
{
    g_send_cb = cb;
}

void led_can_watchdog_init(uint32_t now_ms)
{
    g_wd_enabled = true;

    // Fail-safe defaults:
    // - all 4 MCU nodes start in ALARM
    // - PI and ORIN start in ALARM
    g_mcu_alarm_active[0] = false;
    g_mcu_alarm_active[1] = true;
    g_mcu_alarm_active[2] = true;
    g_mcu_alarm_active[3] = true;
    g_mcu_alarm_active[4] = true;

    for (int i = 1; i <= LED_NODE_COUNT; i++) {
        g_nodes[i].state = WD_ALARM;
        g_nodes[i].last_traffic_ms = now_ms;
        g_nodes[i].probe_pending = false;
        g_nodes[i].probe_deadline_ms = 0u;
        g_nodes[i].traffic_missing = true;      // not yet validated / missing
        g_nodes[i].last_probe_sent_ms = 0u;
        g_nodes[i].probe_seq = 0u;
        g_nodes[i].last_validation_probe_ms = 0u;

        apply_alarm_to_led((led_node_id_t)i, true);
    }
}

static void on_traffic_seen(led_node_id_t node, uint32_t now_ms)
{
    wd_node_t *n = &g_nodes[(int)node];
    n->last_traffic_ms = now_ms;

    // If traffic appears while node is marked missing/not validated,
    // force a validation probe. This includes startup and traffic return.
    if (n->traffic_missing) {
        n->traffic_missing = false;
        send_probe_if_allowed(node, now_ms);
    }
}

static void on_im_good(led_node_id_t node, uint32_t now_ms)
{
    wd_node_t *n = &g_nodes[(int)node];

    n->probe_pending = false;
    n->traffic_missing = false;
    n->last_traffic_ms = now_ms;
    n->last_validation_probe_ms = now_ms;

    n->state = WD_GOOD;
    apply_alarm_to_led(node, false);
}

void led_can_watchdog_on_can_rx(uint32_t can_id, const uint8_t *data, uint8_t len, uint32_t now_ms)
{
    if (!g_wd_enabled) return;

    // 1) Passive traffic monitoring by configured monitor IDs
    for (int i = 1; i <= LED_NODE_COUNT; i++) {
        if (g_monitor_can_id[i] != 0u && can_id == g_monitor_can_id[i]) {
            on_traffic_seen((led_node_id_t)i, now_ms);
            break;
        }
    }

    // 2) Explicit IM_GOOD decoding (only thing that clears ALARM -> GOOD)
    if (!data || len < 2u) return;

    const uint8_t type      = data[0];
    const uint8_t parameter = data[1];

    if (type != MSG_CLEAR) return;      // CLEAR is used as "system good" marker

    // PI/ORIN: CLEAR 0x00 on their own CAN IDs is considered IM_GOOD
    if (can_id == CAN_ID_PI) {
        if (parameter == 0x00u) on_im_good(LED_NODE_PI, now_ms);
        return;
    }

    if (can_id == CAN_ID_ORIN) {
        if (parameter == 0x00u) on_im_good(LED_NODE_ORIN, now_ms);
        return;
    }

    // MCUs: on CAN_ID_MCU_POWER the parameter is a single detail value 1..4
    if (can_id == CAN_ID_MCU_POWER) {
        switch (parameter & 0x0Fu) {
            case 1u: on_im_good(LED_NODE_GRIPPER,   now_ms); break;
            case 2u: on_im_good(LED_NODE_BMS,       now_ms); break;
            case 3u: on_im_good(LED_NODE_THRUSTER,  now_ms); break;
            case 4u: on_im_good(LED_NODE_ACOUSTICS, now_ms); break;
            default: break;
        }
    }
}

void led_can_watchdog_tick(uint32_t now_ms)
{
    if (!g_wd_enabled) return;

    for (int i = 1; i <= LED_NODE_COUNT; i++) {
        led_node_id_t node = (led_node_id_t)i;
        wd_node_t *n = &g_nodes[i];

        const bool silent = ((uint32_t)(now_ms - n->last_traffic_ms) > WATCHDOG_SILENCE_MS);

        if (silent) {
            // No traffic seen recently -> mark missing and probe
            n->traffic_missing = true;

            if (!n->probe_pending) {
                send_probe_if_allowed(node, now_ms);
            }
        } else {
            // Traffic exists. If node is still in ALARM, keep trying validation
            // periodically until IM_GOOD is received.
            if ((n->state == WD_ALARM) && !n->probe_pending) {
                if ((uint32_t)(now_ms - n->last_validation_probe_ms) >= WATCHDOG_REVALIDATE_MS) {
                    send_probe_if_allowed(node, now_ms);
                    n->last_validation_probe_ms = now_ms;
                }
            }
        }

        // probe timeout -> latch / keep ALARM
        if (n->probe_pending && now_ms >= n->probe_deadline_ms) {
            n->probe_pending = false;
            n->state = WD_ALARM;
            apply_alarm_to_led(node, true);
        }
    }
}