#include "led_logic.h"
#include "led_facade.h"
#include <string.h>

// ------------ Simple RGB ------------
typedef struct { uint8_t r,g,b; } rgb_t;
static const rgb_t C_OFF = {0,0,0};
static const rgb_t C_RED = {255,0,0};
static const rgb_t C_YEL = {200,140,0};
static const rgb_t C_WHT = {160,160,160};
static const rgb_t C_GRN = {0,120,0};
static const rgb_t C_BLU = {0,0,90};

static inline uint8_t mask4(uint8_t v) { return (uint8_t)(v & 0x0Fu); }

// ------------ Fault table ------------
typedef struct {
    bool           active;
    led_severity_t sev;
    uint8_t        subsystem_id4;
    bool           has_detail;
    uint8_t        detail_bits;
} fault_t;

static fault_t s_faults[LED_LOGIC_MAX_FAULTS];

// ------------ Native indicators ------------
static bool s_pressure_ok = false;
static bool s_orin_wifi   = false;
static bool s_pi_wifi     = false;

// physical LED indices for b3,b2,b1,b0
static uint8_t s_led_index_for_bit[4] = {3,2,1,0};

// ------------ Frame sequencing ------------
typedef enum { FRAME_NATIVE=0, FRAME_SUBSYS=1, FRAME_DETAIL=2 } frame_type_t;
typedef struct { frame_type_t type; led_severity_t sev; uint8_t mask4; } frame_t;

// Base was: 1 + max_faults*2
// Extra needed now:
// - subsystem 3 (MCU watchdog group): up to 4 detail frames instead of 1 => +3
// - subsystem 8 (thrusters): up to 8 detail frames instead of 1 => +7
#ifndef LED_LOGIC_MAX_FRAMES
#define LED_LOGIC_MAX_FRAMES (1u + LED_LOGIC_MAX_FAULTS*2u + 10u)
#endif

static frame_t s_frames[LED_LOGIC_MAX_FRAMES];
static uint8_t s_frame_count = 0;
static uint8_t s_frame_index = 0;

// ------------ Phase timing ------------
typedef enum { PHASE_OFF=0, PHASE_HOLD=1 } phase_t;
static phase_t  s_phase = PHASE_HOLD;
static uint32_t s_phase_started_ms = 0;
static bool     s_tick_initialized = false;

// One dirty flag to trigger render+commit when driver is ready
static bool s_output_dirty = true;

// ------------ Helpers ------------
bool led_logic_has_any_faults(void)
{
    for (unsigned i=0;i<LED_LOGIC_MAX_FAULTS;i++) {
        if (s_faults[i].active) return true;
    }
    return false;
}

static void sort_faults_by_id(void)
{
    for (unsigned i=0;i<LED_LOGIC_MAX_FAULTS;i++) {
        for (unsigned j=i+1;j<LED_LOGIC_MAX_FAULTS;j++) {
            if (!s_faults[i].active && s_faults[j].active) {
                fault_t t=s_faults[i]; s_faults[i]=s_faults[j]; s_faults[j]=t;
            } else if (s_faults[i].active && s_faults[j].active &&
                       s_faults[j].subsystem_id4 < s_faults[i].subsystem_id4) {
                fault_t t=s_faults[i]; s_faults[i]=s_faults[j]; s_faults[j]=t;
            }
        }
    }
}

static void append_detail_frame(uint8_t value4)
{
    if (s_frame_count < LED_LOGIC_MAX_FRAMES) {
        s_frames[s_frame_count++] = (frame_t){
            .type  = FRAME_DETAIL,
            .sev   = LED_SEV_WARN,
            .mask4 = mask4(value4)
        };
    }
}

static void append_indexed_detail_frames_from_bits(uint8_t detail_bits, uint8_t bit_count)
{
    for (uint8_t bit = 0u; bit < bit_count; bit++) {
        if ((detail_bits & (uint8_t)(1u << bit)) != 0u) {
            append_detail_frame((uint8_t)(bit + 1u));
        }
    }
}

