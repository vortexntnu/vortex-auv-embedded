#include "wsen_pads.h"
#include "plib_sercom2_i2c_master.h"
#include <string.h>

/* Timeout for blokkering (enkle polle-sløyfer) */
#ifndef PADS_I2C_TIMEOUT_LOOPS
#define PADS_I2C_TIMEOUT_LOOPS  (100000UL)
#endif

static uint8_t devAddr = WSEN_PADS_I2C_ADDR;

/* --- Lokale hjelpere: blokkerende wrappers over PLIB --- */
static bool i2c_wait_done_with_timeout(void)
{
    uint32_t loops = 0;
    while (SERCOM2_I2C_IsBusy())
    {
        if (++loops >= PADS_I2C_TIMEOUT_LOOPS)
            return false;
    }
    return (SERCOM2_I2C_ErrorGet() == SERCOM_I2C_ERROR_NONE);
}

static bool i2c_write_reg(uint8_t reg, const uint8_t *data, uint32_t len)
{
    uint8_t buf[1+16];
    if (len > 16) return false; /* enkelt vern */
    buf[0] = reg;
    if (len) memcpy(&buf[1], data, len);

    if (!SERCOM2_I2C_Write(devAddr, buf, (uint32_t)(1+len))) return false;
    if (!i2c_wait_done_with_timeout()) return false;
    return true;
}

static bool i2c_read_reg(uint8_t reg, uint8_t *data, uint32_t len)
{
    if (!SERCOM2_I2C_WriteRead(devAddr, &reg, 1, data, len)) return false;
    if (!i2c_wait_done_with_timeout()) return false;
    return true;
}

/* --- Bit-funksjoner for CTRL-registre --- */
static bool read_u8(uint8_t reg, uint8_t *val)
{
    return i2c_read_reg(reg, val, 1);
}

static bool write_u8(uint8_t reg, uint8_t val)
{
    return i2c_write_reg(reg, &val, 1);
}

static bool update_bits(uint8_t reg, uint8_t mask, uint8_t val)
{
    uint8_t r;
    if (!read_u8(reg, &r)) return false;
    r = (uint8_t)((r & ~mask) | (val & mask));
    return write_u8(reg, r);
}

/* --- Sign-extension for 24-bit 2's complement --- */
static int32_t sign_extend_24(uint32_t v24)
{
    if (v24 & 0x00800000UL) /* bit 23 */
        v24 |= 0xFF000000UL;
    return (int32_t)v24;
}

/* --- Offentlig API --- */
bool PADS_Init(void)
{
    uint8_t who = 0;
    if (!PADS_SoftwareReset()) return false;

    /* Les WHO_AM_I */
    if (!i2c_read_reg(PADS_REG_WHO_AM_I, &who, 1)) return false;
    if (who != PADS_WHO_AM_I_VALUE) return false;

    /* Sett IF_ADD_INC=1 i CTRL_2 for auto-inkrement ved burst */
    if (!update_bits(PADS_REG_CTRL_2, PADS_CTRL2_IF_ADD_INC, PADS_CTRL2_IF_ADD_INC))
        return false;

    /* Default: power-down, BDU=0 */
    (void)PADS_SetBDU(false);
    (void)PADS_SetODR(PADS_ODR_POWER_DOWN);

    return true;
}

bool PADS_SoftwareReset(void)
{
    /* Skriv SWRESET=1; mange sensorer trenger liten ventetid,
       men her bruker vi bare en liten “poll” på WHO_AM_I etterpå
       via PADS_Init, så vi gjør kun skriveoperasjonen. */
    if (!update_bits(PADS_REG_CTRL_2, PADS_CTRL2_SWRESET, PADS_CTRL2_SWRESET))
        return false;

    /* Ingen eksplisitt poll; neste aksess vil avdekke om den svarer. */
    return true;
}

bool PADS_SetODR(pads_odr_t odr)
{
    uint8_t val = (uint8_t)((odr & 0x07) << PADS_CTRL1_ODR_Pos);
    return update_bits(PADS_REG_CTRL_1, (uint8_t)(0x07u << PADS_CTRL1_ODR_Pos), val);
}

bool PADS_SetBDU(bool enable)
{
    return update_bits(PADS_REG_CTRL_1, PADS_CTRL1_BDU, (enable ? PADS_CTRL1_BDU : 0u));
}

bool PADS_OneShot(void)
{
    /* ONE_SHOT bit clears av hardware etter “conversion start” */
    return update_bits(PADS_REG_CTRL_2, PADS_CTRL2_ONE_SHOT, PADS_CTRL2_ONE_SHOT);
}

bool PADS_ReadStatus(uint8_t *status)
{
    if (!status) return false;
    return i2c_read_reg(PADS_REG_STATUS, status, 1);
}

bool PADS_ReadRawPressure(int32_t *raw)
{
    if (!raw) return false;
    uint8_t buf[3];
    if (!i2c_read_reg(PADS_REG_DATA_P_XL, buf, 3)) return false;
    uint32_t u = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16);
    *raw = sign_extend_24(u);
    return true;
}

bool PADS_ReadRawTemperature(int16_t *raw)
{
    if (!raw) return false;
    uint8_t buf[2];
    if (!i2c_read_reg(PADS_REG_DATA_T_L, buf, 2)) return false;
    *raw = (int16_t)((uint16_t)buf[0] | ((uint16_t)buf[1] << 8));
    return true;
}

bool PADS_ReadPressure_kPa(float *kPa)
{
    if (!kPa) return false;
    int32_t r;
    if (!PADS_ReadRawPressure(&r)) return false;
    *kPa = PADS_RawPressure_to_kPa(r);
    return true;
}

bool PADS_ReadTemperature_C(float *degC)
{
    if (!degC) return false;
    int16_t t;
    if (!PADS_ReadRawTemperature(&t)) return false;
    *degC = PADS_RawTemp_to_C(t);
    return true;
}

bool PADS_ReadAll(pads_sample_t *out)
{
    if (!out) return false;

    /* Burst-les fra P_XL (0x28) til T_H (0x2C), totalt 5 byte,
       men vi leser 5 og så henter STATUS separat (eller les STATUS først om du vil).
       Mange velger 6+ byte og inkluderer STATUS; for enkelhet tar vi to aksesser. */
    uint8_t d[5];
    if (!i2c_read_reg(PADS_REG_DATA_P_XL, d, sizeof d)) return false;

    uint32_t p24 = (uint32_t)d[0] | ((uint32_t)d[1] << 8) | ((uint32_t)d[2] << 16);
    int32_t  rp  = sign_extend_24(p24);
    int16_t  rt  = (int16_t)((uint16_t)d[3] | ((uint16_t)d[4] << 8));

    out->raw_pressure   = rp;
    out->raw_temperature= rt;
    out->pressure_kPa   = PADS_RawPressure_to_kPa(rp);
    out->temperature_C  = PADS_RawTemp_to_C(rt);

    /* Les STATUS til slutt (rydder P_DA/T_DA hvis BDU=1 og alle bytes er hentet) */
    uint8_t st = 0;
    if (!PADS_ReadStatus(&st)) return false;
    out->status = st;

    return true;
}
