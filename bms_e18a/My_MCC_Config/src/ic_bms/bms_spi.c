#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/types.h>
#include "peripheral/sercom/spi_master/plib_sercom_spi_master_common.h"
#include "peripheral/sercom/spi_master/plib_sercom0_spi_master.h"
#include "definitions.h"
#include "bms_spi.h"


static inline void bq_cs_low(void)  { CS_Clear(); }
static inline void bq_cs_high(void) { CS_Set(); }

static inline void _delay(uint32_t cycles){

    for (volatile uint32_t i=0; i<cycles; i++);
}

unsigned char Checksum(unsigned char *ptr, unsigned char len)
// Calculates the checksum when writing to a RAM register. The checksum is the inverse of the sum of the bytes.	
{
	unsigned char i;
	unsigned char checksum = 0;

	for(i=0; i<len; i++)
		checksum += ptr[i];

	checksum = 0xff & ~checksum;

	return(checksum);
}

#define BQ_SPI_WAIT_MAX_LOOPS  (2000000UL)

// static bool bq_spi_wait_idle(void)
// {
//     uint32_t timeout = BQ_SPI_WAIT_MAX_LOOPS;
//
//     while (SERCOM0_SPI_IsBusy())
//     {
//         if (timeout-- == 0U)
//         {
//             return false;
//         }
//     }
//
//     return true;
// }
    

