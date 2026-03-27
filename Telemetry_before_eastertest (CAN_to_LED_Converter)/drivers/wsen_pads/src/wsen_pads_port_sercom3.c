#include <stdbool.h>
#include <stdint.h>
#include "definitions.h"

#define WSEN_PADS_ADDR 0x5D  // SAO = 1 (0x5C if SAO = 0)

// Register addresses
#define REG_DEVICE_ID 0x0F
#define REG_CTRL_1 0x10
#define REG_CTRL_3 0x12
#define REG_STATUS 0x27
#define REG_DATA_P_XL 0x28
#define REG_DATA_T_L 0x2B

#define EXPECTED_DEVICE_ID 0xB3

typedef enum {
    WSEN_IDLE = 0,
    WSEN_START,  // initiate read of 5 bytes (pressure + temp)
    WSEN_WAIT,   // waiting for callback (when the WriteRead is finished)
    WSEN_DONE,
    WSEN_ERROR
} WSEN_STATE;

struct wsen_cycle {
    volatile WSEN_STATE state;
    volatile bool done;

    volatile float last_pressure;
    volatile float last_temp;

    uint8_t read_buf[5];
};

static struct wsen_cycle cycle;

/* static bool read_measurements(uint8_t* reg,
                              uint8_t* buf,
                              uint8_t len,
                              bool is_read) {
    if (is_read) {
        return SERCOM3_SPI_WriteRead(&reg, 1, cycle.read_buf, 5);
    }
} */
static void wsen_cs_set(bool on) {
    if (on) {
        PORT_REGS->GROUP[0].PORT_OUTSET = (1 << 7);  // Set PA7 high (CS for WSEN PADS on)
    } else {
        PORT_REGS->GROUP[0].PORT_OUTCLR = (1 << 7);  // Set PA7 low
    }
}

/**
 * @brief Initializes WSEN PADS --> sets output data rate, enables the block
 * data update feature and enables data ready interrupts.
 * @return -1 on failure to write to wsen-pads registers, 0 otherwise.
 */
int wsen_init(void) {
    uint8_t buf[2];

    // Set sampling rate
    uint8_t ctrl1 = 0x72;  // ODR 200Hz and BDU high
    buf[0] = REG_CTRL_1;
    buf[1] = ctrl1;

    wsen_cs_set(true);
    if (!SERCOM3_SPI_Write(buf, 2)) {
        wsen_cs_set(false);
        return -1;
    };
    // Enable data ready interrupts
    uint8_t ctrl3 = 0x04;  // DRDY = 1, INT_S = 00
    buf[0] = REG_CTRL_3;
    buf[1] = ctrl3;

    wsen_cs_set(true);
    if (!SERCOM3_SPI_Write(buf, 2)) {
        wsen_cs_set(false);
        return -1;
    };
    wsen_cs_set(false);
    return 0;
}

/**
 * @brief Can be used to check successful connection with wsen-pads.
 * @return -1 on SPI failure, -2 on unexpected device id, and 0 on success.
 */
int wsen_check_device_id(void) {
    wsen_cs_set(true);
    uint8_t reg = REG_DEVICE_ID | 0x80; // MSB 1 for read
    uint8_t device_id = 0;
    if (!SERCOM3_SPI_WriteRead(&reg, 1, &device_id, 1)) {
        wsen_cs_set(false);
        return -1;
    };

    wsen_cs_set(false);
    if (device_id == EXPECTED_DEVICE_ID) {
        return 0;
    } else {
        return -2;
    }
}

// Helper function that reads the 5 wsen-pads registers containing the pressure
// and temperature data.
static bool read_measurements() {
    uint8_t reg = REG_DATA_P_XL | 0x80; // MSB is 1 for read
    wsen_cs_set(true);
    bool result = SERCOM3_SPI_WriteRead(&reg, 1, cycle.read_buf, 5);
    return result;
}

static void sercom3_spi_cb(uintptr_t context) {
    (void)context;
    
    wsen_cs_set(false);

    switch (cycle.state) {
        case WSEN_WAIT: {
            int32_t raw_p =
                (int32_t)((cycle.read_buf[2] << 16) | (cycle.read_buf[1] << 8) |
                          cycle.read_buf[0]);
            if (raw_p & 0x00800000)
                raw_p |= 0xFF000000;
            cycle.last_pressure = (float)raw_p / 40960.0f;

            int16_t raw_t =
                (int16_t)((cycle.read_buf[4] << 8) | cycle.read_buf[3]);
            cycle.last_temp = (float)raw_t * 0.01f;  // 0.01 °C per LSB

            cycle.state = WSEN_DONE;
            cycle.done = true;
            break;
        }

        default:
            cycle.state = WSEN_ERROR;
            cycle.done = true;
            break;
    }
}

void spi_init(void) {
    //SERCOM3_SPI_CallbackRegister(sercom3_spi_cb, 0);
    cycle.state = WSEN_IDLE;
    cycle.done = false;
}

