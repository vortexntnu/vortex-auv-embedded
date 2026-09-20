/*******************************************************************************
  Main Source File

  Company:
    Microchip Technology Inc.

  File Name:
    main.c

  Summary:
    This file contains the "main" function for a project.

  Description:
    This file contains the "main" function for a project.  The
    "main" function calls the "SYS_Initialize" function to initialize the state
    machines of all modules in the system
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include <stdbool.h>  // Defines true
#include <stddef.h>   // Defines NULL
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>  // Defines EXIT_FAILURE
#include "app/can_telemetry.h"
#include "app/state_machine.h"
#include "config/default/peripheral/rtc/plib_rtc.h"
#include "config/default/peripheral/systick/plib_systick.h"
#include "definitions.h"  // SYS function prototypes
#include "ic_bms/bms_spi.h"
#include "ic_bms/spi_test.h"
#include "peripheral/port/plib_port.h"
#include "app/can_facade.h"

// *****************************************************************************
// *****************************************************************************
// Section: Main Entry Point
// *****************************************************************************
// *****************************************************************************
/*
static void CAN_Wake_EIC_Callback(uintptr_t context) // called when a CAN
wake-up interrupt occurs on EIC pin 14
{
    (void)context;
    sm_on_can_wake();
}


*/


typedef struct {
    bool initialized;

    float soc;                 // 0.0 to 1.0
    float remaining_mAh;
    float capacity_mAh;

    int32_t current_offset_mA;

    uint32_t last_update_ms;

    bool full_reference_seen;
    bool empty_reference_seen;
} soc_estimator_t;

void soc_init(soc_estimator_t* soc);
void soc_update(soc_estimator_t* soc);
void WDT_Enable(void);
void WDT_Clear(void);

#define BMS_CELL_COUNT 6
#define BMS_TEMP_COUNT 3
#define PACK_SERIES_CELLS      6
#define PACK_PARALLEL_CELLS    7

#define CELL_CAPACITY_MAH      3000   // conservative VTC6 value
#define PACK_CAPACITY_MAH      (PACK_PARALLEL_CELLS * CELL_CAPACITY_MAH)

typedef struct {
    uint32_t timestamp_ms;

    int16_t cell_mV[BMS_CELL_COUNT];
    int32_t pack_mV;

    int16_t temp_dC[BMS_TEMP_COUNT];

    int16_t current_mA;       // calibrated current from CC2Current
    int32_t cc2_raw_counts;   // optional diagnostic / backup

    bool valid;
} bms_sample_t;

typedef struct {
    uint32_t low_current_start_ms;
    bool relaxed;
} rest_detector_t;



volatile bool rtc_timer = false;
extern volatile bool can_tx_avaliable;

volatile uint32_t ticks_dsg_off = 0;
static soc_estimator_t soc_estimator;

static soc_estimator_t soc;
static rest_detector_t rest;

void bms_task(void);


static void TelemetryRtcCb(
    RTC_TIMER32_INT_MASK intCause,
    uintptr_t context)  // called every 100ms by the RTC timer interrupt
{
    (void)intCause;
    (void)context;
    // sm_on_rtc_tick();
    // CAN_telemetry_tickISR();
    // printf("RTC interrupt\r\n");
    rtc_timer = true;
}

int main(void) {
    /* Initialize all modules */
    SYS_Initialize(NULL);
    SYSTICK_TimerStart();
    BOTHOFF_Clear();

    BQ769x2_Init();
    // BOTHOFF_Clear();
    //  bms_init_comm_voltage();
    SYSTICK_DelayMs(100U);
    RTC_Timer32CompareSet(1000);
    RTC_Timer32CallbackRegister(TelemetryRtcCb, 0);
    RTC_Timer32InterruptEnable(RTC_TIMER32_INT_MASK_CMP0);
    RTC_Timer32Start();
    // bms_alert_irq_init();
    CAN_Init();
    // pwr_set_idle0();

    // CommandSubcommands(FET_ENABLE); // FET_ENABLE
    voltage_test_init();
    soc_init(&soc_estimator);
    WDT_Enable(); // Enable watchdog

    // can_scope_test_init();

    // printf("CAN scope test running\r\n");

    while (true) {
        SYS_Tasks();
        if (rtc_timer) {
            uint8_t fet = voltage_test_step();
            rtc_timer = false;
            // CAN_voltage_send();
            if (can_tx_avaliable) {
                CAN_alert_pfa_send();
                CAN_alert_ssa_send();
                CAN_current_send();
                CAN_voltage_send();
                CAN_temp_send();
            }

            // if ((fet & (1 << 2)) != (1 << 2)) {
            //     ticks_dsg_off++;
            //
            //     if (ticks_dsg_off == 10000) {
            //         NVIC_SystemReset();
            //     }
            // }
            bms_task();
        }

        // bms_alert_step();
        // spi_write_probe_step();

        // can_sope_test_step();
        state_machine_simple();
        WDT_Clear(); // Clear watchdog timer
    }

    /* Execution should not come here during normal operation */

    return (EXIT_FAILURE);
}

