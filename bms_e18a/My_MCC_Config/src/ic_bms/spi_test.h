#ifndef SPI_TEST_H
#define SPI_TEST_H

#include <stdint.h>

void voltage_test_init(void);
void spi_write_probe_step(void);
uint8_t voltage_test_step(void);
void can_scope_test_init(void);
void can_scope_test_step(void);

#endif /* SPI_TEST_H */
