#include <assert.h>
#include <malloc.h>
#include <pthread.h>
#include <string.h>
#include "ite/itu.h"
#include "itu_cfg.h"
#include "itu_private.h"

static const char iconName[] = "ITUIcon";

void ituIconExit(ITUWidget* widget)
{
    ITUIcon* icon = (ITUIcon*) widget;
    assert(widget);

    if (icon->filePath)
    {
        free(icon->filePath);
        icon->filePath = NULL;
    }

    if (widget->flags & ITU_LOADED)
    {
        if (icon->loadedSurf)
        {
            ituSurfaceRelease(icon->loadedSurf);
            icon->loadedSurf = NULL;
        }
        widget->flags &= ~ITU_LOADED;
    }

    if (icon->surf)
    {
        ituSurfaceRelease(icon->surf);
        icon->surf = NULL;
    }
    ituWidgetExitImpl(widget);
}

bool ituIconClone(ITUWidget* widget, ITUWidget** cloned)
{
    ITUIcon* icon = (ITUIcon*)widget;
    assert(widget);
    assert(cloned);

    if (*cloned == NULL)
    {
        ITUWidget* newWidget = malloc(sizeof(ITUIcon));
        if (newWidget == NULL)
            return false;

        memcpy(newWidget, widget, sizeof(ITUIcon));
        newWidget->tree.child = newWidget->tree.parent = newWidget->tree.sibling = NULL;
        *cloned = newWidget;
    }

    if (!(icon->widget.flags & ITU_EXTERNAL) && icon->staticSurf)
    {
        ITUIcon* newIcon = (ITUIcon*)*cloned;
        ITUSurface* surf = icon->staticSurf;
        if (surf->flags & ITU_COMPRESSED)
            newIcon->surf = NULL;
        else
            newIcon->surf = ituCreateSurface(surf->width, surf->height, surf->pitch, surf->format, (const uint8_t*)surf->addr, surf->flags);

        ituWidgetUpdate(newIcon, ITU_EVENT_LOAD, 0, 0, 0);
    }
    return ituWidgetCloneImpl(widget, cloned);
}

static void IconLoadExternalData(ITUIcon* icon, ITULayer* layer)
{
    ITUWidget* widget = (ITUWidget*)icon;
    ITUSurface* surf;

    assert(widget);

    if (!icon->staticSurf || icon->surf || !(widget->flags & ITU_EXTERNAL))
        return;

    if (!layer)
        layer = ituGetLayer(widget);

    surf = ituLayerLoadExternalSurface(layer, (uint32_t)icon->staticSurf);

    if (surf->flags & ITU_COMPRESSED)
        icon->surf = ituSurfaceDecompress(surf);
    else
        icon->surf = ituCreateSurface(surf->width, surf->height, surf->pitch, surf->format, (const uint8_t*)surf->addr, surf->flags);
}

static void IconLoadImage(ITUIcon* icon, char* path)
{
    ITUWidget* widget = (ITUWidget*)icon;
    ITURectangle* rect = &widget->rect;
    ITUSurface* surf;
    char filepath[PATH_MAX];
    char* ptr;

    assert(widget);

    if (!(widget->flags & ITU_EXTERNAL_IMAGE))
        return;

    strcpy(filepath, path);
    strcat(filepath, widget->name);

    ptr = strrchr(filepath, '.');
    ptr++;
    if (stricmp(ptr, "jpg") == 0)
    {
        surf = ituJpegLoadFile(0, 0, filepath);
    }
    else if (stricmp(ptr, "png") == 0)
    {
        surf = ituPngLoadFile(0, 0, filepath);
    }

    if (surf)
    {
        ituIconReleaseSurface(icon);
        icon->surf = surf;
    }
}

