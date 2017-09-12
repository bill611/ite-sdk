#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "ite/itu.h"
#include "itu_cfg.h"
#include "itu_private.h"


static const char coverFlowName[] = "ITUCoverFlow";

int CoverFlowGetVisibleChildCount(ITUCoverFlow* coverflow)
{
    int i = 0;
    ITCTree *child, *tree = (ITCTree*)coverflow;
    assert(tree);

    for (child = tree->child; child; child = child->sibling)
    {
        ITUWidget* widget = (ITUWidget*)child;
        if (widget->visible)
            i++;
    }
    return i;
}

ITUWidget* CoverFlowGetVisibleChild(ITUCoverFlow* coverflow, int index)
{
    int i = 0;
    ITCTree *child, *tree = (ITCTree*)coverflow;
    assert(tree);

    for (child = tree->child; child; child = child->sibling)
    {
        ITUWidget* widget = (ITUWidget*)child;

		if (widget->visible)
		{
			if (i++ == index)
				return widget;
		}
    }
    return NULL;
}

bool ituCoverFlowUpdate(ITUWidget* widget, ITUEvent ev, int arg1, int arg2, int arg3)
{
    bool result = false;
    ITUCoverFlow* coverFlow = (ITUCoverFlow*) widget;
    assert(coverFlow);

    if ((widget->flags & ITU_TOUCHABLE) && ituWidgetIsEnabled(widget) && (ev == ITU_EVENT_MOUSEDOWN || ev == ITU_EVENT_MOUSEUP))
    {
        int x = arg2 - widget->rect.x;
        int y = arg3 - widget->rect.y;

        if (ituWidgetIsInside(widget, x, y))
            result |= ituFlowWindowUpdate(widget, ev, arg1, arg2, arg3);
    }
    else
    {
        result |= ituFlowWindowUpdate(widget, ev, arg1, arg2, arg3);
    }

    if (widget->flags & ITU_TOUCHABLE) 
    {
        if ((ev == ITU_EVENT_TOUCHSLIDELEFT || ev == ITU_EVENT_TOUCHSLIDERIGHT) && ((coverFlow->coverFlowFlags & ITU_COVERFLOW_VERTICAL) == 0))
        {
            coverFlow->touchCount = 0;

			if (ituWidgetIsEnabled(widget)) // && (widget->flags & ITU_DRAGGABLE))
            {
                int x = arg2 - widget->rect.x;
                int y = arg3 - widget->rect.y;

                if (ituWidgetIsInside(widget, x, y))
                {
					int count = CoverFlowGetVisibleChildCount(coverFlow);
					ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, 0);
					bool boundary_touch = false;
					////try to fix the mouse up shadow(last frame) diff when sliding start(frame 0)
					int offset, absoffset, interval;
					offset = x - coverFlow->touchPos;
					interval = offset / child->rect.width;
					offset -= (interval * child->rect.width);
					absoffset = offset > 0 ? offset : -offset;

					if (absoffset > child->rect.width / 2)
						coverFlow->frame = absoffset / (child->rect.width / coverFlow->totalframe) + 1;
					else if (absoffset)
						coverFlow->frame = coverFlow->totalframe - absoffset / (child->rect.width / coverFlow->totalframe);
					else
						coverFlow->frame = 0;

					if (!(widget->flags & ITU_DRAGGABLE))
						coverFlow->frame = 0;

                    ituSetPressedButton(widget, false);

					if (coverFlow->boundaryAlign)
					{
						int max_neighbor_item = ((widget->rect.width / child->rect.width) - 1) / 2;

						coverFlow->slideCount = 0;

						if (max_neighbor_item == 0)
							max_neighbor_item++;

						if (coverFlow->focusIndex >= max_neighbor_item)
						{
							if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_neighbor_item))
								boundary_touch = true;
						}
						else
							boundary_touch = true;
					}

                    if (ev == ITU_EVENT_TOUCHSLIDELEFT)
                    {
						//ituWidgetSetCustomData(coverFlow, -1);

                        if ((coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE) ||
                            (coverFlow->focusIndex < count - 1))
                        {
                            if (count > 0)
                            {
                                if (widget->flags & ITU_DRAGGING)
                                {
                                    widget->flags &= ~ITU_DRAGGING;
                                    ituScene->dragged = NULL;
                                    coverFlow->inc = 0;
                                }

								if (boundary_touch)
									coverFlow->focusIndex += ((coverFlow->focusIndex < (count - 2)) ? (1) : (0));

								if (coverFlow->boundaryAlign)
									coverFlow->coverFlowFlags |= ITU_COVERFLOW_SLIDING;

                                if (coverFlow->inc == 0)
                                    coverFlow->inc = 0 - child->rect.width;

                                //coverFlow->frame = 1;
                            }
                        }
                        else if (coverFlow->focusIndex >= count - 1)
                        {
                            if ((count) > 0 && !(widget->flags & ITU_DRAGGING) && coverFlow->bounceRatio > 0)
                            {
                                if (coverFlow->inc == 0)
                                    coverFlow->inc = 0 - (child->rect.width / coverFlow->bounceRatio);

                                widget->flags |= ITU_BOUNCING;
                                //coverFlow->frame = 1;
                            }
                        }
                    }
                    else // if (ev == ITU_EVENT_TOUCHSLIDERIGHT)
                    {
						//ituWidgetSetCustomData(coverFlow, 1);

                        if ((coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE) ||
                            (coverFlow->focusIndex > 0))
                        {
                            if (count > 0)
                            {
                                if (widget->flags & ITU_DRAGGING)
                                {
                                    widget->flags &= ~ITU_DRAGGING;
                                    ituScene->dragged = NULL;
                                    coverFlow->inc = 0;
                                }

								if (boundary_touch)
									coverFlow->focusIndex -= ((coverFlow->focusIndex > 1) ? (1) : (0));

								if (coverFlow->boundaryAlign)
									coverFlow->coverFlowFlags |= ITU_COVERFLOW_SLIDING;

                                if (coverFlow->inc == 0)
                                    coverFlow->inc = child->rect.width;

                                //coverFlow->frame = 1;
                            }
                        }
                        else if (coverFlow->focusIndex <= 0)
                        {
                            if (count > 0 && !(widget->flags & ITU_DRAGGING) && coverFlow->bounceRatio > 0)
                            {
                                if (coverFlow->inc == 0)
                                    coverFlow->inc = (child->rect.width / coverFlow->bounceRatio);

                                widget->flags |= ITU_BOUNCING;
                                //coverFlow->frame = 1;
                            }
                        }
                    }
                    result = true;
                }
            }
        }
        else if ((ev == ITU_EVENT_TOUCHSLIDEUP || ev == ITU_EVENT_TOUCHSLIDEDOWN) && (coverFlow->coverFlowFlags & ITU_COVERFLOW_VERTICAL))
        {
            coverFlow->touchCount = 0;

			if (ituWidgetIsEnabled(widget) && (widget->flags & ITU_DRAGGABLE))
            {
                int x = arg2 - widget->rect.x;
                int y = arg3 - widget->rect.y;

                if (ituWidgetIsInside(widget, x, y))
                {
					int count = CoverFlowGetVisibleChildCount(coverFlow);
					ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, 0);
					bool boundary_touch = false;

                    ituSetPressedButton(widget, false);

					if (coverFlow->boundaryAlign)
					{
						int max_neighbor_item = ((widget->rect.height / child->rect.height) - 1) / 2;

						coverFlow->slideCount = 0;

						if (max_neighbor_item == 0)
							max_neighbor_item++;

						if (coverFlow->focusIndex >= max_neighbor_item)
						{
							if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_neighbor_item))
								boundary_touch = true;
						}
						else
							boundary_touch = true;
					}

                    if (ev == ITU_EVENT_TOUCHSLIDEUP)
                    {
                        if ((coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE) ||
                            (coverFlow->focusIndex < count - 1))
                        {
                            if (count > 0)
                            {
                                if (widget->flags & ITU_DRAGGING)
                                {
                                    widget->flags &= ~ITU_DRAGGING;
                                    ituScene->dragged = NULL;
                                    coverFlow->inc = 0;
                                }

								if (boundary_touch)
									coverFlow->focusIndex += ((coverFlow->focusIndex < (count - 2)) ? (1) : (0));

								if (coverFlow->boundaryAlign)
									coverFlow->coverFlowFlags |= ITU_COVERFLOW_SLIDING;

                                if (coverFlow->inc == 0)
                                    coverFlow->inc = 0 - child->rect.height;

                                coverFlow->frame = 1;
                            }
                        }
                        else if (coverFlow->focusIndex >= count - 1)
                        {
                            if (count > 0 && !(widget->flags & ITU_DRAGGING) && coverFlow->bounceRatio > 0)
                            {
                                if (coverFlow->inc == 0)
                                    coverFlow->inc = 0 - (child->rect.height / coverFlow->bounceRatio);

                                widget->flags |= ITU_BOUNCING;
                                coverFlow->frame = 1;
                            }
                        }
                    }
                    else // if (ev == ITU_EVENT_TOUCHSLIDEDOWN)
                    {
                        if ((coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE) ||
                            (coverFlow->focusIndex > 0))
                        {
                            if (count > 0)
                            {
                                if (widget->flags & ITU_DRAGGING)
                                {
                                    widget->flags &= ~ITU_DRAGGING;
                                    ituScene->dragged = NULL;
                                    coverFlow->inc = 0;
                                }

								if (boundary_touch)
									coverFlow->focusIndex -= ((coverFlow->focusIndex > 1) ? (1) : (0));

								if (coverFlow->boundaryAlign)
									coverFlow->coverFlowFlags |= ITU_COVERFLOW_SLIDING;

                                if (coverFlow->inc == 0)
                                    coverFlow->inc = child->rect.height;

                                coverFlow->frame = 1;
                            }
                        }
                        else if (coverFlow->focusIndex <= 0)
                        {
                            if (count > 0 && !(widget->flags & ITU_DRAGGING) && coverFlow->bounceRatio > 0)
                            {
                                if (coverFlow->inc == 0)
                                    coverFlow->inc = (child->rect.height / coverFlow->bounceRatio);

                                widget->flags |= ITU_BOUNCING;
                                coverFlow->frame = 1;
                            }
                        }
                    }
                    result = true;
                }
            }
        }
        else if (ev == ITU_EVENT_MOUSEMOVE)
        {
            if (ituWidgetIsEnabled(widget) && (widget->flags & ITU_DRAGGING))
            {
                int x = arg2 - widget->rect.x;
                int y = arg3 - widget->rect.y;

                if (ituWidgetIsInside(widget, x, y))
                {
                    int i, dist, offset, count = CoverFlowGetVisibleChildCount(coverFlow);
                    
                    if (coverFlow->coverFlowFlags & ITU_COVERFLOW_VERTICAL)
                    {
                        dist = y - coverFlow->touchPos;
                    }
                    else
                    {
                        dist = x - coverFlow->touchPos;
                    }
                    if (dist < 0)
                        dist = -dist;

                    //printf("dist=%d\n", dist);

                    if (dist >= ITU_DRAG_DISTANCE)
                    {
                        ituSetPressedButton(widget, false);
                        ituWidgetUpdate(widget, ITU_EVENT_DRAGGING, 0, 0, 0);
                    }

                    if (coverFlow->coverFlowFlags & ITU_COVERFLOW_VERTICAL)
                    {
                        offset = y - coverFlow->touchPos;
                        //printf("0: offset=%d\n", offset);
                        if (coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE)
                        {
                            int index, count2;
                        
                            count2 = count / 2 + 1;
                            index = coverFlow->focusIndex;

                            for (i = 0; i < count2; ++i)
                            {
                                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                                int fy = widget->rect.height / 2 - child->rect.height / 2;
                                fy += i * child->rect.height;
                                fy += offset;

                                ituWidgetSetY(child, fy);

                                if (index >= count - 1)
                                    index = 0;
                                else
                                    index++;
                            }

                            count2 = count - count2;
                            for (i = 0; i < count2; ++i)
                            {
                                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                                int fy = widget->rect.height / 2 - child->rect.height / 2;
                                fy -= count2 * child->rect.height;
                                fy += i * child->rect.height;
                                fy += offset;

                                ituWidgetSetY(child, fy);

                                if (index >= count - 1)
                                    index = 0;
                                else
                                    index++;
                            }
                        }
                        else
                        {
							//limit the move under non-cycle/Vertical boundaryAlign mode
							bool b_touch = false;

							if (coverFlow->boundaryAlign)
							{
								ITUWidget* child_1 = CoverFlowGetVisibleChild(coverFlow, 0);
								ITUWidget* child_2 = CoverFlowGetVisibleChild(coverFlow, count - 1);
								int child_height = child_1->rect.height;

								if (child_1->rect.y > 0)
									b_touch = true;

								if ((child_2->rect.y + child_height) < widget->rect.height)
									b_touch = true;
							}

                            if (coverFlow->focusIndex <= 0)
                            {
                                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, 0);
                                if (offset >= child->rect.height / coverFlow->bounceRatio)
                                    offset = child->rect.height / coverFlow->bounceRatio;
                            }
                            else if (coverFlow->focusIndex >= count - 1)
                            {
                                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, 0);
                                if (offset <= -child->rect.height / coverFlow->bounceRatio)
                                    offset = -child->rect.height / coverFlow->bounceRatio;
                            }

                            for (i = 0; i < count; ++i)
                            {//[MOVE][Vertical][non-cycle][layout]
                                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, i);
                                int fy = widget->rect.height / 2 - child->rect.height / 2;

								if (coverFlow->boundaryAlign && b_touch)
									break;

								if (coverFlow->boundaryAlign)
								{
									int max_neighbor_item = ((widget->rect.height / child->rect.height) - 1) / 2;
									int max_height_item = widget->rect.height / child->rect.height;
									fy = 0;

									if (max_neighbor_item == 0)
										max_neighbor_item++;

									if (coverFlow->focusIndex > 0)//>= max_neighbor_item) //Bless debug now
									{
										//if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_neighbor_item))
										if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_height_item))		
											fy = widget->rect.height - (count * child->rect.height);
										else
											fy -= child->rect.height * coverFlow->focusIndex;
									}
									else
										fy = 0;
								}
								else
								{
									fy -= child->rect.height * coverFlow->focusIndex;
								}

                                fy += i * child->rect.height;
                                fy += offset;

                                ituWidgetSetY(child, fy);
                            }
                        }
                    }
                    else
                    {
                        offset = x - coverFlow->touchPos;
                        //printf("0: offset=%d\n", offset);
                        if (coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE)
                        {
                            int index, count2;
                            //workaround for wrong left-side display with hide child
                            count2 = count / 2 + 1 - ((offset>0)?(1):(0));
							//count2 = count / 2 + 1;
							index = coverFlow->focusIndex;

                            for (i = 0; i < count2; ++i)
                            {
                                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                                int fx = widget->rect.width / 2 - child->rect.width / 2;
                                fx += i * child->rect.width;
                                fx += offset;

                                ituWidgetSetX(child, fx);

                                if (index >= count - 1)
                                    index = 0;
                                else
                                    index++;
                            }

                            count2 = count - count2;
                            for (i = 0; i < count2; ++i)
                            {
                                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                                int fx = widget->rect.width / 2 - child->rect.width / 2;
                                fx -= count2 * child->rect.width;
                                fx += i * child->rect.width;
                                fx += offset;

                                ituWidgetSetX(child, fx);

                                if (index >= count - 1)
                                    index = 0;
                                else
                                    index++;
                            }
                        }
                        else
                        {
							//limit the move under non-cycle/Horizontal boundaryAlign mode
							bool b_touch = false;

							if (coverFlow->boundaryAlign)
							{
								ITUWidget* child_1 = CoverFlowGetVisibleChild(coverFlow, 0);
								ITUWidget* child_2 = CoverFlowGetVisibleChild(coverFlow, count - 1);
								int child_width = child_1->rect.width;

								if (child_1->rect.x > 0)
									b_touch = true;

								if ((child_2->rect.x + child_width) < widget->rect.width)
									b_touch = true;
							}

                            if (coverFlow->focusIndex <= 0)
                            {
                                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, 0);
                                if (coverFlow->bounceRatio > 0)
                                {
                                    if (offset >= child->rect.width / coverFlow->bounceRatio)
                                        offset = child->rect.width / coverFlow->bounceRatio;
                                }
                                else
                                {
                                    offset = 0;
                                }
                            }
                            else if (coverFlow->focusIndex >= count - 1)
                            {
                                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, 0);
                                if (coverFlow->bounceRatio > 0)
                                {
                                    if (offset <= -child->rect.width / coverFlow->bounceRatio)
                                        offset = -child->rect.width / coverFlow->bounceRatio;
                                }
                                else
                                {
                                    offset = 0;
                                }
                            }

							

                            for (i = 0; i < count; ++i)
                            {
                                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, i);
                                int fx = widget->rect.width / 2 - child->rect.width / 2;

								if (coverFlow->boundaryAlign && b_touch)
									break;
								
								if (coverFlow->boundaryAlign)
								{//[MOVE][Horizontal][non-cycle][layout]
									int max_neighbor_item = ((widget->rect.width / child->rect.width) - 1) / 2;
									int max_width_item = widget->rect.width / child->rect.width;
									fx = 0;

									if (max_neighbor_item == 0)
										max_neighbor_item++;

									

									if (coverFlow->focusIndex > 0) //>= max_neighbor_item) //Bless debug now
									{
										//if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_neighbor_item))
										if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_width_item))
										    fx = widget->rect.width - (count * child->rect.width);
										else
											fx -= child->rect.width * coverFlow->focusIndex;
									}
									else
										fx = 0;
								}
								else
								{
									fx -= child->rect.width * coverFlow->focusIndex;
								}

                                fx += i * child->rect.width;
                                fx += offset;

                                ituWidgetSetX(child, fx);
                            }
                        }
                    }
                    result = true;
                }
            }
        }
        else if (ev == ITU_EVENT_MOUSEDOWN)
        {
            if (ituWidgetIsEnabled(widget) && (widget->flags & ITU_DRAGGABLE) && coverFlow->bounceRatio > 0)
            {
                int x = arg2 - widget->rect.x;
                int y = arg3 - widget->rect.y;

                if (ituWidgetIsInside(widget, x, y))
                {
                    if (coverFlow->coverFlowFlags & ITU_COVERFLOW_VERTICAL)
                    {
                        coverFlow->touchPos = y;
                    }
                    else
                    {
                        coverFlow->touchPos = x;
                    }

                    if (widget->flags & ITU_HAS_LONG_PRESS)
                    {
                        coverFlow->touchCount = 1;
                    }
                    else
                    {
                        widget->flags |= ITU_DRAGGING;
                        ituScene->dragged = widget;
                    }
                    //result = true;
                }
            }
        }
        else if (ev == ITU_EVENT_MOUSEUP)
        {
            if (ituWidgetIsEnabled(widget) && (widget->flags & ITU_DRAGGABLE) && (widget->flags & ITU_DRAGGING))
            {
                int count = CoverFlowGetVisibleChildCount(coverFlow);
                int x = arg2 - widget->rect.x;
                int y = arg3 - widget->rect.y;
                
                result = false; //Bless debug

                if (coverFlow->coverFlowFlags & ITU_COVERFLOW_VERTICAL)
                {
                    if (coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE)
                    {
                        if (!result && count > 0)
                        {
                            ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, 0);

                            if (coverFlow->inc == 0)
                            {
                                int offset, absoffset, interval;
                                
                                offset = y - coverFlow->touchPos;
                                interval = offset / child->rect.height;
                                offset -= (interval * child->rect.height);
                                absoffset = offset > 0 ? offset : -offset;

                                if (absoffset > child->rect.height / 2)
                                {
                                    coverFlow->frame = absoffset / (child->rect.height / coverFlow->totalframe) + 1;
                                    coverFlow->focusIndex -= interval;

                                    if (offset >= 0)
                                    {
                                        coverFlow->inc = child->rect.height;

                                        if (coverFlow->focusIndex < 0)
                                            coverFlow->focusIndex += count;

                                        //printf("1: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                    else
                                    {
                                        coverFlow->inc = -child->rect.height;

                                        if (coverFlow->focusIndex >= count)
                                            coverFlow->focusIndex -= count;

                                        //printf("2: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                }
                                else if (absoffset)
                                {
                                    coverFlow->frame = coverFlow->totalframe - absoffset / (child->rect.height / coverFlow->totalframe);

                                    if (offset >= 0)
                                    {
                                        coverFlow->inc = -child->rect.height;
                                        coverFlow->focusIndex -= interval + 1;

                                        if (coverFlow->focusIndex < 0)
                                            coverFlow->focusIndex += count;

                                        //printf("3: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                    else
                                    {
                                        coverFlow->inc = child->rect.height;
                                        coverFlow->focusIndex -= interval - 1;

                                        if (coverFlow->focusIndex >= count)
                                            coverFlow->focusIndex -= count;

                                        //printf("4: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                }
                            }
                            widget->flags |= ITU_UNDRAGGING;
                            ituScene->dragged = NULL;
                        }
                    }
                    else
                    {
                        if (!result && count > 0)
                        {//[MOUSEUP][Vertical][non-cycle][layout]
                            ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, 0);
							bool boundary_touch = false;
							
							if (coverFlow->boundaryAlign)
							{
								int max_neighbor_item = ((widget->rect.height / child->rect.height) - 1) / 2;
								int max_height_item = widget->rect.height / child->rect.height;

								if (max_neighbor_item == 0)
									max_neighbor_item++;

								if (coverFlow->focusIndex >= max_neighbor_item)
								{
									if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_neighbor_item))
										boundary_touch = true;
									else
									{
										if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_height_item))
											boundary_touch = true;
									}
								}
								else
									boundary_touch = true;
							}

                            if (coverFlow->inc == 0)
                            {
                                int offset, absoffset, interval;
                                
                                offset = y - coverFlow->touchPos;
                                interval = offset / child->rect.height;
                                offset -= (interval * child->rect.height);
                                absoffset = offset > 0 ? offset : -offset;

                                if (absoffset > child->rect.height / 2)
                                {//small shift alignment
                                    if (offset >= 0)
                                    {
                                        coverFlow->frame = absoffset / (child->rect.height / coverFlow->totalframe) + 1;

                                        if (coverFlow->focusIndex > interval)
                                        {
											if (boundary_touch)
												coverFlow->focusIndex = CoverFlowGetVisibleChildCount(coverFlow) - widget->rect.height / child->rect.height;
                                            coverFlow->inc = child->rect.height;
                                            coverFlow->focusIndex -= interval;
                                            //printf("5: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
											//if (boundary_touch)
											//	coverFlow->focusIndex -= (interval != 0) ? (((offset >= 0) ? (1) : (-1))) : (((absoffset > child->rect.height / 2) ? (((offset >= 0) ? (1) : (-1))) : (0)));
                                        }
                                        else
                                        {
                                            coverFlow->inc = -child->rect.height;
                                            coverFlow->focusIndex = -1;
                                            //printf("6: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                        }
                                    }
                                    else
                                    {
                                        coverFlow->frame = absoffset / (child->rect.height / coverFlow->totalframe) + 1;

                                        if (coverFlow->focusIndex < count + interval - 1)
                                        {
                                            coverFlow->inc = -child->rect.height;
                                            coverFlow->focusIndex -= interval;
                                            //printf("7: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
											//if (boundary_touch)
											//	coverFlow->focusIndex -= (interval != 0) ? (((offset >= 0) ? (1) : (-1))) : (((absoffset > child->rect.height / 2) ? (((offset >= 0) ? (1) : (-1))) : (0)));
                                        }
                                        else
                                        {
                                            coverFlow->inc = child->rect.height;
                                            coverFlow->focusIndex = count;
                                            //printf("8: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                        }
                                    }
                                }
                                else if (absoffset)
                                {
                                    coverFlow->frame = coverFlow->totalframe - absoffset / (child->rect.height / coverFlow->totalframe);

                                    if (offset >= 0)
                                    {//big shift alignment
										if ((boundary_touch) && (coverFlow->focusIndex > 0))
											coverFlow->focusIndex = CoverFlowGetVisibleChildCount(coverFlow) - widget->rect.height / child->rect.height;

                                        coverFlow->inc = -child->rect.height;
                                        coverFlow->focusIndex -= interval + 1;

										//if (boundary_touch)
										//	coverFlow->focusIndex -= (interval != 0) ? (((offset >= 0) ? (1) : (-1))) : (((absoffset > child->rect.height / 2) ? (((offset >= 0) ? (1) : (-1))) : (0)));

                                        if (coverFlow->focusIndex < -1)
                                            coverFlow->focusIndex = -1;

                                        //printf("9: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                    else
                                    {
                                        coverFlow->inc = child->rect.height;
                                        coverFlow->focusIndex -= interval - 1;

										if (boundary_touch)
											coverFlow->focusIndex -= (interval != 0) ? (((offset >= 0) ? (1) : (-1))) : (((absoffset > child->rect.height / 2) ? (((offset >= 0) ? (1) : (-1))) : (0)));

                                        if (coverFlow->focusIndex >= count + 1)
                                            coverFlow->focusIndex = count;

                                        //printf("10: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                }
                            }
                        }
                        widget->flags |= ITU_UNDRAGGING;
                        ituScene->dragged = NULL;
                    }
                }
                else
                {
                    if (coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE)
                    {
                        if (count > 0)
                        {
                            ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, 0);

                            if (coverFlow->inc == 0)
                            {
                                int offset, absoffset, interval;
                                
                                offset = x - coverFlow->touchPos;
                                interval = offset / child->rect.width;
                                offset -= (interval * child->rect.width);
                                absoffset = offset > 0 ? offset : -offset;

                                if (absoffset > child->rect.width / 2)
                                {
                                    coverFlow->frame = absoffset / (child->rect.width / coverFlow->totalframe) + 1;
                                    coverFlow->focusIndex -= interval;

                                    if (offset >= 0)
                                    {
                                        coverFlow->inc = child->rect.width;

                                        if (coverFlow->focusIndex < 0)
                                            coverFlow->focusIndex += count;

                                        //printf("1: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                    else
                                    {
                                        coverFlow->inc = -child->rect.width;

                                        if (coverFlow->focusIndex >= count)
                                            coverFlow->focusIndex -= count;

                                        //printf("2: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                }
                                else if (absoffset)
                                {
                                    coverFlow->frame = coverFlow->totalframe - absoffset / (child->rect.width / coverFlow->totalframe);

                                    if (offset >= 0)
                                    {
                                        coverFlow->inc = -child->rect.width;
                                        coverFlow->focusIndex -= interval + 1;

                                        if (coverFlow->focusIndex < 0)
                                            coverFlow->focusIndex += count;

                                        //printf("3: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                    else
                                    {
                                        coverFlow->inc = child->rect.width;
                                        coverFlow->focusIndex -= interval - 1;

                                        if (coverFlow->focusIndex >= count)
                                            coverFlow->focusIndex -= count;

                                        //printf("4: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                }
                                widget->flags |= ITU_UNDRAGGING;
                                ituScene->dragged = NULL;
                            }
                        }
                    }
                    else
                    {
                        if (count > 0)
                        {
                            ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, 0);
							bool boundary_touch = false;
							//[MOUSEUP][Horizontal][non-cycle][layout]
							if (coverFlow->boundaryAlign)
							{
								int max_neighbor_item = ((widget->rect.width / child->rect.width) - 1) / 2;
								int max_width_item = widget->rect.width / child->rect.width;

								if (max_neighbor_item == 0)
									max_neighbor_item++;

								if (coverFlow->focusIndex >= max_neighbor_item)
								{
									if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_neighbor_item))
										boundary_touch = true;
									else
									{
										if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_width_item))
											boundary_touch = true;
									}
								}
								else
									boundary_touch = true;
							}

                            if (coverFlow->inc == 0)
                            {
                                int offset, absoffset, interval;
                                
                                offset = x - coverFlow->touchPos;
                                interval = offset / child->rect.width;
                                offset -= (interval * child->rect.width);
                                absoffset = offset > 0 ? offset : -offset;

                                if (absoffset > child->rect.width / 2)
                                {
                                    if (offset >= 0)
                                    {
                                        coverFlow->frame = absoffset / (child->rect.width / coverFlow->totalframe) + 1;

                                        if (coverFlow->focusIndex > interval)
                                        {
											//small shift alignment
											if (boundary_touch)
												coverFlow->focusIndex = CoverFlowGetVisibleChildCount(coverFlow) - widget->rect.width / child->rect.width;
                                            coverFlow->inc = child->rect.width;
                                            coverFlow->focusIndex -= interval;
                                            //printf("5: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
											//if (boundary_touch)
											//	coverFlow->focusIndex -= (interval != 0) ? (((offset >= 0) ? (1) : (-1))) : (((absoffset > child->rect.width / 2) ? (((offset >= 0) ? (1) : (-1))) : (0)));
                                        }
                                        else
                                        {
                                            coverFlow->inc = -child->rect.width;
                                            coverFlow->focusIndex = -1;
                                            //printf("6: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                        }
                                    }
                                    else
                                    {
                                        coverFlow->frame = absoffset / (child->rect.width / coverFlow->totalframe) + 1;

                                        if (coverFlow->focusIndex < count + interval - 1)
                                        {
                                            coverFlow->inc = -child->rect.width;
                                            coverFlow->focusIndex -= interval;
                                            //printf("7: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
											//3
											//if (boundary_touch)
											//	coverFlow->focusIndex -= (interval != 0) ? (((offset >= 0) ? (1) : (-1))) : (((absoffset > child->rect.width / 2) ? (((offset >= 0) ? (1) : (-1))) : (0)));
                                        }
                                        else
                                        {
                                            coverFlow->inc = child->rect.width;
                                            coverFlow->focusIndex = count;
                                            //printf("8: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                        }
                                    }
                                }
                                else if (absoffset)
                                {
                                    coverFlow->frame = coverFlow->totalframe - absoffset / (child->rect.width / coverFlow->totalframe);

                                    if (offset >= 0)
                                    {
										//big shift alignment
										if ((boundary_touch) && (coverFlow->focusIndex > 0))
											coverFlow->focusIndex = CoverFlowGetVisibleChildCount(coverFlow) - widget->rect.width / child->rect.width;
										
                                        coverFlow->inc = -child->rect.width;
                                        coverFlow->focusIndex -= interval + 1;

										//if (boundary_touch)
										//	coverFlow->focusIndex -= (interval != 0) ? (((offset >= 0) ? (1) : (-1))) : (((absoffset > child->rect.width / 2) ? (((offset >= 0) ? (1) : (-1))) : (0)));

                                        if (coverFlow->focusIndex < -1)
                                            coverFlow->focusIndex = -1;

                                        //printf("9: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                    else
                                    {
                                        coverFlow->inc = child->rect.width;
                                        coverFlow->focusIndex -= interval - 1;

										if (boundary_touch)
											coverFlow->focusIndex -= (interval != 0) ? (((offset >= 0) ? (1) : (-1))) : (((absoffset > child->rect.width / 2) ? (((offset >= 0) ? (1) : (-1))) : (0)));

                                        if (coverFlow->focusIndex >= count + 1)
                                            coverFlow->focusIndex = count;

                                        //printf("10: frame=%d offset=%d inc=%d interval=%d focusIndex=%d\n", coverFlow->frame, offset, coverFlow->inc, interval, coverFlow->focusIndex);
                                    }
                                }
                                widget->flags |= ITU_UNDRAGGING;
                                ituScene->dragged = NULL;
                            }
                        }
                    }
                }
                result = true;
            }
            widget->flags &= ~ITU_DRAGGING;
            coverFlow->touchCount = 0;
        }
    }

    if (ev == ITU_EVENT_TIMER)
    {
        if (coverFlow->touchCount > 0)
        {
            int x, y, dist;

            assert(widget->flags & ITU_HAS_LONG_PRESS);

            ituWidgetGetGlobalPosition(widget, &x, &y);

            if (coverFlow->coverFlowFlags & ITU_COVERFLOW_VERTICAL)
            {
                dist = ituScene->lastMouseY - (y + coverFlow->touchPos);
            }
            else
            {
                dist = ituScene->lastMouseX - (x + coverFlow->touchPos);
            }

            if (dist < 0)
                dist = -dist;

            if (dist >= ITU_DRAG_DISTANCE)
            {
                widget->flags |= ITU_DRAGGING;
                ituScene->dragged = widget;
                coverFlow->touchCount = 0;
            }
        }

        if (coverFlow->inc)
        {
            int i, count = CoverFlowGetVisibleChildCount(coverFlow);

            if (coverFlow->coverFlowFlags & ITU_COVERFLOW_VERTICAL)
            {
                if (coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE)
                {
                    int index, count2;
                    float step = (float)coverFlow->frame / coverFlow->totalframe;
                    step = step * (float)M_PI / 2;
                    step = sinf(step);

                    //printf("step=%f\n", step);

                    count2 = count / 2 + 1;
                    index = coverFlow->focusIndex;

                    for (i = 0; i < count2; ++i)
                    {
                        ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                        int fy = widget->rect.height / 2 - child->rect.height / 2;
                        fy += i * child->rect.height;
                        fy += (int)(coverFlow->inc * step);
                        ituWidgetSetY(child, fy);

                        if (index >= count - 1)
                            index = 0;
                        else
                            index++;
                    }

                    count2 = count - count2;
                    for (i = 0; i < count2; ++i)
                    {
                        ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                        int fy = widget->rect.height / 2 - child->rect.height / 2;
                        fy -= count2 * child->rect.height;
                        fy += i * child->rect.height;
                        fy += (int)(coverFlow->inc * step);
                        ituWidgetSetY(child, fy);

                        if (index >= count - 1)
                            index = 0;
                        else
                            index++;
                    }
                }
                else if (widget->flags & ITU_BOUNCING)
                {
                    float step = (float)coverFlow->frame / coverFlow->totalframe;
                    step = step * (float)M_PI;
                    step = sinf(step);

                    //printf("step=%f\n", step);

                    for (i = 0; i < count; ++i)
                    {
                        ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, i);
                        int fy = widget->rect.height / 2 - child->rect.height / 2;
                        fy -= child->rect.height * coverFlow->focusIndex;
                        fy += i * child->rect.height;
                        fy += (int)(coverFlow->inc * step);

                        ituWidgetSetY(child, fy);
                    }

                    coverFlow->frame++;

                    if (coverFlow->frame > coverFlow->totalframe)
                    {
                        coverFlow->frame = 0;
                        coverFlow->inc = 0;
                        widget->flags &= ~ITU_BOUNCING;
                    }
                    result = true;
                    return widget->visible ? result : false;
                }
                else if (coverFlow->boundaryAlign == 0) //sync back to original update
                {
                    float step = (float)coverFlow->frame / coverFlow->totalframe;
                    step = step * (float)M_PI / 2;
                    step = sinf(step);

                    //printf("step=%f\n", step);

                    for (i = 0; i < count; ++i)
                    {
                        ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, i);
                        int fy = widget->rect.height / 2 - child->rect.height / 2;
                        fy -= child->rect.height * coverFlow->focusIndex;
                        fy += i * child->rect.height;
                        fy += (int)(coverFlow->inc * step);

                        ituWidgetSetY(child, fy);
                    }
                }
            }
            else
            {
                if (coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE)
                {
                    int index, count2;
                    float step = (float)coverFlow->frame / coverFlow->totalframe;
                    step = step * (float)M_PI / 2;
                    step = sinf(step);
                    
                    //printf("step=%f\n", step);
					//workaround for wrong left-side display with hide child
                    count2 = count / 2 + 1 - ((coverFlow->inc <= 0)?(0):(1));
                    index = coverFlow->focusIndex;

                    for (i = 0; i < count2; ++i)
                    {
                        ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                        int fx = widget->rect.width / 2 - child->rect.width / 2;
                        fx += i * child->rect.width;
                        fx += (int)(coverFlow->inc * step);
                        ituWidgetSetX(child, fx);

                        if (index >= count - 1)
                            index = 0;
                        else
                            index++;
                    }

                    count2 = count - count2;
                    for (i = 0; i < count2; ++i)
                    {
                        ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                        int fx = widget->rect.width / 2 - child->rect.width / 2;
                        fx -= count2 * child->rect.width;
                        fx += i * child->rect.width;
                        fx += (int)(coverFlow->inc * step);
                        ituWidgetSetX(child, fx);

                        if (index >= count - 1)
                            index = 0;
                        else
                            index++;
                    }
                }
                else if (widget->flags & ITU_BOUNCING)
                {
                    float step = (float)coverFlow->frame / coverFlow->totalframe;
                    step = step * (float)M_PI;
                    step = sinf(step);

                    //printf("frame=%d step=%f\n", coverFlow->frame, step);

                    for (i = 0; i < count; ++i)
                    {
                        ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, i);
                        int fx = widget->rect.width / 2 - child->rect.width / 2;
                        fx -= child->rect.width * coverFlow->focusIndex;
                        fx += i * child->rect.width;
                        fx += (int)(coverFlow->inc * step);

                        ituWidgetSetX(child, fx);
                    }

                    coverFlow->frame++;

                    if (coverFlow->frame > coverFlow->totalframe)
                    {
                        coverFlow->frame = 0;
                        coverFlow->inc = 0;
                        widget->flags &= ~ITU_BOUNCING;
                    }
                    result = true;
                    return widget->visible ? result : false;
                }
                else if (coverFlow->boundaryAlign == 0) //sync back to original update
                {
                    float step = (float)coverFlow->frame / coverFlow->totalframe;
                    step = step * (float)M_PI / 2;
                    step = sinf(step);

                    //printf("step=%f\n", step);

                    for (i = 0; i < count; ++i)
                    {
                        ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, i);
                        int fx = widget->rect.width / 2 - child->rect.width / 2;
                        fx -= child->rect.width * coverFlow->focusIndex;
                        fx += i * child->rect.width;
                        fx += (int)(coverFlow->inc * step);

                        ituWidgetSetX(child, fx);
                    }
                }
            }
            coverFlow->frame++;

            if (coverFlow->frame > coverFlow->totalframe)
            {
                if (coverFlow->inc > 0)
                {
                    if (coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE)
                    {
                        if (coverFlow->focusIndex <= 0)
                            coverFlow->focusIndex = count - 1;
                        else
                            coverFlow->focusIndex--;
                    }
                    else
                    {
						if (coverFlow->focusIndex > 0)
							coverFlow->focusIndex--;
                    }
                }
                else // if (coverFlow->inc < 0)
                {
                    if (coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE)
                    {
                        if (coverFlow->focusIndex >= count - 1)
                            coverFlow->focusIndex = 0;
                        else
                            coverFlow->focusIndex++;
                    }
                    else
                    {
						if (coverFlow->focusIndex < (count - 1))
							coverFlow->focusIndex++;
                    }
                }

				if ((coverFlow->coverFlowFlags & ITU_COVERFLOW_SLIDING) && coverFlow->boundaryAlign)
				{
					coverFlow->slideCount++;

					if (((coverFlow->slideCount + 1) >= coverFlow->slideMaxCount) || (coverFlow->focusIndex <= 0) || (coverFlow->focusIndex >= (count - 1)))
					{
						coverFlow->slideCount = 0;
						coverFlow->coverFlowFlags &= ~ITU_COVERFLOW_SLIDING;
					}
				}
				else
				{
					int* customdata = ituWidgetGetCustomData(coverFlow);
					int i = 0;
					bool no_queue = true;
					coverFlow->frame = 0;

					ituExecActions(widget, coverFlow->actions, ITU_EVENT_CHANGED, coverFlow->focusIndex);
					ituCoverFlowOnCoverChanged(coverFlow, widget);
					//Bless added for PoWei requirement --> prev/next work queue
					//memcpy(customdata, ituWidgetGetCustomData(coverFlow), sizeof(int)*ITU_ACTIONS_SIZE);

					if (customdata)
					{
						for (i = 0; i < ITU_ACTIONS_SIZE; i++)
						{
							if (customdata[i] != 0)
							{
								customdata[i] = 0;

								if ((i + 1) < ITU_ACTIONS_SIZE)
								{
									if (customdata[i + 1] != 0)
										no_queue = false;
								}
								
								ituWidgetSetCustomData(coverFlow, customdata);
								
								break;
							}
						}
					}

					if (no_queue)
					{
						coverFlow->inc = 0;
						widget->flags &= ~ITU_UNDRAGGING;
						ituWidgetUpdate(widget, ITU_EVENT_LAYOUT, 0, 0, 0);
					}
					else
					{
						ITUWidget* childtmp = CoverFlowGetVisibleChild(coverFlow, 0);
						coverFlow->inc = 0;
						if (coverFlow->coverFlowFlags & ITU_COVERFLOW_VERTICAL)
						{
							if (customdata[i + 1] < 0)
								//coverFlow->inc = childtmp->rect.height;
								ituWidgetUpdate(widget, ITU_EVENT_TOUCHSLIDEDOWN, 0, widget->rect.x, widget->rect.y);
							else if (customdata[i + 1] > 0)
								//coverFlow->inc = 0 - childtmp->rect.height;
								ituWidgetUpdate(widget, ITU_EVENT_TOUCHSLIDEUP, 0, widget->rect.x, widget->rect.y);
						}
						else
						{
							if (customdata[i + 1] < 0)
								//coverFlow->inc = childtmp->rect.width;
								ituWidgetUpdate(widget, ITU_EVENT_TOUCHSLIDERIGHT, 0, widget->rect.x, widget->rect.y);
							else if (customdata[i + 1] > 0)
								//coverFlow->inc = 0 - childtmp->rect.width;
								ituWidgetUpdate(widget, ITU_EVENT_TOUCHSLIDELEFT, 0, widget->rect.x, widget->rect.y);
						}
					}
				}
            }
        }
    }

    if (widget->flags & ITU_TOUCHABLE)
    {
        if (ev == ITU_EVENT_MOUSEDOWN || ev == ITU_EVENT_MOUSEUP || ev == ITU_EVENT_MOUSEDOUBLECLICK || ev == ITU_EVENT_MOUSELONGPRESS ||
            ev == ITU_EVENT_TOUCHSLIDELEFT || ev == ITU_EVENT_TOUCHSLIDEUP || ev == ITU_EVENT_TOUCHSLIDERIGHT || ev == ITU_EVENT_TOUCHSLIDEDOWN)
        {
            if (ituWidgetIsEnabled(widget))
            {
                int x = arg2 - widget->rect.x;
                int y = arg3 - widget->rect.y;

                if (ituWidgetIsInside(widget, x, y))
                {
                    result |= widget->dirty;
                    return widget->visible ? result : false;
                }
            }
        }
    }

    if (ev == ITU_EVENT_LAYOUT)
    {
        int i, count = CoverFlowGetVisibleChildCount(coverFlow);

        if (coverFlow->coverFlowFlags & ITU_COVERFLOW_VERTICAL)
        {
            if (coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE)
            {
                int index, count2;
                
                count2 = count / 2 + 1;
                index = coverFlow->focusIndex;

                for (i = 0; i < count2; ++i)
                {
                    ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                    int fy = widget->rect.height / 2 - child->rect.height / 2;
                    fy += i * child->rect.height;
                    ituWidgetSetY(child, fy);

                    if (index >= count - 1)
                        index = 0;
                    else
                        index++;
                }

                count2 = count - count2;
                for (i = 0; i < count2; ++i)
                {
                    ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                    int fy = widget->rect.height / 2 - child->rect.height / 2;
                    fy -= count2 * child->rect.height;
                    fy += i * child->rect.height;
                    ituWidgetSetY(child, fy);

                    if (index >= count - 1)
                        index = 0;
                    else
                        index++;
                }
            }
            else
            {
                for (i = 0; i < count; ++i)
                {
                    ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, i);
                    int fy = widget->rect.height / 2 - child->rect.height / 2;

					if (coverFlow->boundaryAlign)
					{//[LAYOUT][Vertical][non-cycle]
						int max_neighbor_item = ((widget->rect.height / child->rect.height) - 1) / 2;
						int max_height_item = (widget->rect.height / child->rect.height);
						fy = 0;

						if (max_neighbor_item == 0)
							max_neighbor_item++;

						if (coverFlow->focusIndex > 0) //>= max_neighbor_item) //Bless new debug
						{
							//if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_neighbor_item))
							if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_height_item))
								fy = widget->rect.height - (count * child->rect.height);
							else
								fy -= child->rect.height * coverFlow->focusIndex;
						}
						else
							fy = 0;
					}
					else
					{
						fy -= child->rect.height * coverFlow->focusIndex;
					}

                    fy += i * child->rect.height;
                    ituWidgetSetY(child, fy);
                }
            }
        }
        else
        {
            if (coverFlow->coverFlowFlags & ITU_COVERFLOW_CYCLE)
            {
                int index, count2;
                
                count2 = count / 2 + 1;
                index = coverFlow->focusIndex;

                for (i = 0; i < count2; ++i)
                {
                    ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                    int fx = widget->rect.width / 2 - child->rect.width / 2;
                    fx += i * child->rect.width;
                    ituWidgetSetX(child, fx);

                    if (index >= count - 1)
                        index = 0;
                    else
                        index++;
                }

                count2 = count - count2;
                for (i = 0; i < count2; ++i)
                {
                    ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, index);
                    int fx = widget->rect.width / 2 - child->rect.width / 2;
                    fx -= count2 * child->rect.width;
                    fx += i * child->rect.width;
                    ituWidgetSetX(child, fx);

                    if (index >= count - 1)
                        index = 0;
                    else
                        index++;
                }
            }
            else
            {
                for (i = 0; i < count; ++i)
                {//[LAYOUT][Horizontal][non-cycle]
                    ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, i);
                    int fx = widget->rect.width / 2 - child->rect.width / 2;
					
					if (coverFlow->boundaryAlign)
					{
						int max_neighbor_item = ((widget->rect.width / child->rect.width) - 1) / 2;
						int max_width_item = (widget->rect.width / child->rect.width);

						fx = 0;

						if (max_neighbor_item == 0)
							max_neighbor_item++;

						if (coverFlow->focusIndex > 0) //>= max_neighbor_item) //Bless debug now
						{
							//if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_neighbor_item))
							if ((count >= (max_neighbor_item * 2 + 1)) && ((count - coverFlow->focusIndex - 1) < max_width_item))
								fx = widget->rect.width - (count * child->rect.width);
							else
								fx -= child->rect.width * coverFlow->focusIndex;
						}
						else
							fx = 0;
					}
					else
					{
						fx -= child->rect.width * coverFlow->focusIndex;
					}
                    
					fx += i * child->rect.width;
                    ituWidgetSetX(child, fx);
                }				
            }
        }

        if ((coverFlow->coverFlowFlags & ITU_COVERFLOW_ENABLE_ALL) == 0)
        {
            for (i = 0; i < count; ++i)
            {
                ITUWidget* child = CoverFlowGetVisibleChild(coverFlow, i);

                if (i == coverFlow->focusIndex)
                    ituWidgetEnable(child);
                else
                    ituWidgetDisable(child);
            }
        }
        widget->flags &= ~ITU_DRAGGING;
        coverFlow->touchCount = 0;
    }

    result |= widget->dirty;
    return widget->visible ? result : false;
}

