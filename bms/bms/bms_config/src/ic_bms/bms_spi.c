#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <sys/types.h>
#include "peripheral/sercom/spi_master/plib_sercom_spi_master_common.h"
#include "peripheral/sercom/spi_master/plib_sercom0_spi_master.h"
#include "definitions.h"
#include "bms_spi.h"




static inline void bq_cs_low(void)  { PORT_REGS->GROUP[bq_cs_group].PORT_OUTCLR = bq_cs_mask; }
static inline void bq_cs_high(void) { PORT_REGS->GROUP[bq_cs_group].PORT_OUTSET = bq_cs_mask; }

static inline void _delay(uint32_t cycles){

    for (volatile uint32_t i=0; i<cycles; i++);
}
    

//SPI + set CS high 
void bq76942_init(void)
{
    // init SPI 
    SERCOM0_SPI_Initialize();

    // configure CS pin as output and set HIGH (inactive) 
    PORT_REGS->GROUP[bq_cs_group].PORT_DIRSET = bq_cs_mask;
    PORT_REGS->GROUP[bq_cs_group].PORT_OUTSET = bq_cs_mask;
}

bool Spi_TransferBytes(uint8_t *tx, uint8_t *rx, uint8_t length)
{
    if (SERCOM0_SPI_IsBusy())
        return false;

    bq_cs_low();
    bool ok = SERCOM0_SPI_WriteRead(tx, length, rx, length);
    bq_cs_high();
    
    return ok;
}

bool write_reg(uint8_t regAddr, const uint8_t *data, uint8_t length)
{
    

    if (SERCOM0_SPI_IsBusy())
        return false;

    uint8_t tx[length + 1];
    tx[0] = 0x80 | (regAddr & 0x7F);  // Write flag + address
    memcpy(&tx[1], data, length); //(memcpy (dest, src, length))
    
    bq_cs_low();
    bool ok = SERCOM0_SPI_Write(tx,length +1);
    bq_cs_high();

    return ok;
}


bool read_reg(uint8_t regAddr, uint8_t *data, uint8_t length)
{
    
    if (SERCOM0_SPI_IsBusy())
        return false;

    uint8_t tx[length + 1];
    uint8_t rx[length + 1];
    tx[0] = (regAddr & 0x7F);
    memset(&tx[1], 0x00, length);

    bq_cs_low();
    bool ok = SERCOM0_SPI_WriteRead(tx, length+1, rx, length+1);
    bq_cs_high();

    if (ok)
        memcpy(&tx[1],0x00,length);
    return ok;

}

bool bq_direct_read(uint8_t command, uint8_t *data, uint8_t count)
{
   return read_reg(command,data,count);
}

bool bq_direct_write(uint8_t command, const uint8_t *data, uint8_t count)
{
    return write_reg(command, data, count);
}

bool bq_direct_command(uint8_t command, uint16_t *data, char type)
{
    uint8_t buf[2];
    bool ok = true;

    if (type == 'W') // Write
    {
        buf[0] = (*data) & 0xFF;         // LSB first
        buf[1] = (*data >> 8) & 0xFF;    // MSB
        ok = bq_direct_write(command, buf, 2);
    }
    else if (type == 'R') // Read
    {
        ok = bq_direct_read(command, buf, 2);
        if (ok)
            *data = (uint16_t)(buf[0] | (buf[1] << 8)); // Little endian
    }

    
    return ok;
}


bool bq_command_only(uint16_t subcmd){


     // Send 0x3E/0x3F in one SPI frame (lead’s request)
     uint8_t two[2] = {
        (uint8_t)(subcmd & 0xFF), 
        (uint8_t)((subcmd >> 8) & 0xFF)
    };
    if (!write_reg(0x3E, two, 2))
        return false;
    
    return true;
}

/*
    example of BQ_commandOnly usage:

    Reset the BQ76942
    BQ_CommandOnly(RESET);
*/


bool bq_read_sub_command(uint16_t subcmd, uint8_t *data, uint8_t length)
{
    if (length > 32)
        length = 32;

    // Write subcommand (0x3E LSB, 0x3F MSB) in one frame
    uint8_t sub[2] = { (uint8_t)(subcmd & 0xFF), (uint8_t)((subcmd >> 8) & 0xFF) };
    if (!write_reg(0x3E, sub, 2))
        return false;

    // Poll for echo of 0x3E/0x3F (in as few frames as possible)
    uint8_t echo[2] = {0xFF, 0xFF};
    uint32_t tries = 0;
    do {
        if (!read_reg(0x3E, echo, 2))
            return false;
        if (++tries > BQ_SUBCMD_MAX_POLLS)
            return false;
    } while ((echo[0] == 0xFF && echo[1] == 0xFF) || echo[0] != sub[0] || echo[1] != sub[1]);

    // Read full buffer 0x40..0x61 (34 bytes) in one frame
    uint8_t buf[34];
    if (!read_reg(0x40, buf, sizeof(buf)))
        return false;

    uint8_t len_total = buf[0x61 - 0x40];
    if (len_total < 4)  // must include 0x3E,0x3F,0x60,0x61 => 4 minimum
        return false;

    uint8_t buf_len = (uint8_t)(len_total - 4);
    if (buf_len > 32)
        buf_len = 32;

    // Copy out payload (0x40.. as the buffer start)
    uint8_t to_copy = (length < buf_len) ? length : buf_len;
    memcpy(data, buf, to_copy);

    // Verify checksum
    uint8_t sum = (uint8_t)(sub[0] + sub[1]);
    for (uint8_t i = 0; i < buf_len; i++)
        sum = (uint8_t)(sum + buf[i]);

    uint8_t ck_calc = (uint8_t)(0xFF - (sum & 0xFF));
    uint8_t ck_read = buf[0x60 - 0x40];

    return (ck_read == ck_calc);
}