/*******************************************************************************
 End of File
*/


void soc_init(soc_estimator_t *e)
{
    memset(e, 0, sizeof(*e));

    e->capacity_mAh = 21000.0f;
    e->remaining_mAh = 21000.0f * 0.5f;  // temporary until voltage init
    e->soc = 0.5f;
    e->initialized = false;
}

float soc_from_ocv_mV(int16_t mv)
{
    if (mv >= 4200) return 1.00f;
    if (mv >= 4100) return 0.90f;
    if (mv >= 4000) return 0.80f;
    if (mv >= 3900) return 0.68f;
    if (mv >= 3800) return 0.55f;
    if (mv >= 3700) return 0.40f;
    if (mv >= 3600) return 0.25f;
    if (mv >= 3500) return 0.12f;
    if (mv >= 3300) return 0.04f;
    return 0.00f;
}

static int16_t min_cell_mV(const int16_t cell_mV[6])
{
    int16_t min = cell_mV[0];

    for (int i = 1; i < 6; i++) {
        if (cell_mV[i] < min) {
            min = cell_mV[i];
        }
    }

    return min;
}

void soc_initialize_from_voltage(soc_estimator_t *e, const bms_sample_t *s)
{
    int16_t lowest_mV = min_cell_mV(s->cell_mV);

    float initial_soc = soc_from_ocv_mV(lowest_mV);

    e->soc = initial_soc;
    e->remaining_mAh = e->soc * e->capacity_mAh;
    e->last_update_ms = s->timestamp_ms;
    e->initialized = true;
}

bool bms_read_sample(bms_sample_t *s)
{
    if (s == NULL) {
        return false;
    }

    memset(s, 0, sizeof(*s));
    // s->timestamp_ms = sys_get_ms();

    if (!bms_read_ts_temp(TS1_TEMP, &s->temp_dC[0])) return false;
    if (!bms_read_ts_temp(TS2_TEMP, &s->temp_dC[1])) return false;
    if (!bms_read_ts_temp(TS3_TEMP, &s->temp_dC[2])) return false;

    int16_t current = 0;
    if (!bq_direct_command(CC2Current, (uint16_t *)&current, R)) {
        return false;
    }
    s->current_mA = current;

    uint8_t da_status5[32];
    if (!read_data_memory(DASTATUS5, da_status5, sizeof(da_status5))) {
        return false;
    }

    /*
     * Only keep this if you are certain about endian and offset.
     * DASTATUS5 raw CC2 counts are useful for diagnostics,
     * but I would not base the main SoC estimator on raw counts
     * unless you have verified the scaling.
     */
    s->cc2_raw_counts =
        ((int32_t)da_status5[27] << 24) |
        ((int32_t)da_status5[26] << 16) |
        ((int32_t)da_status5[25] << 8)  |
        ((int32_t)da_status5[24]);

    if (!read_cells_1to6(s->cell_mV)) {
        return false;
    }

    int32_t total = 0;
    for (int i = 0; i < BMS_CELL_COUNT; i++) {
        total += s->cell_mV[i];
    }
    s->pack_mV = total;

    s->valid = true;
    return true;
}

void soc_update_coulomb_counting(soc_estimator_t *e, const bms_sample_t *s)
{
    if (!e->initialized) {
        soc_initialize_from_voltage(e, s);
        return;
    }

    uint32_t now = s->timestamp_ms;
    uint32_t dt_ms = now - e->last_update_ms;
    e->last_update_ms = now;

    if (dt_ms == 0 || dt_ms > 10000) {
        return;
    }

    /*
     * Choose sign convention:
     * positive current = charging
     * negative current = discharging
     */
    int32_t corrected_current_mA = s->current_mA - e->current_offset_mA;

    float dt_h = (float)dt_ms / 3600000.0f;
    float delta_mAh = corrected_current_mA * dt_h;

    e->remaining_mAh += delta_mAh;

    if (e->remaining_mAh > e->capacity_mAh) {
        e->remaining_mAh = e->capacity_mAh;
    }

    if (e->remaining_mAh < 0.0f) {
        e->remaining_mAh = 0.0f;
    }

    e->soc = e->remaining_mAh / e->capacity_mAh;
}

