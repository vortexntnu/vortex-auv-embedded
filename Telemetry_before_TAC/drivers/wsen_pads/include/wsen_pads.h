#ifndef WSEN_PADS_H
#define WSEN_PADS_H

#include <stdint.h>
#include <stdbool.h>

/* 7-bit I2C-adresse (WSEN-PADS, SAO=0). Endre til 0x5D hvis SAO=1. */
#ifndef WSEN_PADS_I2C_ADDR
#define WSEN_PADS_I2C_ADDR   0x5C
#endif

/* Registeradresser (utdrag brukt av driveren) */
#define PADS_REG_WHO_AM_I    0x0F  /* DEVICE_ID = 0xB3 */
#define PADS_REG_CTRL_1      0x10
#define PADS_REG_CTRL_2      0x11
#define PADS_REG_STATUS      0x27
#define PADS_REG_DATA_P_XL   0x28  /* XL, L, H (24-bit) */
#define PADS_REG_DATA_P_L    0x29
#define PADS_REG_DATA_P_H    0x2A
#define PADS_REG_DATA_T_L    0x2B  /* L, H (16-bit) */
#define PADS_REG_DATA_T_H    0x2C

/* CTRL_1 bits */
#define PADS_CTRL1_ODR_Pos   4
#define PADS_CTRL1_BDU       (1u<<1)

/* CTRL_2 bits */
#define PADS_CTRL2_ONE_SHOT   (1u<<0)
#define PADS_CTRL2_LOW_NOISE  (1u<<1)
#define PADS_CTRL2_SWRESET    (1u<<2)
#define PADS_CTRL2_IF_ADD_INC (1u<<4)

/* STATUS bits */
#define PADS_STATUS_P_DA     (1u<<0)
#define PADS_STATUS_T_DA     (1u<<1)

/* Forventet WHO_AM_I/DEVICE_ID */
#define PADS_WHO_AM_I_VALUE  0xB3

/* ODR valg (ODR[2:0] i CTRL_1[6:4]) */
typedef enum {
    PADS_ODR_POWER_DOWN = 0, /* 000 */
    PADS_ODR_1_HZ       = 1, /* 001 */
    PADS_ODR_10_HZ      = 2, /* 010 */
    PADS_ODR_25_HZ      = 3, /* 011 */
    PADS_ODR_50_HZ      = 4, /* 100 */
    PADS_ODR_75_HZ      = 5, /* 101 */
    PADS_ODR_100_HZ     = 6, /* 110 */
    PADS_ODR_200_HZ     = 7  /* 111 */
} pads_odr_t;

/* Konstanter for konvertering (må matche emulatoren din):
   - Trykk: 1/40960 kPa per LSB => kPa = raw / 40960.0
   - Temp : 100 LSB/°C          => °C  = raw / 100.0
*/
#define PADS_PRESS_LSB_PER_kPa   (40960.0f)
#define PADS_TEMP_LSB_PER_C      (100.0f)

/* Resultatstruktur for en burst-lesning */
typedef struct {
    int32_t raw_pressure;  /* 24-bit 2’s complement i 32-bit beholder */
    int16_t raw_temperature;
    float   pressure_kPa;
    float   temperature_C;
    uint8_t status;
} pads_sample_t;

/* Offentlige APIer */
bool PADS_Init(void);                            /* Les WHO_AM_I, sett IF_ADD_INC=1 */
bool PADS_SoftwareReset(void);                   /* CTRL_2.SWRESET */
bool PADS_SetODR(pads_odr_t odr);                /* CTRL_1[6:4] */
bool PADS_SetBDU(bool enable);                   /* CTRL_1.BDU */
bool PADS_OneShot(void);                         /* CTRL_2.ONE_SHOT=1 */
bool PADS_ReadStatus(uint8_t *status);           /* STATUS */
bool PADS_ReadRawPressure(int32_t *raw);         /* 24-bit -> sign-extend */
bool PADS_ReadRawTemperature(int16_t *raw);      /* 16-bit signed */
bool PADS_ReadPressure_kPa(float *kPa);          /* konvertert */
bool PADS_ReadTemperature_C(float *degC);        /* konvertert */
bool PADS_ReadAll(pads_sample_t *out);           /* burst 0x28..0x2C + STATUS */

/* Små hjelpefunksjoner (inline) */
static inline float PADS_RawPressure_to_kPa(int32_t raw24)
{
    return ((float)raw24) / PADS_PRESS_LSB_PER_kPa;
}
static inline float PADS_RawTemp_to_C(int16_t raw16)
{
    return ((float)raw16) / PADS_TEMP_LSB_PER_C;
}

#endif /* WSEN_PADS_H */