bool bq_write_subcommand(uint16_t subcmd, const uint8_t *data, uint8_t length)
{
    if (length > 32)
        return false;

    // Subcommand in one frame
    uint8_t sub[2] = { (uint8_t)(subcmd & 0xFF), (uint8_t)((subcmd >> 8) & 0xFF) };
    if (!write_reg(0x3E, sub, 2))
        return false;

    // Write payload 0x40.. in one frame
    if (length > 0 && !write_reg(0x40, data, length))
        return false;

    // Write checksum+length (0x60, 0x61) in one frame
    uint8_t sum = (uint8_t)(sub[0] + sub[1]);
    for (uint8_t i = 0; i < length; i++)
        sum = (uint8_t)(sum + data[i]);

    //verify checksum
    uint8_t tail[2];
    tail[0] = (uint8_t)(0xFF - (sum & 0xFF));   
    tail[1] = (uint8_t)(4 + length);            
    if (!write_reg(0x60, tail, 2))
        return false;

    return true;
}


   
void bms_set_protection_thresholds(void)
{
    
    bq_command_only(ENTER_CONFIG_UPDATE);


    uint8_t cov_val = (uint8_t)(COV_THRESHOLD_MV / 50.6f + 0.5f);
    if (cov_val < 20)  cov_val = 20;
    if (cov_val > 110) cov_val = 110;
    bq_write_subcommand(COV_THRESHOLD_ADDR, &cov_val, 1);

    uint16_t cov_delay_ticks = (uint16_t)(COV_DELAY_MS / 3.3f + 0.5f);
    uint8_t cov_delay_bytes[2] = {
        (uint8_t)(cov_delay_ticks & 0xFF),
        (uint8_t)((cov_delay_ticks >> 8) & 0xFF)
    };
    bq_write_subcommand(COV_DELAY_ADDR, cov_delay_bytes, 2);

    
    uint8_t cuv_val = (uint8_t)(CUV_THRESHOLD_MV / 50.6f + 0.5f);
    if (cuv_val < 20)  cuv_val = 20;
    if (cuv_val > 110) cuv_val = 110;
    bq_write_subcommand(CUV_THRESHOLD_ADDR, &cuv_val, 1);

    uint16_t cuv_delay_ticks = (uint16_t)(CUV_DELAY_MS / 3.3f + 0.5f);
    uint8_t cuv_delay_bytes[2] = {
        (uint8_t)(cuv_delay_ticks & 0xFF),
        (uint8_t)((cuv_delay_ticks >> 8) & 0xFF)
    };
    bq_write_subcommand(CUV_DELAY_ADDR, cuv_delay_bytes, 2);


    bq_command_only(EXIT_CONFIG_UPDATE);
}

void bms_battery_status(void){

    uint8_t fetReg=0;

    if(!bq_direct_read(FET_STATUS, &fetReg, 1)){
        printf("Failed to read FET status\n");
        return;
    }

    bool chg_on  = (fetReg & (1 << 0));  // CHG_FET bit
    bool pchg_on = (fetReg & (1 << 1));  // PCHG_FET bit
    bool dsg_on  = (fetReg & (1 << 2));  // DSG_FET bit


    if (pchg_on)
        printf("Battery in precharge mode\n");  //maybe not printf
    else if (chg_on && !dsg_on) 
        printf("Battery is charging\n");
    else if (dsg_on && !chg_on)
        printf("Battery is discharging\n");
    else if (!chg_on && !dsg_on)
        printf("Battery is idle\n");
    else
        printf("Both CHG_FET and DSG_FET ACTIVE (transition)\n");



}

void read_cells_1to6(){

    const uint8_t CellVoltageAddr[6]={CELL_1_VOLTAGE, CELL_2_VOLTAGE, CELL_3_VOLTAGE, CELL_4_VOLTAGE, CELL_5_VOLTAGE, CELL_6_VOLTAGE};
    uint16_t raw = 0;
    float voltage = 0.0f;
    uint8_t i = 0;
  
    for (i=0; i<6; i++){
      if (bq_direct_command(CellVoltageAddr[i], &raw , R))
      {
        voltage = raw*0.001f; // Convert mV to V
        printf("Cell %u Voltage: %.3f V\n", i+1, voltage);
      }
      else {
      {
        printf("Failed to read Cell %u Voltage\n", i+1);  
      }
      }
  
  
    }
  
  }
  
    



