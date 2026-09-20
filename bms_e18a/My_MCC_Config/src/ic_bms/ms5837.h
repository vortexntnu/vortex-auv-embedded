#ifndef MS5837_H
#define MS5837_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#define MS5837_OSR_256   (0x00U)
#define MS5837_OSR_512   (0x02U)
#define MS5837_OSR_1024  (0x04U)
#define MS5837_OSR_2048  (0x06U)
#define MS5837_OSR_4096  (0x08U)

struct ms5837_t
{
    uint8_t osr_code;
    uint16_t C[8];
    uint32_t D1;
    uint32_t D2;
    float temp_C;
    float press_kPa;
    bool has_fresh_sample;
};

int8_t ms5837_init(struct ms5837_t *s);
bool ms5837_read_sample(struct ms5837_t *s);
void ms5837_task(struct ms5837_t *s);

#ifdef __cplusplus
}
#endif

#endif /* MS5837_H */
