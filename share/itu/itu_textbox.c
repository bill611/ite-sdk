#include <assert.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include "ite/itu.h"
#include "itu_cfg.h"
#include "itu_private.h"

static const char textboxName[] = "ITUTextBox";

bool ituTextBoxClone(ITUWidget* widget, ITUWidget** cloned)
{
    ITUTextBox* textbox = (ITUTextBox*)widget;
    assert(widget);
    assert(cloned);

    if (*cloned == NULL)
    {
        ITUWidget* newWidget = malloc(sizeof(ITUTextBox));
        if (newWidget == NULL)
            return false;

        memcpy(newWidget, widget, sizeof(ITUTextBox));
        newWidget->tree.child = newWidget->tree.parent = newWidget->tree.sibling = NULL;
        *cloned = newWidget;
    }
    return ituTextClone(widget, cloned);
}

void ituTextBoxSetString(ITUTextBox* textbox, char* string)
{
    ituTextSetString(textbox, NULL);
    textbox->cursorIndex = 0;
    if (string)
    {
        ituTextBoxInput(textbox, string);
    }
    else
    {
        ituCopyColor(&textbox->text.widget.color, &textbox->defColor);
    }
}

void ituTextBoxInput(ITUTextBox* textbox, char* input)
{
    ITUText* text = &textbox->text;
    char *ptr, *ptr0;
    int i, len;
    char c, *suffix = NULL;
    assert(textbox);

    if (textbox->maxLen <= 0 || !input)
        return;

    if (*input == '\b') // delete
    {
        ituTextBoxBack(textbox);
        return;
    }

    len = strlen(input);

    if (len > 0)
    {
        if (text->string == NULL)
        {
            text->string = malloc(textbox->maxLen + 1);
            if (text->string == NULL)
                return;

            text->string[0] = '\0';
            textbox->cursorIndex = 0;

            ituCopyColor(&text->widget.color, &textbox->fgColor);
        }

        if (text->string[textbox->cursorIndex] != '\0')
            suffix = strdup(&text->string[textbox->cursorIndex]);
    }

    ptr = input;
    ptr0 = ptr;

    while (*ptr != '\0')
    {
        wchar_t buf;

        len = strlen(ptr);
        len = mbtowc(&buf, ptr, len);

        if (textbox->cursorIndex + len > textbox->maxLen)
            break;

        for (i = 0; i < len; i++)
        {
            c = ptr[i];

            if (c == '\r' ||
                c == '\t' ||
                c == '\177')
                continue;

            if (textbox->textboxFlags & ITU_TEXTBOX_UPPER)
            {
                text->string[textbox->cursorIndex] = (char)toupper(c);
            }
            else if (textbox->textboxFlags & ITU_TEXTBOX_LOWER)
            {
                text->string[textbox->cursorIndex] = (char)tolower(c);
            }
            else
            {
                text->string[textbox->cursorIndex] = (char)c;
            }
            textbox->cursorIndex++;
        }

        ptr0 = ptr;
        ptr += len;
    }

    if (suffix)
    {
        strncpy(&text->string[textbox->cursorIndex], suffix, textbox->maxLen - textbox->cursorIndex);
        free(suffix);
    }
    else if (text->string)
        text->string[textbox->cursorIndex] = '\0';

    ituWidgetSetDirty(textbox, true);
}

void ituTextBoxBack(ITUTextBox* textbox)
{
    ITUText* text = &textbox->text;
    char *ptr, *ptr0;
    int len = 0;
    assert(textbox);

    if (!text->string || textbox->cursorIndex == 0)
        return;

    ptr = text->string;
    ptr0 = ptr;

    while (*ptr != '\0')
    {
        wchar_t buf;

        len = strlen(ptr);
        len = mbtowc(&buf, ptr, len);

        if (ptr == &text->string[textbox->cursorIndex])
            break;

        ptr0 = ptr;
        ptr += len;
    }

    strcpy(ptr0, ptr);
    textbox->cursorIndex -= len;

    if (text->string[0] == '\0')
    {
        free(text->string);
        text->string = NULL;
        ituCopyColor(&text->widget.color, &textbox->defColor);
    }
}