void ituCoverFlowDraw(ITUWidget* widget, ITUSurface* dest, int x, int y, uint8_t alpha)
{
    int destx, desty;
    uint8_t desta;
    ITURectangle prevClip;
    ITURectangle* rect = (ITURectangle*) &widget->rect;
    assert(widget);
    assert(dest);

    destx = rect->x + x;
    desty = rect->y + y;
    desta = alpha * widget->color.alpha / 255;
    desta = desta * widget->alpha / 255;
   
    ituWidgetSetClipping(widget, dest, x, y, &prevClip);

    if (desta == 255)
    {
        ituColorFill(dest, destx, desty, rect->width, rect->height, &widget->color);
    }
    else if (desta > 0)
    {
        ITUSurface* surf = ituCreateSurface(rect->width, rect->height, 0, dest->format, NULL, 0);
        if (surf)
        {
            ituColorFill(surf, 0, 0, rect->width, rect->height, &widget->color);
            ituAlphaBlend(dest, destx, desty, rect->width, rect->height, surf, 0, 0, desta);                
            ituDestroySurface(surf);
        }
    }
    ituFlowWindowDraw(widget, dest, x, y, alpha);
    ituSurfaceSetClipping(dest, prevClip.x, prevClip.y, prevClip.width, prevClip.height);
    ituDirtyWidget(widget, false);
}

