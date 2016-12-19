#include <sys/stat.h>
#include <assert.h>
#include <dirent.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "ite/itu.h"
#include "itu_cfg.h"
#include "itu_private.h"

#define IMAGE_EXTENSIONS "jpg;jpeg"

static const char imageCoverFlowName[] = "ITUImageCoverFlow";

static void* ImageCoverFlowCreateTask(void* arg)
{
    DIR *pdir = NULL;
    ITUCoverFlow* coverflow = (ITUCoverFlow*) arg;
    ITUImageCoverFlow* imagecoverflow = (ITUImageCoverFlow*) arg;
    int count;
    assert(imagecoverflow);

    if (coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_DESTROYING)
        goto end;

    if (strlen(imagecoverflow->path) == 0)
    {
	    LOG_WARN "empty path\n" LOG_END
	    goto end;
    }

    // Open the directory
    pdir = opendir(imagecoverflow->path);
    if (pdir)
    {
        struct dirent *pent;

        // Make this our current directory
        chdir(imagecoverflow->path);

        imagecoverflow->imageIndex = -1;
        imagecoverflow->imageData = NULL;

        count = itcTreeGetChildCount(coverflow);
        if (count > 0)
        {
            // List the contents of the directory
            while ((pent = readdir(pdir)) != NULL)
            {
                bool found = false;
                char *ptr, *saveptr;

                if (coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_DESTROYING)
                    goto end;

                if (pent->d_type == DT_DIR)
                    continue;

                ptr = strrchr(pent->d_name, '.');
                if (ptr)
                {
                    char buf[ITU_EXTENSIONS_SIZE];
                    char* ext;

                    ptr++;

                    strcpy(buf, IMAGE_EXTENSIONS);
                    ext = strtok_r(buf, ";", &saveptr);

                    do
                    {
                        if (stricmp(ptr, ext) == 0)
                        {
                            found = true;
                            break;
                        }
                        ext = strtok_r(NULL, ";", &saveptr);
                    } while (ext);
                }

                if (found)
                {
                    FILE* f;

                    strcpy(imagecoverflow->filePath, imagecoverflow->path);
                    strcat(imagecoverflow->filePath, pent->d_name);

                    f = fopen(imagecoverflow->filePath, "rb");
                    if (f)
                    {
                        struct stat sb;

                        if (fstat(fileno(f), &sb) != -1)
                        {
                            imagecoverflow->imageSize = sb.st_size;
                            imagecoverflow->imageData = malloc(imagecoverflow->imageSize);
                            if (imagecoverflow->imageData)
                            {
                                imagecoverflow->imageSize = fread(imagecoverflow->imageData, 1, imagecoverflow->imageSize, f);
                                imagecoverflow->imageIndex++;
                                coverflow->coverFlowFlags |= ITU_IMAGECOVERFLOW_CREATED;
                            }
                            fclose(f);

                            if (imagecoverflow->imageIndex >= count - 1)
                                break;

                            do
                            {
                                usleep(33000);

                                if (coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_DESTROYING)
                                    goto end;

                            } while (coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_CREATED);
                        }
                    }
                }
            }
        }

        if (coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_DESTROYING)
            goto end;

        if ((coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_CREATED) == 0 &&  imagecoverflow->imageIndex + 1 < count)
        {
            coverflow->coverFlowFlags |= ITU_IMAGECOVERFLOW_CREATE_FINISHED;
        }
    }
    else
    {
        LOG_WARN "opendir(%s) failed\n", imagecoverflow->path LOG_END
	    goto end;
    }

end:
    // Close the directory
    if (pdir)
        closedir(pdir);

    coverflow->coverFlowFlags &= ~ITU_IMAGECOVERFLOW_BUSYING;
    return NULL;
}

static void ImageCoverFlowExit(ITUImageCoverFlow* imagecoverflow)
{
    ITUWidget* widget = (ITUWidget*) imagecoverflow;
    ITUCoverFlow* coverflow = (ITUCoverFlow*) widget;
    ITCTree* node;
    assert(imagecoverflow);

    if (coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_BUSYING)
        coverflow->coverFlowFlags |= ITU_IMAGECOVERFLOW_DESTROYING;

    while (coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_BUSYING)
        usleep(1000);

    free(imagecoverflow->imageData);
    imagecoverflow->imageData = NULL;
    imagecoverflow->imageSize = 0;

    for (node = widget->tree.child; node; node = node->sibling)
    {
        ITUWidget* child = (ITUWidget*)node;
        if (child->visible)
        {
            char* filepath = ituWidgetGetCustomData(child);
            free(filepath);
            ituWidgetSetCustomData(child, NULL);
        }
        else
            break;
    }
    coverflow->coverFlowFlags &= ~ITU_IMAGECOVERFLOW_DESTROYING;
    coverflow->coverFlowFlags &= ~ITU_IMAGECOVERFLOW_CREATED;
    coverflow->coverFlowFlags &= ~ITU_IMAGECOVERFLOW_CREATE_FINISHED;
}


void ituImageCoverFlowExit(ITUWidget* widget)
{
    ITUImageCoverFlow* imagecoverflow = (ITUImageCoverFlow*) widget;
    assert(imagecoverflow);

    ImageCoverFlowExit(imagecoverflow);

    ituWidgetExitImpl(widget);
}

