#ifndef BMS_SPI_H
#define BMS_SPI_H

#include<stdint.h>
#include<stdbool.h>



//Direct Commands

#define BatteryStatus 0x12
#define StackVoltage 0x34

#define Cell1Voltage 0x14
#define Cell2Voltage 0x16
#define Cell3Voltage 0x18
#define Cell4Voltage 0x1A
#define Cell5Voltage 0x1C
#define Cell6Voltage 0x1E
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

#define BQ_CS_GROUP   (0U)          
#define BQ_CS_MASK    (1UL << 18)   

#define R 0 // Read; Used in DirectCommands and Subcommands functions
#define W 1 // Write; Used in DirectCommands and Subcommands functions



#define SWAP_COMM_MODE 0x29BC
#define SWAP_TO_SPI 0x7C35
#define SWAP_TO_I2C 0x29E7 //maybe needed

#define THERMISOR_TEMP 0x6A
#define INTERNAL_TEMP 0x68

//Command only (R)

#define RESET 0x0012
#define SHUTDOWN 0x0010

// Thresholds and Delays values (TBC)

#define COV_THRESHOLD_MV   4250
#define COV_DELAY_MS       200
#define CUV_THRESHOLD_MV   3000
#define CUV_DELAY_MS       300

// Addresses
#define COV_THRESHOLD_ADDR 0x9278
#define COV_DELAY_ADDR     0x9279
#define CUV_THRESHOLD_ADDR 0x9275
#define CUV_DELAY_ADDR     0x9276

//Config Mode
#define ENTER_CONFIG_UPDATE 0x0090
#define EXIT_CONFIG_UPDATE  0x0092


/* Minimal driver: only SPI+CS bring-up */
void BQ76942_Init(void);
bool WriteReg(uint8_t regAddr, uint8_t value);
bool ReadReg(uint8_t regAddr, uint8_t *value);
bool BQ_DirectCommand(uint8_t command, uint16_t *data, char type);
bool BQ_CommandOnly(uint16_t subcmd);
bool BQ_DirectRead(uint8_t command, uint8_t *data, uint8_t count);
bool BQ_DirectWrite(uint8_t command, const uint8_t *data, uint8_t count);
bool BQ_ReadSubCommand(uint16_t subcmd, uint8_t *data, uint8_t length);
bool BQ_WriteSubCommand(uint16_t subcmd, const uint8_t *data, uint8_t length);
void BMS_SetProtectionThresholds(void);








#endif /* BQ76942_H */
