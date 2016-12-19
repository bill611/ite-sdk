#include <assert.h>
#include <malloc.h>
#include <string.h>
#include "ite/itu.h"
#include "itu_cfg.h"
#include "itu_private.h"

static const char bgName[] = "ITUBackground";

bool ituBackgroundClone(ITUWidget* widget, ITUWidget** cloned)
{
    ITUIcon* icon = (ITUIcon*)widget;
    assert(widget);
    assert(cloned);

    if (*cloned == NULL)
    {
        ITUWidget* newWidget = malloc(sizeof(ITUBackground));
        if (newWidget == NULL)
            return false;

        memcpy(newWidget, widget, sizeof(ITUBackground));
        newWidget->tree.child = newWidget->tree.parent = newWidget->tree.sibling = NULL;
        *cloned = newWidget;
    }

    return ituIconClone(widget, cloned);
}

void ituBackgroundDraw(ITUWidget* widget, ITUSurface* dest, int x, int y, uint8_t alpha)
{
    int destx, desty;
    uint8_t desta;
    ITURectangle prevClip;
    ITUBackground* bg = (ITUBackground*) widget;
    ITURectangle* rect = (ITURectangle*) &widget->rect;
    assert(bg);
    assert(dest);

    destx = rect->x + x;
    desty = rect->y + y;
    desta = alpha * widget->color.alpha / 255;
    desta = desta * widget->alpha / 255;

    if (widget->angle == 0)
        ituWidgetSetClipping(widget, dest, x, y, &prevClip);

    if (!bg->icon.surf || 
        (bg->icon.surf->width < widget->rect.width || bg->icon.surf->height < widget->rect.height) ||
        (bg->icon.surf->format == ITU_ARGB1555 || bg->icon.surf->format == ITU_ARGB4444 || bg->icon.surf->format == ITU_ARGB8888))
    {
        if (desta == 255)
        {
            if (bg->graidentMode == ITU_GF_NONE)
                ituColorFill(dest, destx, desty, rect->width, rect->height, &widget->color);
            else
                ituGradientFill(dest, destx, desty, rect->width, rect->height, &widget->color, &bg->graidentColor, bg->graidentMode);
        }
        else if (desta > 0)
        {
            ITUSurface* surf = ituCreateSurface(rect->width, rect->height, 0, dest->format, NULL, 0);
            if (surf)
            {
                if (bg->graidentMode == ITU_GF_NONE)
                    ituColorFill(surf, 0, 0, rect->width, rect->height, &widget->color);
                else
                    ituGradientFill(surf, 0, 0, rect->width, rect->height, &widget->color, &bg->graidentColor, bg->graidentMode);

                ituAlphaBlend(dest, destx, desty, rect->width, rect->height, surf, 0, 0, desta);                
                ituDestroySurface(surf);
            }
        }
    }
    
    if (bg->icon.surf)
    {
        desta = alpha * widget->alpha / 255;
        if (desta > 0)
        {
            if (widget->flags & ITU_STRETCH)
            {
#if (CFG_CHIP_FAMILY == 9070)
                if (desta == 255)
                {
                    if (widget->angle == 0)
                    {
                        ituStretchBlt(dest, destx, desty, rect->width, rect->height, bg->icon.surf, 0, 0, bg->icon.surf->width, bg->icon.surf->height);
                    }
                    else
                    {
                        float scaleX = (float)rect->width / bg->icon.surf->width;
                        float scaleY = (float)rect->height / bg->icon.surf->height;

                        ituRotate(dest, destx + rect->width / 2, desty + rect->height / 2, bg->icon.surf, bg->icon.surf->width / 2, bg->icon.surf->height / 2, (float)widget->angle, scaleX, scaleY);
                    }
                }
                else
                {
                    ITUSurface* surf = ituCreateSurface(rect->width, rect->height, 0, dest->format, NULL, 0);
                    if (surf)
                    {
                        ituBitBlt(surf, 0, 0, rect->width, rect->height, dest, destx, desty);

                        if (widget->angle == 0)
                        {
                            ituStretchBlt(surf, 0, 0, rect->width, rect->height, bg->icon.surf, 0, 0, bg->icon.surf->width, bg->icon.surf->height);
                        }
                        else
                        {
                            float scaleX = (float)rect->width / bg->icon.surf->width;
                            float scaleY = (float)rect->height / bg->icon.surf->height;

                            ituRotate(surf, rect->width / 2, rect->height / 2, bg->icon.surf, bg->icon.surf->width / 2, bg->icon.surf->height / 2, (float)widget->angle, scaleX, scaleY);
                        }
                        ituAlphaBlend(dest, destx, desty, rect->width, rect->height, surf, 0, 0, desta);
                        ituDestroySurface(surf);
                    }
                }
#else
            float scaleX = (float)rect->width / bg->icon.surf->width;
            float scaleY = (float)rect->height / bg->icon.surf->height;

            //printf("scaleX:%f,scaleY:%f (%d %d) (%d %d)\n",scaleX,scaleY,rect->width,rect->height,bg->icon.surf->width,bg->icon.surf->height);
            ituTransform(
                dest, destx, desty, rect->width, rect->height,
                bg->icon.surf, 0, 0, bg->icon.surf->width, bg->icon.surf->height,
                bg->icon.surf->width / 2, bg->icon.surf->height / 2,
                scaleX,
                scaleY,
                (float)widget->angle,
                0,
                true,
                true,
                desta);
#endif
        }
            else
            {
                if (desta == 255 && widget->angle != 0)
                {
#if (CFG_CHIP_FAMILY == 9070)
                    ituRotate(dest, destx + rect->width / 2, desty + rect->height / 2, bg->icon.surf, bg->icon.surf->width / 2, bg->icon.surf->height / 2, (float)widget->angle, 1.0f, 1.0f);
#else
                    ituRotate(dest, destx, desty, bg->icon.surf, bg->icon.surf->width / 2, bg->icon.surf->height / 2, (float)widget->angle, 1.0f, 1.0f);
#endif
                }
                else
                {
                    ituAlphaBlend(dest, destx, desty, bg->icon.surf->width, bg->icon.surf->height, bg->icon.surf, 0, 0, desta);
                }
            }

        }
    }
    if (widget->angle == 0)
        ituSurfaceSetClipping(dest, prevClip.x, prevClip.y, prevClip.width, prevClip.height);

    if (widget->flags & ITU_TRANSFER_ALPHA)
        ituWidgetDrawImpl(widget, dest, x, y, desta);
    else
        ituWidgetDrawImpl(widget, dest, x, y, alpha);
}

void ituBackgroundInit(ITUBackground* bg)
{
    assert(bg);

    memset(bg, 0, sizeof (ITUBackground));

    ituIconInit(&bg->icon);
    
    ituWidgetSetType(bg, ITU_BACKGROUND);
    ituWidgetSetName(bg, bgName);
    ituWidgetSetClone(bg, ituBackgroundClone);
    ituWidgetSetDraw(bg, ituBackgroundDraw);
}

void ituBackgroundLoad(ITUBackground* bg, uint32_t base)
{
    assert(bg);

    ituIconLoad(&bg->icon, base);
    ituWidgetSetClone(bg, ituBackgroundClone);
    ituWidgetSetDraw(bg, ituBackgroundDraw);
}
