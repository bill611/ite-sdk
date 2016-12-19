/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 * HAL ISP functions.
 *
 * @author Jim Tan
 * @version 1.0
 */
#include "ith_cfg.h"

void ithIspEnableClock(void)
{
    ithWriteRegMaskH(ITH_ISP_CLK2_REG,
        (0x1 << ITH_EN_N5CLK_BIT) | (0x1 << ITH_EN_M5CLK_BIT) | (0x1 << ITH_EN_ICLK_BIT) | (0x1 << ITH_EN_I1CLK_BIT),
        (0x1 << ITH_EN_N5CLK_BIT) | (0x1 << ITH_EN_M5CLK_BIT) | (0x1 << ITH_EN_ICLK_BIT) | (0x1 << ITH_EN_I1CLK_BIT));
    ithSetRegBitH(ITH_ISP_CLK1_REG, ITH_EN_DIV_ICLK_BIT);
    ithSetRegBitH(ITH_EN_MMIO_REG, ITH_EN_ISP_MMIO_BIT);
}

void ithIspDisableClock(void)
{
    ithWriteRegMaskH(ITH_ISP_CLK2_REG,
        (0x0 << ITH_EN_M5CLK_BIT) | (0x0 << ITH_EN_ICLK_BIT) | (0x0 << ITH_EN_I1CLK_BIT),
        (0x1 << ITH_EN_M5CLK_BIT) | (0x1 << ITH_EN_ICLK_BIT) | (0x1 << ITH_EN_I1CLK_BIT));

    if ((ithReadRegH(ITH_CQ_CLK_REG) & (0x1 << ITH_EN_M3CLK_BIT)) == 0)
        ithClearRegBitH(ITH_ISP_CLK2_REG, ITH_EN_N5CLK_BIT);   // disable N5 clock safely

    ithClearRegBitH(ITH_ISP_CLK1_REG, ITH_EN_DIV_ICLK_BIT);
    ithClearRegBitH(ITH_EN_MMIO_REG, ITH_EN_ISP_MMIO_BIT);
}

void ithIspSetClockSource(ITHClockSource src)
{
    ithClearRegBitH(ITH_ISP_CLK1_REG, ITH_EN_DIV_ICLK_BIT);
    ithWriteRegMaskH(ITH_ISP_CLK1_REG, src << ITH_ICLK_SRC_SEL_BIT, ITH_ICLK_SRC_SEL_MASK);
    ithSetRegBitH(ITH_ISP_CLK1_REG, ITH_EN_DIV_ICLK_BIT);
}

void ithIspResetReg(void)
{
    ithSetRegBitH(ITH_ISP_CLK2_REG, ITH_ISP_REG_RST_BIT);
    ithClearRegBitH(ITH_ISP_CLK2_REG, ITH_ISP_REG_RST_BIT);
}

void ithIspQResetReg(void)
{
    ithSetRegBitH(ITH_ISP_CLK2_REG, ITH_ISPQ_RST_BIT);
    ithClearRegBitH(ITH_ISP_CLK2_REG, ITH_ISPQ_RST_BIT);
}

void ithIspResetEngine(void)
{
    ithSetRegBitH(ITH_ISP_CLK2_REG, ITH_ISP_RST_BIT);
    ithClearRegBitH(ITH_ISP_CLK2_REG, ITH_ISP_RST_BIT);
}