char* ituTextBoxGetString(ITUTextBox* textbox)
{
    ITUText* text = &textbox->text;
    assert(text);

    if (text->string)
        return text->string;
    else
        return NULL;
}

bool ituTextBoxUpdate(ITUWidget* widget, ITUEvent ev, int arg1, int arg2, int arg3)
{
    bool result;
    ITUTextBox* textbox = (ITUTextBox*) widget;
    assert(textbox);

    result = ituTextUpdate(widget, ev, arg1, arg2, arg3);

    if (ev == ITU_EVENT_MOUSEDOWN)
    {
        if (ituWidgetIsEnabled(widget))
        {
            int x = arg2 - widget->rect.x;
            int y = arg3 - widget->rect.y;

            if (ituWidgetIsInside(widget, x, y))
            {
                ituFocusWidget(widget);

                if (textbox->textboxFlags & ITU_TEXTBOX_CURSOR)
                {
                    ITUText* text = &textbox->text;
                    char* orgstr = text->string;
                    char* buf = NULL;

                    if (text->string)
                    {
                        int len = strlen(text->string);
                        int xx = 0;

                        ITURectangle* rect = (ITURectangle*) &widget->rect;
                        int destx, desty, cursorIndex;
                        char *pch, *saveptr;

                        buf = malloc(len + 2);
                        if (!buf)
                            return false;

                        destx = 0;
                        desty = 0;

                        strcpy(buf, orgstr);
                        pch = strtok_r(buf, "\n", &saveptr);
                        cursorIndex = 0;

                        if (text->fontHeight > 0)
                        {
                            ituFtSetCurrentFont(text->fontIndex);
                            ituFtSetFontSize(text->fontWidth, text->fontHeight);
                        }

                        while (pch != NULL)
                        {
                            int size, w;
                            char* pch2 = pch;

                            xx = 0;
                            while (*pch2 != '\0')
                            {
                                size = ituFtGetCharWidth(pch2, &w);

                                if (size == 0)
                                    break;

                                if (destx + xx < x && desty < y)
                                    textbox->cursorIndex = cursorIndex;

                                if (textbox->textboxFlags & ITU_TEXTBOX_WORDWRAP)
                                {
                                    if (xx + w >= rect->width)
                                    {
                                        xx = 0;
                                        desty += textbox->lineHeight;
                                    }
                                }
                                xx += w;
                                pch2 += size;
                                cursorIndex += size;
                            }

                            if (textbox->textboxFlags & ITU_TEXTBOX_MULTILINE)
                            {
                                pch = strtok_r(NULL, "\n", &saveptr);
                                if (pch)
                                {
                                    desty += textbox->lineHeight;
                                    cursorIndex++;
                                }
                            }
                            else
                            {
                                break;
                            }
                        }
                        if (destx + xx <= x && desty <= y)
                            textbox->cursorIndex = cursorIndex;
                    }
                    free(buf);
                }
                ituExecActions((ITUWidget*)textbox, textbox->actions, ev, 0);
            }
        }
    }
    else if (ev >= ITU_EVENT_CUSTOM || ev == ITU_EVENT_TIMER)
    {
        if (ituWidgetIsEnabled(widget))
        {
            ituExecActions(widget, textbox->actions, ev, arg1);
            result |= widget->dirty;
        }
    }
    else if (ituWidgetIsActive(widget) && ituWidgetIsEnabled(widget))
    {
        switch (ev)
        {
        case ITU_EVENT_KEYDOWN:
            if (isascii(arg1))
            {
                char buf[2];
                
                ituFocusWidget(widget);

                buf[0] = (char)arg1;
                buf[1] = '\0';
                ituTextBoxInput(textbox, buf);
                ituExecActions((ITUWidget*)textbox, textbox->actions, ev, arg1);
            }
            break;
        }
        result |= widget->dirty;
    }

    return widget->visible ? result : false;
}

