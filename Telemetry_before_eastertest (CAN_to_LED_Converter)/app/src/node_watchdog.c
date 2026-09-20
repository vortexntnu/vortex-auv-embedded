#include "node_watchdog.h"

static inline int idx_of(node_id_t node) { return (int)node - 1; }
static inline bool node_valid(node_id_t node) { return (node >= NODE_GRIPPER && node <= NODE_ORIN); }

typedef struct {
    node_watch_state_t state;

    uint32_t last_traffic_ms;

    bool     probe_pending;
    uint32_t probe_deadline_ms;

    bool     traffic_missing;         // latched "we were silent"
    uint32_t last_probe_sent_ms;      // anti-spam
    uint8_t  probe_seq;              // increments per probe
} node_watch_t;

// Defaults live in the module
static node_watchdog_config_t g_cfg = {
    .silence_ms = 5000u,
    .probe_timeout_ms = 800u,
    .min_probe_interval_ms = 500u
};

static node_watchdog_callbacks_t g_cbs = {
    .send_alive_req = 0,
    .set_alarm = 0,
    .on_state_changed = 0
};

static node_watch_t g_nodes[NODE_COUNT];

void node_watchdog_set_callbacks(const node_watchdog_callbacks_t *cbs)
{
    if (!cbs) {
        g_cbs.send_alive_req = 0;
        g_cbs.set_alarm = 0;
        g_cbs.on_state_changed = 0;
        return;
    }
    g_cbs = *cbs;
}

void node_watchdog_set_config(const node_watchdog_config_t *cfg)
{
    if (!cfg) return;
    g_cfg = *cfg;
}

static void set_state(node_id_t node, node_watch_state_t st)
{
    int i = idx_of(node);
    if (g_nodes[i].state == st) return;

    g_nodes[i].state = st;

    if (g_cbs.on_state_changed) {
        g_cbs.on_state_changed(node, st);
    }
}

static void alarm_set(node_id_t node, bool alarm)
{
    if (g_cbs.set_alarm) {
        g_cbs.set_alarm(node, alarm);
    }
}

static void send_probe_if_allowed(node_id_t node, uint32_t now_ms)
{
    int i = idx_of(node);

    if ((uint32_t)(now_ms - g_nodes[i].last_probe_sent_ms) < g_cfg.min_probe_interval_ms) {
        return;
    }

    g_nodes[i].last_probe_sent_ms = now_ms;
    g_nodes[i].probe_seq++;

    if (g_cbs.send_alive_req) {
        g_cbs.send_alive_req(node, g_nodes[i].probe_seq);
    }

    g_nodes[i].probe_pending = true;
    g_nodes[i].probe_deadline_ms = now_ms + g_cfg.probe_timeout_ms;
}

void node_watchdog_init_now(uint32_t now_ms)
{
    for (int i = 0; i < NODE_COUNT; i++) {
        node_id_t node = (node_id_t)(i + 1);

        g_nodes[i].state = NODE_STATE_ALARM;          // fail-safe
        g_nodes[i].last_traffic_ms = now_ms;

        g_nodes[i].probe_pending = false;
        g_nodes[i].probe_deadline_ms = 0;

        g_nodes[i].traffic_missing = true;           // missing until IM_GOOD proves otherwise
        g_nodes[i].last_probe_sent_ms = 0;
        g_nodes[i].probe_seq = 0;

        alarm_set(node, true);
    }
}

void node_watchdog_on_traffic_seen(node_id_t node, uint32_t now_ms)
{
    if (!node_valid(node)) return;
    int i = idx_of(node);

    g_nodes[i].last_traffic_ms = now_ms;

    // Traffic never clears ALARM.
    // If traffic returns after silence, force re-validation probe.
    if (g_nodes[i].traffic_missing) {
        g_nodes[i].traffic_missing = false;
        send_probe_if_allowed(node, now_ms);
    }
}

void node_watchdog_on_im_good(node_id_t node, uint32_t now_ms)
{
    if (!node_valid(node)) return;
    int i = idx_of(node);

    g_nodes[i].probe_pending = false;
    g_nodes[i].traffic_missing = false;
    g_nodes[i].last_traffic_ms = now_ms;

    set_state(node, NODE_STATE_GOOD);
    alarm_set(node, false);
}

void node_watchdog_tick(uint32_t now_ms)
{
    for (int i = 0; i < NODE_COUNT; i++) {
        node_id_t node = (node_id_t)(i + 1);

        const bool silent = ((uint32_t)(now_ms - g_nodes[i].last_traffic_ms) > g_cfg.silence_ms);
        if (silent) {
            g_nodes[i].traffic_missing = true;

            if (!g_nodes[i].probe_pending) {
                send_probe_if_allowed(node, now_ms);
            }
        }

        if (g_nodes[i].probe_pending && now_ms >= g_nodes[i].probe_deadline_ms) {
            g_nodes[i].probe_pending = false;

            set_state(node, NODE_STATE_ALARM);
            alarm_set(node, true);
        }
    }
}

node_watch_state_t node_watchdog_get_state(node_id_t node)
{
    if (!node_valid(node)) return NODE_STATE_ALARM;
    return g_nodes[idx_of(node)].state;
}