void ituCoverFlowOnAction(ITUWidget* widget, ITUActionType action, char* param)
{
    ITUCoverFlow* coverFlow = (ITUCoverFlow*) widget;
    assert(coverFlow);

    switch (action)
    {
    case ITU_ACTION_PREV:
        ituCoverFlowPrev((ITUCoverFlow*)widget);
        break;

    case ITU_ACTION_NEXT:
        ituCoverFlowNext((ITUCoverFlow*)widget);
        break;

    case ITU_ACTION_GOTO:
        ituCoverFlowGoto((ITUCoverFlow*)widget, atoi(param));
        break;

    case ITU_ACTION_DODELAY0:
        ituExecActions(widget, coverFlow->actions, ITU_EVENT_DELAY0, atoi(param));
        break;

    case ITU_ACTION_DODELAY1:
        ituExecActions(widget, coverFlow->actions, ITU_EVENT_DELAY1, atoi(param));
        break;

    case ITU_ACTION_DODELAY2:
        ituExecActions(widget, coverFlow->actions, ITU_EVENT_DELAY2, atoi(param));
        break;

    case ITU_ACTION_DODELAY3:
        ituExecActions(widget, coverFlow->actions, ITU_EVENT_DELAY3, atoi(param));
        break;

    case ITU_ACTION_DODELAY4:
        ituExecActions(widget, coverFlow->actions, ITU_EVENT_DELAY4, atoi(param));
        break;

    case ITU_ACTION_DODELAY5:
        ituExecActions(widget, coverFlow->actions, ITU_EVENT_DELAY5, atoi(param));
        break;

    case ITU_ACTION_DODELAY6:
        ituExecActions(widget, coverFlow->actions, ITU_EVENT_DELAY6, atoi(param));
        break;

    case ITU_ACTION_DODELAY7:
        ituExecActions(widget, coverFlow->actions, ITU_EVENT_DELAY7, atoi(param));
        break;

    default:
        ituWidgetOnActionImpl(widget, action, param);
        break;
    }
}

