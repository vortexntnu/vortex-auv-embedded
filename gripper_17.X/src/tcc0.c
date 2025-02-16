

/*
 * File:   tcc.c
 * Author: nathaniel
 *
 * Created on January 19, 2025, 6:34 PM
 */

#include "tcc0.h"
#include <stdio.h>

TCC_CALLBACK_OBJECT TCC0_CallbackObj;

/* Initialize TCC module */
void TCC0_PWMInitialize(void) {
    /* Reset TCC */
    TCC0_REGS->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    while (TCC0_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_SWRST_Msk)) {
        /* Wait for sync */
    }
    /* Clock prescaler */
    TCC0_REGS->TCC_CTRLA = TCC_CTRLA_PRESCALER_DIV4;
    TCC0_REGS->TCC_WEXCTRL = TCC_WEXCTRL_OTMX(1U);
    /* Dead time configurations */
    TCC0_REGS->TCC_WEXCTRL |= TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTIEN1_Msk |
                              TCC_WEXCTRL_DTIEN2_Msk | TCC_WEXCTRL_DTIEN3_Msk |
                              TCC_WEXCTRL_DTLS(64U) | TCC_WEXCTRL_DTHS(64U);

    TCC0_REGS->TCC_WAVE = TCC_WAVE_WAVEGEN_DSTOP;

    TCC0_REGS->TCC_DRVCTRL |= (1 << 19);

    /* Configure duty cycle values */  // All set to 1500 micro seconds duty
                                       // cycle
    TCC0_REGS->TCC_CC[0] = 9000;
    TCC0_REGS->TCC_CC[1] = 9000;
    TCC0_REGS->TCC_CC[2] = 9000;
    TCC0_REGS->TCC_CC[3] = 9000;
    TCC0_REGS->TCC_PER = 119999;
  
    // Max duty cycle: 2300 micro seconds = 13800
    // Idle duty cycle: 1500 micro seconds = 9000
    // Mni duty cycle: 700 micro seconds = 4200

    TCC0_REGS->TCC_INTENSET = TCC_INTENSET_OVF_Msk;

    while (TCC0_REGS->TCC_SYNCBUSY) {
        /* Wait for sync */
    }
}

/* Start the PWM generation */
void TCC0_PWMStart(void) {
    TCC0_REGS->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    while (TCC0_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_ENABLE_Msk)) {
        /* Wait for sync */
    }
}

/* Stop the PWM generation */
void TCC0_PWMStop(void) {
    TCC0_REGS->TCC_CTRLA &= ~TCC_CTRLA_ENABLE_Msk;
    while (TCC0_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_ENABLE_Msk)) {
        /* Wait for sync */
    }
}

/* Configure PWM period */
void TCC0_PWM24bitPeriodSet(uint32_t period) {
    TCC0_REGS->TCC_PERBUF = period & 0xFFFFFF;
    while ((TCC0_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_PER_Msk)) ==
           TCC_SYNCBUSY_PER_Msk) {
        /* Wait for sync */
    }
}

/* Read TCC period */
uint32_t TCC0_PWM24bitPeriodGet(void) {
    while (TCC0_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_PER_Msk)) {
        /* Wait for sync */
    }
    return (TCC0_REGS->TCC_PER & 0xFFFFFF);
}

/* Configure dead time */
void TCC0_PWMDeadTimeSet(uint8_t deadtime_high, uint8_t deadtime_low) {
    TCC0_REGS->TCC_WEXCTRL &= ~(TCC_WEXCTRL_DTHS_Msk | TCC_WEXCTRL_DTLS_Msk);
    TCC0_REGS->TCC_WEXCTRL |=
        TCC_WEXCTRL_DTHS(deadtime_high) | TCC_WEXCTRL_DTLS(deadtime_low);
}

void TCC0_PWMPatternSet(uint8_t pattern_enable, uint8_t pattern_output) {
    TCC0_REGS->TCC_PATTBUF = (uint16_t)(pattern_enable | (pattern_output << 8));
    while ((TCC0_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_PATT_Msk)) ==
           TCC_SYNCBUSY_PATT_Msk) {
        /* Wait for sync */
    }
}

/* Set the counter*/
void TCC0_PWM24bitCounterSet(uint32_t count_value) {
    TCC0_REGS->TCC_COUNT = count_value & 0xFFFFFF;
    while (TCC0_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_COUNT_Msk)) {
        /* Wait for sync */
    }
}

/* Enable forced synchronous update */
void TCC0_PWMForceUpdate(void) {
    TCC0_REGS->TCC_CTRLBSET |= TCC_CTRLBCLR_CMD_UPDATE;
    while (TCC0_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_CTRLB_Msk)) {
        /* Wait for sync */
    }
}

/* Enable the period interrupt - overflow or underflow interrupt */
void TCC0_PWMPeriodInterruptEnable(void) {
    TCC0_REGS->TCC_INTENSET = TCC_INTENSET_OVF_Msk;
}

/* Disable the period interrupt - overflow or underflow interrupt */
void TCC0_PWMPeriodInterruptDisable(void) {
    TCC0_REGS->TCC_INTENCLR = TCC_INTENCLR_OVF_Msk;
}

/* Register callback function */
void TCC0_PWMCallbackRegister(TCC_CALLBACK callback, uintptr_t context) {
    TCC0_CallbackObj.callback_fn = callback;
    TCC0_CallbackObj.context = context;
}

/* Interrupt Handler */
void __attribute__((used)) TCC0_Handler(void) {
    uint32_t status;
    status = TCC0_REGS->TCC_INTFLAG;
    /* Clear interrupt flags */
    TCC0_REGS->TCC_INTFLAG = TCC_INTFLAG_Msk;
    if (TCC0_CallbackObj.callback_fn != NULL) {
        TCC0_CallbackObj.callback_fn(status, TCC0_CallbackObj.context);
    }
}
