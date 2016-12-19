#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "ite/itu.h"
#include "itu_cfg.h"
#include "itu_private.h"


static const char wheelName[] = "ITUWheel";

#define STEP_SPEED_BASE 5

static void use_normal_color(ITUWidget* widget, ITUColor* color)
{
	ITUWheel* wheel = (ITUWheel*)widget;
	ITUColor Cr;

	assert(wheel);

	Cr.alpha = wheel->normalColor.alpha;
	Cr.red   = wheel->normalColor.red;
	Cr.green = wheel->normalColor.green;
	Cr.blue  = wheel->normalColor.blue;
	memcpy(color, &Cr, sizeof (ITUColor));

	return;
}

static bool focus_change(ITUWidget* widget, int newfocus, int line)
{
	ITUWheel* wheel = (ITUWheel*)widget;
	ITUWidget* child;
	ITUColor color;
	ITUText* text;
	int check_1, check_2;

	assert(wheel);

	if (wheel->cycle_tor <= 0)
		return false;

	check_1 = ((wheel->focus_c - 1) < wheel->minci) ? (wheel->maxci) : (wheel->focus_c - 1);
	check_2 = ((wheel->focus_c + 1) > wheel->maxci) ? (wheel->minci) : (wheel->focus_c + 1);

	if ((newfocus != check_1) && (newfocus != check_2))
	{
		//printf("===[wheel][prev][next] [%d %d]===\n", wheel->focus_c, newfocus);
		return false;
	}

	//memcpy(&color, &NColor, sizeof (ITUColor));
	use_normal_color(widget, &color);
	child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->focus_c);
	text = (ITUText*)child;

	ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
	ituTextSetFontHeight(text, wheel->fontHeight);
	//printf("===[wheel][last][child %d][focusIndex %d][focusStr: %s][line: %d]===\n", wheel->focus_c, wheel->focusIndex, ituTextGetString(text), line);

	wheel->focus_c = newfocus;
	wheel->focusIndex = newfocus - wheel->focus_dev;
	child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->focus_c);
	text = (ITUText*)child;

	ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);
	ituTextSetFontHeight(text, wheel->focusFontHeight);

	//printf("===[wheel][now][child %d][focusIndex %d][focusStr: %s][line: %d]===\n", wheel->focus_c, wheel->focusIndex, ituTextGetString(text), line);

	return true;
}

static void cycle_arrange(ITUWidget* widget, bool shiftway)
{
	int i = 0;
	int fy = 0;
	ITUWheel* wheel = (ITUWheel*)widget;
	ITUWidget* child;
	ITUText* text;

	assert(wheel);

	if (wheel->cycle_arr_count <= 0)
		return;

	if (shiftway)
	{
		child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[wheel->cycle_arr_count - 1]);
		ituWidgetSetY(child, wheel->tempy);

		for (i = 0; i < wheel->cycle_arr_count; i++)
		{
			wheel->cycle_arr[i]--;
			
			if (wheel->cycle_arr[i] < wheel->minci)
				wheel->cycle_arr[i] = wheel->maxci;
		}
	}
	else
	{
		child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[0]);
		ituWidgetSetY(child, wheel->tempy);

		for (i = 0; i < wheel->cycle_arr_count; i++)
		{
			wheel->cycle_arr[i]++;

			if (wheel->cycle_arr[i] > wheel->maxci)
				wheel->cycle_arr[i] = wheel->minci;
		}
	}

	for (i = 0; i < wheel->cycle_arr_count; i++)
	{
		child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[i]);
		text = (ITUText*)child;

		if (text->fontIndex >= ITU_FREETYPE_MAX_FONTS)
			text->fontIndex = 0;

		if (i == 0)
			fy = 0 - child->rect.height * (wheel->itemCount / 2 + wheel->cycle_tor) + (wheel->itemCount / 4 * child->rect.height);

		if (wheel->cycle_arr[i] == wheel->focus_c)
		{
			wheel->layout_ci = (fy - wheel->focus_c * child->rect.height) * (-1);
			//printf("===[wheel][cycle][focus at child %d][fy %d][layout_ci %d]===\n", wheel->focus_c, fy, wheel->layout_ci);
		}

		ituWidgetSetY(child, fy);
		//printf("===[wheel][cycle][%d][child %d][str %s][fy %d]===\n", i, wheel->cycle_arr[i], ituTextGetString(text), fy);
		fy += child->rect.height;
	}

	return;
}

static void get_normal_color(ITUWidget* widget)
{
	int i, count;
	ITUWheel* wheel = (ITUWheel*)widget;
	ITUWidget* child;
	ITUText* text;

	assert(wheel);

	count = itcTreeGetChildCount(wheel);

	for (i = 0; i < count; i++)
	{
		char* str = NULL;
		child = (ITUWidget*)itcTreeGetChildAt(wheel, i);
		text = (ITUText*)child;

		if (text->fontIndex >= ITU_FREETYPE_MAX_FONTS)
			text->fontIndex = 0;

		str = ituTextGetString(text);

		if (str && (str[0] != '\0'))
		{
			if ((child->color.alpha == wheel->focusColor.alpha) && (child->color.red == wheel->focusColor.red) && (child->color.green == wheel->focusColor.green) && (child->color.blue == wheel->focusColor.blue))
			{
				continue;
			}
			else
			{
				//memcpy(color, &child->color, sizeof (ITUColor));
				wheel->normalColor.alpha = child->color.alpha;
				wheel->normalColor.red   = child->color.red;
				wheel->normalColor.green = child->color.green;
				wheel->normalColor.blue  = child->color.blue;
				break;
			}
		}
	}

	return;
}

static int get_max_focusindex(ITUWidget* widget)
{
	ITUWheel* wheel = (ITUWheel*)widget;
	int i;
	int realcount = 0;
	int count;
	ITUWidget* child;
	ITUText* text;

	assert(wheel);

	count = itcTreeGetChildCount(wheel);

	for (i = 0; i < count; i++)
	{
		char* str = NULL;

		child = (ITUWidget*)itcTreeGetChildAt(wheel, i);
		text = (ITUText*)child;

		if (text->fontIndex >= ITU_FREETYPE_MAX_FONTS)
			text->fontIndex = 0;

		str = ituTextGetString(text);

		if (str && (str[0] != '\0'))
		{
			realcount++;
		}
	}

	realcount--;

	return realcount;
}

