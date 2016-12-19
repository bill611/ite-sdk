/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 * HAL JPEG functions.
 *
 * @author Jim Tan
 * @version 1.0
 */
#include "ith_cfg.h"


void
ithJpegVideoEnableClock(
    void)
{
    // enable clock
    ithSetRegBitH(ITH_VIDEO_CLK2_REG, ITH_EN_M7CLK_BIT);
    ithSetRegBitH(ITH_VIDEO_CLK2_REG, ITH_EN_XCLK_BIT);
    ithSetRegBitH(ITH_VIDEO_CLK1_REG, ITH_EN_DIV_XCLK_BIT);
    ithSetRegBitH(ITH_EN_MMIO_REG, ITH_EN_VIDEO_MMIO_BIT);
    ithSetRegBitH(ITH_EN_MMIO_REG, ITH_EN_MPEG_MMIO_BIT);
}

void
ithJpegVideoDisableClock(
    void)
{
    // disable clock
    ithClearRegBitH(ITH_VIDEO_CLK2_REG, ITH_EN_M7CLK_BIT);
    ithClearRegBitH(ITH_VIDEO_CLK2_REG, ITH_EN_XCLK_BIT);
    ithClearRegBitH(ITH_VIDEO_CLK1_REG, ITH_EN_DIV_XCLK_BIT);
    ithClearRegBitH(ITH_EN_MMIO_REG, ITH_EN_VIDEO_MMIO_BIT);
    ithClearRegBitH(ITH_EN_MMIO_REG, ITH_EN_MPEG_MMIO_BIT);
}


void ithJpegEnableClock(void)
{
    // enable clock
    ithSetRegBitH(ITH_JPEG_CLK_REG, ITH_EN_JCLK_BIT);
    ithSetRegBitH(ITH_JPEG_CLK_REG, ITH_EN_DIV_JCLK_BIT);
    ithSetRegBitH(ITH_EN_MMIO_REG, ITH_EN_JPEG_MMIO_BIT);
}

void ithJpegDisableClock(void)
{
    // disable clock
    ithClearRegBitH(ITH_JPEG_CLK_REG, ITH_EN_JCLK_BIT);
    ithClearRegBitH(ITH_EN_MMIO_REG, ITH_EN_JPEG_MMIO_BIT);
}

void ithJpegResetReg(void)
{
    ithSetRegBitH(ITH_VIDEO_CLK2_REG, ITH_JPEG_REG_RST_BIT);
    ithClearRegBitH(ITH_VIDEO_CLK2_REG, ITH_JPEG_REG_RST_BIT);
}

void ithJpegResetEngine(void)
{
    ithSetRegBitH(ITH_VIDEO_CLK2_REG, ITH_JPEG_RST_BIT);
    ithClearRegBitH(ITH_VIDEO_CLK2_REG, ITH_JPEG_RST_BIT);
}
