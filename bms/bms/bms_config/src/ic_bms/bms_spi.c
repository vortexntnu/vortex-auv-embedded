#include <stdint.h>
#include "peripheral/sercom/spi_master/plib_sercom_spi_master_common.h"
#include "peripheral/sercom/spi_master/plib_sercom0_spi_master.h"
#include "peripheral/sercom/spi_master/plib_sercom_spi_master_common.h"
#include "definitions.h"





#define BQ_CS_GROUP   (0U)          
#define BQ_CS_MASK    (1UL << 18)   

static inline void BQ_CS_Low(void)  { PORT_REGS->GROUP[BQ_CS_GROUP].PORT_OUTCLR = BQ_CS_MASK; }
static inline void BQ_CS_High(void) { PORT_REGS->GROUP[BQ_CS_GROUP].PORT_OUTSET = BQ_CS_MASK; }


//SPI + set CS high 
void BQ76942_Init(void)
{
    // init SPI 
    SERCOM0_SPI_Initialize();

    // configure CS pin as output and set HIGH (inactive) 
    PORT_REGS->GROUP[BQ_CS_GROUP].PORT_DIRSET = BQ_CS_MASK;
    PORT_REGS->GROUP[BQ_CS_GROUP].PORT_OUTSET = BQ_CS_MASK;
}

bool Spi_2B(uint8_t tx0, uint8_t tx1, uint8_t *rx0, uint8_t *rx1)
{
    uint8_t tx[2] = { tx0, tx1 };
    uint8_t rx[2] = { 0, 0 };

    
    //wait while prev transfer is ongoing

    if (SERCOM0_SPI_IsBusy())
        return false;

    BQ_CS_Low();
    bool ok = SERCOM0_SPI_WriteRead(tx, 2U, rx, 2U);
    BQ_CS_High();

    if (!ok) 
        return false;

    if (rx0) *rx0 = rx[0];
    if (rx1) *rx1 = rx[1];

    return true;
}

bool WriteReg(uint8_t regAddr, uint8_t value)
{
    /* First byte: MSB=1 to indicate WRITE, lower 7 bits = register address */
    uint8_t tx[2];
    tx[0] = (uint8_t)(0x80 | (regAddr & 0x7F));  /* W flag + address */
    tx[1] = value;

    /* (optional) ensure no active transfer */
    if (SERCOM0_SPI_IsBusy())
        return false;

    /* CS low -> transfer -> CS high */
    PORT_REGS->GROUP[BQ_CS_GROUP].PORT_OUTCLR = BQ_CS_MASK;
    bool ok = SERCOM0_SPI_Write(tx, 2U);  
    PORT_REGS->GROUP[BQ_CS_GROUP].PORT_OUTSET = BQ_CS_MASK;

    return ok;
}






