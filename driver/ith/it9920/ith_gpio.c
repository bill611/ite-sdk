/*
 * Copyright(c) 2015 ITE Tech.Inc.All Rights Reserved.
 */
/** @file
 * HAL GPIO functions.
 *
 * @author Jim Tan
 * @author Benson Lin
 * @author I-Chun Lai
 * @version 1.0
 */
#include "../ith_cfg.h"

static uint32_t gpioRegs[(ITH_GPIO_REV_REG - ITH_GPIO1_PINDIR_REG + 4) / 4];

void ithGpioSetMode(unsigned int pin, ITHGpioMode mode)
{
    // TODO: IMPLEMENT
}

void ithGpioCtrlEnable(unsigned int pin, ITHGpioCtrl ctrl)
{
    // TODO: IMPLEMENT
}

void ithGpioCtrlDisable(unsigned int pin, ITHGpioCtrl ctrl)
{
    // TODO: IMPLEMENT
}

void ithGpioSuspend(void)
{
    // TODO: IMPLEMENT
}

void ithGpioResume(void)
{
    // TODO: IMPLEMENT
}

void ithGpioEnableClock(void)
{
    // enable clock
    ithSetRegBitH(ITH_APB_CLK2_REG, ITH_EN_W1CLK_BIT);
}

void ithGpioDisableClock(void)
{
    // disable clock
    ithClearRegBitH(ITH_APB_CLK2_REG, ITH_EN_W1CLK_BIT);
}

void ithGpioStats(void)
{
    PRINTF("GPIO:\r\n");
    ithPrintRegA(ITH_GPIO_BASE + ITH_GPIO1_DATAOUT_REG, ITH_GPIO_REV_REG - ITH_GPIO1_DATAOUT_REG + sizeof(uint32_t));   //for 9850
}

inline void ithGpioSetIn(unsigned int pin)
{
    // TODO: IMPLEMENT
}

inline void ithGpioSetOut(unsigned int pin)
{
    // TODO: IMPLEMENT
}

inline void ithGpioSet(unsigned int pin)
{
    // TODO: IMPLEMENT
}

inline void ithGpioClear(unsigned int pin)
{
    // TODO: IMPLEMENT
}

inline uint32_t ithGpioGet(unsigned int pin)
{
    // TODO: IMPLEMENT
    return 0;
}

inline void ithGpioEnableIntr(unsigned int pin)
{
    // TODO: IMPLEMENT
}

inline void ithGpioDisableIntr(unsigned int pin)
{
    // TODO: IMPLEMENT
}

inline void ithGpioClearIntr(unsigned int pin)
{
    // TODO: IMPLEMENT
}

inline void ithGpioEnableBounce(unsigned int pin)
{
    // TODO: IMPLEMENT
}

inline void ithGpioDisableBounce(unsigned int pin)
{
    // TODO: IMPLEMENT
}

inline void ithGpioSetDebounceClock(unsigned int clk)
{
    // TODO: IMPLEMENT
}