bool ituIconUpdate(ITUWidget* widget, ITUEvent ev, int arg1, int arg2, int arg3)
{
    bool result = false;
    ITUIcon* icon = (ITUIcon*) widget;
    assert(icon);

    if (ev == ITU_EVENT_LOAD)
    {
        ituIconLoadStaticData(icon);
        result = true;
    }
    else if (ev == ITU_EVENT_LOAD_EXTERNAL)
    {
        IconLoadExternalData(icon, (ITULayer*)arg1);
        result = true;
    }
    else if (ev == ITU_EVENT_LOAD_IMAGE)
    {
        IconLoadImage(icon, (char*)arg1);
        result = true;
    }
    else if (ev == ITU_EVENT_RELEASE)
    {
        if (!(widget->flags & ITU_EXTERNAL_IMAGE))
        {
            ituIconReleaseSurface(icon);
            result = true;
        }
    }
    else if (ev == ITU_EVENT_TIMER)
    {
        if (widget->flags & ITU_LOADED)
        {
            if (icon->filePath)
            {
                free(icon->filePath);
                icon->filePath = NULL;
            }

            if (icon->loadedSurf)
            {
                if (icon->surf)
                    ituSurfaceRelease(icon->surf);

                icon->surf = icon->loadedSurf;
                icon->loadedSurf = NULL;
            }
            widget->flags &= ~ITU_LOADED;
            result = true;
        }
    }
    result |= ituWidgetUpdateImpl(widget, ev, arg1, arg2, arg3);
    return result;
}

