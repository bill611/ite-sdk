#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ite/itu.h"
#include "itu_cfg.h"
#include "itu_private.h"

#ifdef CFG_VIDEO_ENABLE
#include "castor3player.h"
#include "ite/itv.h"
#include "ite/ith_video.h"
#endif // CFG_VIDEO_ENABLE

bool isOtherVideoPlaying = false;

static pthread_t tid;
static const char videoName[] = "ITUVideo";
static bool videoPlayerIsFileEOF = false;
static bool videoloopIsPlaying = false;
static bool isVideoWidgetStartPlaying = false;

#ifdef CFG_VIDEO_ENABLE
static bool videoPlayerSetWindow = false;
static MTAL_SPEC mtal_spec = {0};
#endif

#ifdef CFG_VIDEO_ENABLE
static void ItuEnterVideoState(void)
{
#ifndef DISABLE_SWITCH_VIDEO_STATE
    ituFrameFuncInit();
#endif
}

static void ItuLeaveVideoState(void)
{
#ifndef DISABLE_SWITCH_VIDEO_STATE
    ituFrameFuncExit();
    ituLcdInit();

    #ifdef CFG_M2D_ENABLE
    ituM2dInit();
    #else
    ituSWInit();
    #endif
#endif
}

static void EventHandler(PLAYER_EVENT nEventID, void *arg)
{
    switch(nEventID)
    {
        case PLAYER_EVENT_EOF:
            printf("File EOF\n");
            videoPlayerIsFileEOF = true;
            break;
        case PLAYER_EVENT_OPEN_FILE_FAIL:
            printf("Open file fail\n");
            videoPlayerIsFileEOF = true;
            break;
        case PLAYER_EVENT_UNSUPPORT_FILE:
            printf("File not support\n");
            videoPlayerIsFileEOF = true;
            break;
        default:
            break;
    }
}

static void *polling_video_widget_status(void *arg)
{
    ITUVideo* video = (ITUVideo*)arg;
    while(videoloopIsPlaying)
    {
        if(videoPlayerIsFileEOF)
        {
            videoPlayerIsFileEOF = false;
            mtal_pb_seekto(0);
        }
        usleep(2000);
    }
    pthread_exit(NULL);
}
#endif
static void VideoOnStop(ITUVideo* video)
{
    // DO NOTHING
}

void ituVideoExit(ITUWidget* widget)
{
    ITUVideo* video = (ITUVideo*) widget;
    assert(widget);

#ifdef CFG_VIDEO_ENABLE
    mtal_pb_stop();
    mtal_pb_exit();
    videoloopIsPlaying = false;
    ItuLeaveVideoState();
    if (video->repeat)
    {
#ifndef WIN32        
        if(tid)
#endif                
        {
	        pthread_join(tid, NULL);
#ifndef WIN32                
	        tid = 0;
#endif
        }
    }
#endif // CFG_VIDEO_ENABLE

    ituWidgetExitImpl(widget);
}

bool ituVideoUpdate(ITUWidget* widget, ITUEvent ev, int arg1, int arg2, int arg3)
{
    ITUVideo* video = (ITUVideo*) widget;
    bool result = false;
    assert(video);

    if (ev == ITU_EVENT_TIMER)
    {
        if(!isOtherVideoPlaying && !isVideoWidgetStartPlaying)
        {
#ifdef CFG_VIDEO_ENABLE        
            ItuEnterVideoState();        
            videoPlayerSetWindow = true;
            mtal_pb_init(EventHandler);

            if (video->playing && video->filePath[0] != '\0')
            {
                strcpy(mtal_spec.srcname, video->filePath);
                mtal_pb_select_file(&mtal_spec);
                if (video->repeat)
                {
                    mtal_pb_play_videoloop();
                    videoloopIsPlaying = true;
                    pthread_create(&tid, NULL, polling_video_widget_status, (void *)video);
                }
                else
                {
                	mtal_pb_play();
            	}
                isVideoWidgetStartPlaying = true;
            }
#endif
        } 
        
        if (video->playing)
        {
            if(videoPlayerIsFileEOF)
            {
                if (!video->repeat)
                {
                    videoPlayerIsFileEOF = false;
#ifdef CFG_VIDEO_ENABLE                
                    mtal_pb_stop();
#endif
                }
            }
            result = widget->dirty = true;
        }
    }
    else if (ev == ITU_EVENT_LOAD)
    {
        isVideoWidgetStartPlaying = false;
        if(!isOtherVideoPlaying)
        {
#ifdef CFG_VIDEO_ENABLE        
            ItuEnterVideoState();        
            videoPlayerSetWindow = true;
            mtal_pb_init(EventHandler);

            if (video->playing && video->filePath[0] != '\0')
            {
                strcpy(mtal_spec.srcname, video->filePath);
                mtal_pb_select_file(&mtal_spec);
                if (video->repeat)
                {
                    mtal_pb_play_videoloop();
                    videoloopIsPlaying = true;
                    pthread_create(&tid, NULL, polling_video_widget_status, (void *)video);
                }
                else
                {
                	mtal_pb_play();
            	}
                isVideoWidgetStartPlaying = true;
            }
#endif
        }
    }
    else if (ev == ITU_EVENT_RELEASE)
    {
#ifdef CFG_VIDEO_ENABLE    
        mtal_pb_stop();
        mtal_pb_exit();
        videoloopIsPlaying = false;
        ItuLeaveVideoState();
        if (video->repeat)
        {
#ifndef WIN32        
            if(tid)
#endif                
	        {
		        pthread_join(tid, NULL);
#ifndef WIN32                
		        tid = 0;
#endif
	        }
        }        
#endif    
    }
    else if (ev == ITU_EVENT_LAYOUT)
    {
        result = widget->dirty = true;
    }
    result |= ituWidgetUpdateImpl(widget, ev, arg1, arg2, arg3);
    return widget->visible ? result : false;
}

