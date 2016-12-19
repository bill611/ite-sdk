/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 * HAL USB functions.
 *
 * @author Jim Tan
 * @version 1.0
 */
#include "ith_cfg.h"

void ithUsbSuspend(ITHUsbModule usb)
{
    switch (usb)
    {
    case ITH_USB0:
        ithClearRegBitH(ITH_USB0_PHY_CTRL_REG, ITH_USB0_PHY_OSC_OUT_EN_BIT);
        ithClearRegBitH(ITH_USB0_PHY_CTRL_REG, ITH_USB0_PHY_PLL_ALIV_BIT);
        break;

    case ITH_USB1:
        ithClearRegBitH(ITH_USB1_PHY_CTRL_REG, ITH_USB1_PHY_OSC_OUT_EN_BIT);
        ithClearRegBitH(ITH_USB1_PHY_CTRL_REG, ITH_USB1_PHY_PLL_ALIV_BIT);
        break;
    }
    ithSetRegBitA(usb + ITH_USB_HC_MISC_REG, ITH_USB_HOSTPHY_SUSPEND_BIT);
}

void ithUsbResume(ITHUsbModule usb)
{
    switch (usb)
    {
    case ITH_USB0:
        ithSetRegBitH(ITH_USB0_PHY_CTRL_REG, ITH_USB0_PHY_OSC_OUT_EN_BIT);
        ithSetRegBitH(ITH_USB0_PHY_CTRL_REG, ITH_USB0_PHY_PLL_ALIV_BIT);
        break;

    case ITH_USB1:
        ithSetRegBitH(ITH_USB1_PHY_CTRL_REG, ITH_USB1_PHY_OSC_OUT_EN_BIT);
        ithSetRegBitH(ITH_USB1_PHY_CTRL_REG, ITH_USB1_PHY_PLL_ALIV_BIT);
        break;
    }
    ithClearRegBitA(usb + ITH_USB_HC_MISC_REG, ITH_USB_HOSTPHY_SUSPEND_BIT);
}

void ithUsbEnableClock(void)
{
    ithSetRegBitH(ITH_USB_CLK_REG, ITH_EN_N6CLK_BIT);
    ithSetRegBitH(ITH_USB_CLK_REG, ITH_EN_M11CLK_BIT);
}

void ithUsbDisableClock(void)
{
    ithClearRegBitH(ITH_USB_CLK_REG, ITH_EN_N6CLK_BIT);
    ithClearRegBitH(ITH_USB_CLK_REG, ITH_EN_M11CLK_BIT);
}