bool ituWheelUpdate(ITUWidget* widget, ITUEvent ev, int arg1, int arg2, int arg3)
{
    bool result = false;
    ITUWheel* wheel = (ITUWheel*) widget;

    assert(wheel);

	if (ev == ITU_EVENT_LAYOUT)
	{
		ITUText* text;
		int i, fy, height, height0;
		int count = itcTreeGetChildCount(wheel);
		ITUColor color;

		if (widget->flags & ITU_DRAGGABLE)
		{
			if (wheel->slide_itemcount >= 2)
			{
				wheel->totalframe = wheel->slide_itemcount;
			}
			else
			{
				if (count <= 30)
				{
					wheel->totalframe = (count / 2) + 2;
				}
				else
				{
					if ((wheel->totalframe < 20) || (wheel->totalframe > 30))
						wheel->totalframe = 20;
				}
			}
			//fix for real totalframe when sliding will repeat more then 1 time
			wheel->totalframe -= 1;
			//printf("===[wheel][FrameCount fixed to %d under drag mode]===\n", wheel->totalframe);
		}

		if (wheel->scal < 1) //for scal
			wheel->scal = 1;

		wheel->moving_step = 0; //for special slide

		if (wheel->inside > 0) //for inside check
			wheel->inside = 0;

		if ((wheel->slide_step <= 0) || (wheel->slide_step > 10)) //for wheel slide step speed
			wheel->slide_step = 2;

		if (wheel->idle > 0) //for state check
			wheel->idle = 0;

		if (wheel->cycle_tor > 0) //LAYOUT cycle mode
		{
			ITUWidget* child;
			int realcount = 0;
			int si = 0;

			//fix for wheel itemcount with wrong rect real size
			if ((wheel->itemCount % 2) != 0)
			{
				wheel->itemCount++;
				printf("===[wheel][Setup wrong ItemCount(%d) >> Fixed to (%d)!]===\n", wheel->itemCount - 1, wheel->itemCount);
			}

			for (i = 0; i < count; i++)
			{
				char* str = NULL;

				child = (ITUWidget*)itcTreeGetChildAt(wheel, i);
				text = (ITUText*)child;

				if (text->fontIndex >= ITU_FREETYPE_MAX_FONTS)
					text->fontIndex = 0;

				str = ituTextGetString(text);

				if (str && str[0] != '\0')
				{
					realcount++;

					if ((realcount - 1) == wheel->focusIndex)
					{
						wheel->focus_c = i;
						wheel->focus_dev = i - wheel->focusIndex;
					}

					if (wheel->minci == 0)
						wheel->minci = i;
				}
			}

			if ((wheel->minci == 1) && (realcount == count))
				wheel->minci = 0;

			wheel->maxci = wheel->minci + realcount - 1;

			si = wheel->focus_c;
			
			for (i = 0; i < (wheel->itemCount / 2 + wheel->cycle_tor); i++)
			{
				si--;
				if (si < wheel->minci)
					si = wheel->maxci;
			}
			
			wheel->cycle_arr_count = 0;

			for (i = 0; i < (wheel->itemCount + 2 * wheel->cycle_tor); i++)
			{
				if (i == 0)
				{
					get_normal_color(widget);
					//memcpy(&color, &NColor, sizeof (ITUColor));
					use_normal_color(widget, &color);
					height0 = height = child->rect.height;
					fy = 0 - height * (wheel->itemCount / 2 + wheel->cycle_tor) + (wheel->itemCount / 4 * height);
					wheel->tempy = fy - (height * wheel->cycle_tor * 2);
				}

				child = (ITUWidget*)itcTreeGetChildAt(wheel, si);
				text = (ITUText*)child;

				if (si == wheel->focus_c)
				{
					ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);

					child->rect.height = height = wheel->focusFontHeight;
					ituTextSetFontHeight(text, wheel->focusFontHeight);
					wheel->layout_ci = (fy - wheel->focus_c * height0) * (-1);
					//printf("===[wheel][layout][focus at child %d][fy %d][layout_ci %d]===\n", wheel->focus_c, fy, wheel->layout_ci);
				}
				else
				{
					ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
					ituTextSetFontHeight(text, wheel->fontHeight);
				}

				ituWidgetSetY(child, fy);
				wheel->cycle_arr[i] = si;
				wheel->cycle_arr_count++;
				//printf("===[wheel][layout][child %d][fy %d]===\n", si, fy);

				child->rect.height = height = height0;
				fy += height;

				if ((si + 1) > wheel->maxci)
					si = wheel->minci;
				else
					si++;
			}
			widget->dirty = true;
			result |= widget->dirty;

			widget->flags |= ITU_UNDRAGGING;
			widget->flags &= ~ITU_DRAGGING;
		}
		else //LAYOUT no cycle
		{
			//fix wrong count
			for (i = 0; i < count; i++)
			{
				char* str = NULL;
				ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, i);
				text = (ITUText*)child;

				if (text->fontIndex >= ITU_FREETYPE_MAX_FONTS)
					text->fontIndex = 0;

				str = ituTextGetString(text);

				if (str && str[0] != '\0')
				{
					wheel->focus_dev = i;
					break;
				}
			}

			for (i = 0; i < count; ++i)
			{
				ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, i);

				if (i == 0)
				{
					get_normal_color(widget);
					//memcpy(&color, &NColor, sizeof (ITUColor));
					use_normal_color(widget, &color);
					height0 = height = child->rect.height;
					fy = 0 - (height * (wheel->focusIndex + wheel->focus_dev - (wheel->itemCount / 2)));
				}

				ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);

				text = (ITUText*)child;

				if (text->fontIndex >= ITU_FREETYPE_MAX_FONTS)
					text->fontIndex = 0;

				ituTextSetFontHeight(text, wheel->fontHeight);
				ituWidgetSetY(child, fy);
				//printf("===[wheel][layout][child %d][fy %d]===\n", i, fy);

				child->rect.height = height = height0;

				if (i == (wheel->focusIndex + wheel->focus_dev))
				{
					ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);

					ituTextSetFontHeight(text, wheel->focusFontHeight);
				}
				fy += height;
			}
		}
	}

	if (wheel->cycle_tor > 0 || ev == ITU_EVENT_LANGUAGE)
		result |= ituFlowWindowUpdate(widget, ev, arg1, arg2, arg3);

    if (widget->flags & ITU_TOUCHABLE)
    {
        if (ev == ITU_EVENT_TOUCHSLIDEUP || ev == ITU_EVENT_TOUCHSLIDEDOWN)
        {
            wheel->touchCount = 0;

			if (ituWidgetIsEnabled(widget) && !wheel->inc && !result)
            {
                int x = arg2 - widget->rect.x;
                int y = arg3 - widget->rect.y;

				if ((ituWidgetIsInside(widget, x, y)) || (wheel->inside > 0))
                {
					wheel->inside = 0;

					wheel->moving_step = 1;
					
                    if (widget->flags & ITU_DRAGGING)
                    {
                        widget->flags &= ~ITU_DRAGGING;
                        ituScene->dragged = NULL;
                        wheel->inc = 0;
                    }
					
                    if (ev == ITU_EVENT_TOUCHSLIDEUP)
                    {
						int fmax = get_max_focusindex(widget);

						if ((wheel->focusIndex <= fmax) || (wheel->cycle_tor > 0))
                        {
                            ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, 0);

							if (wheel->inc == 0)
							{
								wheel->inc = 0 - child->rect.height;
								wheel->frame = 0;
								wheel->sliding = 1;
							}
                        }
                    }
                    else if (ev == ITU_EVENT_TOUCHSLIDEDOWN)
                    {
						if ((wheel->focusIndex >= 0) || (wheel->cycle_tor > 0))
                        {
                            ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, 0);

							if (wheel->inc == 0)
							{
								wheel->inc = child->rect.height;
								wheel->frame = 0;
								wheel->sliding = 1;
							}
                        }
                    }
					widget->flags &= ~ITU_DRAGGING;
					//widget->flags &= ~ITU_UNDRAGGING;
                    result = true;
                }
				else
				{
					widget->flags &= ~ITU_DRAGGING;
					//widget->flags |= ITU_UNDRAGGING;
					//result = true; //BUG
					//ituWidgetUpdate(wheel, ITU_EVENT_LAYOUT, 0, 0, 0);
				}
            }
        }
        else if (ev == ITU_EVENT_MOUSEMOVE) 
        {
            if (ituWidgetIsEnabled(widget) && (widget->flags & ITU_DRAGGING))
            {
                int x = arg2 - widget->rect.x;
                int y = arg3 - widget->rect.y;

				if (wheel->cycle_tor <= 0)
				{
					if (ituWidgetIsInside(widget, x, y))
					{
						int i, fy, fc, fmax, height, shift, offset, count = itcTreeGetChildCount(wheel);
						ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, 0);
						ITUColor color;
						ITUText* text;

						//memcpy(&color, &NColor, sizeof (ITUColor));
						use_normal_color(widget, &color);

						height = child->rect.height;
						fc = wheel->focusIndex + wheel->focus_dev;
						fmax = get_max_focusindex(widget);
						offset = y - wheel->touchY;

						if (wheel->scal > 0)
							offset /= wheel->scal;

						if (((offset > 0) && (wheel->focusIndex > 0)) || ((offset < 0) && (wheel->focusIndex < fmax)))
						{
							for (i = 0; i < count; ++i)
							{
								child = (ITUWidget*)itcTreeGetChildAt(wheel, i);
								fy = 0 - child->rect.height * ((wheel->focusIndex + wheel->focus_dev - (wheel->itemCount / 2)) + (wheel->fix_count * ((offset >= 0) ? (1) : (-1))));
								fy += i * child->rect.height;
								fy += offset;
								ituWidgetSetY(child, fy);
							}
						}

						if ((offset >= height) || (offset <= (0 - height)))
						{
							if (offset > 0)
							{
								shift = offset / height;

								if ((wheel->fix_count < shift) && (wheel->focusIndex > 0))
								{
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
									ituTextSetFontHeight(text, wheel->fontHeight);
									fc--;
									wheel->focusIndex--;
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);
									ituTextSetFontHeight(text, wheel->focusFontHeight);

									wheel->fix_count++;
									//printf("===[wheel][A][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
								else if ((wheel->fix_count > shift) && (wheel->focusIndex < fmax))
								{
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
									ituTextSetFontHeight(text, wheel->fontHeight);
									fc++;
									wheel->focusIndex++;
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);
									ituTextSetFontHeight(text, wheel->focusFontHeight);

									wheel->fix_count--;
									//printf("===[wheel][B][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
							}
							else
							{
								shift = (-1) * offset / height;

								if ((wheel->fix_count < shift) && (wheel->focusIndex < fmax))
								{
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
									ituTextSetFontHeight(text, wheel->fontHeight);
									fc++;
									wheel->focusIndex++;
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);
									ituTextSetFontHeight(text, wheel->focusFontHeight);

									wheel->fix_count++;
									//printf("===[wheel][C][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
								else if ((wheel->fix_count > shift) && (wheel->focusIndex > 0))
								{
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
									ituTextSetFontHeight(text, wheel->fontHeight);
									fc--;
									wheel->focusIndex--;
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);
									ituTextSetFontHeight(text, wheel->focusFontHeight);

									wheel->fix_count--;
									//printf("===[wheel][D][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
							}
						}
						else
						{
							if (offset > 0)
							{
								if ((wheel->fix_count) && (wheel->focusIndex < fmax))
								{
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
									ituTextSetFontHeight(text, wheel->fontHeight);
									fc++;
									wheel->focusIndex++;
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);
									ituTextSetFontHeight(text, wheel->focusFontHeight);

									wheel->fix_count--;
									//printf("===[wheel][E][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
							}
							else
							{
								if ((wheel->fix_count) && (wheel->focusIndex > 0))
								{
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
									ituTextSetFontHeight(text, wheel->fontHeight);
									fc--;
									wheel->focusIndex--;
									child = (ITUWidget*)itcTreeGetChildAt(wheel, fc);
									text = (ITUText*)child;
									ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);
									ituTextSetFontHeight(text, wheel->focusFontHeight);

									wheel->fix_count--;
									//printf("===[wheel][F][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
							}
						}

						result = true;
					}
				}
				else //for cycle mode
				{
					if (ituWidgetIsInside(widget, x, y))
					{
						int i, fy, fm, offset, shift, count = itcTreeGetChildCount(wheel);
						ITUWidget* child;

						offset = y - wheel->touchY;
						child = (ITUWidget*)itcTreeGetChildAt(wheel, 0);

						if (wheel->scal > 0)
							offset /= wheel->scal;

						for (i = 0; i < wheel->cycle_arr_count; i++)
						{
							child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[i]);
							
							if (i == 0)
								fy = 0 - child->rect.height * (wheel->itemCount / 2 + wheel->cycle_tor) + (wheel->itemCount / 4 * child->rect.height);

							ituWidgetSetY(child, fy + offset + ((offset>0)?(-1):(1)) * ((wheel->fix_count) ? (child->rect.height * wheel->fix_count) : (0)));
							fy += child->rect.height;
						}

						if ((offset >= child->rect.height) || (offset <= (0 - child->rect.height)))
						{
							if (offset > 0)
							{
								shift = offset / child->rect.height;

								if (wheel->fix_count < shift)
								{
									for (i = 0; i < wheel->cycle_arr_count; i++)
									{
										wheel->cycle_arr[i]--;

										if (wheel->cycle_arr[i] < wheel->minci)
											wheel->cycle_arr[i] = wheel->maxci;

										child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[i]);

										if (i == 0)
											fy = 0 - child->rect.height * (wheel->itemCount / 2 + wheel->cycle_tor) + (wheel->itemCount / 4 * child->rect.height);

										fy += child->rect.height;
									}
									wheel->fix_count++;
									fm = wheel->focus_c - 1;

									if (fm < wheel->minci)
										fm = wheel->maxci;

									focus_change(widget, fm, __LINE__);
									//printf("===[wheel][A][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
								else if (wheel->fix_count > shift)
								{
									child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[0]);
									ituWidgetSetY(child, wheel->tempy);

									for (i = 0; i < wheel->cycle_arr_count; i++)
									{
										wheel->cycle_arr[i]++;

										if (wheel->cycle_arr[i] > wheel->maxci)
											wheel->cycle_arr[i] = wheel->minci;

										child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[i]);

										if (i == 0)
											fy = 0 - child->rect.height * (wheel->itemCount / 2 + wheel->cycle_tor) + (wheel->itemCount / 4 * child->rect.height);

										fy += child->rect.height;
									}
									wheel->fix_count--;
									fm = wheel->focus_c + 1;

									if (fm > wheel->maxci)
										fm = wheel->minci;

									focus_change(widget, fm, __LINE__);
									//printf("===[wheel][B][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
							}
							else
							{
								shift = (-1) * offset / child->rect.height;

								if (wheel->fix_count < shift)
								{
									for (i = 0; i < wheel->cycle_arr_count; i++)
									{
										wheel->cycle_arr[i]++;

										if (wheel->cycle_arr[i] > wheel->maxci)
											wheel->cycle_arr[i] = wheel->minci;

										child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[i]);

										if (i == 0)
											fy = 0 - child->rect.height * (wheel->itemCount / 2 + wheel->cycle_tor) + (wheel->itemCount / 4 * child->rect.height);

										fy += child->rect.height;
									}
									wheel->fix_count++;
									fm = wheel->focus_c + 1;

									if (fm > wheel->maxci)
										fm = wheel->minci;

									focus_change(widget, fm, __LINE__);
									//printf("===[wheel][C][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
								else if (wheel->fix_count > shift)
								{
									child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[wheel->cycle_arr_count - 1]);
									ituWidgetSetY(child, wheel->tempy);

									for (i = 0; i < wheel->cycle_arr_count; i++)
									{
										wheel->cycle_arr[i]--;

										if (wheel->cycle_arr[i] < wheel->minci)
											wheel->cycle_arr[i] = wheel->maxci;

										child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[i]);

										if (i == 0)
											fy = 0 - child->rect.height * (wheel->itemCount / 2 + wheel->cycle_tor) + (wheel->itemCount / 4 * child->rect.height);

										fy += child->rect.height;
									}
									wheel->fix_count--;
									fm = wheel->focus_c - 1;

									if (fm < wheel->minci)
										fm = wheel->maxci;

									focus_change(widget, fm, __LINE__);
									//printf("===[wheel][D][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
							}
						}
						else
						{
							if (offset > 0)
							{
								if (wheel->fix_count)
								{
									child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[0]);
									ituWidgetSetY(child, wheel->tempy);

									for (i = 0; i < wheel->cycle_arr_count; i++)
									{
										wheel->cycle_arr[i]++;

										if (wheel->cycle_arr[i] > wheel->maxci)
											wheel->cycle_arr[i] = wheel->minci;

										child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[i]);

										if (i == 0)
											fy = 0 - child->rect.height * (wheel->itemCount / 2 + wheel->cycle_tor) + (wheel->itemCount / 4 * child->rect.height);

										fy += child->rect.height;
									}
									wheel->fix_count--;
									fm = wheel->focus_c + 1;

									if (fm > wheel->maxci)
										fm = wheel->minci;

									focus_change(widget, fm, __LINE__);
									//printf("===[wheel][E][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
							}
							else
							{
								if (wheel->fix_count)
								{
									child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[wheel->cycle_arr_count - 1]);
									ituWidgetSetY(child, wheel->tempy);

									for (i = 0; i < wheel->cycle_arr_count; i++)
									{
										wheel->cycle_arr[i]--;

										if (wheel->cycle_arr[i] < wheel->minci)
											wheel->cycle_arr[i] = wheel->maxci;

										child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[i]);

										if (i == 0)
											fy = 0 - child->rect.height * (wheel->itemCount / 2 + wheel->cycle_tor) + (wheel->itemCount / 4 * child->rect.height);

										fy += child->rect.height;
									}
									wheel->fix_count--;
									fm = wheel->focus_c - 1;

									if (fm < wheel->minci)
										fm = wheel->maxci;

									focus_change(widget, fm, __LINE__);
									//printf("===[wheel][F][wheel->fix_count %d][shift %d]===\n", wheel->fix_count, shift);
								}
							}
						}

						result = true;
					}
				}
            }
        }
        else if (ev == ITU_EVENT_MOUSEDOWN)
        {
			int x = arg2 - widget->rect.x;
			int y = arg3 - widget->rect.y;
			//printf("[x,y,touchy] [%d,%d,%d]\n",x ,y , wheel->touchY);
            if (ituWidgetIsEnabled(widget) && (widget->flags & ITU_DRAGGABLE) && !result)
            {
                if (ituWidgetIsInside(widget, x, y))
                {
					wheel->inside = 1;
					wheel->fix_count = 0;
                    wheel->touchY = y;
                    widget->flags |= ITU_DRAGGING;
                    ituScene->dragged = widget;
                    result = true;
                }
            }

			if (ituWidgetIsEnabled(widget) && ituWidgetIsInside(widget, x, y))
			{
				get_normal_color(widget);
				wheel->frame = wheel->totalframe;
				wheel->inc = 0;

				if (wheel->sliding == 1)
				{
					wheel->idle = 1;
				}
			}
        }
        else if (ev == ITU_EVENT_MOUSEUP)
        {
            if (ituWidgetIsEnabled(widget) && (widget->flags & ITU_DRAGGABLE) && (widget->flags & ITU_DRAGGING))
            {
                int count = itcTreeGetChildCount(wheel);
				int fmax = get_max_focusindex(widget);
                int y = arg3 - widget->rect.y;
				ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, 0);
				wheel->sliding = 0;
				wheel->inside = 0;
				if ((count > 0) && (wheel->cycle_tor <= 0))
                {
					if ((wheel->inc == 0) && (wheel->fix_count == 0))
                    {
                        int offset, absoffset, interval;
                        offset = y - wheel->touchY;
                        interval = offset / child->rect.height;
                        offset -= (interval * child->rect.height);
                        absoffset = offset > 0 ? offset : -offset;

                        if (absoffset > child->rect.height / 2)
                        {
                            wheel->frame = absoffset / (child->rect.height / wheel->totalframe) + 1;

                            if (offset > 0)
                            {
                                wheel->inc = child->rect.height;
                                wheel->focusIndex -= interval;
								//printf("===[wheel][1][frame %d][offset %d][inc %d][interval %d][focusIndex %d]===\n", wheel->frame, offset, wheel->inc, interval, wheel->focusIndex);
                            }
							else if (offset < 0)
                            {
                                wheel->inc = -child->rect.height;
                                wheel->focusIndex -= interval;
								//printf("===[wheel][2][frame %d][offset %d][inc %d][interval %d][focusIndex %d]===\n", wheel->frame, offset, wheel->inc, interval, wheel->focusIndex);
                            }
                        }
                        else
                        {
                            wheel->frame = wheel->totalframe - absoffset / (child->rect.height / wheel->totalframe);

                            if (offset > 0)
                            {
                                wheel->inc = -child->rect.height;
								wheel->focusIndex -= interval;// +1;
								if (interval == 0)
									wheel->inc = 0;
								//printf("===[wheel][3][frame %d][offset %d][inc %d][interval %d][focusIndex %d]===\n", wheel->frame, offset, wheel->inc, interval, wheel->focusIndex);
                            }
							else if (offset < 0)
                            {
                                wheel->inc = child->rect.height;
								wheel->focusIndex -= interval;// -1;
								if (interval == 0)
									wheel->inc = 0;
								//printf("===[wheel][4][frame %d][offset %d][inc %d][interval %d][focusIndex %d]===\n", wheel->frame, offset, wheel->inc, interval, wheel->focusIndex);
                            }
                        }

                        if (wheel->inc > 0)
                        {
                            if (wheel->focusIndex <= 0)
                            {
                                wheel->focusIndex = 0;
                                wheel->frame = wheel->totalframe;
                                wheel->inc = 0;
								//printf("===[wheel][5]\n");
                            }
                            else
                            {
                                if (wheel->focusIndex >= fmax)
                                {
                                    wheel->focusIndex = fmax;
                                    wheel->frame = wheel->totalframe;
                                    wheel->inc = 0;
									//printf("===[wheel][6]\n");
                                }
                            }
                        }
                        else // if (wheel->inc < 0)
                        {
                            if (wheel->focusIndex <= 0)
                            {
                                wheel->focusIndex = 0;
                                wheel->frame = wheel->totalframe;
                                wheel->inc = 0;
								//printf("===[wheel][7]\n");
                            }
                            else
                            {
                                if (wheel->focusIndex >= fmax)
                                {
                                    wheel->focusIndex = fmax;
                                    wheel->frame = wheel->totalframe;
                                    wheel->inc = 0;
									//printf("===[wheel][8]\n");
                                }
                            }

							if (wheel->idle == 0)
							{
								if ((absoffset <= 1) && (ituWidgetIsVisible(widget)))
								{
									ituExecActions(widget, wheel->actions, ITU_EVENT_CUSTOM, 0);
									wheel->idle = 10;
								}
							}
							else
							{
								wheel->idle = 0;
							}
                        }
                        widget->flags |= ITU_UNDRAGGING;
                    }
					else if (wheel->fix_count)
					{
						wheel->frame = wheel->totalframe;
						wheel->inc = 0;
						wheel->fix_count = 0;
						widget->flags |= ITU_UNDRAGGING;
					}
					else
					{
						widget->flags |= ITU_UNDRAGGING;
						wheel->frame = wheel->totalframe;
						widget->flags &= ~ITU_DRAGGING;
					}
                }
				else if ((count > 0) && wheel->cycle_tor) //for cycle mode
				{
					int i = 0;
					int newfocus = 0;

					if (wheel->inc == 0)
					{
						int offset, absoffset, interval;

						offset = y - wheel->touchY;
						interval = offset / child->rect.height;
						absoffset = offset > 0 ? offset : -offset;

						if (absoffset > child->rect.height / 2)
						{
							wheel->frame = absoffset / (child->rect.height / wheel->totalframe) + 1;

							if (offset >= 0)
							{
								wheel->inc = child->rect.height;
								newfocus = wheel->focus_c;

								for (i = 1; i <= interval; i++)
								{
									newfocus--;

									if (newfocus < wheel->minci)
										newfocus = wheel->maxci;

									if ((interval != 0) && (wheel->fix_count == 0))
									{
										if (focus_change(widget, newfocus, __LINE__))
											cycle_arrange(widget, true);
									}
								}

								if (interval != 0)
								{
									if (wheel->fix_count)
										wheel->inc = 0;
								}
								else
								{
									wheel->inc = 0;
									wheel->frame = wheel->totalframe;
								}
								//printf("===[wheel][1][frame %d][offset %d][inc %d][interval %d][focusIndex %d]===\n", wheel->frame, offset, wheel->inc, interval, wheel->focusIndex);
							}
							else
							{
								wheel->inc = -child->rect.height;
								newfocus = wheel->focus_c;

								for (i = interval; i < 0; i++)
								{
									newfocus++;

									if (newfocus > wheel->maxci)
										newfocus = wheel->minci;

									if ((interval != 0) && (wheel->fix_count == 0))
									{
										if (focus_change(widget, newfocus, __LINE__))
											cycle_arrange(widget, false);
									}
								}

								if (interval != 0)
								{
									if (wheel->fix_count)
										wheel->inc = 0;
								}
								else
								{
									wheel->inc = 0;
									wheel->frame = wheel->totalframe;
								}
								//printf("===[wheel][2][frame %d][offset %d][inc %d][interval %d][focusIndex %d]===\n", wheel->frame, offset, wheel->inc, interval, wheel->focusIndex);
							}
						}
						else
						{
							wheel->frame = wheel->totalframe - absoffset / (child->rect.height / wheel->totalframe);

							if (offset >= 0)
							{
								wheel->inc = -child->rect.height;
								newfocus = wheel->focus_c;

								for (i = interval; i < 0; i++)
								{
									newfocus++;

									if (newfocus > wheel->maxci)
										newfocus = wheel->minci;

									if ((interval != 0) && (wheel->fix_count == 0))
									{
										if (focus_change(widget, newfocus, __LINE__))
											cycle_arrange(widget, false);
									}
								}

								if (interval != 0)
								{
									if (wheel->fix_count)
										wheel->inc = 0;
								}
								else
								{
									wheel->inc = 0;
									wheel->frame = wheel->totalframe;
								}
								//printf("===[wheel][3][frame %d][offset %d][inc %d][interval %d][focusIndex %d]===\n", wheel->frame, offset, wheel->inc, interval, wheel->focusIndex);
							}
							else
							{
								wheel->inc = child->rect.height;
								newfocus = wheel->focus_c;

								for (i = 1; i <= interval; i++)
								{
									newfocus--;

									if (newfocus < wheel->minci)
										newfocus = wheel->maxci;

									if ((interval != 0) && (wheel->fix_count == 0))
									{
										if (focus_change(widget, newfocus, __LINE__))
											cycle_arrange(widget, true);
									}
								}

								if (interval != 0)
								{
									if (wheel->fix_count)
										wheel->inc = 0;
								}
								else
								{
									wheel->inc = 0;
									wheel->frame = wheel->totalframe;
								}
								//printf("===[wheel][4][frame %d][offset %d][inc %d][interval %d][focusIndex %d]===\n", wheel->frame, offset, wheel->inc, interval, wheel->focusIndex);
							}
						}

						if (wheel->inc > 0)
						{
							if (wheel->focusIndex < 0)
							{
								wheel->focus_c = wheel->maxci;
								wheel->focusIndex = wheel->focus_c - wheel->focus_dev;
								wheel->frame = wheel->totalframe;
								wheel->inc = 0;
							}
							else
							{
								if (wheel->focusIndex > (wheel->maxci - wheel->focus_dev))
								{
									wheel->focus_c = wheel->minci;
									wheel->focusIndex = wheel->focus_c - wheel->focus_dev;
									wheel->frame = wheel->totalframe;
									wheel->inc = 0;
								}
							}
						}
						else // if (wheel->inc <= 0)
						{
							if (wheel->focusIndex < 0)
							{
								wheel->focus_c = wheel->maxci;
								wheel->focusIndex = wheel->focus_c - wheel->focus_dev;
								wheel->frame = wheel->totalframe;
								wheel->inc = 0;
							}
							else
							{
								if (wheel->focusIndex >(wheel->maxci - wheel->focus_dev))
								{
									wheel->focus_c = wheel->minci;
									wheel->focusIndex = wheel->focus_c - wheel->focus_dev;
									wheel->frame = wheel->totalframe;
									wheel->inc = 0;
								}
							}
						}
						widget->flags |= ITU_UNDRAGGING;
					}
					else
					{
						wheel->frame = wheel->totalframe;
					}
				}
                result = true;
            }

            widget->flags &= ~ITU_DRAGGING;
            wheel->touchCount = 0;
        }
    }
    
    if (ev == ITU_EVENT_TIMER)
    {
		if (wheel->cycle_tor <= 0)
		{
			int fmax = get_max_focusindex(widget);

			if ((widget->flags & ITU_UNDRAGGING) && (wheel->sliding == 0))//
			{
				int i, count = itcTreeGetChildCount(wheel);
				ITUColor color;
				ITUText* text;

				for (i = 0; i < count; ++i)
				{
				   int fy;
					ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, i);
					text = (ITUText*)itcTreeGetChildAt(wheel, i);
					fy = 0 - (child->rect.height * (wheel->focusIndex + wheel->focus_dev - (wheel->itemCount / 2)));
					fy += i * child->rect.height;
					fy += wheel->inc * wheel->frame / wheel->totalframe;

					if (i == 0)
					{
						//memcpy(&color, &NColor, sizeof (ITUColor));
						use_normal_color(widget, &color);
					}

					ituWidgetSetColor(text, color.alpha, color.red, color.green, color.blue);
					ituTextSetFontHeight(text, wheel->fontHeight);
					ituWidgetSetY(child, fy);
				}

				//if (wheel->sliding == 0)
				//	wheel->frame = wheel->totalframe;

				wheel->frame++;

				if (wheel->frame > wheel->totalframe)
				{
					if ((wheel->inc > 0) && (wheel->focusIndex > 0))
						wheel->focusIndex--;
					else if ((wheel->inc < 0) && (wheel->focusIndex < fmax))
						wheel->focusIndex++;

					wheel->frame = 0;

					if (wheel->sliding == 0)
					{
						wheel->inc = 0;
					}
					else
					{
						wheel->sliding = 0;
					}

					widget->flags &= ~ITU_UNDRAGGING;

					for (i = 0; i < count; ++i)
					{
						ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, i);
						int fy = 0 - (child->rect.height * (wheel->focusIndex + wheel->focus_dev - (wheel->itemCount / 2)));
						fy += i * child->rect.height;

						ituWidgetSetY(child, fy);
					}

					text = (ITUText*)itcTreeGetChildAt(wheel, wheel->focusIndex + wheel->focus_dev);
					ituWidgetSetColor(text, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);
					ituTextSetFontHeight(text, wheel->focusFontHeight);
					//widget->flags |= ITU_UNDRAGGING;
					//widget->flags &= ~ITU_DRAGGING;

					ituExecActions(widget, wheel->actions, ITU_EVENT_CHANGED, wheel->focusIndex);
					//value = ituTextGetString(text);
					//ituWheelOnValueChanged(wheel, value);
				}
			}////////////////// working area
			else if ((wheel->inc) && (wheel->sliding) && (wheel->focusIndex != 0) && (wheel->focusIndex !=fmax))
			{
				int i, fac, fy, height, height0, count = itcTreeGetChildCount(wheel);
				//int xx = 10 + (int)((float)(wheel->slide_step * 2) * (float)(wheel->frame) / (float)(wheel->totalframe));
				int xx = (int)(STEP_SPEED_BASE * ((float)(wheel->frame) / (float)(wheel->totalframe))) + (int)((float)(wheel->slide_step * 2));
				int sig;
				float xxx = (float)xx;
				ITUColor color;
				ITUText* text;

				sig = ((wheel->inc > 0) ? (1) : (-1));

				for (i = 0; i < count; ++i)
				{
					ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, i);
					int unit;

					if (i == 0)
					{
						height0 = height = child->rect.height;
						fy = 0 - (child->rect.height * (wheel->focusIndex + wheel->focus_dev - (wheel->itemCount / 2)));
						use_normal_color(widget, &color);
					}

					if (i == wheel->focusIndex + wheel->focus_dev)
					{
						ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);
					}
					else
					{
						ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
					}
					ituWidgetSetY(child, fy + wheel->inc * wheel->frame / wheel->totalframe);
					unit = (int)(((float)(height0 * sig)) * (((((float)wheel->moving_step) / xxx)) / 1.0f));
					//fac = fy + ((float)wheel->inc * ((float)(wheel->frame - 1) + ((float)wheel->moving_step / xxx)) / (float)wheel->totalframe);
					fac = fy + unit;
					//printf("[SLIDE %d] unit --> %d\n", xx, unit);
					ituWidgetSetY(child, fac);

					child->rect.height = height = height0;
					fy += height;
				}

				wheel->moving_step += 1;

				if (wheel->moving_step > xx)
				{
					if (widget->flags & ITU_DRAGGABLE)
					{
						//usleep(33000 * wheel->slide_step * wheel->frame / wheel->totalframe);

						if ((wheel->inc > 0) && (wheel->focusIndex == 1))
							wheel->frame = wheel->totalframe;
						else if ((wheel->inc < 0) && (wheel->focusIndex == (fmax - 1)))
							wheel->frame = wheel->totalframe;
					}

					//wheel->frame++;
					wheel->moving_step = 1;

					if ((wheel->shift_one) && (widget->flags & ITU_DRAGGABLE))
					{
						wheel->frame = wheel->totalframe;
						wheel->shift_one = 0;
					}
					wheel->frame++;

					if (wheel->frame > wheel->totalframe)
					{
						if ((wheel->inc > 0) && (wheel->focusIndex > 0))
							wheel->focusIndex--;
						else if ((wheel->inc < 0) && (wheel->focusIndex < fmax))
							wheel->focusIndex++;

						wheel->frame = 0;
						wheel->inc = 0;
						widget->flags &= ~ITU_UNDRAGGING;

						wheel->moving_step = 0;

						if (wheel->sliding == 1)
							wheel->sliding = 0;

						for (i = 0; i < count; ++i)
						{
							ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, i);

							if (i == 0)
							{
								height0 = height = child->rect.height;
								fy = 0 - (child->rect.height * (wheel->focusIndex + wheel->focus_dev - (wheel->itemCount / 2)));
							}

							text = (ITUText*)child;

							ituWidgetSetY(child, fy);

							child->rect.height = height = height0;

							if (i == (wheel->focusIndex + wheel->focus_dev))
							{
								ituWidgetSetColor(text, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);

								ituTextSetFontHeight(text, wheel->focusFontHeight);

								ituExecActions(widget, wheel->actions, ITU_EVENT_CHANGED, wheel->focusIndex);
								//value = ituTextGetString(text);
								//ituWheelOnValueChanged(wheel, value);
							}
							else
							{
								ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
								ituTextSetFontHeight(text, wheel->fontHeight);
							}
							fy += height;
						}
						//check here (fix the wrong position when slide stop by itself)
						widget->dirty = true;
						result |= widget->dirty;
						return widget->visible ? result : false;
					}
					else if (widget->flags & ITU_DRAGGABLE)
					{
						if ((wheel->inc > 0) && (wheel->focusIndex > 0))
							wheel->focusIndex--;
						else if ((wheel->inc < 0) && (wheel->focusIndex < fmax))
							wheel->focusIndex++;

						for (i = 0; i < count; ++i)
						{
							ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, i);

							if (i == 0)
							{
								height0 = height = child->rect.height;
								fy = 0 - (child->rect.height * (wheel->focusIndex + wheel->focus_dev - (wheel->itemCount / 2)));
							}

							text = (ITUText*)child;

							ituWidgetSetY(child, fy);

							child->rect.height = height = height0;

							if (i == (wheel->focusIndex + wheel->focus_dev))
							{
								ituWidgetSetColor(text, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);

								ituTextSetFontHeight(text, wheel->focusFontHeight);

								ituExecActions(widget, wheel->actions, ITU_EVENT_CHANGED, wheel->focusIndex);
								//value = ituTextGetString(text);
								//ituWheelOnValueChanged(wheel, value);
							}
							else
							{
								ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
								ituTextSetFontHeight(text, wheel->fontHeight);
							}
							fy += height;
						}
					}
				}
				else
				{
					//usleep(200000);
					//printf("[SLEEP] here!\n");
					widget->dirty = true;
					result |= widget->dirty;
					return widget->visible ? result : false;
				}
			}
			else if (wheel->inc)
			{
				int i, fy, height, height0, count = itcTreeGetChildCount(wheel);
				ITUColor color;
				ITUText* text;

				for (i = 0; i < count; ++i)
				{
					ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, i);

					if (i == 0)
					{
						height0 = height = child->rect.height;
						fy = 0 - (child->rect.height * (wheel->focusIndex + wheel->focus_dev - (wheel->itemCount / 2)));
						//memcpy(&color, &NColor, sizeof (ITUColor));
						use_normal_color(widget, &color);
					}

					ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
					ituWidgetSetY(child, fy + wheel->inc * wheel->frame / wheel->totalframe);
					//ggg = fy + ((float)wheel->inc * ((float)(wheel->frame - 1) + ((float)j / 5.0f)) / (float)wheel->totalframe);
					//ituWidgetSetY(child, ggg);

					child->rect.height = height = height0;

					if ((widget->flags & ITU_DRAGGABLE) == 0)
					{
						ITUText* text = (ITUText*)child;

						if (i == (wheel->focusIndex + wheel->focus_dev))
						{
							int a = color.alpha + (wheel->focusColor.alpha - color.alpha) * (wheel->totalframe - wheel->frame) / wheel->totalframe;
							int r = color.red + (wheel->focusColor.red - color.red) * (wheel->totalframe - wheel->frame) / wheel->totalframe;
							int g = color.green + (wheel->focusColor.green - color.green) * (wheel->totalframe - wheel->frame) / wheel->totalframe;
							int b = color.blue + (wheel->focusColor.blue - color.blue) * (wheel->totalframe - wheel->frame) / wheel->totalframe;
							int h = height + (wheel->focusFontHeight - height) * (wheel->totalframe - wheel->frame) / wheel->totalframe;

							//child->rect.height = height = h;
							ituWidgetSetColor(text, a, r, g, b);
							ituTextSetFontHeight(text, wheel->fontHeight + (wheel->focusFontHeight - wheel->fontHeight) * (wheel->totalframe - wheel->frame) / wheel->totalframe);
						}
						else if (((i == wheel->focusIndex + wheel->focus_dev - 1) && (wheel->inc > 0)) ||
							((i == wheel->focusIndex + wheel->focus_dev + 1) && (wheel->inc < 0)))
						{
							int a = color.alpha + (wheel->focusColor.alpha - color.alpha) * wheel->frame / wheel->totalframe;
							int r = color.red + (wheel->focusColor.red - color.red) * wheel->frame / wheel->totalframe;
							int g = color.green + (wheel->focusColor.green - color.green) * wheel->frame / wheel->totalframe;
							int b = color.blue + (wheel->focusColor.blue - color.blue) * wheel->frame / wheel->totalframe;
							int h = height + (wheel->focusFontHeight - height) * wheel->frame / wheel->totalframe;

							//child->rect.height = height = h;
							ituWidgetSetColor(text, a, r, g, b);
							ituTextSetFontHeight(text, wheel->fontHeight + (wheel->focusFontHeight - wheel->fontHeight) * wheel->frame / wheel->totalframe);
						}
					}
					fy += height;
				}
				if (widget->flags & ITU_DRAGGABLE)
				{
					usleep(33000 * wheel->slide_step * wheel->frame / wheel->totalframe);

					if ((wheel->inc > 0) && (wheel->focusIndex == 1))
						wheel->frame = wheel->totalframe;
					else if ((wheel->inc < 0) && (wheel->focusIndex == (fmax - 1)))
						wheel->frame = wheel->totalframe;
				}

				if ((wheel->shift_one) && (widget->flags & ITU_DRAGGABLE))
				{
					wheel->frame = wheel->totalframe;
					wheel->shift_one = 0;
				}

				wheel->frame++;

				if (wheel->frame > wheel->totalframe)
				{
					if ((wheel->inc > 0) && (wheel->focusIndex > 0))
						wheel->focusIndex--;
					else if ((wheel->inc < 0) && (wheel->focusIndex < fmax))
						wheel->focusIndex++;

					wheel->frame = 0;
					wheel->inc = 0;
					widget->flags &= ~ITU_UNDRAGGING;

					if (wheel->sliding == 1) //
						wheel->sliding = 0; //

					for (i = 0; i < count; ++i)
					{
						ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, i);

						if (i == 0)
						{
							height0 = height = child->rect.height;
							fy = 0 - (child->rect.height * (wheel->focusIndex + wheel->focus_dev - (wheel->itemCount / 2)));
						}

						text = (ITUText*)child;

						ituWidgetSetY(child, fy);

						child->rect.height = height = height0;

						if (i == wheel->focusIndex + wheel->focus_dev)
						{
							ituWidgetSetColor(text, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);

							ituTextSetFontHeight(text, wheel->focusFontHeight);

							ituExecActions(widget, wheel->actions, ITU_EVENT_CHANGED, wheel->focusIndex);
							//value = ituTextGetString(text);
							//ituWheelOnValueChanged(wheel, value);
						}
						else
						{
							ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
							ituTextSetFontHeight(text, wheel->fontHeight);
						}
						fy += height;
					}
				}
				else if (widget->flags & ITU_DRAGGABLE)
				{
					if ((wheel->inc > 0) && (wheel->focusIndex > 0))
						wheel->focusIndex--;
					else if ((wheel->inc < 0) && (wheel->focusIndex < fmax))
						wheel->focusIndex++;

					for (i = 0; i < count; ++i)
					{
						ITUWidget* child = (ITUWidget*)itcTreeGetChildAt(wheel, i);

						if (i == 0)
						{
							height0 = height = child->rect.height;
							fy = 0 - (child->rect.height * (wheel->focusIndex + wheel->focus_dev - (wheel->itemCount / 2)));
						}

						text = (ITUText*)child;

						ituWidgetSetY(child, fy);

						child->rect.height = height = height0;

						if (i == (wheel->focusIndex + wheel->focus_dev))
						{
							ituWidgetSetColor(text, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);

							ituTextSetFontHeight(text, wheel->focusFontHeight);

							ituExecActions(widget, wheel->actions, ITU_EVENT_CHANGED, wheel->focusIndex);
							//value = ituTextGetString(text);
							//ituWheelOnValueChanged(wheel, value);
						}
						else
						{
							ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
							ituTextSetFontHeight(text, wheel->fontHeight);
						}
						fy += height;
					}
				}
			}
			else
			{
				if ((wheel->focusIndex == 0) || (wheel->focusIndex == fmax))
				{
					wheel->frame = 0;
					wheel->inc = 0;
					widget->flags &= ~ITU_UNDRAGGING;
				}
			}
		}
		else //for cycle TIMER
		{
			if ((widget->flags & ITU_UNDRAGGING) && (wheel->sliding == 0))
			{
				int i, newfocus, count = itcTreeGetChildCount(wheel);
				ITUColor color;
				ITUText* text;
				int fy = 0;

				for (i = 0; i < wheel->cycle_arr_count; i++)
				{
					ITUWidget* child;

					if (i == 0)
					{
						child = (ITUWidget*)itcTreeGetChildAt(wheel, 0);
						//memcpy(&color, &NColor, sizeof (ITUColor));
						use_normal_color(widget, &color);
						fy = 0 - child->rect.height * (wheel->itemCount / 2 + wheel->cycle_tor) + (wheel->itemCount / 4 * child->rect.height);
					}

					child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[i]);
					text = (ITUText*)child;
					
					ituWidgetSetY(child, fy);
					//printf("===[wheel][timer][child %d][str %s][fy %d]===\n", wheel->cycle_arr[i], ituTextGetString(text), fy);

					fy += child->rect.height;
					fy += wheel->inc * wheel->frame / wheel->totalframe;
				}

				//if (wheel->sliding == 0)
				//	wheel->frame = wheel->totalframe;

				wheel->frame++;

				if (wheel->frame > wheel->totalframe)
				{
					if ((wheel->inc > 0) && (wheel->fix_count == 0)) // check
					{
						newfocus = wheel->focus_c - 1;
						if (wheel->cycle_tor && (newfocus < wheel->minci))
							newfocus = wheel->maxci;
						
						if (focus_change(widget, newfocus, __LINE__))
							cycle_arrange(widget, true);
					}
					else if ((wheel->inc < 0) && (wheel->fix_count == 0)) // check
					{
						newfocus = wheel->focus_c + 1;
						if (wheel->cycle_tor && (newfocus > wheel->maxci))
							newfocus = wheel->minci;
						
						if (focus_change(widget, newfocus, __LINE__))
							cycle_arrange(widget, false);
					}
					else if (wheel->inc > 0) //for slide
					{
						for (i = 0; i < wheel->fix_count; i++)
						{
							newfocus = wheel->focus_c - 1;
							if (newfocus < wheel->minci)
								newfocus = wheel->maxci;
							
							
							if (focus_change(widget, newfocus, __LINE__))
								cycle_arrange(widget, true);
						}
					}
					else if (wheel->inc < 0) // for slide
					{
						for (i = 0; i < wheel->fix_count; i++)
						{
							newfocus = wheel->focus_c + 1;
							if (newfocus > wheel->maxci)
								newfocus = wheel->minci;
							
							
							if (focus_change(widget, newfocus, __LINE__))
								cycle_arrange(widget, false);
						}
					}
					wheel->frame = 0;
					//wheel->inc = 0;

					if (wheel->sliding == 0)
					{
						wheel->inc = 0;
					}
					else
					{
						wheel->sliding = 0;
					}


					widget->flags &= ~ITU_UNDRAGGING;

					//text = (ITUText*)itcTreeGetChildAt(wheel, wheel->focus_c);
					//value = ituTextGetString(text);

					//ituExecActions(widget, wheel->actions, ITU_EVENT_CHANGED, wheel->focusIndex);
					//ituWheelOnValueChanged(wheel, value);
				}
			}
			else if (wheel->inc) //non-drag-mode
			{
				int i, fy, height, count = itcTreeGetChildCount(wheel);
				ITUColor color;
				ITUText* text;

				for (i = 0; i < wheel->cycle_arr_count; ++i)
				{
					ITUWidget* child;

					if (i == 0)
					{
						child = (ITUWidget*)itcTreeGetChildAt(wheel, 0);
						//memcpy(&color, &NColor, sizeof (ITUColor));
						use_normal_color(widget, &color);
						height = child->rect.height;
						fy = 0 - (height * (wheel->itemCount / 2 + wheel->cycle_tor)) + ((wheel->itemCount / 4) * height);
					}

					child = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->cycle_arr[i]);
					text = (ITUText*)child;

					if (wheel->cycle_arr[i] == wheel->focus_c)
					{
						ituWidgetSetColor(child, wheel->focusColor.alpha, wheel->focusColor.red, wheel->focusColor.green, wheel->focusColor.blue);
						ituTextSetFontHeight(text, wheel->focusFontHeight);
					}
					else
					{
						ituWidgetSetColor(child, color.alpha, color.red, color.green, color.blue);
						ituTextSetFontHeight(text, wheel->fontHeight);
					}

					ituWidgetSetY(child, fy + wheel->inc * wheel->frame / wheel->totalframe);
					fy += height;
				}

				if (widget->flags & ITU_DRAGGABLE)
				{
					usleep(33000 * wheel->slide_step * wheel->frame / wheel->totalframe);
				}

				if ((wheel->shift_one) && (widget->flags & ITU_DRAGGABLE))
				{
					wheel->frame = wheel->totalframe;
					wheel->shift_one = 0;
				}

				wheel->frame++;

				if (wheel->frame > wheel->totalframe)
				{
					int newfocus;

					if (wheel->inc > 0)
					{
						newfocus = wheel->focus_c;

						for (i = 0; i < (1 + 0); i++) // +wheel->fix_count
						{
							newfocus--;

							if (newfocus < wheel->minci)
								newfocus = wheel->maxci;
						}
						if (focus_change(widget, newfocus, __LINE__))
							cycle_arrange(widget, true);
					}
					else if (wheel->inc < 0)
					{
						newfocus = wheel->focus_c;

						for (i = 0; i < (1 + 0); i++) // +wheel->fix_count
						{
							newfocus++;

							if (newfocus > wheel->maxci)
								newfocus = wheel->minci;
						}
						if (focus_change(widget, newfocus, __LINE__))
							cycle_arrange(widget, false);
					}

					wheel->frame = 0;
					wheel->inc = 0;
					widget->flags &= ~ITU_UNDRAGGING;

					if (wheel->sliding == 1) //
						wheel->sliding = 0; //
				}
				else if (widget->flags & ITU_DRAGGABLE)
				{
					int newfocus;
					if (wheel->inc > 0)
					{
						newfocus = wheel->focus_c;

						for (i = 0; i < (1 + 0); i++) // +wheel->fix_count
						{
							newfocus--;

							if (newfocus < wheel->minci)
								newfocus = wheel->maxci;
						}
						if (focus_change(widget, newfocus, __LINE__))
							cycle_arrange(widget, true);
					}
					else if (wheel->inc < 0)
					{
						newfocus = wheel->focus_c;

						for (i = 0; i < (1 + 0); i++) // +wheel->fix_count
						{
							newfocus++;

							if (newfocus > wheel->maxci)
								newfocus = wheel->minci;
						}
						if (focus_change(widget, newfocus, __LINE__))
							cycle_arrange(widget, false);
					}
				}
			} 
		}
    }
    result |= widget->dirty;
    return widget->visible ? result : false;
}