void ituVideoDraw(ITUWidget* widget, ITUSurface* dest, int x, int y, uint8_t alpha)
{
    int destx, desty;
    ITURectangle prevClip;
    ITUVideo* video = (ITUVideo*) widget;
    ITURectangle* rect = (ITURectangle*) &widget->rect;
    assert(video);
    assert(dest);

    destx = rect->x + x;
    desty = rect->y + y;
#ifdef CFG_VIDEO_ENABLE 
    if (videoPlayerSetWindow)
    {
    	itv_set_video_window(destx, desty, rect->width, rect->height);
    	videoPlayerSetWindow = false;
    }
#endif
    ituWidgetSetClipping(widget, dest, x, y, &prevClip);

#if (CFG_CHIP_FAMILY == 9850) && (CFG_VIDEO_ENABLE)        
    ituDrawVideoSurface(dest, destx, desty, rect->width, rect->height);
#else    
    ituColorFill(dest, destx, desty, rect->width, rect->height, &widget->color);
#endif

    ituSurfaceSetClipping(dest, prevClip.x, prevClip.y, prevClip.width, prevClip.height);
    ituWidgetDrawImpl(widget, dest, x, y, alpha);
}

void ituVideoOnAction(ITUWidget* widget, ITUActionType action, char* param)
{
    assert(widget);

    switch (action)
    {
    case ITU_ACTION_PLAY:
        ituVideoPlay((ITUVideo*)widget, atoi(param));
        break;

    case ITU_ACTION_STOP:
        ituVideoStop((ITUVideo*)widget);
        break;

    case ITU_ACTION_GOTO:
        ituVideoGoto((ITUVideo*)widget, atoi(param));
        break;

    default:
        ituWidgetOnActionImpl(widget, action, param);
        break;
    }
}

void ituVideoInit(ITUVideo* video)
{
    assert(video);

    memset(video, 0, sizeof (ITUVideo));

    ituWidgetInit(&video->widget);

    ituWidgetSetType(video, ITU_VIDEO);
    ituWidgetSetName(video, videoName);
    ituWidgetSetExit(video, ituVideoExit);
    ituWidgetSetUpdate(video, ituVideoUpdate);
    ituWidgetSetDraw(video, ituVideoDraw);
    ituWidgetSetOnAction(video, ituVideoOnAction);
    ituVideoSetOnStop(video, VideoOnStop);
}

void ituVideoLoad(ITUVideo* video, uint32_t base)
{
    assert(video);

    ituWidgetLoad((ITUWidget*)video, base);

    ituWidgetSetExit(video, ituVideoExit);
    ituWidgetSetUpdate(video, ituVideoUpdate);
    ituWidgetSetDraw(video, ituVideoDraw);
    ituWidgetSetOnAction(video, ituVideoOnAction);
    ituVideoSetOnStop(video, VideoOnStop);
}

void ituVideoPlay(ITUVideo* video, int frame)
{
    assert(video);

    if (video->filePath[0] != '\0')
    {
#ifdef CFG_VIDEO_ENABLE
        strcpy(mtal_spec.srcname, video->filePath);
        mtal_pb_select_file(&mtal_spec);
        mtal_pb_play();
#endif // CFG_VIDEO_ENABLE
    }

    if (frame >= 0)
        ituVideoGoto(video, frame);

    video->playing         = true;
    video->widget.dirty    = true;
}

void ituVideoStop(ITUVideo* video)
{
    assert(video);

    if (video->playing)
    {
#ifdef CFG_VIDEO_ENABLE
        mtal_pb_stop();
#endif // CFG_VIDEO_ENABLE

        video->playing         = false;
        video->widget.dirty    = true;
    }
}

void ituVideoGoto(ITUVideo* video, int frame)
{
    assert(video);

    if (video->playing)
    {
#ifdef CFG_VIDEO_ENABLE
        mtal_pb_seekto(frame);
#endif // CFG_VIDEO_ENABLE

        video->widget.dirty    = true;
    }
}
