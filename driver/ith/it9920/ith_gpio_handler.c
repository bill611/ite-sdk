/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 * HAL gpio interrupt handler functions.
 *
 * @author Jim Tan
 * @version 1.0
 */
#include "../ith_cfg.h"

static ITHGpioIntrHandler gpioHandlerTable[102];
static void* gpioArgTable[102];

static void GpioDefaultHandler(unsigned int pin, void* arg)
{
    // DO NOTHING
}
    
void ithGpioInit(void)
{
    int i;
    
    for (i = 0; i < 102; ++i)
        gpioHandlerTable[i] = GpioDefaultHandler;
}

void ithGpioRegisterIntrHandler(unsigned int pin, ITHGpioIntrHandler handler, void* arg)
{
	if (pin < 102)
	{
        gpioHandlerTable[pin]   = handler ? handler : GpioDefaultHandler;
        gpioArgTable[pin]       = arg;
    }
}

void ithGpioDoIntr(void)
{
    // TODO: IMPLEMENT
}