static void build_frames(void)
{
    s_frame_count = 0;

    // Native frame always first
    s_frames[s_frame_count++] = (frame_t){ .type=FRAME_NATIVE, .sev=LED_SEV_WARN, .mask4=0 };

    for (unsigned i=0;i<LED_LOGIC_MAX_FAULTS;i++) {
        if (!s_faults[i].active) continue;

        if (s_frame_count < LED_LOGIC_MAX_FRAMES) {
            s_frames[s_frame_count++] = (frame_t){
                .type=FRAME_SUBSYS,
                .sev=s_faults[i].sev,
                .mask4=mask4(s_faults[i].subsystem_id4)
            };
        }

        if (!s_faults[i].has_detail) continue;

        // Subsystem 3 = MCU_POWER:
        // detail_bits is an active-set bitmap for details 1..4.
        if (s_faults[i].subsystem_id4 == 3u) {
            append_indexed_detail_frames_from_bits(s_faults[i].detail_bits, 4u);
            continue;
        }

        // Subsystem 8 = THRUSTERS:
        // detail_bits is an active-set bitmap for thrusters 1..8.
        // Each active thruster is shown one by one as binary 1..8 on the 4 LEDs.
        if (s_faults[i].subsystem_id4 == 8u) {
            append_indexed_detail_frames_from_bits(s_faults[i].detail_bits, 8u);
            continue;
        }

        // All other subsystems keep previous behavior:
        // one white detail frame using the low 4 bits directly.
        append_detail_frame(s_faults[i].detail_bits);
    }

    if (s_frame_count == 0u) {
        s_frame_count = 1u;
        s_frames[0] = (frame_t){ .type=FRAME_NATIVE, .sev=LED_SEV_WARN, .mask4=0 };
    }

    if (s_frame_index >= s_frame_count) s_frame_index = 0;
}

static void set_led(uint8_t idx, rgb_t c) { led_set(idx, c.r,c.g,c.b); }

static void show_all_off(void)
{
    for (unsigned k=0;k<4;k++) set_led(s_led_index_for_bit[k], C_OFF);
}

static void show_native(void)
{
    const bool any_faults = led_logic_has_any_faults();

    // b3 = overall status
    set_led(s_led_index_for_bit[0], any_faults ? C_RED : C_GRN);

    // b2 = pressure ok
    set_led(s_led_index_for_bit[1], s_pressure_ok ? C_GRN : C_RED);

    // b1 = ORIN WiFi
    set_led(s_led_index_for_bit[2], s_orin_wifi ? C_BLU : C_YEL);

    // b0 = PI WiFi
    set_led(s_led_index_for_bit[3], s_pi_wifi ? C_BLU : C_YEL);
}

static void show_mask(rgb_t on, uint8_t m4)
{
    for (unsigned k=0;k<4;k++) {
        const uint8_t bitpos = (uint8_t)(3u - k); // b3..b0
        const bool on_bit = ((m4 >> bitpos) & 1u) != 0u;
        set_led(s_led_index_for_bit[k], on_bit ? on : C_OFF);
    }
}

static void render_current_frame_to_led_buffer(void)
{
    build_frames();

    if (s_phase == PHASE_OFF) {
        show_all_off();
        return;
    }

    const frame_t *f = &s_frames[s_frame_index];
    switch (f->type) {
        case FRAME_NATIVE:
            show_native();
            break;
        case FRAME_SUBSYS: {
            const rgb_t c = (f->sev == LED_SEV_FAULT) ? C_RED : C_YEL;
            show_mask(c, f->mask4);
        } break;
        case FRAME_DETAIL:
            show_mask(C_WHT, f->mask4);
            break;
        default:
            show_all_off();
            break;
    }
}

static void request_redraw(void)
{
    s_output_dirty = true;
}

static void commit_if_ready(void)
{
    if (!s_output_dirty) return;
    if (led_busy()) return;

    render_current_frame_to_led_buffer();

    if (led_commit_async()) {
        s_output_dirty = false;
    }
}

static int find_fault(uint8_t subsystem_id4)
{
    for (unsigned i=0;i<LED_LOGIC_MAX_FAULTS;i++) {
        if (s_faults[i].active && s_faults[i].subsystem_id4 == subsystem_id4) return (int)i;
    }
    return -1;
}

static int find_free_slot(void)
{
    for (unsigned i=0;i<LED_LOGIC_MAX_FAULTS;i++) if (!s_faults[i].active) return (int)i;
    return -1;
}

