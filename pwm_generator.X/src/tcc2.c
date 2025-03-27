
/*
 * File:   tcc.c
 * Author: nathaniel
 *
 * Created on January 19, 2025, 6:34 PM
 */

#include "tcc.h"
#include <stdio.h>
#include "sam.h"

TCC_CALLBACK_OBJECT TCC2_CallbackObj;

/* Initialize TCC module */
void TCC2_PWMInitialize(void) {
    /* Reset TCC */
    TCC2_REGS->TCC_CTRLA = TCC_CTRLA_SWRST_Msk;
    while (TCC2_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_SWRST_Msk)) {
        /* Wait for sync */
    }
    /* Clock prescaler */
    TCC2_REGS->TCC_CTRLA = TCC_CTRLA_PRESCALER_DIV8;
    TCC2_REGS->TCC_WEXCTRL = TCC_WEXCTRL_OTMX(1U);
    /* Dead time configurations */
    TCC2_REGS->TCC_WEXCTRL |= TCC_WEXCTRL_DTIEN0_Msk | TCC_WEXCTRL_DTIEN1_Msk |
                              TCC_WEXCTRL_DTIEN2_Msk | TCC_WEXCTRL_DTIEN3_Msk |
                              TCC_WEXCTRL_DTLS(64U) | TCC_WEXCTRL_DTHS(64U);

    TCC2_REGS->TCC_DRVCTRL |= (1 << 16) | (1 << 17);
    TCC2_REGS->TCC_WAVE = TCC_WAVE_WAVEGEN_DSTOP;

    /* Configure duty cycle values */  // All set to 1500 micro seconds duty
                                       // cycle
    TCC2_REGS->TCC_CC[0] = 4500;
    TCC2_REGS->TCC_CC[1] = 4500;
    TCC2_REGS->TCC_PER = 60250;

    // Max duty cycle: 2300 micro seconds
    // Idle duty cycle: 1500 micro seconds = 9000
    // Mni duty cycle: 700 micro seconds = 4200

    // TCC2_REGS->TCC_INTENSET = TCC_INTENSET_OVF_Msk;

    while (TCC2_REGS->TCC_SYNCBUSY) {
        /* Wait for sync */
    }
}

/* Start the PWM generation */
void TCC2_PWMStart(void) {
    TCC2_REGS->TCC_CTRLA |= TCC_CTRLA_ENABLE_Msk;
    while (TCC2_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_ENABLE_Msk)) {
        /* Wait for sync */
    }
}

/* Stop the PWM generation */
void TCC2_PWMStop(void) {
    TCC2_REGS->TCC_CTRLA &= ~TCC_CTRLA_ENABLE_Msk;
    while (TCC2_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_ENABLE_Msk)) {
        /* Wait for sync */
    }
}

/* Configure PWM period */
void TCC2_PWM16bitPeriodSet(uint16_t period) {
    TCC2_REGS->TCC_PERBUF = period & 0xFFFF;
    while ((TCC2_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_PER_Msk)) ==
           TCC_SYNCBUSY_PER_Msk) {
        /* Wait for sync */
    }
}

/* Read TCC period */
uint16_t TCC2_PWM16bitPeriodGet(void) {
    while (TCC2_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_PER_Msk)) {
        /* Wait for sync */
    }
    return (TCC2_REGS->TCC_PER & 0xFFFF);
}

/* Configure dead time */
void TCC2_PWMDeadTimeSet(uint8_t deadtime_high, uint8_t deadtime_low) {
    TCC2_REGS->TCC_WEXCTRL &= ~(TCC_WEXCTRL_DTHS_Msk | TCC_WEXCTRL_DTLS_Msk);
    TCC2_REGS->TCC_WEXCTRL |=
        TCC_WEXCTRL_DTHS(deadtime_high) | TCC_WEXCTRL_DTLS(deadtime_low);
}

void TCC2_PWMPatternSet(uint8_t pattern_enable, uint8_t pattern_output) {
    TCC2_REGS->TCC_PATTBUF = (uint16_t)(pattern_enable | (pattern_output << 8));
    while ((TCC2_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_PATT_Msk)) ==
           TCC_SYNCBUSY_PATT_Msk) {
        /* Wait for sync */
    }
}

/* Set the counter*/
void TCC2_PWM16bitCounterSet(uint16_t count_value) {
    TCC2_REGS->TCC_COUNT = count_value & 0xFFFF;
    while (TCC2_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_COUNT_Msk)) {
        /* Wait for sync */
    }
}

/* Enable forced synchronous update */
void TCC2_PWMForceUpdate(void) {
    TCC2_REGS->TCC_CTRLBSET |= TCC_CTRLBCLR_CMD_UPDATE;
    while (TCC2_REGS->TCC_SYNCBUSY & (TCC_SYNCBUSY_CTRLB_Msk)) {
        /* Wait for sync */
    }
}

/* Enable the period interrupt - overflow or underflow interrupt */
void TCC2_PWMPeriodInterruptEnable(void) {
    TCC2_REGS->TCC_INTENSET = TCC_INTENSET_OVF_Msk;
}

/* Disable the period interrupt - overflow or underflow interrupt */
void TCC2_PWMPeriodInterruptDisable(void) {
    TCC2_REGS->TCC_INTENCLR = TCC_INTENCLR_OVF_Msk;
}

/* Register callback function */
void TCC2_PWMCallbackRegister(TCC_CALLBACK callback, uintptr_t context) {
    TCC2_CallbackObj.callback_fn = callback;
    TCC2_CallbackObj.context = context;
}

/* Interrupt Handler */
void __attribute__((used)) TCC2_Handler(void) {
    uint32_t status;
    status = TCC2_REGS->TCC_INTFLAG;
    /* Clear interrupt flags */
    TCC2_REGS->TCC_INTFLAG = TCC_INTFLAG_Msk;
    if (TCC2_CallbackObj.callback_fn != NULL) {
        TCC2_CallbackObj.callback_fn(status, TCC2_CallbackObj.context);
    }
}
