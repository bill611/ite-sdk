#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ite/itu.h"
#include "itu_cfg.h"
#include "itu_private.h"

static const char circleProgressBarName[] = "ITUCircleProgressBar";

static void ituCircleProgressBarExit(ITUWidget* widget)
{
    ITUCircleProgressBar* bar = (ITUCircleProgressBar*) widget;
    assert(widget);
}

bool ituCircleProgressBarUpdate(ITUWidget* widget, ITUEvent ev, int arg1, int arg2, int arg3)
{
    bool result = false;
    ITUCircleProgressBar* bar = (ITUCircleProgressBar*) widget;
    assert(bar);

    if (ev == ITU_EVENT_LAYOUT)
    {
        if (!bar->progressIcon && (bar->progressName[0] != '\0'))
        {
            bar->progressIcon = (ITUIcon*) ituSceneFindWidget(ituScene, bar->progressName);
        }

        if (!bar->percentText && (bar->percentName[0] != '\0'))
        {
            bar->percentText = (ITUText*) ituSceneFindWidget(ituScene, bar->percentName);
        }

        if (bar->percentText)
        {
            char buf[5];

            sprintf(buf, "%i", bar->value);
            ituTextSetString(bar->percentText, buf);

            result = widget->dirty = true;
        }
    }
    result |= ituIconUpdate(widget, ev, arg1, arg2, arg3);
    return widget->visible ? result : false;
}

void ituCircleProgressBarDraw(ITUWidget* widget, ITUSurface* dest, int x, int y, uint8_t alpha)
{
    ITUCircleProgressBar* bar = (ITUCircleProgressBar*) widget;
    ITURectangle prevClip;
    uint8_t progressIconVisible;

    if (bar->progressIcon)
    {
        progressIconVisible = bar->progressIcon->widget.visible;
        bar->progressIcon->widget.visible = false;
    }
    ituBackgroundDraw(widget, dest, x, y, alpha);

    if (bar->progressIcon)
    {
        bar->progressIcon->widget.visible = progressIconVisible;
    }

    ituWidgetSetClipping(widget, dest, x, y, &prevClip);

    x += widget->rect.x;
    y += widget->rect.y;
    alpha = alpha * widget->alpha / 255;

    if (bar->progressIcon && bar->progressIcon->surf && bar->value > 0 && (bar->endAngle != bar->startAngle))
    {
        int i;
        for (i = 0; i <= bar->value; i++)
        {
            float angle;

            if (bar->endAngle > bar->startAngle)
                angle = bar->startAngle + (bar->endAngle - bar->startAngle) * i / (float)bar->maxValue;
            else if (bar->endAngle < bar->startAngle)
                angle = bar->startAngle - (bar->startAngle - bar->endAngle) * i / (float)bar->maxValue;
#if (CFG_CHIP_FAMILY == 9850)
            ituRotate(dest, x + bar->progressIcon->widget.rect.x, y + bar->progressIcon->widget.rect.y, bar->progressIcon->surf, 0, bar->progressIcon->surf->height, angle, 1.0f, 1.0f);
#else
            ituRotate(dest, x + bar->progressIcon->widget.rect.x, y + bar->progressIcon->widget.rect.y + bar->progressIcon->surf->height, bar->progressIcon->surf, 0, bar->progressIcon->surf->height, angle, 1.0f, 1.0f);
#endif
        }
        ituWidgetSetDirty(bar->progressIcon, false);
    }

    ituSurfaceSetClipping(dest, prevClip.x, prevClip.y, prevClip.width, prevClip.height);
    ituDirtyWidget(bar, false);
}

void ituCircleProgressBarOnAction(ITUWidget* widget, ITUActionType action, char* param)
{
    assert(widget);

    switch (action)
    {
    case ITU_ACTION_GOTO:
        ituCircleProgressBarSetValue((ITUCircleProgressBar*)widget, atoi(param));
        break;

    default:
        ituWidgetOnActionImpl(widget, action, param);
        break;
    }
}

void ituCircleProgressBarInit(ITUCircleProgressBar* bar)
{
    assert(bar);

    memset(bar, 0, sizeof (ITUCircleProgressBar));

    ituBackgroundInit(&bar->bg);

    ituWidgetSetType(bar, ITU_CIRCLEPROGRESSBAR);
    ituWidgetSetName(bar, circleProgressBarName);
    ituWidgetSetExit(bar, ituCircleProgressBarExit);
    ituWidgetSetUpdate(bar, ituCircleProgressBarUpdate);
    ituWidgetSetDraw(bar, ituCircleProgressBarDraw);
    ituWidgetSetOnAction(bar, ituCircleProgressBarOnAction);
}

void ituCircleProgressBarLoad(ITUCircleProgressBar* bar, uint32_t base)
{
    assert(bar);

    ituBackgroundLoad(&bar->bg, base);

    if (bar->progressIcon)
        bar->progressIcon = (ITUIcon*)((uint32_t)bar->progressIcon + base);

    if (bar->percentText)
        bar->percentText = (ITUText*)((uint32_t)bar->percentText + base);

    ituWidgetSetExit(bar, ituCircleProgressBarExit);
    ituWidgetSetUpdate(bar, ituCircleProgressBarUpdate);
    ituWidgetSetDraw(bar, ituCircleProgressBarDraw);
    ituWidgetSetOnAction(bar, ituCircleProgressBarOnAction);
}

void ituCircleProgressBarSetValue(ITUCircleProgressBar* bar, int value)
{
    assert(bar);

    if (value < 0 || value > bar->maxValue)
    {
        LOG_WARN "incorrect value: %d\n", value LOG_END
        return;
    }
    bar->value = value;

    ituWidgetUpdate(bar, ITU_EVENT_LAYOUT, 0, 0, 0);
    ituWidgetSetDirty(bar, true);
}