void ituWheelDraw(ITUWidget* widget, ITUSurface* dest, int x, int y, uint8_t alpha)
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
}

static void WheelOnValueChanged(ITUWheel* wheel, char* value)
{
    // DO NOTHING
}

void ituWheelInit(ITUWheel* wheel)
{
    assert(wheel);

    memset(wheel, 0, sizeof (ITUWheel));

    ituFlowWindowInit(&wheel->fwin, ITU_LAYOUT_UP);

    ituWidgetSetType(wheel, ITU_WHEEL);
    ituWidgetSetName(wheel, wheelName);
    ituWidgetSetUpdate(wheel, ituWheelUpdate);
    ituWidgetSetDraw(wheel, ituWheelDraw);
    ituWidgetSetOnAction(wheel, ituWheelOnAction);
    ituWheelSetValueChanged(wheel, WheelOnValueChanged);
}

void ituWheelLoad(ITUWheel* wheel, uint32_t base)
{
	ITUWidget* widget = (ITUWidget*)wheel;

    assert(wheel);

    ituFlowWindowLoad(&wheel->fwin, base);

    ituWidgetSetUpdate(wheel, ituWheelUpdate);
    ituWidgetSetDraw(wheel, ituWheelDraw);
    ituWidgetSetOnAction(wheel, ituWheelOnAction);
    ituWheelSetValueChanged(wheel, WheelOnValueChanged);
}

