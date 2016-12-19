/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 * PAL Screen functions.
 *
 * @author Jim Tan
 * @version 1.0
 */
#include <errno.h>
#include <unistd.h>
#include "itp_cfg.h"

static ITPTvOut screenLastMode;

#ifdef CFG_LCD_ENABLE

static const uint16_t lcdScript[] =
{
#include "lcd.inc"
};

#endif // CFG_LCD_ENABLE

#ifdef CFG_TVOUT_ENABLE

static const uint16_t tvNtscScript[] =
{
#include "tv_ntsc.inc"
};

static const uint16_t tvPalScript[] =
{
#include "tv_pal.inc"
};

#ifdef CFG_TVOUT_HDMI

static const uint16_t tvNtsc480iScript[] =
{
#include "tv_ntsc_480i.inc"
};

static const uint16_t tvPal576iScript[] =
{
#include "tv_pal_576i.inc"
};

static const uint16_t tvNtsc480pScript[] =
{
#include "tv_ntsc_480p.inc"
};

static const uint16_t tvPal576pScript[] =
{
#include "tv_pal_576p.inc"
};

#endif // CFG_TVOUT_HDMI

static void ScreenTvOut(ITPTvOut mode)
{
    // backup lcd base addresses
#ifdef CFG_LCD_ENABLE
    uint32_t lcdBaseA = ithLcdGetBaseAddrA();
    uint32_t lcdBaseB = ithLcdGetBaseAddrB();
#endif // CFG_LCD_ENABLE

#ifdef CFG_CMDQ_ENABLE
    ithCmdQWaitEmpty();
#endif

    ithTveDisablePower();
    ithTveReset();
    ithIspResetEngine();

#ifdef CFG_LCD_ENABLE
    ithLcdReset();
#endif

    switch (mode)
    {
#ifdef CFG_LCD_ENABLE
    case ITP_TVOUT_LCD:
        ithLcdLoadScriptFirst(lcdScript, ITH_COUNT_OF(lcdScript));
        break;
#endif // CFG_LCD_ENABLE

#ifdef CFG_TVOUT_ENABLE
    case ITP_TVOUT_NTSC:
        ithLcdLoadScriptFirst(tvNtscScript, ITH_COUNT_OF(tvNtscScript));
        break;

    case ITP_TVOUT_PAL:
        ithLcdLoadScriptFirst(tvPalScript, ITH_COUNT_OF(tvPalScript));
        break;
#endif // CFG_TVOUT_ENABLE

#ifdef CFG_TVOUT_HDMI
    case ITP_TVOUT_NTSC_I:
        ithLcdLoadScriptFirst(tvNtsc480iScript, ITH_COUNT_OF(tvNtsc480iScript));
        break;

    case ITP_TVOUT_PAL_I:
        ithLcdLoadScriptFirst(tvPal576iScript, ITH_COUNT_OF(tvPal576iScript));
        break;

    case ITP_TVOUT_NTSC_P:
        ithLcdLoadScriptFirst(tvNtsc480pScript, ITH_COUNT_OF(tvNtsc480pScript));
        break;

    case ITP_TVOUT_PAL_P:
        ithLcdLoadScriptFirst(tvPal576pScript, ITH_COUNT_OF(tvPal576pScript));
        break;
#endif // CFG_TVOUT_HDMI

    default:
        LOG_ERR "unknown tvout mode: %d\n", mode LOG_END
        return;
    }

#ifdef CFG_LCD_ENABLE
    // restore lcd base addresses
    ithLcdSetBaseAddrA(lcdBaseA);
    ithLcdSetBaseAddrB(lcdBaseB);

    ithSetVram(lcdBaseA, 0, CFG_LCD_PITCH * CFG_LCD_HEIGHT);
    ithSetVram(lcdBaseB, 0, CFG_LCD_PITCH * CFG_LCD_HEIGHT);

    ithLcdLoadScriptNext();
#endif // CFG_LCD_ENABLE

    if (mode != ITP_TVOUT_LCD)
        ithTveEnablePower();

    screenLastMode = mode;
}

#endif // CFG_TVOUT_ENABLE

#ifdef CFG_BACKLIGHT_ENABLE
static const unsigned int blDutyCycleTable[] = { CFG_BACKLIGHT_DUTY_CYCLES };
static unsigned int blLastValue = CFG_BACKLIGHT_DEFAULT_DUTY_CYCLE;

#endif // CFG_BACKLIGHT_ENABLE

#ifdef CFG_POWER_SUSPEND

#ifdef CFG_LCD_ENABLE
static uint32_t screenBaseA, screenBaseB;
#endif

