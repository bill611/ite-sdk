/*
 * Copyright (c) 2011 ITE Technology Corp. All Rights Reserved.
 *
 * @description Used as System Time Clock in front panel control.
 * @file ${Program}/ith_stc.c
 * @author Irene Wang
 * @version 1.0.0
 */
#include "ite/ith.h"

static uint32_t stcBaseCountLo;
STCInfo stc_info[STC_MAX_CNT] = {0};

uint64_t ithStcGetBaseClock64(STCInfo *pstc_info)
{
    //printf("[%s] hi=%u lo = %u\n", __FUNCTION__, pstc_info->stcBaseCountHi, stcBaseCountLo);
    uint64_t tempBaseCount = ((uint64_t)pstc_info->stcBaseCountHi << 32) | stcBaseCountLo;
    tempBaseCount = (tempBaseCount * 9)/10;
    return tempBaseCount;
}

void ithStcUpdateBaseClock(void)
{
    int index;
	uint32_t baseCount = ithStcGetBaseClock();

    if (baseCount < stcBaseCountLo)
    {
        for (index = 0; index < STC_MAX_CNT; index++)
        {
            if (stc_info[index].state != STATE_FREE)
            {
                stc_info[index].stcBaseCountHi++;
            }
        }
    }
    stcBaseCountLo = baseCount;
	//ithPrintf("[%s] hi=%u, lo=%u\n", __FUNCTION__, stc_info[index].stcBaseCountHi, stcBaseCountLo);
}
