#ifndef MS5837_H
#define MS5837_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint16_t C[8];
    uint32_t D1_pres;
    uint32_t D2_temp;
    int32_t TEMP;
    int32_t P;
    uint8_t model;
    float fluidDensity;
    bool initialized;
} MS5837_t;

enum
{
    MS5837_30BA = 0,
    MS5837_02BA = 1,
    MS5837_UNRECOGNISED = 255
};

#define MS5837_PA    100.0f
#define MS5837_BAR   0.001f
#define MS5837_MBAR  1.0f

bool MS5837_Init(MS5837_t *dev);
bool MS5837_Begin(MS5837_t *dev);

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