void ituTextBoxDraw(ITUWidget* widget, ITUSurface* dest, int x, int y, uint8_t alpha)
{
    ITUTextBox* textbox = (ITUTextBox*) widget;
    ITUText* text = &textbox->text;
    char* orgstr = text->string;
    char* buf = NULL;
    const int borderSize = 1;
    assert(textbox);
    assert(dest);

    if (text->string)
    {
        int len = strlen(text->string);

        ituFtSetCurrentFont(text->fontIndex);

        if (textbox->textboxFlags & ITU_TEXTBOX_MULTILINE)
        {
            ITURectangle* rect = (ITURectangle*) &widget->rect;
            int destx, desty, cursorIndex;
            uint8_t desta, destbga;
            ITURectangle prevClip;
            char *pch, *saveptr;

            buf = malloc(len + 2);
            if (!buf)
                return;

            destx = rect->x + x;
            desty = rect->y + y;
            desta = alpha * widget->color.alpha / 255;
            desta = desta * widget->alpha / 255;
            destbga = alpha * text->bgColor.alpha / 255;
            destbga = destbga * widget->alpha / 255;

            ituWidgetSetClipping(widget, dest, x, y, &prevClip);

            if (destbga == 255)
            {
                ituColorFill(dest, destx, desty, rect->width, rect->height, &text->bgColor);
            }
            else if (destbga > 0)
            {
                ITUSurface* surf = ituCreateSurface(rect->width, rect->height, 0, dest->format, NULL, 0);
                if (surf)
                {
                    ituColorFill(surf, 0, 0, rect->width, rect->height, &text->bgColor);
                    ituAlphaBlend(dest, destx, desty, rect->width, rect->height, surf, 0, 0, destbga);                
                    ituDestroySurface(surf);
                }
            }

            strcpy(buf, orgstr);
            pch = strtok_r(buf, "\n", &saveptr);
            cursorIndex = 0;

            if (desta == 255)
            {
                int xx;

                ituSetColor(&dest->fgColor, desta, widget->color.red, widget->color.green, widget->color.blue);

                if (text->fontHeight > 0)
                    ituFtSetFontSize(text->fontWidth, text->fontHeight);

                while (pch != NULL)
                {
                    if (textbox->textboxFlags & ITU_TEXTBOX_WORDWRAP || textbox->textboxFlags & ITU_TEXTBOX_CURSOR)
                    {
                        int size, w;
                        char* pch2 = pch;

                        xx = 0;
                        while (*pch2 != '\0')
                        {
                            if (textbox->textboxFlags & ITU_TEXTBOX_CURSOR)
                            {
                                if (textbox->cursorIndex == cursorIndex)
                                    ituColorFill(dest, destx + xx, desty, 1, text->fontHeight, &widget->color);
                            }

                            size = ituFtGetCharWidth(pch2, &w);

                            if (size == 0)
                                break;

                            if (textbox->textboxFlags & ITU_TEXTBOX_WORDWRAP)
                            {
                                if (xx + w >= rect->width)
                                {
                                    xx = 0;
                                    desty += textbox->lineHeight;
                                }
                            }
                            ituFtDrawChar(dest, destx + xx, desty, pch2);
                            xx += w;
                            pch2 += size;
                            cursorIndex += size;
                        }
                    }
                    else
                    {
                        ituFtDrawText(dest, destx + borderSize, desty, pch);
                    }

                    pch = strtok_r(NULL, "\n", &saveptr);
                    if (pch)
                    {
                        desty += textbox->lineHeight;
                        cursorIndex++;
                    }
                }
                if (textbox->textboxFlags & ITU_TEXTBOX_CURSOR)
                {
                    if (textbox->cursorIndex == cursorIndex)
                        ituColorFill(dest, destx + xx, desty, 1, text->fontHeight, &widget->color);
                }
            }
            else if (desta > 0)
            {
                ITUSurface* surf = ituCreateSurface(rect->width, rect->height, 0, dest->format, NULL, 0);
                if (surf)
                {
                    int yy = 0;
                    ituBitBlt(surf, 0, 0, rect->width, rect->height, dest, destx, desty);
                    ituSetColor(&surf->fgColor, desta, widget->color.red, widget->color.green, widget->color.blue);
                    if (text->fontHeight > 0)
                        ituFtSetFontSize(text->fontWidth, text->fontHeight);

                    while (pch != NULL)
                    {
                        if (textbox->textboxFlags & ITU_TEXTBOX_WORDWRAP)
                        {
                            int size, w, xx = 0;
                            char* pch2 = pch;

                            while (*pch2 != '\0')
                            {
                                size = ituFtGetCharWidth(pch2, &w);

                                if (size == 0)
                                    break;

                                if (xx + w >= rect->width)
                                {
                                    xx = 0;
                                    desty += textbox->lineHeight;
                                }

                                ituFtDrawChar(dest, destx + borderSize + xx, desty, pch2);
                                xx += w;
                                pch2 += size;
                            }
                        }
                        else
                            ituFtDrawText(surf, borderSize, yy, pch);

                        pch = strtok_r(NULL, "\n", &saveptr);
                        yy += textbox->lineHeight;
                    }
                    ituAlphaBlend(dest, destx, desty, rect->width, rect->height, surf, 0, 0, desta);                
                    ituDestroySurface(surf);
                }
            }
            ituSurfaceSetClipping(dest, prevClip.x, prevClip.y, prevClip.width, prevClip.height);
            ituWidgetDrawImpl(widget, dest, x, y, alpha);
            free(buf);
        }
        else
        {
            if (textbox->textboxFlags & ITU_TEXTBOX_PASSWORD)
            {
                buf = malloc(len + 2);
                if (!buf)
                    return;

                memset(buf, '*', len);
                buf[len] = '\0';
                text->string = buf;
            }

            ituTextDraw(widget, dest, x + borderSize, y, alpha);

            if (textbox->textboxFlags & ITU_TEXTBOX_CURSOR)
            {
                int size, w, xx = 0, cursorIndex = 0;
                char* pch2 = text->string;

                while (pch2 != NULL)
                {
                    if (textbox->cursorIndex == cursorIndex)
                    {
                        int destx = widget->rect.x + x;
                        int desty = widget->rect.y + y;

                        if (xx < widget->rect.width)
                            ituColorFill(dest, destx + xx, desty, 1, text->fontHeight, &widget->color);

                        break;
                    }

                    size = ituFtGetCharWidth(pch2, &w);

                    if (size == 0)
                        break;

                    xx += w;
                    pch2 += size;
                    cursorIndex += size;
                }
            }
            text->string = orgstr;
            free(buf);
        }
    }
    else if (textbox->textboxFlags & ITU_TEXTBOX_MULTILINE)
    {
        ITURectangle* rect = (ITURectangle*) &widget->rect;
        int len, destx, desty, cursorIndex;
        uint8_t desta, destbga;
        ITURectangle prevClip;
        char *pch, *saveptr;

        if (text->stringSet)
            orgstr = text->stringSet->strings[text->lang];
        else
            return;

        len = strlen(orgstr);

        buf = malloc(len + 2);
        if (!buf)
            return;

        destx = rect->x + x;
        desty = rect->y + y;
        desta = alpha * widget->color.alpha / 255;
        desta = desta * widget->alpha / 255;
        destbga = alpha * text->bgColor.alpha / 255;
        destbga = destbga * widget->alpha / 255;

        ituWidgetSetClipping(widget, dest, x, y, &prevClip);

        if (destbga == 255)
        {
            ituColorFill(dest, destx, desty, rect->width, rect->height, &text->bgColor);
        }
        else if (destbga > 0)
        {
            ITUSurface* surf = ituCreateSurface(rect->width, rect->height, 0, dest->format, NULL, 0);
            if (surf)
            {
                ituColorFill(surf, 0, 0, rect->width, rect->height, &text->bgColor);
                ituAlphaBlend(dest, destx, desty, rect->width, rect->height, surf, 0, 0, destbga);                
                ituDestroySurface(surf);
            }
        }

        strcpy(buf, orgstr);
        pch = strtok_r(buf, "\r\n", &saveptr);
        cursorIndex = 0;

        if (desta == 255)
        {
            int xx = 0;

            ituSetColor(&dest->fgColor, desta, widget->color.red, widget->color.green, widget->color.blue);

            if (text->fontHeight > 0)
                ituFtSetFontSize(text->fontWidth, text->fontHeight);

            while (pch != NULL)
            {
                if (textbox->textboxFlags & ITU_TEXTBOX_WORDWRAP || textbox->textboxFlags & ITU_TEXTBOX_CURSOR)
                {
                    int size, w;
                    char* pch2 = pch;

                    xx = 0;
                    while (*pch2 != '\0')
                    {
                        if (*pch2 == '\r')
                        {
                            pch2++;
                            continue;
                        }

                        if (textbox->textboxFlags & ITU_TEXTBOX_CURSOR)
                        {
                            if (textbox->cursorIndex == cursorIndex)
                                ituColorFill(dest, destx + xx, desty, 1, text->fontHeight, &widget->color);
                        }

                        size = ituFtGetCharWidth(pch2, &w);

                        if (size == 0)
                            break;

                        if (textbox->textboxFlags & ITU_TEXTBOX_WORDWRAP)
                        {
                            if (xx + w >= rect->width)
                            {
                                xx = 0;
                                desty += textbox->lineHeight;
                            }
                        }
                        ituFtDrawChar(dest, destx + xx, desty, pch2);
                        xx += w;
                        pch2 += size;
                        cursorIndex += size;
                    }
                }
                else
                {
                    ituFtDrawText(dest, destx + borderSize, desty, pch);
                }

                pch = strtok_r(NULL, "\r\n", &saveptr);
                if (pch)
                {
                    desty += textbox->lineHeight;
                    cursorIndex++;
                }
            }
            if (textbox->textboxFlags & ITU_TEXTBOX_CURSOR)
            {
                if (textbox->cursorIndex == cursorIndex)
                    ituColorFill(dest, destx + xx, desty, 1, text->fontHeight, &widget->color);
            }
        }
        else if (desta > 0)
        {
            ITUSurface* surf = ituCreateSurface(rect->width, rect->height, 0, dest->format, NULL, 0);
            if (surf)
            {
                int yy = 0;
                ituBitBlt(surf, 0, 0, rect->width, rect->height, dest, destx, desty);
                ituSetColor(&surf->fgColor, desta, widget->color.red, widget->color.green, widget->color.blue);
                if (text->fontHeight > 0)
                    ituFtSetFontSize(text->fontWidth, text->fontHeight);

                while (pch != NULL)
                {
                    if (textbox->textboxFlags & ITU_TEXTBOX_WORDWRAP)
                    {
                        int size, w, xx = 0;
                        char* pch2 = pch;

                        while (*pch2 != '\0')
                        {
                            size = ituFtGetCharWidth(pch2, &w);

                            if (size == 0)
                                break;

                            if (xx + w >= rect->width)
                            {
                                xx = 0;
                                desty += textbox->lineHeight;
                            }

                            ituFtDrawChar(dest, destx + borderSize + xx, desty, pch2);
                            xx += w;
                            pch2 += size;
                        }
                    }
                    else
                        ituFtDrawText(surf, borderSize, yy, pch);

                    pch = strtok_r(NULL, "\r\n", &saveptr);
                    yy += textbox->lineHeight;
                }
                ituAlphaBlend(dest, destx, desty, rect->width, rect->height, surf, 0, 0, desta);                
                ituDestroySurface(surf);
            }
        }
        ituSurfaceSetClipping(dest, prevClip.x, prevClip.y, prevClip.width, prevClip.height);
        ituWidgetDrawImpl(widget, dest, x, y, alpha);
        free(buf);
    }
    else
        ituTextDraw(widget, dest, x + borderSize, y, alpha);

    if (ituWidgetIsActive(widget))
    {
        int destx, desty;
        uint8_t desta;
        ITURectangle* rect = (ITURectangle*) &widget->rect;

        destx = rect->x + x;
        desty = rect->y + y;
        desta = alpha * textbox->focusColor.alpha / 255;

        if (desta == 255)
        {
            ituColorFill(dest, destx, desty, rect->width, borderSize, &textbox->focusColor);
            ituColorFill(dest, destx, desty + rect->height - borderSize, rect->width, borderSize, &textbox->focusColor);
            ituColorFill(dest, destx, desty + borderSize, borderSize, rect->height - borderSize * 2, &textbox->focusColor);
            ituColorFill(dest, destx + rect->width - borderSize, desty + borderSize, borderSize, rect->height - borderSize * 2, &textbox->focusColor);
        }
        else if (desta > 0)
        {
            ITUSurface* surf = ituCreateSurface(rect->width, rect->height, 0, ITU_ARGB4444, NULL, 0);
            if (surf)
            {
                ITUColor black = { 0, 0, 0, 0 };
                ituColorFill(surf, 0, 0, rect->width, rect->height, &black);
                ituColorFill(surf, 0, 0, rect->width, borderSize, &widget->color);
                ituColorFill(surf, 0, rect->height - borderSize, rect->width, borderSize, &widget->color);
                ituColorFill(surf, 0, borderSize, borderSize, rect->height - borderSize * 2, &widget->color);
                ituColorFill(surf, rect->width - borderSize, borderSize, borderSize, rect->height - borderSize * 2, &widget->color);
                ituAlphaBlend(dest, destx, desty, rect->width, rect->height, surf, 0, 0, desta);                
                ituDestroySurface(surf);
            }
        }
    }
}

