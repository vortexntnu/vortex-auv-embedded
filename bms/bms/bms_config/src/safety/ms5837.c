#include "peripheral/sercom/i2c_master/plib_sercom2_i2c_master.h"
#include <stdint.h>
#include "peripheral/tc/plib_tc0.h"
#include "peripheral/tc/plib_tc1.h"
#include "peripheral/tc/plib_tc_common.h"
#include "ms5837.h"

#define MS_ADDR 0x76

#define CMD_RESET 0x1E
#define CMD_PROM_READ 0xA0
#define CMD_ADC_READ  0x00
#define CMD_CONV_D1 0x40    // pressure conversion base
#define CMD_CONV_D2 0x50    // temp conversion base

//static uint16_t ms_data[7];

static volatile bool conversion_done = false;
static volatile bool rdy_to_sample = false;

static struct ms5837_t ms;

static void tc0_cb(TC_TIMER_STATUS status, uintptr_t ctx)
{
    conversion_done = true;
}

static void tc1_cb(TC_TIMER_STATUS status, uintptr_t ctx) {
    rdy_to_sample = true;
}

// reset sequence shall be sent once after power-on
static int reset_seq() {
    uint8_t cmd_reset = CMD_RESET;
    if (!SERCOM2_I2C_Write(MS_ADDR, &cmd_reset, 1)) {
        return -1;
    }
    return 0;
}

static uint8_t ms5837_crc4(uint16_t n_prom[])
{
    uint16_t n_rem = 0;
    n_prom[0] &= 0x0FFF;
    n_prom[7] = 0; // subsidiary word set to 0

    for (int cnt = 0; cnt < 16; cnt++) // performed on bytes
    {
        // choose MSB or LSB
        if (cnt & 1) n_rem ^= (uint16_t)(n_prom[cnt >> 1] & 0x00FF);
        else         n_rem ^= (uint16_t)(n_prom[cnt >> 1] >> 8);

        for (int n_bit = 0; n_bit < 8; n_bit++)
        {
            if (n_rem & 0x8000) n_rem = (n_rem << 1) ^ 0x3000;
            else                n_rem = (n_rem << 1);
        }
    }

    return (uint8_t)((n_rem >> 12) & 0x0F);
}


// read 2 bytes from 7 addresses
static int prom_read_seq(struct ms5837_t* s) {
    uint8_t cmd_prom_read = CMD_PROM_READ;
    for (int i=0; i<7; i++) {
        uint8_t rx[2];
        if (!SERCOM2_I2C_WriteRead(MS_ADDR, &cmd_prom_read, 1, rx, 2)) {
            return -1;
        }
        s->C[i] = (uint16_t)((rx[0] << 8) | rx[1]);
        cmd_prom_read = (uint8_t)(cmd_prom_read + 2);
    }
    s->C[7] = 0;
    return 0;
}

static int conversion_seq() {
    uint8_t D1 = 0x48;  // pressure with ODR = 4096
    if (!SERCOM2_I2C_Write(MS_ADDR, &D1, 1)) {
        return -1;
    }
    return 0;
}

static int check_crc(struct ms5837_t* s) {
    uint8_t crc_stored = (s->C[0] >> 12) & 0x0F;
    uint16_t prom_copy[7];
    for (int i = 0; i < 8; i++) prom_copy[i] = s->C[i];    
    uint8_t crc_calc = ms5837_crc4(prom_copy);

    if (crc_stored != crc_calc) {
        return -1;  // reset sensor and repeat prom read sequence if crc is not correct
    }
    return 0;
}



static void ms5837_i2c_cb(uintptr_t context)
{
    struct ms5837_t* s = (struct ms5837_t*)context;
    s->i2c_ok = (SERCOM2_I2C_ErrorGet() == SERCOM_I2C_ERROR_NONE);
    s->i2c_done = true;
}

static bool ms5837_write_cmd(struct ms5837_t* s, uint8_t cmd)
{
    s->i2c_done = false;
    s->i2c_ok = false;
    s->tx[0] = cmd;
    conversion_done = false;
    if (!SERCOM2_I2C_Write(MS_ADDR, s->tx, 1)) {
        return false;
    };
    TC0_TimerStart();
    return true;
}

static bool ms5837_read_adc24(struct ms5837_t* s)
{
    s->i2c_done = false;
    s->i2c_ok = false;
    s->tx[0] = CMD_ADC_READ;
    rdy_to_sample = false;
    return SERCOM2_I2C_WriteRead(MS_ADDR, s->tx, 1, s->rx, 3);
}

static uint32_t ms5837_unpack_adc24(const uint8_t rx[3])
{
    return ((uint32_t)rx[0] << 16) | ((uint32_t)rx[1] << 8) | rx[2];
}