static void ScreenSuspend(void)
{
#ifdef CFG_LCD_ENABLE
    // backup lcd base addresses
    screenBaseA = ithLcdGetBaseAddrA();
    screenBaseB = ithLcdGetBaseAddrB();
#endif // CFG_LCD_ENABLE
}

static void ScreenResume(void)
{
#ifdef CFG_TVOUT_ENABLE
    ithTveDisablePower();
    ithTveReset();
    ithIspResetEngine();
#endif // CFG_TVOUT_ENABLE

#ifdef CFG_LCD_ENABLE
    ithLcdReset();
#endif

    switch (screenLastMode)
    {
#ifdef CFG_LCD_ENABLE
    case ITP_TVOUT_LCD:
        ithLcdLoadScriptFirst(lcdScript, ITH_COUNT_OF(lcdScript));
        break;
#endif // CFG_LCD_ENABLE

#ifdef CFG_TVOUT_ENABLE

    case ITP_TVOUT_NTSC:
        ithLcdLoadScriptFirst(tvNtscScript, ITH_COUNT_OF(tvNtscScript));
        break;

    case ITP_TVOUT_PAL:
        ithLcdLoadScriptFirst(tvPalScript, ITH_COUNT_OF(tvPalScript));
        break;

#ifdef CFG_TVOUT_HDMI
    case ITP_TVOUT_NTSC_I:
        ithLcdLoadScriptFirst(tvNtsc480iScript, ITH_COUNT_OF(tvNtsc480iScript));
        break;

    case ITP_TVOUT_PAL_I:
        ithLcdLoadScriptFirst(tvPal576iScript, ITH_COUNT_OF(tvPal576iScript));
        break;

    case ITP_TVOUT_NTSC_P:
        ithLcdLoadScriptFirst(tvNtsc480pScript, ITH_COUNT_OF(tvNtsc480pScript));
        break;

    case ITP_TVOUT_PAL_P:
        ithLcdLoadScriptFirst(tvPal576pScript, ITH_COUNT_OF(tvPal576pScript));
        break;
#endif // CFG_TVOUT_HDMI
#endif // CFG_TVOUT_ENABLE

    default:
        LOG_ERR "unknown screen mode: %d\n", screenLastMode LOG_END
        return;
    }

#ifdef CFG_LCD_ENABLE
    // restore lcd base addresses
    ithLcdSetBaseAddrA(screenBaseA);
    ithLcdSetBaseAddrB(screenBaseB);

    ithSetVram(screenBaseA, 0, CFG_LCD_PITCH * CFG_LCD_HEIGHT);
    ithSetVram(screenBaseB, 0, CFG_LCD_PITCH * CFG_LCD_HEIGHT);

    ithLcdLoadScriptNext();
#endif // CFG_LCD_ENABLE

#ifdef CFG_TVOUT_ENABLE
    if (screenLastMode != ITP_TVOUT_LCD)
        ithTveEnablePower();
#endif // CFG_TVOUT_ENABLE
}

#endif // CFG_POWER_SUSPEND

static int ScreenIoctl(int file, unsigned long request, void* ptr, void* info)
{
    switch (request)
    {
    case ITP_IOCTL_RESET:
#if defined(CFG_LCD_ENABLE) && !defined(CFG_LCD_INIT_ON_BOOTING)
        ithLcdLoadScriptFirst(lcdScript, ITH_COUNT_OF(lcdScript));
#endif
        break;

    case ITP_IOCTL_POST_RESET:
#if defined(CFG_LCD_ENABLE) && !defined(CFG_LCD_INIT_ON_BOOTING)
        ithLcdLoadScriptNext();
#endif
        break;

#ifdef CFG_TVOUT_ENABLE
    case ITP_IOCTL_TVOUT:
        ScreenTvOut((ITPTvOut)ptr);
        break;
#endif // CFG_TVOUT_ENABLE

#ifdef CFG_POWER_SUSPEND
    case ITP_IOCTL_SUSPEND:
        ScreenSuspend();
        break;

    case ITP_IOCTL_RESUME:
        ScreenResume();
        break;
#endif // CFG_POWER_SUSPEND

    default:
        errno = (ITP_DEVICE_SCREEN << ITP_DEVICE_ERRNO_BIT) | 1;
        return -1;
    }
    return 0;
}

const ITPDevice itpDeviceScreen =
{
    ":screen",
    itpOpenDefault,
    itpCloseDefault,
    itpReadDefault,
    itpWriteDefault,
    itpLseekDefault,
    ScreenIoctl,
    NULL
};