void rest_detector_update(rest_detector_t *r, const bms_sample_t *s)
{
    const int32_t relaxed_current_mA = 500;
    const uint32_t required_rest_ms = 10UL * 60UL * 1000UL;

    if (abs(s->current_mA) < relaxed_current_mA) {
        if (r->low_current_start_ms == 0) {
            r->low_current_start_ms = s->timestamp_ms;
        }

        if ((s->timestamp_ms - r->low_current_start_ms) > required_rest_ms) {
            r->relaxed = true;
        }
    } else {
        r->low_current_start_ms = 0;
        r->relaxed = false;
    }
}
void soc_correct_from_ocv_if_relaxed(soc_estimator_t *e,
                                     const bms_sample_t *s,
                                     bool relaxed)
{
    if (!relaxed) {
        return;
    }

    int16_t lowest_mV = min_cell_mV(s->cell_mV);
    float ocv_soc = soc_from_ocv_mV(lowest_mV);

    /*
     * Small correction only.
     * Do not jump the SoC unless you are very sure.
     */
    e->soc = 0.98f * e->soc + 0.02f * ocv_soc;
    e->remaining_mAh = e->soc * e->capacity_mAh;
}

bool bms_is_full_reference(const bms_sample_t *s)
{
    const int16_t full_cell_mV = 4180;
    const int32_t taper_current_mA = 500;

    if (s->current_mA < 0) {
        return false; // not charging, depending on your sign convention
    }

    if (abs(s->current_mA) > taper_current_mA) {
        return false;
    }

    for (int i = 0; i < BMS_CELL_COUNT; i++) {
        if (s->cell_mV[i] < full_cell_mV) {
            return false;
        }
    }

    return true;
}

static int16_t min_temp_dC(const int16_t temp_dC[3])
{
    int16_t min = temp_dC[0];

    for (int i = 1; i < 3; i++) {
        if (temp_dC[i] < min) {
            min = temp_dC[i];
        }
    }

    return min;
}

float capacity_temp_factor(int16_t min_temp_dC)
{
    float temp_C = min_temp_dC / 10.0f;

    if (temp_C < 0.0f) {
        return 0.75f;
    }

    if (temp_C < 10.0f) {
        return 0.90f;
    }

    return 1.0f;
}

float soc_get_usable(const soc_estimator_t *e, const bms_sample_t *s)
{
    int16_t coldest_dC = min_temp_dC(s->temp_dC);
    float factor = capacity_temp_factor(coldest_dC);

    float usable_remaining = e->remaining_mAh * factor;
    return usable_remaining / e->capacity_mAh;
}

void soc_apply_full_reference(soc_estimator_t *e, const bms_sample_t *s)
{
    if (bms_is_full_reference(s)) {
        e->remaining_mAh = e->capacity_mAh;
        e->soc = 1.0f;
        e->full_reference_seen = true;
    }
}


bool bms_sample_plausible(const bms_sample_t *s)
{
    for (int i = 0; i < BMS_CELL_COUNT; i++) {
        if (s->cell_mV[i] < 2000 || s->cell_mV[i] > 4300) {
            return false;
        }
    }

    for (int i = 0; i < BMS_TEMP_COUNT; i++) {
        if (s->temp_dC[i] < -400 || s->temp_dC[i] > 1000) {
            return false;
        }
    }

    if (abs(s->current_mA) > 200000) {
        return false;
    }

    return true;
}

void WDT_Enable( void )
{
    /* Checking if Always On Bit is Enabled */
    if((WDT_REGS->WDT_CTRLA & WDT_CTRLA_ALWAYSON_Msk) != WDT_CTRLA_ALWAYSON_Msk)
    {
        /* Enable Watchdog Timer */
        WDT_REGS->WDT_CTRLA |= WDT_CTRLA_ENABLE_Msk;

        /* Wait for synchronization */
        while(WDT_REGS->WDT_SYNCBUSY);
    }
}


void WDT_Clear( void )
{
    if ((WDT_REGS->WDT_SYNCBUSY & WDT_SYNCBUSY_CLEAR_Msk) != WDT_SYNCBUSY_CLEAR_Msk)
    {
        /* Clear WDT and reset the WDT timer before the
        timeout occurs */
        WDT_REGS->WDT_CLEAR = WDT_CLEAR_CLEAR_KEY;
    }
}

void bms_task(void)
{
    bms_sample_t sample;

    if (!bms_read_sample(&sample)) {
        return;
    }

    if (!bms_sample_plausible(&sample)) {
        return;
    }

    rest_detector_update(&rest, &sample);

    soc_update_coulomb_counting(&soc, &sample);
    soc_correct_from_ocv_if_relaxed(&soc, &sample, rest.relaxed);
    soc_apply_full_reference(&soc, &sample);

    float nominal_soc = soc.soc;
    float usable_soc = soc_get_usable(&soc, &sample);

    // bms_publish_soc(nominal_soc, usable_soc, soc.remaining_mAh);
}
