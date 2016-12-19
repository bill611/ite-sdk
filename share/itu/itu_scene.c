#include <sys/ioctl.h>
#include <assert.h>
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include "itu_cfg.h"
#include "ite/itp.h"
#include "ite/itu.h"
#include "ucl/ucl.h"

void ituSceneInit(ITUScene *scene, ITUWidget *root)
{
    assert(scene);

    memset(scene, 0, sizeof(ITUScene));
    scene->root = root;

    ituScene    = scene;
}

void ituSceneExit(ITUScene *scene)
{
    ITCTree *node;
    assert(scene);

    for (node = (ITCTree *)scene->root; node; node = node->sibling)
    {
        ituWidgetExit(node);
    }

    if (scene->bufferAllocated)
    {
        free(scene->buffer);
    }
}

bool ituSceneUpdate(ITUScene *scene, ITUEvent ev, int arg1, int arg2, int arg3)
{
    bool    result = false;
    ITCTree *node;
    int     i, childCount;
    ITUWidget* children[ITU_WIDGET_CHILD_MAX];

    assert(scene);
    assert(scene->root);

    if (ev == ITU_EVENT_MOUSELONGPRESS && scene->dragged)
        return false;

    if (ev == ITU_EVENT_MOUSEDOWN
        || ev == ITU_EVENT_MOUSEUP
        || ev == ITU_EVENT_MOUSEDOUBLECLICK
        || ev == ITU_EVENT_MOUSEMOVE
        || ev == ITU_EVENT_MOUSELONGPRESS
        || ev == ITU_EVENT_TOUCHSLIDELEFT
        || ev == ITU_EVENT_TOUCHSLIDEUP
        || ev == ITU_EVENT_TOUCHSLIDERIGHT
        || ev == ITU_EVENT_TOUCHSLIDEDOWN
        || ev == ITU_EVENT_TOUCHPINCH)
    {
        if (scene->rotation == ITU_ROT_0)
        {
            scene->lastMouseX = arg2;
            scene->lastMouseY = arg3;
        }
        else
        {
            int x, y;

            if (scene->rotation == ITU_ROT_90)
            {
                x = arg3;
                y = scene->screenWidth - arg2;

                switch (ev)
                {
                case ITU_EVENT_TOUCHSLIDELEFT:
                    ev = ITU_EVENT_TOUCHSLIDEDOWN;
                    break;

                case ITU_EVENT_TOUCHSLIDEUP:
                    ev = ITU_EVENT_TOUCHSLIDELEFT;
                    break;

                case ITU_EVENT_TOUCHSLIDERIGHT:
                    ev = ITU_EVENT_TOUCHSLIDEUP;
                    break;

                case ITU_EVENT_TOUCHSLIDEDOWN:
                    ev = ITU_EVENT_TOUCHSLIDERIGHT;
                    break;
                }
            }
            else if (scene->rotation == ITU_ROT_180)
            {
                x = scene->screenWidth - arg2;
                y = scene->screenHeight - arg3;

                switch (ev)
                {
                case ITU_EVENT_TOUCHSLIDELEFT:
                    ev = ITU_EVENT_TOUCHSLIDERIGHT;
                    break;

                case ITU_EVENT_TOUCHSLIDEUP:
                    ev = ITU_EVENT_TOUCHSLIDEDOWN;
                    break;

                case ITU_EVENT_TOUCHSLIDERIGHT:
                    ev = ITU_EVENT_TOUCHSLIDELEFT;
                    break;

                case ITU_EVENT_TOUCHSLIDEDOWN:
                    ev = ITU_EVENT_TOUCHSLIDEUP;
                    break;
                }
            }
            else // if (scene->rotation == ITU_ROT_270)
            {
                x = scene->screenHeight - arg3;
                y = arg2;

                switch (ev)
                {
                case ITU_EVENT_TOUCHSLIDELEFT:
                    ev = ITU_EVENT_TOUCHSLIDEUP;
                    break;

                case ITU_EVENT_TOUCHSLIDEUP:
                    ev = ITU_EVENT_TOUCHSLIDERIGHT;
                    break;

                case ITU_EVENT_TOUCHSLIDERIGHT:
                    ev = ITU_EVENT_TOUCHSLIDEDOWN;
                    break;

                case ITU_EVENT_TOUCHSLIDEDOWN:
                    ev = ITU_EVENT_TOUCHSLIDELEFT;
                    break;
                }
            }
            scene->lastMouseX = arg2 = x;
            scene->lastMouseY = arg3 = y;
        }
    }

    childCount = 0;
    for (node = &scene->root->tree; node; node = node->sibling)
        children[childCount++] = (ITUWidget *)node;

    switch (ev)
    {
    case ITU_EVENT_LANGUAGE:
    case ITU_EVENT_LOAD_IMAGE:
        while (--childCount >= 0)
        {
            ITUWidget *widget = children[childCount];
            result |= ituWidgetUpdate(widget, ev, arg1, arg2, arg3);
        }
        break;

    case ITU_EVENT_PRESS:
    case ITU_EVENT_KEYDOWN:
    case ITU_EVENT_KEYUP:
    case ITU_EVENT_MOUSEDOWN:
    case ITU_EVENT_MOUSEUP:
    case ITU_EVENT_MOUSEDOUBLECLICK:
    case ITU_EVENT_MOUSEMOVE:
    case ITU_EVENT_TOUCHSLIDELEFT:
    case ITU_EVENT_TOUCHSLIDEUP:
    case ITU_EVENT_TOUCHSLIDERIGHT:
    case ITU_EVENT_TOUCHSLIDEDOWN:
    case ITU_EVENT_MOUSELONGPRESS:
        while (--childCount >= 0)
        {
            ITUWidget *widget = children[childCount];
            if (widget->visible)
            {
                result |= ituWidgetUpdate(widget, ev, arg1, arg2, arg3);
                if (result)
                    break;
            }
        }
        break;

    default:
        for (node = &scene->root->tree; node; node = node->sibling)
        {
            ITUWidget *widget = (ITUWidget *)node;
            if (widget->visible)
                result |= ituWidgetUpdate(widget, ev, arg1, arg2, arg3);
        }
        break;
    }

    if (scene->focused && (ev == ITU_EVENT_KEYUP || ev == ITU_EVENT_MOUSEUP || ev == ITU_EVENT_TOUCHSLIDEUP))
    {
        ITUWidget *focused = scene->focused;
        if (focused->type == ITU_BUTTON
            || focused->type == ITU_CHECKBOX
            || focused->type == ITU_RADIOBOX)
        {
            if (ituButtonIsPressed(focused))
            {
                ituWidgetUpdate(focused, ev, arg1, arg2, arg3);
            }
        }
    }

    if (ev < ITU_EVENT_CUSTOM)
    {
        // flush action queue
        result |= ituFlushActionQueue(scene->actionQueue, &scene->actionQueueLen);

        // execute delay queue
        ituExecDelayQueue(scene->delayQueue);

        // execute delayed commands
        for (i = 0; i < ITU_COMMAND_SIZE; i++)
        {
            ITUCommand *cmd = &scene->commands[i];
            if (cmd->delay > 0)
            {
                if (--cmd->delay == 0)
                {
                    cmd->func(cmd->arg);
                    result = true;
                }
            }
        }
    }

    if (ev == ITU_EVENT_MOUSEUP)
        scene->dragged = NULL;

    return result;
}

