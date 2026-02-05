#ifndef MS5837_H
#define MS5837_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

typedef enum
{
    MS5837_STATE_IDLE = 0,
    MS5837_STATE_START_D1,
    MS5837_STATE_WAIT_D1,
    MS5837_STATE_READ_D1,
    MS5837_STATE_START_D2,
    MS5837_STATE_WAIT_D2,
    MS5837_STATE_READ_D2,
    MS5837_STATE_COMPUTE,
    MS5837_STATE_ERROR
} ms5837_state_t;

struct ms5837_t
{
    ms5837_state_t state;

    // Config
    uint8_t osr_code;          // 0x00/0x02/0x04/0x06/0x08

    // I2C transaction tracking
    volatile bool i2c_done;
    volatile bool i2c_ok;

    // Scratch buffers
    uint8_t tx[1];
    uint8_t rx[3];

    // Raw ADC
    uint32_t D1;
    uint32_t D2;

    // Calibration PROM (filled elsewhere in init)
    uint16_t C[8];

    // Outputs
    float temp_C;
    float press_kPa;

    bool has_fresh_sample;

} ;

uint8_t ms5837_init(struct ms5837_t* s);
void ms5837_task(struct ms5837_t* s);


#ifdef __cplusplus
}
#endif

#endif