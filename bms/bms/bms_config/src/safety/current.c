#include "current.h"
#include "definitions.h"
#include "peripheral/adc/plib_adc0.h"
#include "samc21j18a.h"
#include <stdint.h>
#include <stdlib.h>


static int current_flags = 0;
uint16_t adc_read_channel(uint8_t channel)
{
    // 1. Select ADC input (MUXPOS)
    ADC0_REGS->ADC_INPUTCTRL = ADC_INPUTCTRL_MUXPOS(channel);
    while (ADC0_REGS->ADC_SYNCBUSY  & ADC_SYNCBUSY_INPUTCTRL_Msk);

    // 2. Start conversion
    ADC0_REGS->ADC_SWTRIG |= ADC_SWTRIG_START_Msk;

    // 3. Wait for conversion to complete
    while ((ADC0_REGS->ADC_INTFLAG & ADC_INTFLAG_RESRDY_Msk) == 0);

    // 4. Read result
    uint16_t result = ADC0_REGS->ADC_RESULT;

    // 5. Clear the RESRDY flag (write 1 to clear)
    ADC0_REGS->ADC_INTFLAG = ADC_INTFLAG_RESRDY_Msk;

    return result;
}



//static float read_current_amps(void){}


void current_init(){

    ADC0_Initialize();
    ADC0_Enable();
    ADC0_ChannelSelect(ADC_POSINPUT_AIN0, ADC_NEGINPUT_GND);
    ADC0_ConversionStart();



}



void current_check(){

    if (ADC0_REGS->ADC_RESULT > CURRENT_THRESHOLD_A){

        
        
        current_flags++;}


}




void check_raise_flags(void){
    //check if current sense flags are raised
    //if raised, take action
    if (current_flags>5){
        //raise system fault
        
    }
}




//raise flags logic if current is out of range
//fault handling logic
//current limiting logic