void wsen_cycle_start(void) {
    if (cycle.state != WSEN_IDLE && cycle.state != WSEN_DONE &&
        cycle.state != WSEN_ERROR) {
        return;  // already running
    }
    cycle.done = false;
    cycle.state = WSEN_START;

    if (!read_measurements()) {
        // If SPI was busy, we'll retry from main loop by calling
        // wsen_cycle_tick()
        cycle.state = WSEN_START;
    } else {
        cycle.state = WSEN_WAIT;
    }
}

/**
 * @brief Used in the main loop to try reading measurements from the WSEN PADS
 * if the measurement cycle is in the WSEN_START state.
 */
void wsen_cycle_tick(void) {
    if (cycle.state == WSEN_START) {
        if (read_measurements()) {
            cycle.state = WSEN_WAIT;
        }
    }
}

/**
 * @brief Check whether the latest measurement cycle completed successfully and
 * obtain the last readings.
 *
 * @param[out] kPa   Pointer to a float that will receive the last measured
 * absolute pressure in kilopascals (kPa). Must be a valid, writable pointer
 * when the function returns true.
 * @param[out] degC  Pointer to a float that will receive the last measured
 * temperature in degrees Celsius (°C). Must be a valid, writable pointer when
 * the function returns true.
 * @return false if a measurement cycle is not done or if the driver is in an
 * error state, true otherwise.
 */
bool wsen_cycle_done_ok(float* kPa, float* degC) {
    if (!cycle.done || cycle.state == WSEN_ERROR)
        return false;
    *kPa = cycle.last_pressure;
    *degC = cycle.last_temp;
    return true;
}

bool wsen_cycle_failed(void) {
    if (cycle.state == WSEN_ERROR) {
        return true;
    }
    return false;
};

/**
 * @brief Reset the WSEN PADS driver cycle to its initial, idle state.
 *
 * @note This routine modifies global driver state. Call it to abort an ongoing
 * cycle, to reinitialize the driver after an error, or before starting a fresh
 * transaction.
 */
void wsen_reset(void) {
    cycle.state = WSEN_IDLE;
    cycle.done = false;
}

static void drdy_isr(uintptr_t context) {
    (void)context;
    wsen_cycle_start();
}

/**
 * @brief Configures the interrupt pin from the WSEN PADS which generates
 * interrupts when pressure data is ready. The callback function will start a
 * new measurement cycle.
 */
void drdy_init(void) {
    PORT_PinPeripheralFunctionConfig(PORT_PIN_PA18, PERIPHERAL_FUNCTION_A);

    EIC_CallbackRegister(EIC_PIN_2, drdy_isr, 0);
    EIC_InterruptEnable(EIC_PIN_2);
}

// This function uses polling to check if the pressure data is ready before
// reading. It is currently not used in the code as we use the interrupt pin
// instead.
int polling_read_pressure(float* pressure) {
    uint8_t status = 0;
    uint8_t rawData[3];
    int32_t rawPressure;

    uint8_t reg = REG_STATUS;
    // Wait until new pressure data available (P_DA bit = 1)
    do {
        wsen_cs_set(true);
        SERCOM3_SPI_WriteRead(&reg, 1, &status, 1);
        wsen_cs_set(false);
    } while (!(status & 0x01));

    // Read the 3 pressure registers (XL, L, H)
    reg = REG_DATA_P_XL;
        wsen_cs_set(true);
    if (!SERCOM3_SPI_WriteRead(&reg, 1, rawData, 3)) {
        wsen_cs_set(false);
        return -1;
    };

    // Combine into signed 24-bit value
    rawPressure =
        (int32_t)((rawData[2] << 16) | (rawData[1] << 8) | rawData[0]);
    if (rawPressure & 0x00800000) {  // Sign extend negative numbers
        rawPressure |= 0xFF000000;
    }

    // Convert to kPa using sensitivity 1/40960 kPa/digit
    *pressure = (float)rawPressure / 40960.0f;

    return 0;
}

// This function uses polling to check if the temperature data is ready before
// reading. It is currently not used in the code as we use the interrupt pin
// instead.
int read_temp(float* temp) {
    uint8_t status = 0;
    uint8_t rawData[2];
    int16_t rawTemp;

    // Wait until new temperature data available (T_DA bit = 1)
    uint8_t reg = REG_STATUS;
    do {
        SERCOM3_SPI_WriteRead(&reg, 1, &status, 1);
    } while (!(status & 0x02));

    reg = REG_DATA_T_L;
    // Read 2 temperature bytes: L, H
    if (!SERCOM3_SPI_WriteRead(&reg, 1, rawData, 2)) {
        return -1;
    };

    // Combine into signed 16-bit value
    rawTemp = (int16_t)((rawData[1] << 8) | rawData[0]);

    // Convert to °C (0.01 °C per digit)
    *temp = (float)rawTemp * 0.01f;

    return 0;
}
