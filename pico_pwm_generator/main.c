#if !defined(PULLUP_ENABLE) && !defined(PULLUP_DISABLE)
    #define PULLUP_ENABLE
    //#define (PULLUP_DISABLE)
#endif // PULLUP_ENABLE

#include <stdio.h>
#include "pico/stdlib.h"
#include "i2c.h"
#include "pwm.h"

// Global LED state variable (PICO_DEFAULT_LED_PIN is usually 25)
bool LED_STATE = false;

void setup_ESCs();
void arm_thrusters();
void led_sequence();

int main() {
    // Initialize the onboard LED.
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    // Initialize the I2C shared data state.
    i2c_data.i2c_state = 0;
    led_sequence();
    
    // Initialize PWM for ESC control.
    esc_init();
    if (esc_clock_auto() < 0) {
        return -1;
    }
    
    // Attach all ESC channels and send the arming signal.
    setup_ESCs();
    arm_thrusters();
    
    // Wait for the startup delay.
    sleep_ms(STARTUP_DELAY);
    
    // Set up I2C communication.
    i2c_setup();
    
    // Main loop: blink the LED.
    while (true) {
        led_sequence();
    }
    
    return 0;
}

void led_sequence() {
    switch(i2c_data.i2c_state) {
        // Startup sequence: blink fast for 20 iterations.
        case 0: {
            for(uint8_t i = 0; i < 20; i++) {
                gpio_put(PICO_DEFAULT_LED_PIN, LED_STATE);
                LED_STATE = !LED_STATE;
                sleep_ms(100);
            }
            i2c_data.i2c_state = 1;
            break;
        }
        // Idle state: slow blinking (toggle every 500 ms)
        case 1: {
            gpio_put(PICO_DEFAULT_LED_PIN, LED_STATE);
            LED_STATE = !LED_STATE;
            sleep_ms(500);
            break;
        }
        // I2C message received: rapid blink sequence
        case 2: {
            for(uint8_t i = 0; i < 5; i++) {
                LED_STATE = !LED_STATE;
                gpio_put(PICO_DEFAULT_LED_PIN, LED_STATE);
                sleep_ms(20);
            }
            i2c_data.i2c_state = 1;
            break;
        }
        default: {
            i2c_data.i2c_state = 1;
            break;
        }
    }
}

// Attach all ESC channels to their corresponding PWM slices.
void setup_ESCs() {
    esc_attach(ESC1);
    esc_attach(ESC2);
    esc_attach(ESC3);
    esc_attach(ESC4);
    esc_attach(ESC5);
    esc_attach(ESC6);
    esc_attach(ESC7);
    esc_attach(ESC8);
}

// Arm thrusters by sending the ARMING_PWM signal to each ESC.
void arm_thrusters() {
    esc_us(ESC1, ARMING_PWM);
    esc_us(ESC2, ARMING_PWM);
    esc_us(ESC3, ARMING_PWM);
    esc_us(ESC4, ARMING_PWM);
    esc_us(ESC5, ARMING_PWM);
    esc_us(ESC6, ARMING_PWM);
    esc_us(ESC7, ARMING_PWM);
    esc_us(ESC8, ARMING_PWM);
}
