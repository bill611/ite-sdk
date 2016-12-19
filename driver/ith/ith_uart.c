/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 * HAL UART functions.
 *
 * @author Jim Tan
 * @version 1.0
 */
#include "ith_cfg.h"

void ithUartSetMode(
    ITHUartPort port,
    ITHUartMode mode,
    unsigned int txPin,
    unsigned int rxPin)
{
    int txgpiomode = -1, rxgpiomode=-1;

    switch(port)
    {
    case ITH_UART0:
#if defined(CFG_CHIP_PKG_IT9910) || defined(CFG_CHIP_PKG_IT9070)
        ithWriteRegMaskA(ITH_GPIO_BASE + ITH_GPIO_HOSTSEL_REG, ITH_GPIO_HOSTSEL_GPIO << ITH_GPIO_HOSTSEL_BIT, ITH_GPIO_HOSTSEL_MASK);

#ifdef CFG_CHIP_PKG_IT9910
        if (mode != ITH_UART_TX)
            ithGpioSetMode(0, ITH_GPIO_MODE1);

        ithGpioSetMode(1, ITH_GPIO_MODE1);
#else
        if (mode != ITH_UART_TX)
            ithGpioSetMode(2, ITH_GPIO_MODE1);

        ithGpioSetMode(3, ITH_GPIO_MODE1);

        // IrDA
        if (mode != ITH_UART_DEFAULT)
        {
            ithGpioSetMode(6, ITH_GPIO_MODE1);

            if (mode == ITH_UART_FIR)
                ithSetRegBitH(ITH_UART_CLK_REG, ITH_UART_CLK_SRC_BIT);
        }
#endif
#endif
        txgpiomode = ITH_GPIO_MODE_TX0;
        rxgpiomode = ITH_GPIO_MODE_RX0;
        break;
    case ITH_UART1:
        txgpiomode = ITH_GPIO_MODE_TX1;
        rxgpiomode = ITH_GPIO_MODE_RX1;
        break;
    case ITH_UART2:
        txgpiomode = ITH_GPIO_MODE_TX2;
        rxgpiomode = ITH_GPIO_MODE_RX2;
        break;
    case ITH_UART3:
        txgpiomode = ITH_GPIO_MODE_TX3;
        rxgpiomode = ITH_GPIO_MODE_RX3;
        break;
    default:
        txgpiomode = ITH_GPIO_MODE_TX1;
        rxgpiomode = ITH_GPIO_MODE_RX1;
        break;
    }
    // cannot be IrDA mode
    if (mode != ITH_UART_TX && rxPin != -1)
    {
        ithGpioSetMode(rxPin, rxgpiomode);
        ithGpioSetIn(rxPin);
    }
    ithGpioSetMode(txPin, txgpiomode);
    ithGpioSetOut(txPin);

    ithWriteRegMaskA(port + ITH_UART_MDR_REG, mode, ITH_UART_MDR_MODE_SEL_MASK);
}

void ithUartReset(
    ITHUartPort port,
    unsigned int baud,
    ITHUartParity parity,
    unsigned int stop,
    unsigned int len)
{
    unsigned int totalDiv, intDiv, fDiv;
    uint32_t lcr;

    // Power on clock
    ithSetRegBitH(ITH_UART_CLK_REG, ITH_UART_CLK_BIT);

    // Temporarily setting?
    #if (CFG_CHIP_FAMILY != 9850)
    if (port == ITH_UART0)
        ithWriteRegMaskA(ITH_GPIO_BASE + ITH_GPIO_HOSTSEL_REG, ITH_GPIO_HOSTSEL_GPIO << ITH_GPIO_HOSTSEL_BIT, ITH_GPIO_HOSTSEL_MASK);
    #endif

    totalDiv = ithGetBusClock() / baud;
    intDiv = totalDiv >> 4;
    fDiv = totalDiv & 0xF;

    lcr = ithReadRegA(port + ITH_UART_LCR_REG) & ~ITH_UART_LCR_DLAB;

    // Set DLAB = 1
    ithWriteRegA(port + ITH_UART_LCR_REG, ITH_UART_LCR_DLAB);

    // Set baud rate
    ithWriteRegA(port + ITH_UART_DLM_REG, (intDiv & 0xF00) >> 8);
    ithWriteRegA(port + ITH_UART_DLL_REG, intDiv & 0xFF);

    // Set fraction rate
    ithWriteRegA(port + ITH_UART_DLH_REG, fDiv & 0xF);

    // Clear orignal parity setting
    lcr &= 0xC0;

    switch (parity)
    {
    case ITH_UART_ODD:
        lcr |= ITH_UART_LCR_ODD;
        break;

    case ITH_UART_EVEN:
        lcr |= ITH_UART_LCR_EVEN;
        break;

    case ITH_UART_MARK:
        lcr |= ITH_UART_LCR_STICKPARITY | ITH_UART_LCR_ODD;
        break;

    case ITH_UART_SPACE:
        lcr |= ITH_UART_LCR_STICKPARITY | ITH_UART_LCR_EVEN;
        break;

    default:
        break;
    }

    if (stop == 2)
        lcr |= ITH_UART_LCR_STOP;

    lcr |= len - 5;

    ithWriteRegA(port + ITH_UART_LCR_REG, lcr);

    ithUartFifoCtrlEnable(port, ITH_UART_FIFO_EN);  // enable fifo as default
    ithUartSetTxTriggerLevel(port, ITH_UART_TRGL2); // default to maximum level
    ithUartSetRxTriggerLevel(port, ITH_UART_TRGL0); // default to maximum level

}
