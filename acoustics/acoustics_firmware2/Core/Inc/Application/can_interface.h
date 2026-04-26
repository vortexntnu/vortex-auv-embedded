/*
 * can_interface.h
 *
 *  Created on: 24. apr. 2026
 *      Author: vikin
 */

#ifndef INC_APPLICATION_CAN_INTERFACE_H_
#define INC_APPLICATION_CAN_INTERFACE_H_

#include "arm_math.h"

#define CAN_HANDLE (&hfdcan1)

/* ── Received IDs ───────────────────────────────────────────── */
#define CAN_RESTART_ID                  0x330
#define CAN_STOP_ID                     0x331
#define CAN_START_ID                    0x332
#define CAN_CHECKUP_ID                  0x333

#define CAN_CHANGE_LERP_THRESHOLD_ID  	0x33C
#define CAN_CHANGE_LOWER_N_ID           0x33D
#define CAN_CHANGE_UPPER_N_ID           0x33E
#define CAN_CHANGE_SNR_THRESHOLD_ID  	0x33F

/* ── Sending IDs ────────────────────────────────────────────── */
#define CAN_OK_ID                       0x340
#define CAN_STOPPED_ID                  0x341
#define CAN_ERRORED_ID                  0x342
#define CAN_UNRECOGNIZED_REQUEST_ID     0x34F

/* ── Nominal operation IDs ──────────────────────────────────── */
#define CAN_DIRECTION_ID                0x350

/* ── Dump IDs ───────────────────────────────────────────────── */
#define CAN_MAGNITUDE_DUMP_ID           0x360
#define CAN_FULL_DUMP_HYDROPHONE_1_ID   0x360
#define CAN_FULL_DUMP_HYDROPHONE_2_ID   0x361
#define CAN_FULL_DUMP_HYDROPHONE_3_ID   0x362
#define CAN_FULL_DUMP_HYDROPHONE_4_ID   0x363
#define CAN_FULL_DUMP_HYDROPHONE_5_ID   0x364

/* ── Filter range for interrupt activation ──────────────────── */
#define CAN_RX_FILTER_ID_LOW            0x330
#define CAN_RX_FILTER_ID_HIGH           0x33F

/* ── Public API ─────────────────────────────────────────────── */
void can_init(void);
void can_handle_requests(void);

/* Status responses */
void can_ok(void);
void can_stopped(void);
void can_errored(void);
void can_unrecognized(void);

/* Nominal TX */
void can_send_direction(float32_t vec[3], float32_t weight);

#endif /* INC_APPLICATION_CAN_INTERFACE_H_ */