void ituSceneDraw(ITUScene *scene, ITUSurface *dest)
{
    ITCTree *node;
    assert(scene);
    assert(scene->root);
    assert(dest);

    for (node = &scene->root->tree; node; node = node->sibling)
    {
        ITUWidget *widget = (ITUWidget *)node;
        if (widget->visible)
            ituWidgetDraw(widget, dest, 0, 0, 255);

        widget->dirty = false;
    }
}

#ifdef CFG_ITU_FT_CACHE_SIZE

static void PreloadFontCache(ITUWidget* widget, ITUSurface* surf)
{
    ITCTree* node;

    for (node = widget->tree.child; node; node = node->sibling)
    {
        ITUWidget* child = (ITUWidget*)node;
        PreloadFontCache(child, surf);
    }

    switch (widget->type)
    {
    case ITU_TEXT:
    case ITU_SCROLLTEXT:
        {
            ITUText* text = (ITUText*)widget;
            char* string;

            if (text->string)
                string = text->string;
            else if (text->stringSet)
                string = text->stringSet->strings[text->lang];

            if (string[0] != '\0')
            {
                if (text->fontHeight > 0)
                    ituFtSetFontSize(text->fontWidth, text->fontHeight);

                ituFtDrawText(surf, 0, 0, string);
            }
        }
        break;

    case ITU_TEXTBOX:
        {
            ITUTextBox* textbox = (ITUTextBox*)widget;
            ITUText* text = &textbox->text;
            char* string;

            if (text->string)
                string = text->string;
            else if (text->stringSet)
                string = text->stringSet->strings[text->lang];

            if (string[0] != '\0')
            {
                if (text->fontHeight > 0)
                    ituFtSetFontSize(text->fontWidth, text->fontHeight);

                ituFtDrawText(surf, 0, 0, string);
            }
        }
        break;

    case ITU_BUTTON:
    case ITU_CHECKBOX:
    case ITU_RADIOBOX:
    case ITU_POPUPBUTTON:
        {
            ITUButton* button = (ITUButton*)widget;
            ITUText* text = &button->text;
            char* string;

            if (text->string)
                string = text->string;
            else if (text->stringSet)
                string = text->stringSet->strings[text->lang];

            if (string[0] != '\0')
            {
                if (text->fontHeight > 0)
                    ituFtSetFontSize(text->fontWidth, text->fontHeight);

                ituFtDrawText(surf, 0, 0, string);
            }
        }
        break;
    }
}
#endif // CFG_ITU_FT_CACHE_SIZE

