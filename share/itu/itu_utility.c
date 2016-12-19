#include <assert.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include "ite/itu.h"
#include "itu_cfg.h"
#include "itu_private.h"

unsigned int ituFormat2Bpp(ITUPixelFormat format)
{
    switch (format)
    {
    case ITU_RGB565:
    case ITU_ARGB1555:
    case ITU_ARGB4444:
    case ITU_RGB565A8:
        return 16;
        break;

    case ITU_ARGB8888:
        return 32;
        break;

    case ITU_A8:
        return 8;
        break;

    case ITU_MONO:
        return 1;
        break;

    default:
        return 0;
    }
}

void ituFocusWidgetImpl(ITUWidget* widget)
{
    assert(ituScene);

    if (widget)
    {
        if (ituScene->focused && ituScene->focused != widget)
        {
            ituWidgetSetActive(ituScene->focused, false);
            ituWidgetUpdate(ituScene->focused, ITU_EVENT_LAYOUT, ITU_ACTION_FOCUS, 0, 0);
            ituScene->focused = NULL;
        }
        ituWidgetSetActive(widget, true);
        ituWidgetUpdate(widget, ITU_EVENT_LAYOUT, ITU_ACTION_FOCUS, 0, 0);
        ituScene->focused = widget;
    }
    else
    {
        if (ituScene->focused)
        {
            ituWidgetSetActive(ituScene->focused, false);
            ituWidgetUpdate(ituScene->focused, ITU_EVENT_LAYOUT, ITU_ACTION_FOCUS, 0, 0);
            ituScene->focused = NULL;
        }
    }
}

void ituDirtyWidgetImpl(ITUWidget* widget, bool dirty)
{
    ITCTree* node;

    for (node = widget->tree.child; node; node = node->sibling)
    {
        ITUWidget* child = (ITUWidget*)node;
        ituDirtyWidgetImpl(child, dirty);
    }
    ituWidgetSetDirty(widget, dirty);
}

void ituSetPressedButtonImpl(ITUWidget* widget, bool pressed)
{
    ITCTree* node;

    for (node = widget->tree.child; node; node = node->sibling)
    {
        ITUWidget* child = (ITUWidget*)node;
            ituSetPressedButtonImpl(child, pressed);
    }
    if (widget->type == ITU_BUTTON || widget->type == ITU_CHECKBOX || widget->type == ITU_RADIOBOX || widget->type == ITU_POPUPBUTTON)
    {
        if (pressed)
        {
            if (!ituButtonIsPressed(widget))
                ituButtonSetPressed((ITUButton*)widget, true);
        }
        else
        {
            if (ituButtonIsPressed(widget))
                ituButtonSetPressed((ITUButton*)widget, false);
        }
    }
}

void ituScreenshot(ITUSurface* surf, char* filepath)
{
    FILE* fp = fopen(filepath, "wb");
    if (fp)
    {
        int size = surf->width * surf->height * 3;
        uint8_t *dest = malloc(size);
        if (dest)
        {
            int h;
            uint8_t* src = ituLockSurface(surf, 0, 0, surf->width, surf->height);
            assert(src);

            sprintf(dest, "P6\n%d\n%d\n255\n", surf->width, surf->height);
            fwrite(dest, 1, strlen(dest), fp);

            for (h = 0; h < surf->height; h++) 
            {
                int i, j;
                uint8_t* ptr = src + surf->width * 2 * h;

                // color trasform from RGB565 to RGB888
                for (i = (surf->width-1)*2, j = (surf->width-1)*3; i >= 0 && j >= 0; i -= 2, j -= 3)
                {
                    dest[surf->width * h * 3 + j+0] = ((ptr[i+1]     ) & 0xf8) + ((ptr[i+1] >> 5) & 0x07);
                    dest[surf->width * h * 3 + j+1] = ((ptr[i+0] >> 3) & 0x1c) + ((ptr[i+1] << 5) & 0xe0) + ((ptr[i+1] >> 1) & 0x3);
                    dest[surf->width * h * 3 + j+2] = ((ptr[i+0] << 3) & 0xf8) + ((ptr[i+0] >> 2) & 0x07);
                }
            }
            fwrite(dest, 1, size, fp);
            ituUnlockSurface(surf);
        }
        else
        {
            LOG_ERR "out of memory: %d.\n", size LOG_END
        }
        fclose(fp);
    }
    else
    {
        LOG_ERR "open %s fail.\n", filepath LOG_END
    }
}

ITULayer* ituGetLayer(ITUWidget* widget)
{
    ITUWidget* parent = (ITUWidget*)widget->tree.parent;

    while (parent)
    {
        if (parent->type == ITU_LAYER)
            return (ITULayer*)parent;

        parent = (ITUWidget*)parent->tree.parent;
    }
    return NULL;
}
