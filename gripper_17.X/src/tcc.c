/*
 * File:   tcc.c
 * Author: nathaniel
 *
 * Created on January 19, 2025, 6:34 PM
 */

#include "tcc.h"
#include <stdio.h>
#include "sam.h"

TCC_CALLBACK_OBJECT TCC1_CallbackObj;

/* Initialize TCC module */
void TCC1_PWMInitialize(void) {
    /* Reset TCC */
    TCC1_REGS->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    while (TCC1_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_SWRST_Msk)) {
        /* Wait for sync */
    }
    /* Clock prescaler */
    TCC1_REGS->TCC_CTRLA = TCC_CTRLA_PRESCALER_DIV4;
    TCC1_REGS->TCC_WEXCTRL = TCC_WEXCTRL_OTMX(1U);
    /* Dead time configurations */
    TCC1_REGS->TCC_WEXCTRL |= TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTIEN1_Msk |
                              TCC_WEXCTRL_DTIEN2_Msk | TCC_WEXCTRL_DTIEN3_Msk |
                              TCC_WEXCTRL_DTLS(64U) | TCC_WEXCTRL_DTHS(64U);

    TCC1_REGS->TCC_DRVCTRL |= (1 << 16) | (1 << 17);
    TCC1_REGS->TCC_WAVE = TCC_WAVE_WAVEGEN_DSTOP;

    /* Configure duty cycle values */  // All set to 1500 micro seconds duty
                                       // cycle
    TCC1_REGS->TCC_CC[0] = 9000;
    TCC1_REGS->TCC_CC[1] = 9000;
    TCC1_REGS->TCC_CC[2] = 9000;
    TCC1_REGS->TCC_CC[3] = 9000;
    TCC1_REGS->TCC_PER = 119999;

    // Max duty cycle: 2300 micro seconds
    // Idle duty cycle: 1500 micro seconds = 9000
    // Mni duty cycle: 700 micro seconds = 4200

    // TCC1_REGS->TCC_INTENSET = TCC_INTENSET_OVF_Msk;

    while (TCC1_REGS->TCC_SYNCBUSY) {
        /* Wait for sync */
    }
}

/* Start the PWM generation */
void TCC1_PWMStart(void) {
    TCC1_REGS->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    while (TCC1_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_ENABLE_Msk)) {
        /* Wait for sync */
    }
}

/* Stop the PWM generation */
void TCC1_PWMStop(void) {
    TCC1_REGS->TCC_CTRLA &= ~TCC_CTRLA_ENABLE_Msk;
    while (TCC1_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_ENABLE_Msk)) {
        /* Wait for sync */
    }
}

/* Configure PWM period */
void TCC1_PWM24bitPeriodSet(uint32_t period) {
    TCC1_REGS->TCC_PERBUF = period & 0xFFFFFF;
    while ((TCC1_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_PER_Msk)) ==
           TCC_SYNCBUSY_PER_Msk) {
        /* Wait for sync */
    }
}

/* Read TCC period */
uint32_t TCC1_PWM24bitPeriodGet(void) {
    while (TCC1_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_PER_Msk)) {
        /* Wait for sync */
    }
    return (TCC1_REGS->TCC_PER & 0xFFFFFF);
}

/* Configure dead time */
void TCC1_PWMDeadTimeSet(uint8_t deadtime_high, uint8_t deadtime_low) {
    TCC1_REGS->TCC_WEXCTRL &= ~(TCC_WEXCTRL_DTHS_Msk | TCC_WEXCTRL_DTLS_Msk);
    TCC1_REGS->TCC_WEXCTRL |=
        TCC_WEXCTRL_DTHS(deadtime_high) | TCC_WEXCTRL_DTLS(deadtime_low);
}

void TCC1_PWMPatternSet(uint8_t pattern_enable, uint8_t pattern_output) {
    TCC1_REGS->TCC_PATTBUF = (uint16_t)(pattern_enable | (pattern_output << 8));
    while ((TCC1_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_PATT_Msk)) ==
           TCC_SYNCBUSY_PATT_Msk) {
        /* Wait for sync */
    }
}

/* Set the counter*/
void TCC1_PWM24bitCounterSet(uint32_t count_value) {
    TCC1_REGS->TCC_COUNT = count_value & 0xFFFFFF;
    while (TCC1_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_COUNT_Msk)) {
        /* Wait for sync */
    }
}

/* Enable forced synchronous update */
void TCC1_PWMForceUpdate(void) {
    TCC1_REGS->TCC_CTRLBSET |= TCC_CTRLBCLR_CMD_UPDATE;
    while (TCC1_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_CTRLB_Msk)) {
        /* Wait for sync */
    }
}

/* Enable the period interrupt - overflow or underflow interrupt */
void TCC1_PWMPeriodInterruptEnable(void) {
    TCC1_REGS->TCC_INTENSET = TCC_INTENSET_OVF_Msk;
}

/* Disable the period interrupt - overflow or underflow interrupt */
void TCC1_PWMPeriodInterruptDisable(void) {
    TCC1_REGS->TCC_INTENCLR = TCC_INTENCLR_OVF_Msk;
}

/* Register callback function */
void TCC1_PWMCallbackRegister(TCC_CALLBACK callback, uintptr_t context) {
    TCC1_CallbackObj.callback_fn = callback;
    TCC1_CallbackObj.context = context;
}

/* Interrupt Handler */
void __attribute__((used)) TCC1_Handler(void) {
    uint32_t status;
    status = TCC1_REGS->TCC_INTFLAG;
    /* Clear interrupt flags */
    TCC1_REGS->TCC_INTFLAG = TCC_INTFLAG_Msk;
    if (TCC1_CallbackObj.callback_fn != NULL) {
        TCC1_CallbackObj.callback_fn(status, TCC1_CallbackObj.context);
    }
}
