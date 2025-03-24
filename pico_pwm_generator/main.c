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
                sleep_ms(80);
            }
            i2c_data.i2c_state = 1;
            break;
        }
        // Idle state: led constantly on
        case 1: {
            gpio_put(PICO_DEFAULT_LED_PIN, true);
            sleep_ms(100);
            break;
        }
        // I2C message received: rapid blink sequence
        case 2: {
            for(uint8_t i = 0; i < 10; i++) {
                LED_STATE = !LED_STATE;
                gpio_put(PICO_DEFAULT_LED_PIN, LED_STATE);
                sleep_ms(30);
            }
            i2c_data.i2c_state = 1;
            break;
        }
        // I2C handler triggered but it went to default state
        case 3: {
            for(uint8_t i = 0; i < 10; i++) {
                LED_STATE = !LED_STATE;
                gpio_put(PICO_DEFAULT_LED_PIN, LED_STATE);
                sleep_ms(60);
                LED_STATE = !LED_STATE;
                gpio_put(PICO_DEFAULT_LED_PIN, LED_STATE);
                sleep_ms(30);
            }
            i2c_data.i2c_state = 1;
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
    esc_set_pwm(ESC1, ARMING_PWM);
    esc_set_pwm(ESC2, ARMING_PWM);
    esc_set_pwm(ESC3, ARMING_PWM);
    esc_set_pwm(ESC4, ARMING_PWM);
    esc_set_pwm(ESC5, ARMING_PWM);
    esc_set_pwm(ESC6, ARMING_PWM);
    esc_set_pwm(ESC7, ARMING_PWM);
    esc_set_pwm(ESC8, ARMING_PWM);
}