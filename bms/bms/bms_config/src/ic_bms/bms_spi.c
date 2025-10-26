#include <stdint.h>
#include <stdbool.h>
#include <sys/types.h>
#include "peripheral/sercom/spi_master/plib_sercom_spi_master_common.h"
#include "peripheral/sercom/spi_master/plib_sercom0_spi_master.h"
#include "peripheral/sercom/spi_master/plib_sercom_spi_master_common.h"
#include "definitions.h"
#include "bms_spi.h"





#define BQ_CS_GROUP   (0U)          
#define BQ_CS_MASK    (1UL << 18)   

#define R 0 // Read; Used in DirectCommands and Subcommands functions
#define W 1 // Write; Used in DirectCommands and Subcommands functions


static inline void BQ_CS_Low(void)  { PORT_REGS->GROUP[BQ_CS_GROUP].PORT_OUTCLR = BQ_CS_MASK; }
static inline void BQ_CS_High(void) { PORT_REGS->GROUP[BQ_CS_GROUP].PORT_OUTSET = BQ_CS_MASK; }

static inline void _delay(uint32_t cycles){

    for (volatile uint32_t i=0; i<cycles; i++);
}

//SPI + set CS high 
void BQ76942_Init(void)
{
    // init SPI 
    SERCOM0_SPI_Initialize();

    // configure CS pin as output and set HIGH (inactive) 
    PORT_REGS->GROUP[BQ_CS_GROUP].PORT_DIRSET = BQ_CS_MASK;
    PORT_REGS->GROUP[BQ_CS_GROUP].PORT_OUTSET = BQ_CS_MASK;
}

bool Spi_TransferBytes(uint8_t *tx, uint8_t *rx, uint8_t length)
{
    if (SERCOM0_SPI_IsBusy())
        return false;

    BQ_CS_Low();
    bool ok = SERCOM0_SPI_WriteRead(tx, length, rx, length);
    BQ_CS_High();

    _delay(100);
    

    return ok;
}




bool WriteReg(uint8_t regAddr, uint8_t value)
{
    /* First byte: MSB=1 to indicate WRITE, lower 7 bits = register address */
    uint8_t tx[2];
    tx[0] = (uint8_t)(0x80 | (regAddr & 0x7F));  /* W flag + address */
    tx[1] = value;

    if (SERCOM0_SPI_IsBusy())
        return false;

    BQ_CS_Low();
    bool ok = SERCOM0_SPI_Write(tx, 2U);
    BQ_CS_High();

    _delay(2000);

    return ok;
}


bool ReadReg(uint8_t regAddr, uint8_t *value)
{
    uint8_t tx[2] = { (regAddr & 0x7F), 0x00 };
    uint8_t rx[2] = { 0, 0 };

    if (SERCOM0_SPI_IsBusy())
        return false;

    BQ_CS_Low();
    bool ok = SERCOM0_SPI_WriteRead(tx, 2U, rx, 2U);
    BQ_CS_High();

    if (ok)
        *value = rx[1];

    _delay(2000);
    return ok;
}

bool BQ_DirectRead(uint8_t command, uint8_t *data, uint8_t count)
{
    bool ok = true;

    for (uint8_t i = 0; i < count; i++)
    {
        if (!ReadReg(command + i, &data[i]))
        {
            ok = false;
            break;
        }
        _delay(2000); // short pause between bytes
    }

    return ok;
}

bool BQ_DirectWrite(uint8_t command, const uint8_t *data, uint8_t count)
{
    bool ok = true;

    for (uint8_t i = 0; i < count; i++)
    {
        if (!WriteReg(command + i, data[i]))
        {
            ok = false;
            break;
        }
        _delay(2000); // short pause between bytes
    }

    return ok;
}


bool BQ_DirectCommand(uint8_t command, uint16_t *data, char type)
{
    uint8_t buf[2];
    bool ok = true;

    if (type == 'W') // Write
    {
        buf[0] = (*data) & 0xFF;         // LSB first
        buf[1] = (*data >> 8) & 0xFF;    // MSB
        ok = BQ_DirectWrite(command, buf, 2);
    }
    else if (type == 'R') // Read
    {
        ok = BQ_DirectRead(command, buf, 2);
        *data = (uint16_t)(buf[0] | (buf[1] << 8)); // Little endian
    }

    _delay(2000); // short pause between frames (~few hundred µs)
    return ok;
}

bool BQ_CommandOnly(uint16_t subcmd){


    uint8_t lsb=(uint8_t)(subcmd & 0xFF);
    uint8_t msb=(uint8_t)((subcmd >> 8) & 0xFF);

    if (!WriteReg(0x3E, lsb))
        return false;
    if (!WriteReg(0x3F, msb))
        return false;
    _delay(20000);

}
/*
    example of BQ_commandOnly usage:

    Reset the BQ76942
    BQ_CommandOnly(RESET);
*/
bool BQ_ReadSubCommand(uint16_t subcmd, uint8_t *data, uint8_t length)
{
    uint8_t lsb = (uint8_t)(subcmd & 0xFF);
    uint8_t msb = (uint8_t)((subcmd >> 8) & 0xFF);
    uint8_t check_lsb=0, check_msb=0;
    bool ok = true;


    if (!WriteReg(0x3E, lsb))
        return false;
    if (!WriteReg(0x3F, msb))
        return false;


    do{
        ReadReg(0x3E, &check_lsb);
        ReadReg(0x3F, &check_msb);
    }while ((check_lsb != lsb) || (check_msb != msb));
    
    for (uint8_t i=0; i<length; i++){

        if(!ReadReg(0x40+i,&data[i]))
        {
            ok=false;
            break;
        }
    }
    _delay(2000);
    return ok;
}

bool BQ_WriteSubCommand(uint16_t subcmd, const uint8_t *data, uint8_t length)
{
    if (length>32)
        return false;

    uint8_t lsb=(uint8_t)(subcmd & 0xFF);
    uint8_t msb=(uint8_t)((subcmd >> 8) & 0xFF);
    uint8_t checksum=0;
    uint16_t sum = 0;

    if (!WriteReg(0x3E, lsb))
        return false;
    if (!WriteReg(0x3F, msb))
        return false;
    
    for (uint8_t i=0; i<length; i++){
        if (!WriteReg(0x40+i,data[i]))
            return false;
    }

    sum = lsb + msb;
    for (uint8_t i=0; i<length; i++){
        sum += data[i];
    checksum=(uint8_t)(0xff -(sum & 0xFF));

    if (!WriteReg(0x60, checksum))
        return false;
    if(!WriteReg(0x61, length))
        return false;
    _delay(20000);

    return true;

}
    