void ms5837_compute(struct ms5837_t* s) {
    int32_t dT = s->D2 - ((int32_t)(s->C[5] << 8));
    int32_t TEMP = 2000 + (((int64_t)dT * s->C[6]) >> 23);

    int64_t OFF = ((int64_t)(s->C[2] << 17)) + ((int64_t)(s->C[4]*dT) >> 6);
    int64_t SENS = ((int64_t)(s->C[1] << 16)) + ((int64_t)(s->C[3]*dT) >> 7);
    int32_t P = (int32_t)(((((int64_t)s->D1 * SENS) >> 21) - OFF) >> 15);

    if (TEMP >= 2000) {
        s->temp_C = (float)TEMP * 0.01f;
        s->press_kPa = (float)P * 0.001f; // convert from 100 x mbar to kPa
    }
    else {
        int64_t Ti = 11 * (((int64_t)dT * (int64_t)dT) >> 35);
        int64_t diff = TEMP - 2000;
        int64_t OFFi = 31 * (diff * diff) >> 3;
        int64_t SENSi = 63 * (diff * diff) >> 5;

        int64_t OFF2 = OFF - OFFi;
        int64_t SENS2 = SENS - SENSi;

        int32_t TEMP2 = (TEMP - Ti) / 100;
        int32_t P2 = (((s->D1 * SENS2 >> 21) - OFF2) >> 15) / 100;

        s->temp_C = (float)TEMP2 * 0.01f;
        s->press_kPa = (float)P2 * 0.001f; // convert from 100 x mbar to kPa
    }
}


void ms5837_task(struct ms5837_t* s)
{

    switch (s->state)
    {
        case MS5837_STATE_IDLE:
            s->has_fresh_sample = false;

            // pace full samples
            if (!rdy_to_sample)
                return;

            s->state = MS5837_STATE_START_D1;
            break;

        case MS5837_STATE_START_D1:
        {
            uint8_t cmd = (uint8_t)(CMD_CONV_D1 + s->osr_code);
            if (!ms5837_write_cmd(s, cmd))
            {
                s->state = MS5837_STATE_ERROR;
                break;
            }
            s->state = MS5837_STATE_WAIT_D1;
            break;
        }

        case MS5837_STATE_WAIT_D1:
            // wait for I2C write completion first
            if (!s->i2c_done) return;
            if (!s->i2c_ok) { s->state = MS5837_STATE_ERROR; break; }

            // then wait conversion time
            if (!conversion_done) return;

            s->state = MS5837_STATE_READ_D1;
            break;

        case MS5837_STATE_READ_D1:
            if (!ms5837_read_adc24(s))
            {
                s->state = MS5837_STATE_ERROR;
                break;
            }
            s->state = MS5837_STATE_START_D2; // advance when read completes
            break;

        case MS5837_STATE_START_D2:
            if (!s->i2c_done) return;               // waiting for ADC read transaction
            if (!s->i2c_ok) { s->state = MS5837_STATE_ERROR; break; }

            s->D1 = ms5837_unpack_adc24(s->rx);

            // start temperature conversion
            if (!ms5837_write_cmd(s, (uint8_t)(CMD_CONV_D2 + s->osr_code)))
            {
                s->state = MS5837_STATE_ERROR;
                break;
            }
            s->state = MS5837_STATE_WAIT_D2;
            break;

        case MS5837_STATE_WAIT_D2:
            if (!s->i2c_done) return;
            if (!s->i2c_ok) { s->state = MS5837_STATE_ERROR; break; }

            if (!conversion_done) return;

            s->state = MS5837_STATE_READ_D2;
            break;

        case MS5837_STATE_READ_D2:
            if (!ms5837_read_adc24(s))
            {
                s->state = MS5837_STATE_ERROR;
                break;
            }
            s->state = MS5837_STATE_COMPUTE;
            break;

        case MS5837_STATE_COMPUTE:
            if (!s->i2c_done) return;
            if (!s->i2c_ok) { s->state = MS5837_STATE_ERROR; break; }

            s->D2 = ms5837_unpack_adc24(s->rx);

            // compute compensated values
            ms5837_compute(s);

            s->has_fresh_sample = true;

            s->state = MS5837_STATE_IDLE;
            break;

        case MS5837_STATE_ERROR:
            // simplest recovery: reset state and try next cycle
            // (better: count failures and issue sensor reset)
            rdy_to_sample = false;
            s->state = MS5837_STATE_IDLE;
            break;

        default:
            s->state = MS5837_STATE_ERROR;
            break;
    }
}

/**
 * @brief Initializes the ms5837 pressure and temperature sensor. 
 * @return -1 if reset sequence failed
 * @return -2 if prom read sequence failed
 * @return -3 if crc check failed
 * @return 0 if initialization succeded
 */
uint8_t ms5837_init(struct ms5837_t* s) {
    SERCOM2_I2C_CallbackRegister(ms5837_i2c_cb, (uintptr_t)&ms);

    if (reset_seq() != 0) {
        return -1;
    }
    if (prom_read_seq(s) != 0) {
        return -2;
    };
    if (check_crc(s) != 0) {
        return -3;
    };

    TC0_TimerCallbackRegister(tc0_cb, 0);
    TC1_TimerCallbackRegister(tc1_cb, 0);
    TC1_TimerStart();
    return 0;
}