void bq76942_init(void)
{
    // --- Chip select pin setup ---
    CS_OutputEnable();
    CS_Set(); // Set HIGH (inactive)

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
//
//
// bool Spi_TransferBytes(uint8_t *tx, uint8_t *rx, uint8_t length)
// {
//     if (SERCOM0_SPI_IsBusy())
//         return false;
//
//     bq_cs_low();
//     bool ok = SERCOM0_SPI_WriteRead(tx, length, rx, length);
//     if (ok)
//     {
//         ok = bq_spi_wait_idle();
//     }
//     bq_cs_high();
//
//     return ok;
// }

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


// bool write_reg(uint8_t regAddr, const uint8_t *data, uint8_t length)
// {
//
//
//     if (SERCOM0_SPI_IsBusy())
//         return false;
//     if (length == 0)
//         return true;
//
//     // Pack back-to-back 24-bit frames: [cmd, data_byte, crc] * length
//     uint8_t tx_bytes[3 * length];
//
//     for (uint8_t i=0;i<length; i++)
//     {
//
//         uint8_t cmd   = (uint8_t)(0x80 | ((regAddr + i) & 0x7F)); // WRITE + addr+i
//         uint8_t byte  = data[i];
//
//         // CRC is computed over the first two bytes only: [cmd, byte]
//         uint8_t pair[2] = { cmd, byte };
//         uint8_t crc     = bq_crc8_calc(pair, 2);
//
//         // Store this mini-frame at position i
//         tx_bytes[3*i + 0] = cmd;
//         tx_bytes[3*i + 1] = byte;
//         tx_bytes[3*i + 2] = crc;
//
//     }
//
//      // Single SPI transfer with CS held low across all frames
//      bq_cs_low();
//      bool ok = SERCOM0_SPI_Write(tx_bytes, sizeof tx_bytes);
//      if (ok)
//      {
//          ok = bq_spi_wait_idle();
//      }
//      bq_cs_high();
//
//     return ok;
// }
// //
// bool write_reg(uint8_t regAddr, const uint8_t *data, uint8_t length)
// {
//     if (SERCOM0_SPI_IsBusy())
//         return false;
//
//     if (length == 0)
//         return true;
//
//     if (data == NULL)
//         return false;
//
//     for (uint8_t i = 0; i < length; i++) {
//         uint8_t tx[3];
//         uint8_t cmd = (uint8_t)(0x80u | ((regAddr + i) & 0x7Fu));  // write bit set
//         tx[0] = cmd;
//         tx[1] = data[i];
//         tx[2] = bq_crc8_calc(tx, 2);   // CRC over first 2 bytes only
//
//         bq_cs_low();
//         bool ok = SERCOM0_SPI_Write(tx, 3);
//         if (ok) {
//             ok = bq_spi_wait_idle();
//         }
//         bq_cs_high();
//
//         if (!ok)
//             return false;
//     }
//
//     return true;
// }


static bool bq_spi_transfer3(const uint8_t tx[3], uint8_t rx[3])
{
    // Replace with your SERCOM full-duplex API.
    // The important point is: transmit 3 bytes and capture 3 bytes.
    return SERCOM0_SPI_WriteRead(tx, 3, rx, 3);
}

bq_status_t write_reg(uint8_t regAddr, const uint8_t *data, uint8_t length)
{
    if (length == 0U)
        return BQ_OK;

    if (data == NULL)
        return BQ_ERR_PARAM;
    // printf("writing\r\n");


    for (uint8_t i = 0; i < length; i++)
    {
        uint8_t tx[3];
        uint8_t rx[3];
        uint8_t cmd = (uint8_t)(0x80U | ((regAddr + i) & 0x7FU));
        uint8_t retries = 10U;
        bool matched = false;

        tx[0] = cmd;
        tx[1] = data[i];
        tx[2] = bq_crc8_calc(tx, 2);

        while (retries-- > 0U)
        {
            bq_cs_low();
            bq_spi_transfer3(tx, rx);
            // if (!bq_spi_transfer3(tx, rx))
            // {
            //     bq_cs_high();
            //     return BQ_ERR_SPI;
            // }

            bq_cs_high();

            // printf("rx0 %x: rx 1: %x: rx 2 %x\r\n", rx[0], rx[1], rx[2]);

            // Verify echo from device
            if ((rx[0] == tx[0]) &&
                (rx[1] == tx[1]) &&
                (rx[2] == bq_crc8_calc(rx, 2)))
            {
                matched = true;
                break;
            }

            SYSTICK_DelayUs(500);   // tune per your system
        }

        SYSTICK_DelayUs(50);   // TI recommends ~50 us minimum between transactions
        // if (!matched)
        //     printf("Transmit matched failed'\r\n");
        //     return BQ_ERR_VERIFY;

    }

    return BQ_OK;
}

//
// bool read_reg(uint8_t regAddr, uint8_t *data, uint8_t length)
// {
//     if (SERCOM0_SPI_IsBusy()) 
//         return false;
//     if (length == 0)
//         return true;
//
//     const uint8_t frames = (uint8_t)(length + 1);
//     uint8_t tx[3 * frames];
//     uint8_t rx[3 * frames];
//
//     // Frame 0: issue read for regAddr + 0 (second byte = dummy 0x00)
//     {
//         uint8_t pair[2] = { (uint8_t)(regAddr & 0x7F), 0x00 };
//         tx[0] = pair[0]; tx[1] = pair[1]; tx[2] = bq_crc8_calc(pair, 2);
//     }
//
//     // Frames 1..(frames-2): issue reads for subsequent addresses; last is dummy flush
//     for (uint8_t i = 1; i < frames; i++) {
//         uint8_t cmd  = (i < frames - 1) ? (uint8_t)((regAddr + i) & 0x7F) : 0x00;
//         uint8_t pair[2] = { cmd, 0x00 };
//         tx[3*i + 0] = pair[0];
//         tx[3*i + 1] = pair[1];
//         tx[3*i + 2] = bq_crc8_calc(pair, 2);
//     }
//
//     bq_cs_low();
//     bool ok = SERCOM0_SPI_WriteRead(tx, sizeof tx, rx, sizeof rx);
//     if (ok)
//     {
//         ok = bq_spi_wait_idle();
//     }
//     bq_cs_high();
//     if (!ok) return false;
//
//     // Parse: chunk j (1..length) is the response for regAddr + (j-1)
//     for (uint8_t j = 1; j <= length; j++) {
//         uint8_t *chunk = &rx[3 * j];
//         if (bq_crc8_calc(chunk, 2) != chunk[2]) return false; // CRC check
//         data[j - 1] = chunk[1];
//     }
//
//     return true;
// }
//
// bool read_reg(uint8_t regAddr, uint8_t *data, uint8_t length)
// {
//     if (length == 0) return true;
//
//     uint8_t tx[3], rx[3];
//
//     for (uint8_t i = 0; i < length + 1; i++) {
//         uint8_t cmd = (i < length) ? ((regAddr + i) & 0x7F) : 0x00;  // flush
//         tx[0] = cmd;      // read = R/W bit 0, so just 7-bit addr
//         tx[1] = 0x00;
//         tx[2] = bq_crc8_calc(tx, 2);
//
//         bq_cs_low();
//         bool ok = SERCOM0_SPI_WriteRead(tx, 3, rx, 3);
//         if (ok) ok = bq_spi_wait_idle();
//         bq_cs_high();
//         if (!ok){
//             return false;
//         } 
//
//         for (int i = 0; i < 3; i++){
//             printf("%d : %x\r\n", i,rx[i]);
//         }
//         if (i > 0) {
//             if (bq_crc8_calc(rx, 2) != rx[2]){
//                 return false;
//             } 
//             if (rx[0] != ((regAddr + (i - 1)) & 0x7F)){
//                 return false;
//             } 
//             data[i - 1] = rx[1];
//         }
//     }
//
//     return true;
// }
bool read_reg(uint8_t regAddr, uint8_t *data, uint8_t length)
{
    if (length == 0U)
        return true;

    if (data == NULL)
        return false;


    for (uint8_t i = 0; i < length; i++)
    {
        uint8_t tx[3];
        uint8_t rx[3];
        uint8_t addr = (uint8_t)((regAddr + i) & 0x7FU);
        uint8_t retries = 10U;
        bool matched = false;

        tx[0] = addr;                  // read command
        tx[1] = 0xFFU;                // dummy byte, matches TI example
        tx[2] = bq_crc8_calc(tx, 2);  // CRC over cmd + dummy

        while (retries-- > 0U)
        {
            bq_cs_low();

            bool ok = SERCOM0_SPI_WriteRead(tx, 3, rx, 3);
            

            bq_cs_high();

            // if (!ok)
            // {
            //     return false;
            // }


            // Check returned frame
            if ((rx[0] == addr) &&
                (rx[2] == bq_crc8_calc(rx, 2)))
            {
                data[i] = rx[1];
                matched = true;
                break;
            }

            // Device may not be ready yet
            SYSTICK_DelayUs(500);
        }

        // if (!matched)
        // {
        //     printf("Failed matched\r\n");
        //     return false;
        // }

        // TI recommends spacing between transactions
        SYSTICK_DelayUs(50);
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
    uint8_t buf[2] = {0U, 0U};
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

bool BQ769x2_SetRegister(uint16_t reg_addr, uint32_t reg_data, uint8_t datalen)
{
    uint8_t tx_buffer[2]   = {0x00U, 0x00U};
    uint8_t tx_reg_data[6] = {0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U};

    // tx_reg_data in little-endian format
    tx_reg_data[0] = (uint8_t)(reg_addr & 0xFFU);
    tx_reg_data[1] = (uint8_t)((reg_addr >> 8) & 0xFFU);
    tx_reg_data[2] = (uint8_t)(reg_data & 0xFFU);

    switch (datalen)
    {
        case 1U:
        {
            write_reg(0x3EU, tx_reg_data, 3U);
            // if (!write_reg(0x3EU, tx_reg_data, 3U))
            //     return false;

            SYSTICK_DelayUs(2000);

            tx_buffer[0] = Checksum(tx_reg_data, 3U);
            tx_buffer[1] = 0x05U;   // register address (2) + data (1) + checksum/len protocol expectation
            write_reg(0x60U, tx_buffer, 2U);
            // if (!write_reg(0x60U, tx_buffer, 2U))
            //     return false;

            SYSTICK_DelayUs(2000);
            break;
        }

        case 2U:
        {
            tx_reg_data[3] = (uint8_t)((reg_data >> 8) & 0xFFU);
            write_reg(0x3EU, tx_reg_data, 4U);
            // if (!write_reg(0x3EU, tx_reg_data, 4U))
            //     return false;

            SYSTICK_DelayUs(2000);

            tx_buffer[0] = Checksum(tx_reg_data, 4U);
            tx_buffer[1] = 0x06U;   // register address (2) + data (2)

            write_reg(0x60U, tx_buffer, 2U);
            // if (!write_reg(0x60U, tx_buffer, 2U))
            //     return false;

            SYSTICK_DelayUs(2000);
            break;
        }

        case 4U:
        {
            tx_reg_data[3] = (uint8_t)((reg_data >> 8) & 0xFFU);
            tx_reg_data[4] = (uint8_t)((reg_data >> 16) & 0xFFU);
            tx_reg_data[5] = (uint8_t)((reg_data >> 24) & 0xFFU);
            write_reg(0x3EU, tx_reg_data, 6U);
            // if (!write_reg(0x3EU, tx_reg_data, 6U))
            //     return false;

            SYSTICK_DelayUs(2000);

            tx_buffer[0] = Checksum(tx_reg_data, 6U);
            tx_buffer[1] = 0x08U;   // register address (2) + data (4)
            write_reg(0x60U, tx_buffer, 2U);
            // if (!write_reg(0x60U, tx_buffer, 2U))
            //     return false;

            SYSTICK_DelayUs(2000);
            break;
        }

        default:
            return false;
    }

    return true;
}

void CommandSubcommands(uint16_t command) //For Command only Subcommands
// See the TRM or the BQ76952 header file for a full list of Command-only subcommands
{	//For DEEPSLEEP/SHUTDOWN subcommand you will need to call this function twice consecutively
	
	uint8_t TX_Reg[2] = {0x00, 0x00};

	//TX_Reg in little endian format
	TX_Reg[0] = command & 0xff; 
	TX_Reg[1] = (command >> 8) & 0xff; 

	write_reg(0x3E,TX_Reg,2); 
	SYSTICK_DelayUs(2000);
}


void BQ769x2_Init() {
	// Configures all parameters in device RAM

	// Enter CONFIGUPDATE mode (Subcommand 0x0090) - It is required to be in CONFIG_UPDATE mode to program the device RAM settings
	// See TRM for full description of CONFIG_UPDATE mode
	CommandSubcommands(SET_CFGUPDATE);

	// After entering CONFIG_UPDATE mode, RAM registers can be programmed. When programming RAM, checksum and length must also be
	// programmed for the change to take effect. All of the RAM registers are described in detail in the BQ769x2 TRM.
	// An easier way to find the descriptions is in the BQStudio Data Memory screen. When you move the mouse over the register name,
	// a full description of the register and the bits will pop up on the screen.

	// 'Power Config' - 0x9234 = 0x2D80
	// Setting the DSLP_LDO bit allows the LDOs to remain active when the device goes into Deep Sleep mode
  	// Set wake speed bits to 00 for best performance
	BQ769x2_SetRegister(PowerConfig, 0x2D80, 2);

	// 'REG0 Config' - set REG0_EN bit to enable pre-regulator
	BQ769x2_SetRegister(REG0Config, 0x01, 1);

	// 'REG12 Config' - Enable REG1 with 3.3V output (0x0D for 3.3V, 0x0F for 5V)
	BQ769x2_SetRegister(REG12Config, 0x0D, 1);

    BQ769x2_SetRegister(0x923C, 0x40, 1);

	// Set DFETOFF pin to control BOTH CHG and DSG FET - 0x92FB = 0x42 (set to 0x00 to disable)
	BQ769x2_SetRegister(DFETOFFPinConfig, 0x00, 1);

	// Set up ALERT Pin - 0x92FC = 0x2A
	// This configures the ALERT pin to drive high (REG1 voltage) when enabled.
	// The ALERT pin can be used as an interrupt to the MCU when a protection has triggered or new measurements are available
	BQ769x2_SetRegister(ALERTPinConfig, 0x2A, 1);

	// // Set TS1 to measure Cell Temperature - 0x92FD = 0x07
	// BQ769x2_SetRegister(TS1Config, 0x07, 1);
	//
	// // Set TS3 to measure FET Temperature - 0x92FF = 0x0F
	// BQ769x2_SetRegister(TS3Config, 0x0F, 1);

    BQ769x2_SetRegister(TS1Config, 0x00, 1);
    BQ769x2_SetRegister(TS3Config, 0x00, 1);

	// Set HDQ to measure Cell Temperature - 0x9300 = 0x07
	BQ769x2_SetRegister(HDQPinConfig, 0x00, 1);   // No thermistor installed on EVM HDQ pin, so set to 0x00

	// 'VCell Mode' - Enable 16 cells - 0x9304 = 0x0000; Writing 0x0000 sets the default of 16 cells
	BQ769x2_SetRegister(VCellMode, 0x021F, 2);

	// Enable protections in 'Enabled Protections A' 0x9261 = 0xBC
	// Enables SCD (short-circuit), OCD1 (over-current in discharge), OCC (over-current in charge),
	// COV (over-voltage), CUV (under-voltage)
	BQ769x2_SetRegister(EnabledProtectionsA, 0xBC, 1);

	// Enable all protections in 'Enabled Protections B' 0x9262 = 0xF7
	// Enables OTF (over-temperature FET), OTINT (internal over-temperature), OTD (over-temperature in discharge),
	// OTC (over-temperature in charge), UTINT (internal under-temperature), UTD (under-temperature in discharge), UTC (under-temperature in charge)
	// BQ769x2_SetRegister(EnabledProtectionsB, 0xF7, 1);
    BQ769x2_SetRegister(EnabledProtectionsB, 0x44, 1);

	// 'Default Alarm Mask' - 0x..82 Enables the FullScan and ADScan bits, default value = 0xF800
	BQ769x2_SetRegister(DefaultAlarmMask, 0xF882, 2);

    //TESTING INIT FUNCTIONS
    BQ769x2_SetRegister(MfgStatusInit, 0x0050, 2);

    BQ769x2_SetRegister(ProtectionConfiguration, 0x0602, 2);

    BQ769x2_SetRegister(FETOptions, 0x0D, 1);
    BQ769x2_SetRegister(ChgPumpControl,0x01,1); 
    BQ769x2_SetRegister(CFETOFFPinConfig, 0x00, 1);
    BQ769x2_SetRegister(DFETOFFPinConfig, 0x00, 1);
    // BQ769x2_SetRegister(CHGFETProtectionsA, 0x98, 1);
    // BQ769x2_SetRegister(CHGFETProtectionsB, 0xD5, 1);
    BQ769x2_SetRegister(CHGFETProtectionsA, 0x00, 1);
    BQ769x2_SetRegister(CHGFETProtectionsB, 0x00, 1);
    BQ769x2_SetRegister(CHGFETProtectionsC, 0x00, 1);
    //BQ769x2_SetRegister(CHGFETProtectionsC, 0x56, 1);
    BQ769x2_SetRegister(DSGFETProtectionsA, 0xE4, 1);
    BQ769x2_SetRegister(DSGFETProtectionsB, 0xE6, 1);
    //BQ769x2_SetRegister(DSGFETProtectionsC, 0xE2, 1);
    BQ769x2_SetRegister(PrechargeStartVoltage, 0, 2);
    BQ769x2_SetRegister(PrechargeStopVoltage, 0, 2);




	// Set up Cell Balancing Configuration - 0x9335 = 0x03   -  Automated balancing while in Relax or Charge modes
	// Also see "Cell Balancing with BQ769x2 Battery Monitors" document on ti.com
	BQ769x2_SetRegister(BalancingConfiguration, 0x00, 1);   // CB_RLX only for test
    BQ769x2_SetRegister(CellBalanceMaxCells, 1, 1);
    BQ769x2_SetRegister(CellBalanceInterval, 10, 1);

    BQ769x2_SetRegister(MinCellTemp, 0, 1);
    BQ769x2_SetRegister(MaxCellTemp, 60, 1);
    BQ769x2_SetRegister(MaxInternalTemp, 70, 1);

    BQ769x2_SetRegister(CellBalanceMinCellVRelax, 3000, 2);
    BQ769x2_SetRegister(CellBalanceMinDeltaRelax, 20, 1);
    BQ769x2_SetRegister(CellBalanceStopDeltaRelax, 10, 1);
    //BQ769x2_SetRegister(PrechargeStartVoltage, 0, 2); //PRECHARGE SET 0 ZERO (DEFAULT ALSO 0)
    //BQ769x2_SetRegister(PrechargeStopVoltage, 0, 2);  
    union { float f; uint32_t u; } cc, cap;
    cc.f  = 14.9536f;
    cap.f = cc.f * 298261.6178f;
    
    
    BQ769x2_SetRegister(CCGain, cc.u, 4);
    BQ769x2_SetRegister(CapacityGain, cap.u, 4);
    
    
    
    

	// Set up CUV (under-voltage) Threshold - 0x9275 = 0x31 (2479 mV)
	// CUV Threshold is this value multiplied by 50.6mV
	BQ769x2_SetRegister(CUVThreshold, 0x42, 1);

	// Set up COV (over-voltage) Threshold - 0x9278 = 0x55 (4301 mV)
	// COV Threshold is this value multiplied by 50.6mV
	BQ769x2_SetRegister(COVThreshold, 0x52, 1);

	// Set up OCC (over-current in charge) Threshold - 0x9280 = 0x05 (10 mV = 10A across 1mOhm sense resistor) Units in 2mV
	BQ769x2_SetRegister(OCCThreshold, 0x05, 1);

	// Set up OCD1 Threshold - 0x9282 = 0x0A (20 mV = 20A across 1mOhm sense resistor) units of 2mV
	BQ769x2_SetRegister(OCD1Threshold, 0x14, 1);
    BQ769x2_SetRegister(OCD2Threshold,0x19 , 1);

	// Set up SCD Threshold - 0x9286 = 0x05 (100 mV = 100A across 1mOhm sense resistor)  0x05=100mV
	BQ769x2_SetRegister(SCDThreshold, 0x05, 1);

    BQ769x2_SetRegister(OCDLRecoveryThreshold, 0, 2);
    BQ769x2_SetRegister(SCDLRecoveryThreshold, 0, 2);

	// Set up SCD Delay - 0x9287 = 0x03 (30 us) Enabled with a delay of (value - 1) * 15 µs; min value of 1    
	BQ769x2_SetRegister(SCDDelay, 0x03, 1);

	// Set up SCDL Latch Limit to 1 to set SCD recovery only with load removal 0x9295 = 0x01
	// If this is not set, then SCD will recover based on time (SCD Recovery Time parameter).
	BQ769x2_SetRegister(SCDLLatchLimit, 0x00, 1);

	// Exit CONFIGUPDATE mode  - Subcommand 0x0092
	CommandSubcommands(EXIT_CFGUPDATE);
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

#define REG12_CONFIG_ADDR        0x9236u
#define REG0_CONFIG_ADDR         0x9237u
#define SPI_CONFIGURATION_ADDR   0x923Cu

void bms_init_comm_voltage(void)
{
    bq_command_only(ENTER_CONFIG_UPDATE);

    // Enable REG0 only if REGIN is NOT supplied externally
    // {
    //     uint8_t reg0_cfg = 0x01;   // REG0_EN = 1
    //     bq_write_subcommand(REG0_CONFIG_ADDR, &reg0_cfg, 1);
    // }

    // REG1 = 3.3 V, enabled
    {
        uint8_t reg12_cfg = 0x0D;  // REG1V=6 (3.3V), REG1_EN=1
        bq_write_subcommand(REG12_CONFIG_ADDR, &reg12_cfg, 1);
    }

    // SPI MISO uses REG1 voltage level
    {
        uint8_t spi_cfg = 0x60;    // MISO_REG1=1, FILT=1
        bq_write_subcommand(SPI_CONFIGURATION_ADDR, &spi_cfg, 1);
    }

    bq_command_only(EXIT_CONFIG_UPDATE);
}

bool bms_battery_status_get(uint8_t *fet_reg, bms_state_t *state)
{
    uint8_t fet = 0U;
    bool chg_on;
    bool pchg_on;
    bool dsg_on;

    if (!bq_direct_read(FET_STATUS, &fet, 1))
    {
        if (state != NULL)
        {
            *state = BMS_STATE_READ_FAIL;
        }
        return false;
    }

    if (fet_reg != NULL)
    {
        *fet_reg = fet;
    }

    chg_on = ((fet & (1U << 0)) != 0U);
    pchg_on = ((fet & (1U << 1)) != 0U);
    dsg_on = ((fet & (1U << 2)) != 0U);

    if (state != NULL)
    {
        if (pchg_on)
        {
            *state = BMS_STATE_PRECHARGE;
        }
        else if (chg_on && !dsg_on)
        {
            *state = BMS_STATE_CHARGING;
        }
        else if (dsg_on && !chg_on)
        {
            *state = BMS_STATE_DISCHARGING;
        }
        else if (!chg_on && !dsg_on)
        {
            *state = BMS_STATE_IDLE;
        }
        else
        {
            *state = BMS_STATE_TRANSITION;
        }
    }

    return true;
}

void bms_battery_status(void)
{
    uint8_t fet = 0U;
    bms_state_t state = BMS_STATE_READ_FAIL;

    (void)bms_battery_status_get(&fet, &state);
}


bool read_cells_1to6(uint16_t cell_mV[6])
{
    const uint8_t addr[6] = {
        CELL_1_VOLTAGE, CELL_2_VOLTAGE, CELL_3_VOLTAGE,
        CELL_4_VOLTAGE, CELL_5_VOLTAGE, CELL_10_VOLTAGE
    };
    uint16_t raw = 0;
    uint8_t i;
    bool ok = true;

    if (cell_mV == NULL) 
        return false;

    for (i = 0; i < 6; i++)
    {
        if (bq_direct_command(addr[i], &raw, R))
        {
            if (raw == 0xFFFFU)
            {
                cell_mV[i] = 0U;
                ok = false;
            }
            else
            {
                cell_mV[i] = raw; // mV
            }
        }
        else
        {
            cell_mV[i] = 0U;
            ok = false;
        }
    }
    return ok;
}

  
    
bool bms_read_ts_temp(uint8_t ts_cmd, int16_t *temp_dC)
{
      uint16_t raw;
  
      if (temp_dC == 0)
          return false;
  
      if (!bq_direct_command(ts_cmd, &raw, 'R'))
          return false;
  
      *temp_dC = (int16_t)raw - 2731;   // raw is signed 0.1 K -> 0.1 C
  
      return true;
  }

bool bms_read_current(int16_t *current_mA) 
{
    uint16_t raw;

    if (current_mA == 0)
        return false;
    if (!bq_direct_command(CC2_CURRENT, &raw, 'R'))
        return false;

    *current_mA = (int16_t)raw;
    return true;


}

void bothoff_init(void)
{
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_PINCFG[PIN_BOTHOFF] &= (uint8_t)(~PORT_PINCFG_PMUXEN_Msk); // GPIO mode
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_DIRSET = BOTHOFF_PIN_MASK; // Set as output
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_OUTSET = BOTHOFF_PIN_MASK;  // Safe default: HIGH (bothoff OFF)
}

void bothoff_high(void)
{
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_OUTSET = BOTHOFF_PIN_MASK; // Set HIGH to turn bothoff OFF
}
void bothoff_low(void)
{
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_OUTCLR = BOTHOFF_PIN_MASK;  // Set LOW to turn bothoff ON
}

void can_wakeup_pin(void){
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_PINCFG[16U] &= (uint8_t)(~PORT_PINCFG_PMUXEN_Msk); 
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_DIRCLR = (1u << 16U); // Set as input
    PORT_REGS->GROUP[GPIO_GROUP_A].PORT_PINCFG[16U] |= PORT_PINCFG_INEN_Msk; // Enable INPUT
}
