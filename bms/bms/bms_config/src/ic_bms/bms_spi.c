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

#define BQ_SPI_WAIT_MAX_LOOPS  (2000000UL)

static bool bq_spi_wait_idle(void)
{
    uint32_t timeout = BQ_SPI_WAIT_MAX_LOOPS;

    while (SERCOM0_SPI_IsBusy())
    {
        if (timeout-- == 0U)
        {
            return false;
        }
    }

    return true;
}
    

void bq76942_init(void)
{
    // --- Chip select pin setup ---
    PORT_REGS->GROUP[bq_cs_group].PORT_DIRSET = bq_cs_mask;
    PORT_REGS->GROUP[bq_cs_group].PORT_OUTSET = bq_cs_mask; // Set HIGH (inactive)

    // --- IC configuration sequence ---
    bq_command_only(ENTER_CONFIG_UPDATE);
    bms_set_protection_threshold();
    bq_command_only(EXIT_CONFIG_UPDATE);

    // Status check
    uint8_t status = 0;
    if (bq_direct_read(0x12, &status, 1))
        printf("BQ76942 communication OK\n");
    else
        printf("BQ76942 communication failed\n");
}


bool Spi_TransferBytes(uint8_t *tx, uint8_t *rx, uint8_t length)
{
    if (SERCOM0_SPI_IsBusy())
        return false;

    bq_cs_low();
    bool ok = SERCOM0_SPI_WriteRead(tx, length, rx, length);
    if (ok)
    {
        ok = bq_spi_wait_idle();
    }
    bq_cs_high();
    
    return ok;
}

static uint8_t bq_crc8_calc(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0x00;              // init

    for (uint8_t i = 0; i < len; i++) {
        
        crc ^= data[i];              // XOR in next byte
        for (uint8_t b = 0; b < 8; b++) {
            if (crc & 0x80)          // test MSB
                crc = (uint8_t)((crc << 1) ^ 0x07);
            else
                crc <<= 1;
        }
    }
    return crc;
}


bool write_reg(uint8_t regAddr, const uint8_t *data, uint8_t length)
{
    

    if (SERCOM0_SPI_IsBusy())
        return false;
    if (length == 0)
        return true;

    // Pack back-to-back 24-bit frames: [cmd, data_byte, crc] * length
    uint8_t tx_bytes[3 * length];

    for (uint8_t i=0;i<length; i++)
    {

        uint8_t cmd   = (uint8_t)(0x80 | ((regAddr + i) & 0x7F)); // WRITE + addr+i
        uint8_t byte  = data[i];

        // CRC is computed over the first two bytes only: [cmd, byte]
        uint8_t pair[2] = { cmd, byte };
        uint8_t crc     = bq_crc8_calc(pair, 2);

        // Store this mini-frame at position i
        tx_bytes[3*i + 0] = cmd;
        tx_bytes[3*i + 1] = byte;
        tx_bytes[3*i + 2] = crc;
        
    }

     // Single SPI transfer with CS held low across all frames
     bq_cs_low();
     bool ok = SERCOM0_SPI_Write(tx_bytes, sizeof tx_bytes);
     if (ok)
     {
         ok = bq_spi_wait_idle();
     }
     bq_cs_high();
 
    return ok;
}


bool read_reg(uint8_t regAddr, uint8_t *data, uint8_t length)
{
    if (SERCOM0_SPI_IsBusy()) 
        return false;
    if (length == 0)
        return true;

    const uint8_t frames = (uint8_t)(length + 1);
    uint8_t tx[3 * frames];
    uint8_t rx[3 * frames];

    // Frame 0: issue read for regAddr + 0 (second byte = dummy 0x00)
    {
        uint8_t pair[2] = { (uint8_t)(regAddr & 0x7F), 0x00 };
        tx[0] = pair[0]; tx[1] = pair[1]; tx[2] = bq_crc8_calc(pair, 2);
    }

    // Frames 1..(frames-2): issue reads for subsequent addresses; last is dummy flush
    for (uint8_t i = 1; i < frames; i++) {
        uint8_t cmd  = (i < frames - 1) ? (uint8_t)((regAddr + i) & 0x7F) : 0x00;
        uint8_t pair[2] = { cmd, 0x00 };
        tx[3*i + 0] = pair[0];
        tx[3*i + 1] = pair[1];
        tx[3*i + 2] = bq_crc8_calc(pair, 2);
    }

    bq_cs_low();
    bool ok = SERCOM0_SPI_WriteRead(tx, sizeof tx, rx, sizeof rx);
    if (ok)
    {
        ok = bq_spi_wait_idle();
    }
    bq_cs_high();
    if (!ok) return false;

    // Parse: chunk j (1..length) is the response for regAddr + (j-1)
    for (uint8_t j = 1; j <= length; j++) {
        uint8_t *chunk = &rx[3 * j];
        if (bq_crc8_calc(chunk, 2) != chunk[2]) return false; // CRC check
        data[j - 1] = chunk[1];
    }

    return true;
}

bool bq_direct_read(uint8_t command, uint8_t *data, uint8_t count)
{
   return read_reg(command, data,count);
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


     // Send 0x3E/0x3F in one SPI frame
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



void bms_set_protection_threshold(void)
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


bool read_cells_1to6(uint16_t cell_mV[6])
{
    const uint8_t addr[6] = {
        CELL_1_VOLTAGE, CELL_2_VOLTAGE, CELL_3_VOLTAGE,
        CELL_4_VOLTAGE, CELL_5_VOLTAGE, CELL_6_VOLTAGE
    };
    uint16_t raw = 0U;
    uint8_t i;
    bool ok = true;

    if (cell_mV == NULL) 
        return false;

    for (i = 0U; i < 6U; i++)
    {
        if (bq_direct_command(addr[i], &raw, R))
        {
            cell_mV[i] = raw; // mV
        }
        else
        {
            cell_mV[i] = 0U;
            ok = false;
        }
    }
    return ok;
}

  
    
bool bms_read_ts_temp(uint8_t ts_cmd, int16_t *temp_dC){
      uint16_t raw;
  
      if (temp_dC == 0)
          return false;
  
      if (!bq_direct_command(ts_cmd, &raw, 'R'))
          return false;
  
      *temp_dC = (int16_t)raw - 2731;   // raw is signed 0.1 K -> 0.1 C
  
      return true;
  }
/*
  void bms_sample_temps(void)
{
    int16_t t1_dC, t2_dC, t3_dC;

    bms_read_ts_temp(TS1_TEMP, &t1_dC);
    bms_read_ts_temp(TS2_TEMP, &t2_dC);
    bms_read_ts_temp(TS3_TEMP, &t3_dC);

}
*/

void bothoff_init(void)
{
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_PINCFG[PIN_BOTHOFF] &= (uint8_t)(~PORT_PINCFG_PMUXEN_Msk); 
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_DIRSET = BOTHOFF_PIN_MASK; 
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_OUTCLR = BOTHOFF_PIN_MASK; 
}

void bothoff_high(void)
{
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_OUTSET = BOTHOFF_PIN_MASK; 
}

void can_wakeup_pin(void){
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_PINCFG[16U] &= (uint8_t)(~PORT_PINCFG_PMUXEN_Msk);
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_DIRCLR = (1u << 16U); // Set as input
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_PINCFG[16U] |= PORT_PINCFG_INEN_Msk; // Enable INPUT
}

