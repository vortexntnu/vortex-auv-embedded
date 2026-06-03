#ifndef MPRLS_PRESSURE_H
#define MPRLS_PRESSURE_H

#ifdef __cplusplus
extern "C" {
#endif

#define MPRLS_ADDRESS 0x18
#define MPRLS_REG 0xAA

#define COUNTS_MIN 0x066666
#define COUNTS_MAX 0x399999
#define PRESSURE_MIN 0
#define PRESSURE_MAX 250

/**
 * @brief Sends an i2c command to the pressure sensor to start measurement. 
 * Call at least 5ms before read_pressure().
 * @return -1 if failure, 0 otherwise
 */
int start_measurement(void);

/**
 * @brief Reads the pressure off the mprls sensor. start_measurement() 
 * must be called before this function, with a delay between of at least 5ms.
 * @param[out] pressure Pointer to a double that will receive the pressure in hPa
 * @return -1 if failure, 0 otherwise
 */
int read_pressure(double* pressure);

#ifdef __cplusplus
}
#endif

#endif