static void CoverFlowOnCoverChanged(ITUCoverFlow* coverFlow, ITUWidget* widget)
{
    // DO NOTHING
}

void ituCoverFlowInit(ITUCoverFlow* coverFlow, ITULayout layout)
{
    assert(coverFlow);

    memset(coverFlow, 0, sizeof (ITUCoverFlow));

    if (layout == ITU_LAYOUT_VERTICAL)
        coverFlow->coverFlowFlags &= ITU_COVERFLOW_VERTICAL;

    ituFlowWindowInit(&coverFlow->fwin, layout);

    ituWidgetSetType(coverFlow, ITU_COVERFLOW);
    ituWidgetSetName(coverFlow, coverFlowName);
    ituWidgetSetUpdate(coverFlow, ituCoverFlowUpdate);
    ituWidgetSetDraw(coverFlow, ituCoverFlowDraw);
    ituWidgetSetOnAction(coverFlow, ituCoverFlowOnAction);
    ituCoverFlowSetCoverChanged(coverFlow, CoverFlowOnCoverChanged);
}

void ituCoverFlowLoad(ITUCoverFlow* coverFlow, uint32_t base)
{
    assert(coverFlow);

    ituFlowWindowLoad(&coverFlow->fwin, base);

    ituWidgetSetUpdate(coverFlow, ituCoverFlowUpdate);
    ituWidgetSetDraw(coverFlow, ituCoverFlowDraw);
    ituWidgetSetOnAction(coverFlow, ituCoverFlowOnAction);
    ituCoverFlowSetCoverChanged(coverFlow, CoverFlowOnCoverChanged);
}

