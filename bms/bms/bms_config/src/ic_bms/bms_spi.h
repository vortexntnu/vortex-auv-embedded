#ifndef BQ76942_H
#define BQ76942_H

#define BatteryStatus 0x12

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


#define StackVoltage 0x34


#define SWAP_COMM_MODE 0x29BC
#define SWAP_TO_SPI 0x7C35
#define SWAP_TO_I2C 0x29E7 //maybe needed

#define THERMISOR_TEMP 0x6A
#define INTERNAL_TEMP 0x68

/* Minimal driver: only SPI+CS bring-up */
void BQ76942_Init(void);



#endif /* BQ76942_H */
