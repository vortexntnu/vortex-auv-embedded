/*
 Platform:
    ATSAMC21 

 Company:
    Vortex NTNU.

 Author:
    Markus Sandvik

 File Name:
    can_facade.h
 */

 #ifndef CAN_FACADE_H
 #define CAN_FACADE_H
 
 #pragma once
 #include <stdint.h>
 #include <stdbool.h>
 #include "definitions.h"
 
 #ifdef __cplusplus
 extern "C" {
 #endif
 
void CAN_Init(void);
bool CAN_Send(uint32_t id, uint8_t *data, uint8_t len);
bool CAN_TryRead(uint32_t *id, uint8_t *len, uint8_t *data);
void APP_CAN_Callback(uintptr_t context);
 
 
 #ifdef __cplusplus
 }
 #endif
 
 #endif // CAN_FACADE_H
     