static void DrawGlyphEmpty(ITUSurface* surf, int x, int y, ITUGlyphFormat format, const uint8_t* bitmap, int w, int h)
{
    // DO NOTHING
}

void ituScenePreDraw(ITUScene *scene, ITUSurface *dest)
{
#ifdef CFG_ITU_FT_CACHE_SIZE
    void (*DrawGlyphPrev)(ITUSurface* surf, int x, int y, ITUGlyphFormat format, const uint8_t* bitmap, int w, int h) = ituDrawGlyph;
    ITCTree *node;
    assert(scene);
    assert(scene->root);
    assert(dest);

    ituDrawGlyph = DrawGlyphEmpty;

    for (node = &scene->root->tree; node; node = node->sibling)
    {
        ITUWidget* widget = (ITUWidget*)node;

        if (widget->flags & ITU_PREDRAW)
        {
            PreloadFontCache(widget, dest);
            LOG_INFO "predraw %s\n", widget->name LOG_END
            //sched_yield();
        }
    }
    ituDrawGlyph = DrawGlyphPrev;

#endif // CFG_ITU_FT_CACHE_SIZE
}

void ituSceneSetRotation(ITUScene *scene, ITURotation rot, int screenWidth, int screenHeight)
{
    assert(scene);

    scene->rotation     = rot;
    scene->screenWidth  = screenWidth;
    scene->screenHeight = screenHeight;

    ituSetRotation(rot);
}

static ITUWidget *FindWidget(ITUWidget *widget, const char *name)
{
    ITCTree *node;

    if (strcmp(name, widget->name) == 0)
        return widget;

    for (node = widget->tree.child; node; node = node->sibling)
    {
        ITUWidget *result = FindWidget((ITUWidget *)node, name);
        if (result)
            return result;
    }
    return NULL;
}

void ituSceneSetFunctionTable(ITUScene *scene, ITUActionFunction *funcTable)
{
    assert(scene);
    scene->actionFuncTable = funcTable;
}

ITUActionFunc ituSceneFindFunction(ITUScene *scene, const char *name)
{
    assert(scene);
    assert(name);

    if (scene->actionFuncTable)
    {
        ITUActionFunction *actionFunc = scene->actionFuncTable;
        while (actionFunc->func)
        {
            if (actionFunc->name == NULL || strcmp(actionFunc->name, name) == 0)
            {
                return actionFunc->func;
            }
            actionFunc++;
        }
    }
    return NULL;
}

void *ituSceneFindWidget(ITUScene *scene, const char *name)
{
    ITCTree *node;
    assert(scene);
    assert(name);

    if (!scene->root)
        return NULL;

    for (node = &scene->root->tree; node; node = node->sibling)
    {
        ITUWidget *result = FindWidget((ITUWidget *)node, name);
        if (result)
            return result;
    }
    return NULL;
}