void ituWheelOnAction(ITUWidget* widget, ITUActionType action, char* param)
{
    unsigned int oldFlags;
	ITUWheel* wheel = (ITUWheel*)widget;
    assert(widget);

    switch (action)
    {
    case ITU_ACTION_PREV:
        oldFlags = widget->flags;
        widget->flags |= ITU_TOUCHABLE;
		wheel->shift_one = 1;
        ituWidgetUpdate(widget, ITU_EVENT_TOUCHSLIDEUP, 0, widget->rect.x, widget->rect.y);
        if ((oldFlags & ITU_TOUCHABLE) == 0)
            widget->flags &= ~ITU_TOUCHABLE;
        break;

    case ITU_ACTION_NEXT:
        oldFlags = widget->flags;
        widget->flags |= ITU_TOUCHABLE;
		wheel->shift_one = 1;
        ituWidgetUpdate(widget, ITU_EVENT_TOUCHSLIDEDOWN, 0, widget->rect.x, widget->rect.y);
        if ((oldFlags & ITU_TOUCHABLE) == 0)
            widget->flags &= ~ITU_TOUCHABLE;
        break;

    case ITU_ACTION_GOTO:
        ituWheelGoto((ITUWheel*)widget, atoi(param));
        break;

    case ITU_ACTION_DODELAY0:
        ituExecActions(widget, ((ITUWheel*)widget)->actions, ITU_EVENT_DELAY0, atoi(param));
        break;

    case ITU_ACTION_DODELAY1:
        ituExecActions(widget, ((ITUWheel*)widget)->actions, ITU_EVENT_DELAY1, atoi(param));
        break;

    case ITU_ACTION_DODELAY2:
        ituExecActions(widget, ((ITUWheel*)widget)->actions, ITU_EVENT_DELAY2, atoi(param));
        break;

    case ITU_ACTION_DODELAY3:
        ituExecActions(widget, ((ITUWheel*)widget)->actions, ITU_EVENT_DELAY3, atoi(param));
        break;

    case ITU_ACTION_DODELAY4:
        ituExecActions(widget, ((ITUWheel*)widget)->actions, ITU_EVENT_DELAY4, atoi(param));
        break;

    case ITU_ACTION_DODELAY5:
        ituExecActions(widget, ((ITUWheel*)widget)->actions, ITU_EVENT_DELAY5, atoi(param));
        break;

    case ITU_ACTION_DODELAY6:
        ituExecActions(widget, ((ITUWheel*)widget)->actions, ITU_EVENT_DELAY6, atoi(param));
        break;

    //case ITU_ACTION_DODELAY7:
    //    ituExecActions(widget, ((ITUWheel*)widget)->actions, ITU_EVENT_DELAY7, atoi(param));
    //    break;

    default:
        ituWidgetOnActionImpl(widget, action, param);
        break;
    }
}

