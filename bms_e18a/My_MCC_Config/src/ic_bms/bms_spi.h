#ifndef BMS_SPI_H
#define BMS_SPI_H

#include<stdint.h>
#include<stdbool.h>



//direct commands

#define BATTERY_STATUS 0x12
#define STACK_VOLTAGE 0x34

#define CELL_1_VOLTAGE 0x14
#define CELL_2_VOLTAGE 0x16
#define CELL_3_VOLTAGE 0x18
#define CELL_4_VOLTAGE 0x1A
#define CELL_5_VOLTAGE 0x1C
#define CELL_6_VOLTAGE 0x1E
#define CC2_CURRENT 0x3A


/*
#define Cell7Voltage 0x20
#define Cell8Voltage 0x22
#define Cell9Voltage 0x24
#define Cell10Voltage 0x26
#define Cell11Voltage 0x28
#define Cell12Voltage 0x2A
#define Cell13Voltage 0x2C
#define Cell14Voltage 0x2E
#define Cell15Voltage 0x30
#define Cell16Voltage 0x32

*/ // 16 cell voltage readings avaliable 

#define R 'R' // Read; Used with bq_direct_command
#define W 'W' // Write; Used with bq_direct_command
#define BQ_SUBCMD_MAX_POLLS 2000u


#define SWAP_COMM_MODE 0x29BC
#define SWAP_TO_SPI 0x7C35
#define SWAP_TO_I2C 0x29E7 //maybe needed

#define THERMISOR_TEMP 0x6A
#define INTERNAL_TEMP 0x68
#define TS1_TEMP 0x70
#define TS2_TEMP 0x72
#define TS3_TEMP 0x74

//command only (R)

#define RESET 0x0012
#define SHUTDOWN 0x0010

// Thresholds and Delays values (TBC)

#define COV_THRESHOLD_MV   4400
#define COV_DELAY_MS       500
#define CUV_THRESHOLD_MV   2500
#define CUV_DELAY_MS       500

// Threshold Addresses
#define COV_THRESHOLD_ADDR 0x9278
#define COV_DELAY_ADDR     0x9279
#define CUV_THRESHOLD_ADDR 0x9275
#define CUV_DELAY_ADDR     0x9276

//Config Mode
#define ENTER_CONFIG_UPDATE 0x0090
#define EXIT_CONFIG_UPDATE  0x0092

#define FET_STATUS 0x7F

// BOTHOFF PINS

#define GPIO_GROUP_A 0u
#define PIN_BOTHOFF 6u
#define BOTHOFF_PIN_MASK (1u << PIN_BOTHOFF)

typedef enum
{
    BMS_STATE_READ_FAIL = 0,
    BMS_STATE_PRECHARGE,
    BMS_STATE_CHARGING,
    BMS_STATE_DISCHARGING,
    BMS_STATE_IDLE,
    BMS_STATE_TRANSITION
} bms_state_t;


typedef enum
{
    BQ_OK = 0,
    BQ_ERR_BUSY,
    BQ_ERR_PARAM,
    BQ_ERR_SPI,
    BQ_ERR_VERIFY,
    BQ_ERR_TIMEOUT
} bq_status_t;


/* Minimal driver: only SPI+CS bring-up */

void bq76942_init(void);
bq_status_t write_reg(uint8_t regAddr, const uint8_t *data, uint8_t length);
bool read_reg(uint8_t regAddr, uint8_t *data, uint8_t length);
bool bq_direct_command(uint8_t command, uint16_t *data, char type);
bool bq_command_only(uint16_t subcmd);
bool bq_direct_read(uint8_t command, uint8_t *data, uint8_t count);
bool bq_direct_write(uint8_t command, const uint8_t *data, uint8_t count);
bool bq_read_subcommand(uint16_t subcmd, uint8_t *data, uint8_t length);
bool bq_write_subcommand(uint16_t subcmd, const uint8_t *data, uint8_t length);
void bms_set_protection_threshold(void);
void bms_battery_status(void);
bool bms_battery_status_get(uint8_t *fet_reg, bms_state_t *state);
bool read_cells_1to6(uint16_t cell_mV[6]);
void bms_sample_temps(void);
bool bms_read_ts_temp(uint8_t ts_cmd, int16_t *temp_dC);
bool bms_current_read(int16_t *current_userA);
void bothoff_init(void);
void bothoff_high(void);
void bothoff_low(void);

void bms_init_comm_voltage(void);





#endif /* bq76942_H */
