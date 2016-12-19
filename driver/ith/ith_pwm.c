/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 * HAL PWM functions.
 *
 * @author Jim Tan
 * @version 1.0
 */
#include "ith_cfg.h"

static uint32_t blCounts[6], blMatchs[6];

void ithPwmInit(ITHPwm pwm, unsigned int freq, unsigned int duty)
{
    blCounts[pwm]   = ithGetBusClock() / freq;
    blMatchs[pwm]    = (uint64_t) blCounts[pwm] * duty / 100;
}

void ithPwmReset(ITHPwm pwm, unsigned int pin, unsigned int gpio_mode)
{
    ithGpioSetMode(pin, gpio_mode);
    ithTimerReset(pwm);
    ithTimerSetCounter(pwm, 0);
    ithTimerSetPwmMatch(pwm, blMatchs[pwm], blCounts[pwm]);

    ithTimerCtrlEnable(pwm, ITH_TIMER_UPCOUNT);
    ithTimerCtrlEnable(pwm, ITH_TIMER_PERIODIC);
    ithTimerCtrlEnable(pwm, ITH_TIMER_PWM);
    ithTimerEnable(pwm);
}

void ithPwmSetDutyCycle(ITHPwm pwm, unsigned int duty)
{
    ithTimerSetPwmMatch(pwm, (uint64_t) blCounts[pwm] * duty / 100, blCounts[pwm]);
}

void ithPwmEnable(ITHPwm pwm, unsigned int pin, unsigned int gpio_mode)
{
    ithGpioSetMode(pin, gpio_mode);
    ithTimerCtrlEnable(pwm, ITH_TIMER_PWM);
    ithTimerCtrlEnable(pwm, ITH_TIMER_EN);
}

void ithPwmDisable(ITHPwm pwm, unsigned int pin)
{
    ithGpioClear(pin);
    ithGpioEnable(pin);
    ithGpioSetOut(pin);
    ithTimerCtrlDisable(pwm, ITH_TIMER_EN);
    ithTimerCtrlDisable(pwm, ITH_TIMER_PWM);
}