void ituCoverFlowGoto(ITUCoverFlow* coverFlow, int index)
{
    assert(coverFlow);

    if (coverFlow->focusIndex == index)
        return;
  
    coverFlow->focusIndex = index;
    ituWidgetUpdate(coverFlow, ITU_EVENT_LAYOUT, 0, 0, 0);
}

void ituCoverFlowPrev(ITUCoverFlow* coverflow)
{
    ITUWidget* widget = (ITUWidget*) coverflow;
    unsigned int oldFlags = widget->flags;

	//Bless added for PoWei requirement --> prev/next work queue
	int i = 0;
	bool no_queue = true;
	int* customdata = (int*)ituWidgetGetCustomData(coverflow);

	if (customdata)
	{
		for (i = ITU_ACTIONS_SIZE - 1; i >= 0; i--)
		{
			if ((i - 1) >= 0)
			{
				if (customdata[i - 1] != 0)
				{
					customdata[i] = -1;
					no_queue = false;
					break;
				}
			}
			else
				customdata[i] = -1;
		}
		ituWidgetSetCustomData(coverflow, customdata);
	}
	else
	{
		int* newdata = malloc(sizeof(int)* ITU_ACTIONS_SIZE);
		memset(newdata, 0, sizeof(int)* ITU_ACTIONS_SIZE);
		newdata[0] = -1;
		ituWidgetSetCustomData(coverflow, newdata);
	}

    widget->flags |= ITU_TOUCHABLE;

	if (no_queue)
	{
		if (coverflow->coverFlowFlags & ITU_COVERFLOW_VERTICAL)
			ituWidgetUpdate(widget, ITU_EVENT_TOUCHSLIDEDOWN, 0, widget->rect.x, widget->rect.y);
		else
			ituWidgetUpdate(widget, ITU_EVENT_TOUCHSLIDERIGHT, 0, widget->rect.x, widget->rect.y);
	}

    if ((oldFlags & ITU_TOUCHABLE) == 0)
        widget->flags &= ~ITU_TOUCHABLE;
}