// ------------ Public API ------------
void led_logic_init(void)
{
    memset(s_faults, 0, sizeof(s_faults));

    s_pressure_ok = false;
    s_orin_wifi   = false;
    s_pi_wifi     = false;

    s_led_index_for_bit[0]=3;
    s_led_index_for_bit[1]=2;
    s_led_index_for_bit[2]=1;
    s_led_index_for_bit[3]=0;

    s_phase = PHASE_HOLD;
    s_phase_started_ms = 0;
    s_frame_index = 0;

    s_tick_initialized = false;
    s_output_dirty     = true;

    // Fail-safe: mark all subsystems as FAULT until cleared
    //for (uint8_t id=1; id <= (uint8_t)LED_LOGIC_NUM_SUBSYSTEMS; id++) {
    for (uint8_t id=1; id <= (uint8_t)LED_LOGIC_NUM_SUBSYSTEMS-6; id++) {
        const unsigned slot = (unsigned)(id-1u);
        if (slot >= LED_LOGIC_MAX_FAULTS) break;
        s_faults[slot].active = true;
        s_faults[slot].sev = LED_SEV_FAULT;
        s_faults[slot].subsystem_id4 = mask4(id);
        s_faults[slot].has_detail = false;
        s_faults[slot].detail_bits = 0u;
    }

    sort_faults_by_id();
    request_redraw();
    commit_if_ready();
}

void led_logic_set_pressure_ok(bool ok)      { s_pressure_ok = ok; request_redraw(); }
void led_logic_set_orin_wifi(bool connected) { s_orin_wifi = connected; request_redraw(); }
void led_logic_set_pi_wifi(bool connected)   { s_pi_wifi = connected; request_redraw(); }

void led_logic_set_subsystem(led_severity_t sev,
                             uint8_t subsystem_id_4bit,
                             bool has_detail,
                             uint8_t detail_mask_4bit)
{
    const uint8_t id4 = mask4(subsystem_id_4bit);

    int idx = find_fault(id4);
    if (idx < 0) idx = find_free_slot();
    if (idx < 0) return;

    s_faults[idx].active = true;
    s_faults[idx].sev = sev;
    s_faults[idx].subsystem_id4 = id4;
    s_faults[idx].has_detail = has_detail;
    s_faults[idx].detail_bits = detail_mask_4bit;

    sort_faults_by_id();
    request_redraw();
}

void led_logic_clear_subsystem(uint8_t subsystem_id_4bit)
{
    const uint8_t id4 = mask4(subsystem_id_4bit);
    int idx = find_fault(id4);
    if (idx >= 0) {
        s_faults[idx].active = false;
        sort_faults_by_id();
        request_redraw();
    }
}

void led_logic_clear_detail(uint8_t subsystem_id_4bit)
{
    const uint8_t id4 = mask4(subsystem_id_4bit);
    int idx = find_fault(id4);
    if (idx >= 0) {
        s_faults[idx].has_detail = false;
        s_faults[idx].detail_bits = 0u;
        request_redraw();
    }
}

void led_logic_clear_all(void)
{
    memset(s_faults, 0, sizeof(s_faults));
    sort_faults_by_id();
    request_redraw();
}

void led_logic_tick(uint32_t now_ms)
{
    if (!s_tick_initialized) {
        s_tick_initialized = true;
        s_phase_started_ms = now_ms;
        request_redraw();
        commit_if_ready();
        return;
    }

    // attempt to flush pending redraw when driver becomes ready
    commit_if_ready();

    const uint32_t elapsed = (uint32_t)(now_ms - s_phase_started_ms);
    const uint32_t dur = (s_phase == PHASE_OFF) ? LED_LOGIC_OFF_MS : LED_LOGIC_HOLD_MS;
    if (elapsed < dur) return;

    // phase boundary
    s_phase_started_ms = now_ms;

    if (s_phase == PHASE_OFF) {
        s_phase = PHASE_HOLD;
        request_redraw();
    } else {
        s_phase = PHASE_OFF;

        // advance frame at end of HOLD -> OFF transition
        build_frames();
        s_frame_index++;
        if (s_frame_index >= s_frame_count) s_frame_index = 0;

        request_redraw();
    }

    commit_if_ready();
}