static void FocusPrev(ITUWidget *widget, ITUWidget *focused, ITUWidget **prev)
{
    ITCTree *node;

    if (widget->flags & ITU_TAPSTOP)
    {
        if (focused)
        {
            if (*prev)
            {
                if ((widget->tabIndex < focused->tabIndex) && (widget->tabIndex > (*prev)->tabIndex))
                    *prev = widget;
            }
            else
            {
                if (widget->tabIndex < focused->tabIndex)
                    *prev = widget;
            }
        }
        else
        {
            if (*prev)
            {
                if (widget->tabIndex > (*prev)->tabIndex)
                    *prev = widget;
            }
            else
            {
                *prev = widget;
            }
        }
    }

    for (node = widget->tree.child; node; node = node->sibling)
    {
        FocusPrev((ITUWidget *)node, focused, prev);
    }
}

ITUWidget *ituSceneFocusPrev(ITUScene *scene)
{
    ITUWidget *prev = NULL;
    ITCTree   *node;

    for (node = &scene->root->tree; node; node = node->sibling)
    {
        FocusPrev((ITUWidget *)node, scene->focused, &prev);
    }

    if (prev == NULL)
        FocusPrev(scene->root, NULL, &prev);

    if (scene->focused)
    {
        ituWidgetSetActive(scene->focused, false);
        ituWidgetUpdate(scene->focused, ITU_EVENT_LAYOUT, 0, 0, 0);
    }

    if (prev)
    {
        ituWidgetSetActive(prev, true);
        ituWidgetUpdate(prev, ITU_EVENT_LAYOUT, 0, 0, 0);
    }
    scene->focused = prev;
    return prev;
}

static void FocusNext(ITUWidget *widget, ITUWidget *focused, ITUWidget **next)
{
    ITCTree *node;

    if (widget->flags & ITU_TAPSTOP)
    {
        if (focused)
        {
            if (*next)
            {
                if ((widget->tabIndex > focused->tabIndex) && (widget->tabIndex < (*next)->tabIndex))
                    *next = widget;
            }
            else
            {
                if (widget->tabIndex > focused->tabIndex)
                    *next = widget;
            }
        }
        else
        {
            if (*next)
            {
                if (widget->tabIndex < (*next)->tabIndex)
                    *next = widget;
            }
            else
            {
                *next = widget;
            }
        }
    }

    for (node = widget->tree.child; node; node = node->sibling)
    {
        FocusNext((ITUWidget *)node, focused, next);
    }
}

ITUWidget *ituSceneFocusNext(ITUScene *scene)
{
    ITUWidget *next = NULL;
    ITCTree   *node;

    for (node = &scene->root->tree; node; node = node->sibling)
    {
        FocusNext((ITUWidget *)node, scene->focused, &next);
    }

    if (next == NULL)
        FocusNext(scene->root, NULL, &next);

    if (scene->focused)
    {
        ituWidgetSetActive(scene->focused, false);
        ituWidgetUpdate(scene->focused, ITU_EVENT_LAYOUT, 0, 0, 0);
    }

    if (next)
    {
        ituWidgetSetActive(next, true);
        ituWidgetUpdate(next, ITU_EVENT_LAYOUT, 0, 0, 0);
    }
    scene->focused = next;
    return next;
}

void ituSceneExecuteCommand(ITUScene *scene, int delay, ITUCommandFunc func, int arg)
{
    int i;
    assert(scene);
    assert(func);

    // try to replace exist command
    for (i = 0; i < ITU_COMMAND_SIZE; i++)
    {
        ITUCommand *cmd = &scene->commands[i];
        if (cmd->func == func && cmd->arg == arg)
        {
            cmd->delay = delay;
            return;
        }
    }

    // add new command
    for (i = 0; i < ITU_COMMAND_SIZE; i++)
    {
        ITUCommand *cmd = &scene->commands[i];
        if (cmd->delay == 0)
        {
            cmd->delay = delay;
            cmd->func  = func;
            cmd->arg   = arg;
            break;
        }
    }
}
