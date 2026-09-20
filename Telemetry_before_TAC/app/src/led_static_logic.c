#include "led_static_logic.h"
#include "led_facade.h"

// LED mapping:
// LED 0 = PI WiFi
// LED 1 = ORIN WiFi
// LED 2 = Temperature
// LED 3 = Pressure
// LED 4 = Software mode

#define LED_PI_INDEX          (0u)
#define LED_ORIN_INDEX        (1u)
#define LED_TEMP_INDEX        (2u)
#define LED_PRESSURE_INDEX    (3u)
#define LED_SOFTWARE_INDEX    (4u)

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

static const rgb_t C_OFF = {0, 0, 0};
static const rgb_t C_RED = {255, 0, 0};
static const rgb_t C_YEL = {200, 140, 0};
static const rgb_t C_GRN = {0, 120, 0};
static const rgb_t C_BLU = {0, 0, 90};

static bool s_pi_wifi_connected = false;
static bool s_orin_wifi_connected = false;
static bool s_temperature_ok = false;
static bool s_pressure_ok = false;
static led_static_software_mode_t s_software_mode = LED_STATIC_SW_KILLSWITCH;

static rgb_t software_mode_color(led_static_software_mode_t mode)
{
    switch (mode)
    {
        case LED_STATIC_SW_AUTONOMOUS:
            return C_BLU;

        case LED_STATIC_SW_MANUAL:
            return C_YEL;

        case LED_STATIC_SW_REFERENCE:
            return C_GRN;

        case LED_STATIC_SW_KILLSWITCH:
        default:
            return C_RED;
    }
}

static void set_led_rgb(uint8_t idx, rgb_t c)
{
    led_set(idx, c.r, c.g, c.b);
}

static void refresh_all_leds(void)
{
    set_led_rgb(
        LED_PI_INDEX,
        s_pi_wifi_connected ? C_BLU : C_YEL
    );

    set_led_rgb(
        LED_ORIN_INDEX,
        s_orin_wifi_connected ? C_BLU : C_YEL
    );

    set_led_rgb(
        LED_TEMP_INDEX,
        s_temperature_ok ? C_GRN : C_RED
    );

    set_led_rgb(
        LED_PRESSURE_INDEX,
        s_pressure_ok ? C_GRN : C_RED
    );

    set_led_rgb(
        LED_SOFTWARE_INDEX,
        software_mode_color(s_software_mode)
    );

    led_commit_async();
}

void led_static_logic_init(void)
{
    /*
     * Startup defaults:
     * LED0 PI        = WiFi off/disconnected = yellow
     * LED1 ORIN      = WiFi off/disconnected = yellow
     * LED2 Temp      = alarm = red
     * LED3 Pressure  = alarm = red
     * LED4 Software  = killswitch = red
     */
    s_pi_wifi_connected = false;
    s_orin_wifi_connected = false;
    s_temperature_ok = false;
    s_pressure_ok = false;
    s_software_mode = LED_STATIC_SW_KILLSWITCH;

    refresh_all_leds();
}

void led_static_set_pi_wifi(bool connected)
{
    s_pi_wifi_connected = connected;
    refresh_all_leds();
}

void led_static_set_orin_wifi(bool connected)
{
    s_orin_wifi_connected = connected;
    refresh_all_leds();
}

void led_static_set_temperature_ok(bool ok)
{
    s_temperature_ok = ok;
    refresh_all_leds();
}

void led_static_set_pressure_ok(bool ok)
{
    s_pressure_ok = ok;
    refresh_all_leds();
}

void led_static_set_software_mode(led_static_software_mode_t mode)
{
    switch (mode)
    {
        case LED_STATIC_SW_AUTONOMOUS:
        case LED_STATIC_SW_MANUAL:
        case LED_STATIC_SW_REFERENCE:
        case LED_STATIC_SW_KILLSWITCH:
            s_software_mode = mode;
            break;

        default:
            s_software_mode = LED_STATIC_SW_KILLSWITCH;
            break;
    }

    refresh_all_leds();
}