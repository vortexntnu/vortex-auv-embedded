#ifndef MS5837_H
#define MS5837_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MS5837_ADDR                0x76U
#define MS5837_RESET               0x1EU
#define MS5837_ADC_READ            0x00U
#define MS5837_PROM_READ           0xA0U
#define MS5837_CONVERT_D1_8192     0x4AU
#define MS5837_CONVERT_D2_8192     0x5AU

#define MS5837_MODEL_30BA          0U
#define MS5837_MODEL_02BA          1U
#define MS5837_MODEL_UNRECOGNISED  255U

#define MS5837_PA                  100.0f
#define MS5837_BAR                 0.001f
#define MS5837_MBAR                1.0f

typedef struct
{
    uint16_t C[8];          // PROM calibration words
    uint32_t D1_pres;       // raw pressure ADC
    uint32_t D2_temp;       // raw temperature ADC

    int32_t TEMP;           // temperature in hundredths of deg C
    int32_t P;              // pressure in model-specific internal units

    uint8_t model;
    float fluidDensity;
    bool initialized;
} MS5837_t;

/* User/platform function:
 * Replace with your Harmony delay method if desired.
 */
void MS5837_DelayMs(uint32_t ms);

bool MS5837_Init(MS5837_t *dev);
void MS5837_SetModel(MS5837_t *dev, uint8_t model);
uint8_t MS5837_GetModel(const MS5837_t *dev);
void MS5837_SetFluidDensity(MS5837_t *dev, float density);

bool MS5837_Read(MS5837_t *dev);
float MS5837_Pressure(const MS5837_t *dev, float conversion);
float MS5837_Temperature(const MS5837_t *dev);
float MS5837_Depth(const MS5837_t *dev);
float MS5837_Altitude(const MS5837_t *dev);

#ifdef __cplusplus
}
#endif

#endif /* MS5837_H */