void ituCoverFlowNext(ITUCoverFlow* coverflow)
{
	ITUWidget* widget = (ITUWidget*)coverflow;
	unsigned int oldFlags = widget->flags;

	//Bless added for PoWei requirement --> prev/next work queue
	int i = 0;
	bool no_queue = true;
	int* customdata = (int*)ituWidgetGetCustomData(coverflow);

	if (customdata)
	{
		for (i = ITU_ACTIONS_SIZE - 1; i >= 0; i--)
		{
			if ((i - 1) >= 0)
			{
				if (customdata[i - 1] != 0)
				{
					customdata[i] = 1;
					no_queue = false;
					break;
				}
			}
			else
				customdata[i] = 1;
		}
		ituWidgetSetCustomData(coverflow, customdata);
	}
	else
	{
		int* newdata = malloc(sizeof(int)* ITU_ACTIONS_SIZE);
		memset(newdata, 0, sizeof(int)* ITU_ACTIONS_SIZE);
		newdata[0] = 1;
		ituWidgetSetCustomData(coverflow, newdata);
	}

	widget->flags |= ITU_TOUCHABLE;

	if (no_queue)
	{
		if (coverflow->coverFlowFlags & ITU_COVERFLOW_VERTICAL)
			ituWidgetUpdate(widget, ITU_EVENT_TOUCHSLIDEUP, 0, widget->rect.x, widget->rect.y);
		else
			ituWidgetUpdate(widget, ITU_EVENT_TOUCHSLIDELEFT, 0, widget->rect.x, widget->rect.y);
	}

    if ((oldFlags & ITU_TOUCHABLE) == 0)
        widget->flags &= ~ITU_TOUCHABLE;
}