void ituWheelPrev(ITUWheel* wheel)
{
    assert(wheel);

    if (wheel->focusIndex > 0)
    {
        ITUText* text;
        char* value;
        wheel->focusIndex--;
        ituWidgetUpdate(wheel, ITU_EVENT_LAYOUT, 0, 0, 0);

        text = (ITUText*)itcTreeGetChildAt(wheel, wheel->itemCount / 2 + wheel->focusIndex);
        value = ituTextGetString(text);

        ituExecActions((ITUWidget*)wheel, wheel->actions, ITU_EVENT_CHANGED, wheel->focusIndex);
        //ituWheelOnValueChanged(wheel, value);
    }
}

void ituWheelNext(ITUWheel* wheel)
{
    int count = itcTreeGetChildCount(wheel) - wheel->itemCount;

    if (wheel->focusIndex < count)
    {
        ITUText* text;
        char* value;
        wheel->focusIndex++;
        ituWidgetUpdate(wheel, ITU_EVENT_LAYOUT, 0, 0, 0);

        text = (ITUText*)itcTreeGetChildAt(wheel, wheel->itemCount / 2 + wheel->focusIndex);
        value = ituTextGetString(text);

        ituExecActions((ITUWidget*)wheel, wheel->actions, ITU_EVENT_CHANGED, wheel->focusIndex);
        //ituWheelOnValueChanged(wheel, value);
    }
}