bool ituImageCoverFlowUpdate(ITUWidget* widget, ITUEvent ev, int arg1, int arg2, int arg3)
{
    bool result = false;
    ITUCoverFlow* coverflow = (ITUCoverFlow*) widget;
    ITUImageCoverFlow* imagecoverflow = (ITUImageCoverFlow*) widget;
    assert(imagecoverflow);

    result |= ituCoverFlowUpdate(widget, ev, arg1, arg2, arg3);

    if ((coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_DESTROYING) == 0)
    {
        if (ev == ITU_EVENT_TIMER)
        {
            if (coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_CREATED)
            {
                ITUIcon* icon = (ITUIcon*)itcTreeGetChildAt(widget, imagecoverflow->imageIndex);
                char* filepath = strdup(imagecoverflow->filePath);

                ituIconLoadJpegData(icon, imagecoverflow->imageData, imagecoverflow->imageSize);
                ituWidgetSetCustomData(icon, filepath);

                free(imagecoverflow->imageData);
                imagecoverflow->imageData = NULL;
                imagecoverflow->imageSize = 0;
                coverflow->coverFlowFlags &= ~ITU_IMAGECOVERFLOW_CREATED;
                result = widget->dirty = true;
            }
            else if (coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_CREATE_FINISHED)
            {
                int index = imagecoverflow->imageIndex + 1;
                ITCTree* child = itcTreeGetChildAt(coverflow, index);

                for (; child; child = child->sibling)
                {
                    ITUWidget* childwidget = (ITUWidget*)child;
                    ituWidgetSetVisible(childwidget, false);
                }
                ituWidgetUpdate(imagecoverflow, ITU_EVENT_LAYOUT, 0, 0, 0);
                coverflow->coverFlowFlags &= ~ITU_IMAGECOVERFLOW_CREATE_FINISHED;
                result = widget->dirty = true;
            }
        }
    }

    return widget->visible ? result : false;
}

void ituImageCoverFlowDraw(ITUWidget* widget, ITUSurface* dest, int x, int y, uint8_t alpha)
{
    assert(widget);
    assert(dest);

    ituCoverFlowDraw(widget, dest, x, y, alpha);
}

void ituImageCoverFlowInit(ITUImageCoverFlow* imageCoverFlow, ITULayout layout, char* path)
{
    assert(imageCoverFlow);

    memset(imageCoverFlow, 0, sizeof (ITUImageCoverFlow));

    ituCoverFlowInit(&imageCoverFlow->coverFlow, layout);

    ituWidgetSetType(imageCoverFlow, ITU_IMAGECOVERFLOW);
    ituWidgetSetName(imageCoverFlow, imageCoverFlowName);
    ituWidgetSetExit(imageCoverFlow, ituImageCoverFlowExit);
    ituWidgetSetUpdate(imageCoverFlow, ituImageCoverFlowUpdate);
    ituWidgetSetDraw(imageCoverFlow, ituImageCoverFlowDraw);
    ituWidgetSetOnAction(imageCoverFlow, ituImageCoverFlowOnAction);

    if (path)
    {
        ITUCoverFlow* coverflow = (ITUCoverFlow*) imageCoverFlow;
        pthread_t task;
        pthread_attr_t attr;

        strncpy(imageCoverFlow->path, path, ITU_PATH_MAX - 1);

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&task, &attr, ImageCoverFlowCreateTask, imageCoverFlow);
        coverflow->coverFlowFlags |= ITU_IMAGECOVERFLOW_BUSYING;
    }
}

void ituImageCoverFlowLoad(ITUImageCoverFlow* imageCoverFlow, uint32_t base)
{
    assert(imageCoverFlow);

    ituCoverFlowLoad(&imageCoverFlow->coverFlow, base);

    ituWidgetSetExit(imageCoverFlow, ituImageCoverFlowExit);
    ituWidgetSetUpdate(imageCoverFlow, ituImageCoverFlowUpdate);
    ituWidgetSetDraw(imageCoverFlow, ituImageCoverFlowDraw);
    ituWidgetSetOnAction(imageCoverFlow, ituImageCoverFlowOnAction);

    if (strlen(imageCoverFlow->path) > 0)
    {
        ITUCoverFlow* coverflow = (ITUCoverFlow*) imageCoverFlow;
        pthread_t task;
        pthread_attr_t attr;

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&task, &attr, ImageCoverFlowCreateTask, imageCoverFlow);
        coverflow->coverFlowFlags |= ITU_IMAGECOVERFLOW_BUSYING;
    }
}

void ituImageCoverFlowOnAction(ITUWidget* widget, ITUActionType action, char* param)
{
    assert(widget);

    switch (action)
    {
    case ITU_ACTION_RELOAD:
        ituImageCoverFlowReload((ITUImageCoverFlow*)widget);
        break;

    default:
        ituCoverFlowOnAction(widget, action, param);
        break;
    }
}

void ituImageCoverFlowReload(ITUImageCoverFlow* imagecoverflow)
{
    ITUCoverFlow* coverflow = (ITUCoverFlow*) imagecoverflow;
    assert(imagecoverflow);

    if ((coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_BUSYING) ||
        (coverflow->coverFlowFlags & ITU_IMAGECOVERFLOW_DESTROYING))
        return;

    if (strlen(imagecoverflow->path) > 0)
    {
        ITUCoverFlow* coverflow = (ITUCoverFlow*) imagecoverflow;
        pthread_t task;
        pthread_attr_t attr;

        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&task, &attr, ImageCoverFlowCreateTask, imagecoverflow);
        coverflow->coverFlowFlags |= ITU_IMAGECOVERFLOW_BUSYING;
    }
}