void ituTextBoxOnAction(ITUWidget* widget, ITUActionType action, char* param)
{
    ITUTextBox* textbox = (ITUTextBox*) widget;
    assert(widget);

    switch (action)
    {
    case ITU_ACTION_INPUT:
        ituTextBoxInput(textbox, param);
        break;

    case ITU_ACTION_BACK:
        ituTextBoxInput(textbox, "\b");
        break;

    case ITU_ACTION_CLEAR:
        ituTextBoxSetString(textbox, NULL);
        break;

    case ITU_ACTION_DODELAY0:
        ituExecActions(widget, textbox->actions, ITU_EVENT_DELAY0, atoi(param));
        break;

    case ITU_ACTION_DODELAY1:
        ituExecActions(widget, textbox->actions, ITU_EVENT_DELAY1, atoi(param));
        break;

    case ITU_ACTION_DODELAY2:
        ituExecActions(widget, textbox->actions, ITU_EVENT_DELAY2, atoi(param));
        break;

    case ITU_ACTION_DODELAY3:
        ituExecActions(widget, textbox->actions, ITU_EVENT_DELAY3, atoi(param));
        break;

    case ITU_ACTION_DODELAY4:
        ituExecActions(widget, textbox->actions, ITU_EVENT_DELAY4, atoi(param));
        break;

    case ITU_ACTION_DODELAY5:
        ituExecActions(widget, textbox->actions, ITU_EVENT_DELAY5, atoi(param));
        break;

    case ITU_ACTION_DODELAY6:
        ituExecActions(widget, textbox->actions, ITU_EVENT_DELAY6, atoi(param));
        break;

    case ITU_ACTION_DODELAY7:
        ituExecActions(widget, textbox->actions, ITU_EVENT_DELAY7, atoi(param));
        break;

    default:
        ituTextOnAction(widget, action, param);
        break;
    }
}