ITUWidget* ituWheelGetFocusItem(ITUWheel* wheel)
{
    assert(wheel);

    if (wheel->focusIndex >= 0)
    {
        ITUWidget* item = (ITUWidget*)itcTreeGetChildAt(wheel, wheel->itemCount / 2 + wheel->focusIndex);
        return item;
    }
    return NULL;
}

void ituWheelGoto(ITUWheel* wheel, int index)
{
	if (wheel->cycle_tor)
	{
		int i, shift, newfocus;
		ITUWidget* widget = (ITUWidget*)wheel;

		if ((wheel->itemCount == 0) || ((index + wheel->focus_dev) > wheel->maxci))
			return;

		if ((index == 0) && (wheel->focusIndex != 0))
		{
			bool check = true;
			bool way = true;

			way = ((wheel->maxci - wheel->focusIndex) > wheel->focusIndex) ? (true) : (false);

			while (check)
			{
				if (way)
				{
					newfocus = wheel->focus_c - 1;

					if (newfocus < wheel->minci)
					{
						check = false;
						newfocus = wheel->maxci;
					}

					if (focus_change(widget, newfocus, __LINE__))
						cycle_arrange(widget, true);
				}
				else
				{
					newfocus = wheel->focus_c + 1;

					if (newfocus > wheel->maxci)
					{
						check = false;
						newfocus = wheel->minci;
					}

					if (focus_change(widget, newfocus, __LINE__))
						cycle_arrange(widget, false);
				}
			}
		}
		else if (index < wheel->focusIndex)
		{
			shift = wheel->focusIndex - index;
			for (i = 0; i < shift; i++)
			{
				newfocus = wheel->focus_c - 1;
				if (newfocus < wheel->minci)
					newfocus = wheel->maxci;
				//cycle_arrange(widget, true);
				if (focus_change(widget, newfocus, __LINE__))
					cycle_arrange(widget, true);
			}
		}
		else if (index > wheel->focusIndex)
		{
			shift = index - wheel->focusIndex;
			for (i = 0; i < shift; i++)
			{
				newfocus = wheel->focus_c + 1;
				if (newfocus > wheel->maxci)
					newfocus = wheel->minci;
				//cycle_arrange(widget, false);
				if (focus_change(widget, newfocus, __LINE__))
					cycle_arrange(widget, false);
			}
		}
		else
		{
			cycle_arrange(widget, true);
			cycle_arrange(widget, false);
		}

		//widget->flags |= ITU_UNDRAGGING;
		//widget->flags &= ~ITU_DRAGGING;
		wheel->touchCount = 0;
		wheel->inc = 0;
		wheel->frame = wheel->totalframe;
		ituWidgetUpdate(wheel, ITU_EVENT_LAYOUT, 0, 0, 0);
	}
	else
	{
		int count = itcTreeGetChildCount(wheel) - wheel->itemCount;

		if (wheel->itemCount == 0 || index > count)
			return;

		wheel->focusIndex = index;
		ituWidgetUpdate(wheel, ITU_EVENT_LAYOUT, 0, 0, 0);
	}
}

void ituWheelScal(ITUWheel* wheel, int scal)
{
	if (scal >= 1)
		wheel->scal = scal;
}

bool ituWheelCheckIdle(ITUWheel* wheel)
{
	return (wheel->idle > 1) ? (true) : (false);
}