void ituIconDraw(ITUWidget* widget, ITUSurface* dest, int x, int y, uint8_t alpha)
{
    int destx, desty;
    uint8_t desta;
    ITUIcon* icon = (ITUIcon*) widget;
    ITURectangle* rect = &widget->rect;
    assert(icon);
    assert(dest);

    if (!icon->surf)
    {
        ituWidgetSetDirty(widget, false);
        return;
    }

    destx = rect->x + x;
    desty = rect->y + y;
    desta = alpha * widget->color.alpha / 255;
    desta = desta * widget->alpha / 255;

    if (desta > 0)
    {
        if (desta == 255)
        {
            if (widget->flags & ITU_STRETCH)
            {
#if (CFG_CHIP_FAMILY == 9070)
                if (widget->angle == 0)
                {
                    ituStretchBlt(dest, destx, desty, rect->width, rect->height, icon->surf, 0, 0, icon->surf->width, icon->surf->height);
                }
                else
                {
                    float scaleX = (float)rect->width / icon->surf->width;
                    float scaleY = (float)rect->height / icon->surf->height;

                    ituRotate(dest, destx + rect->width / 2, desty + rect->height / 2, icon->surf, icon->surf->width / 2, icon->surf->height / 2, (float)widget->angle, scaleX, scaleY);
                }
#else
                float scaleX = (float)rect->width / icon->surf->width;
                float scaleY = (float)rect->height / icon->surf->height;

                ituTransform(
                    dest, destx, desty, rect->width, rect->height,
                    icon->surf, 0, 0, icon->surf->width, icon->surf->height,
                    icon->surf->width / 2, icon->surf->height / 2,
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
                if (widget->angle == 0)
                {
                    ituBitBlt(dest, destx, desty, rect->width, rect->height, icon->surf, 0, 0);
                }
                else
                {
#if (CFG_CHIP_FAMILY == 9070)
                    ituRotate(dest, destx + rect->width / 2, desty + rect->height / 2, icon->surf, icon->surf->width / 2, icon->surf->height / 2, (float)widget->angle, 1.0f, 1.0f);
#else
                    ituRotate(dest, destx, desty, icon->surf, icon->surf->width / 2, icon->surf->height / 2, (float)widget->angle, 1.0f, 1.0f);
#endif
                }
            }
        }
        else
        {
            if (widget->flags & ITU_STRETCH)
            {
#if (CFG_CHIP_FAMILY == 9070)
                ITUSurface* surf = ituCreateSurface(rect->width, rect->height, 0, dest->format, NULL, 0);
                if (surf)
                {
                    ituBitBlt(surf, 0, 0, rect->width, rect->height, dest, destx, desty);

                    if (widget->angle == 0)
                    {
                        ituStretchBlt(surf, 0, 0, rect->width, rect->height, icon->surf, 0, 0, icon->surf->width, icon->surf->height);
                    }
                    else
                    {
                        float scaleX = (float)rect->width / icon->surf->width;
                        float scaleY = (float)rect->height / icon->surf->height;

                        ituRotate(surf, rect->width / 2, rect->height / 2, icon->surf, icon->surf->width / 2, icon->surf->height / 2, (float)widget->angle, scaleX, scaleY);
                    }
                    ituAlphaBlend(dest, destx, desty, rect->width, rect->height, surf, 0, 0, desta);                
                    ituDestroySurface(surf);
                }
#else
                float scaleX = (float)rect->width / icon->surf->width;
                float scaleY = (float)rect->height / icon->surf->height;

                ituTransform(
                    dest, destx, desty, rect->width, rect->height,
                    icon->surf, 0, 0, icon->surf->width, icon->surf->height,
                    icon->surf->width / 2, icon->surf->height / 2,
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
                    ituRotate(dest, destx + rect->width / 2, desty + rect->height / 2, icon->surf, icon->surf->width / 2, icon->surf->height / 2, (float)widget->angle, 1.0f, 1.0f);
#else
                    ituRotate(dest, destx, desty, icon->surf, icon->surf->width / 2, icon->surf->height / 2, (float)widget->angle, 1.0f, 1.0f);
#endif
                }
                else
                {
                    ituAlphaBlend(dest, destx, desty, rect->width, rect->height, icon->surf, 0, 0, desta);                
                }
            }
        }
    }
    ituWidgetDrawImpl(widget, dest, x, y, alpha);
}

void ituIconInit(ITUIcon* icon)
{
    assert(icon);

    memset(icon, 0, sizeof (ITUIcon));

    ituWidgetInit(&icon->widget);

    ituWidgetSetType(icon, ITU_ICON);
    ituWidgetSetName(icon, iconName);
    ituWidgetSetExit(icon, ituIconExit);
    ituWidgetSetClone(icon, ituIconClone);
    ituWidgetSetUpdate(icon, ituIconUpdate);
    ituWidgetSetDraw(icon, ituIconDraw);
}

void ituIconLoad(ITUIcon* icon, uint32_t base)
{
    assert(icon);

    ituWidgetLoad((ITUWidget*)icon, base);
    ituWidgetSetExit(icon, ituIconExit);
    ituWidgetSetClone(icon, ituIconClone);
    ituWidgetSetUpdate(icon, ituIconUpdate);
    ituWidgetSetDraw(icon, ituIconDraw);

    if (!(icon->widget.flags & ITU_EXTERNAL) && icon->staticSurf)
    {
        ITUSurface* surf = (ITUSurface*)(base + (uint32_t)icon->staticSurf);
        if (surf->flags & ITU_COMPRESSED)
            icon->surf = NULL;
        else
            icon->surf = ituCreateSurface(surf->width, surf->height, surf->pitch, surf->format, (const uint8_t*)surf->addr, surf->flags);

        icon->staticSurf = surf;
    }
}

void ituIconLoadJpegData(ITUIcon* icon, uint8_t* data, int size)
{
    ITUWidget* widget = (ITUWidget*)icon;
    ITURectangle* rect = &widget->rect;
    ITUSurface* surf;

    assert(icon);

    if (widget->flags & ITU_LOADING)
    {
        widget->flags &= ~ITU_LOADING;
        icon->loadedSurf = NULL;
    }
    else if (widget->flags & ITU_LOADED)
    {
        if (icon->filePath)
        {
            free(icon->filePath);
            icon->filePath = NULL;
        }

        if (icon->loadedSurf)
        {
            ituSurfaceRelease(icon->loadedSurf);
            icon->loadedSurf = NULL;
        }
        widget->flags &= ~ITU_LOADED;
    }
    else
    {
        free(icon->filePath);
        icon->filePath = NULL;

        surf = ituJpegLoad(0, 0, data, size);
        if (surf)
        {
            if (icon->surf)
                ituSurfaceRelease(icon->surf);

            icon->surf = surf;
        }
        ituWidgetSetDirty(icon, true);
    }
}

static void* IconLoadJpegFileTask(void* arg)
{
    ITUWidget* widget = (ITUWidget*) arg;
    ITUIcon* icon = (ITUIcon*)widget;
    ITURectangle* rect = &widget->rect;
    ITUSurface* surf;
    assert(widget);

    LOG_DBG "+IconLoadJpegFileTask\n" LOG_END

    for (;;)
    {
        char filepath[PATH_MAX];

        widget->flags |= ITU_LOADING;

        if (!icon->filePath)
            goto end;

        strcpy(filepath, icon->filePath);

        surf = ituJpegLoadFile(rect->width, rect->height, filepath);

        if (widget->flags & ITU_LOADING)
        {
            icon->loadedSurf = surf;
            if (surf)
                widget->flags |= ITU_LOADED;

            break;
        }
        else
        {
            icon->loadedSurf = NULL;
            if (surf)
                ituSurfaceRelease(surf);
        }
    }

end:
    widget->flags &= ~ITU_LOADING;

    LOG_DBG "-IconLoadJpegFileTask\n" LOG_END
    return NULL;
}

void ituIconLoadJpegFile(ITUIcon* icon, char* filepath)
{
    ITUWidget* widget = (ITUWidget*)icon;
    assert(widget);

    if (widget->flags & ITU_LOADING)
    {
        if (icon->filePath)
            free(icon->filePath);
            
        icon->filePath = strdup(filepath);
    }
    else
    {
        if (widget->flags & ITU_LOADED)
        {
            if (icon->filePath)
            {
                free(icon->filePath);
                icon->filePath = NULL;
            }

            if (icon->loadedSurf)
            {
                ituSurfaceRelease(icon->loadedSurf);
                icon->loadedSurf = NULL;
            }
            widget->flags &= ~ITU_LOADED;
        }

        if (strlen(filepath) > 0)
        {
            pthread_t task;
            pthread_attr_t attr;

            assert(!icon->filePath);
            icon->filePath = strdup(filepath);

            pthread_attr_init(&attr);
            //pthread_attr_setstacksize(&attr, 100000);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            pthread_create(&task, &attr, IconLoadJpegFileTask, icon);
        }
    }
}

static void* IconLoadPngFileTask(void* arg)
{
    ITUWidget* widget = (ITUWidget*) arg;
    ITUIcon* icon = (ITUIcon*)widget;
    ITURectangle* rect = &widget->rect;
    ITUSurface* surf;
    assert(widget);

    LOG_DBG "+IconLoadPngFileTask\n" LOG_END

    for (;;)
    {
        char filepath[PATH_MAX];

        widget->flags |= ITU_LOADING;

        if (!icon->filePath)
            goto end;

        strcpy(filepath, icon->filePath);

        surf = ituPngLoadFile(rect->width, rect->height, filepath);

        if (widget->flags & ITU_LOADING)
        {
            icon->loadedSurf = surf;
            if (surf)
                widget->flags |= ITU_LOADED;

            break;
        }
        else
        {
            icon->loadedSurf = NULL;
            if (surf)
                ituSurfaceRelease(surf);
        }
    }

end:
    widget->flags &= ~ITU_LOADING;

    LOG_DBG "-IconLoadPngFileTask\n" LOG_END
    return NULL;
}

void ituIconLoadPngFile(ITUIcon* icon, char* filepath)
{
    ITUWidget* widget = (ITUWidget*)icon;
    assert(widget);

    if (widget->flags & ITU_LOADING)
    {
        if (icon->filePath)
            free(icon->filePath);
            
        icon->filePath = strdup(filepath);
    }
    else
    {
        if (widget->flags & ITU_LOADED)
        {
            if (icon->filePath)
            {
                free(icon->filePath);
                icon->filePath = NULL;
            }

            if (icon->loadedSurf)
            {
                ituSurfaceRelease(icon->loadedSurf);
                icon->loadedSurf = NULL;
            }
            widget->flags &= ~ITU_LOADED;
        }

        if (strlen(filepath) > 0)
        {
            pthread_t task;
            pthread_attr_t attr;

            assert(!icon->filePath);
            icon->filePath = strdup(filepath);

            pthread_attr_init(&attr);
            //pthread_attr_setstacksize(&attr, 100000);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            pthread_create(&task, &attr, IconLoadPngFileTask, icon);
        }
    }
}

void ituIconLoadStaticData(ITUIcon* icon)
{
    ITUWidget* widget = (ITUWidget*)icon;
    ITUSurface* surf;

    if (!icon->staticSurf || icon->surf || (widget->flags & ITU_EXTERNAL) || (widget->flags & ITU_EXTERNAL_IMAGE))
        return;

    surf = icon->staticSurf;

    if (surf->flags & ITU_COMPRESSED)
        icon->surf = ituSurfaceDecompress(surf);
    else
        icon->surf = ituCreateSurface(surf->width, surf->height, surf->pitch, surf->format, (const uint8_t*)surf->addr, surf->flags);
}

void ituIconReleaseSurface(ITUIcon* icon)
{
    if (icon->surf)
    {
        ituSurfaceRelease(icon->surf);
        icon->surf = NULL;
    }
}
