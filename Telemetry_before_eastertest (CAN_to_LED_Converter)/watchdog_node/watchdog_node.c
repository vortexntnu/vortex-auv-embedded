#include "watchdog_node.h"
#include "can_facade.h"

/*
 * Watchdog protocol constants
 */
#define CAN_ID_PI         0x101u
#define CAN_ID_ORIN       0x102u
#define CAN_ID_MCU_POWER  0x103u

#define ALIVE_REQ_TYPE    0xA1u
#define MSG_CLEAR         0x00u

typedef struct
{
    uint8_t  probe_target;
    uint32_t reply_id;
    uint8_t  reply_detail;
} watchdog_node_cfg_t;

static watchdog_node_cfg_t g_cfg;
static bool g_initialized = false;

bool watchdog_node_init(watchdog_node_id_t node_id)
{
    g_initialized = true;

    switch (node_id)
    {
        case WD_NODE_GRIPPER:
            g_cfg.probe_target = 1u;
            g_cfg.reply_id     = CAN_ID_MCU_POWER;
            g_cfg.reply_detail = 1u;
            break;

        case WD_NODE_BMS:
            g_cfg.probe_target = 2u;
            g_cfg.reply_id     = CAN_ID_MCU_POWER;
            g_cfg.reply_detail = 2u;
            break;

        case WD_NODE_THRUSTER:
            g_cfg.probe_target = 3u;
            g_cfg.reply_id     = CAN_ID_MCU_POWER;
            g_cfg.reply_detail = 3u;
            break;

        case WD_NODE_ACOUSTICS:
            g_cfg.probe_target = 4u;
            g_cfg.reply_id     = CAN_ID_MCU_POWER;
            g_cfg.reply_detail = 4u;
            break;

        case WD_NODE_PI:
            g_cfg.probe_target = 5u;
            g_cfg.reply_id     = CAN_ID_PI;
            g_cfg.reply_detail = 0u;
            break;

        case WD_NODE_ORIN:
            g_cfg.probe_target = 6u;
            g_cfg.reply_id     = CAN_ID_ORIN;
            g_cfg.reply_detail = 0u;
            break;

        case WD_NODE_NONE:
        default:
            g_initialized = false;
            return false;
    }

    return true;
}

bool watchdog_node_handle_probe(const uint8_t *data, uint8_t len)
{
    uint8_t reply[2];

    if (!g_initialized)
    {
        return false;
    }

    if (data == NULL)
    {
        return false;
    }

    if (len < 2u)
    {
        return false;
    }

    if (data[0] != ALIVE_REQ_TYPE)
    {
        return false;
    }

    if (data[1] != g_cfg.probe_target)
    {
        return false;
    }

    reply[0] = MSG_CLEAR;
    reply[1] = g_cfg.reply_detail;

    return CAN_Send(g_cfg.reply_id, reply, 2u);
}