void ituTextBoxInit(ITUTextBox* textbox)
{
    assert(textbox);

    memset(textbox, 0, sizeof (ITUTextBox));

    ituTextInit(&textbox->text);

    ituWidgetSetType(textbox, ITU_TEXTBOX);
    ituWidgetSetName(textbox, textboxName);
    ituWidgetSetClone(textbox, ituTextBoxClone);
    ituWidgetSetUpdate(textbox, ituTextBoxUpdate);
    ituWidgetSetDraw(textbox, ituTextBoxDraw);
    ituWidgetSetOnAction(textbox, ituTextBoxOnAction);

    ituSetColor(&textbox->text.widget.color, 255, 255, 255, 255);
}

void ituTextBoxLoad(ITUTextBox* textbox, uint32_t base)
{
    assert(textbox);

    ituTextLoad(&textbox->text, base);
    
    ituWidgetSetClone(textbox, ituTextBoxClone);
    ituWidgetSetUpdate(textbox, ituTextBoxUpdate);
    ituWidgetSetDraw(textbox, ituTextBoxDraw);
    ituWidgetSetOnAction(textbox, ituTextBoxOnAction);
}

// xb add 20161209
void ituTextBoxShowCursor(ITUTextBox *textbox)
{
    assert(textbox);
	textbox->textboxFlags |= ITU_TEXTBOX_CURSOR;
}
void ituTextBoxHideCursor(ITUTextBox *textbox)
{
    assert(textbox);
	textbox->textboxFlags &= ~ITU_TEXTBOX_CURSOR;
}

