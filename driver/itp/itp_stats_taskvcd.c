/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 * Task Statistics VCD Dump.
 *
 * To view the VCD file, please download Wave VCD Viewer from
 * http://www.iss-us.com/wavevcd/index.htm.
 *
 * Insert following function call to start trace the task context
 * switch event.
 *
 *   itpTaskVcdOpen();
 *
 * And insert following code to the main task to dump the VCD file
 * when the trace complete.
 *
 *   itpTaskVcdWrite();
 *
 * Insert tag
 *
 *   itpTaskVcdSetTag(int);
 *
 * @author Kuoping Hsu
 * @version 1.0
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "openrtos/FreeRTOS.h"
#include "openrtos/task.h"
#include "itp_cfg.h"

#if defined(CFG_DBG_TRACE_ANALYZER) && defined(CFG_DBG_VCD)

// Timer defines
#define VCD_TIMER           (ITH_TIMER3)

// Enable the define to measure memory bandwidth usage.
#define MEMORY_BANDWIDTH_ANALYSIS

#define VCD_SIG_RANGE       (int)('~' - '!' + 1)
#define MAX_SIG_RANGE       (VCD_SIG_RANGE)*(VCD_SIG_RANGE+1)
#define MODE_MASK           (0xf0000000)
#define MODE_TASK_IN        (0x10000000)
#define MODE_TASK_OUT       (0x20000000)
#define MODE_TASK_DELAY     (0x30000000)
#define MODE_TASK_DELETE    (0x40000000)
#define MODE_MEM_USAGE      (0x50000000)
#define MODE_TAG            (0x60000000)
#define MAX_TASK_RECORD     (2048)      // maximun MAX_SIG_RANGE - 16

/***************************************************************************
 *                              Private Variable
 ***************************************************************************/
static          signed portCHAR pxTCBName[MAX_TASK_RECORD][configMAX_TASK_NAME_LEN+5];
static          unsigned int * g_trace_buf  = (unsigned int*)0;
static          unsigned int * g_buf_ptr    = 0;
static volatile unsigned int   g_buf_idx    = 0;
static          unsigned int   g_buf_size   = 0;
static volatile unsigned int   g_enable     = 0;
static volatile unsigned int   g_dump       = 0;
static          const char   * g_fname      = (char*)0;
static          unsigned int   g_start_time = (unsigned int)0;
static          unsigned int * g_last_ptr   = 0;
static volatile unsigned int   g_max_id     = 0;
static volatile unsigned int   g_max_tag    = 0;

/***************************************************************************
 *                              Private Functions
 ***************************************************************************/
static unsigned int
getClock(
    void)
{
    return ithTimerGetCounter(VCD_TIMER);
}

static unsigned int
getDuration(
    unsigned int clock)
{
    uint32_t diff, time = ithTimerGetCounter(VCD_TIMER);

    if (time >= clock)
    {
        diff = time - clock;
    }
    else
    {
        diff = (0xFFFFFFFF - clock) + 1 + time;
    }

    diff = (uint64_t)diff * 1000000 / ithGetBusClock();
    return diff;
}

static char *
dec2bin(int n)
{
    static char bin[33];
    int i;
    for(i=0; i<32; i++)
    {
        bin[i] = (n&0x80000000) ? '1' : '0';
        n <<= 1;
    }
    bin[32] = 0;
    return bin;
}

static char *
sigsymbol(int n)
{
    int  a, b;
    static char str[4];

    a = n / VCD_SIG_RANGE;
    b = n % VCD_SIG_RANGE;

    if (a == 0) str[1] = 0;
    else        str[1] = '!' + a - 1;
    str[0] = '!' + b;
    str[2] = 0;

    return (char*)&str;
}

static
vcd_dump(void)
{
    #define BUFSIZE   (10 << 10)
    #define TIMESCALE 1
    int i, j;
    FILE* fp;
    char *buf = NULL, *outbuf = NULL;

    g_enable = 0;
    if (!g_fname)
    {
        printf("[RTOS][DUMP] No dump name is specified!!\n");
        goto end;
    }

    LOG_DBG "dump to %s\n", g_fname LOG_END
    if ((fp = fopen(g_fname, "wb")) == NULL)
    {
        printf("[RTOS][DUMP] Can not create dump file!!\n");
        goto end;
    }

    if (!g_buf_idx)
    {
        printf("[RTOS][DUMP] No data to dump!!\n");
        goto end;
    }

    //vTaskSuspendAll();
    printf("[RTOS][DUMP] Starting dump....\n");

    if (!(buf = malloc(BUFSIZE+80)))
    {
        printf("[RTOS][DUMP] Out of memory!!\n");
        goto end;
    }

    if ((g_max_id + 1) + (g_max_tag + 1 ) >= MAX_SIG_RANGE)
    {
        g_max_id = MAX_SIG_RANGE - g_max_tag - 2;
        printf("[RTOS][DUMP] Out of signal to dump!!\n");
    }

    g_buf_ptr = g_trace_buf;

    snprintf(buf, BUFSIZE+80, "$version\n");                          fwrite(buf, 1, strlen(buf), fp);
    snprintf(buf, BUFSIZE+80, "   OpenRTOS Context Switch dump.\n");  fwrite(buf, 1, strlen(buf), fp);
    snprintf(buf, BUFSIZE+80, "$end\n");                              fwrite(buf, 1, strlen(buf), fp);
    snprintf(buf, BUFSIZE+80, "$comment\n");                          fwrite(buf, 1, strlen(buf), fp);
    snprintf(buf, BUFSIZE+80, "   ITE Tech. Corp. Written by Kuoping Hsu, Dec. 2010\n"); fwrite(buf, 1, strlen(buf), fp);
    snprintf(buf, BUFSIZE+80, "$end\n");                              fwrite(buf, 1, strlen(buf), fp);
    snprintf(buf, BUFSIZE+80, "$timescale %dus $end\n", TIMESCALE);   fwrite(buf, 1, strlen(buf), fp);
    snprintf(buf, BUFSIZE+80, "$scope module task $end\n");           fwrite(buf, 1, strlen(buf), fp);

    for(i=1; i<=g_max_id; i++) // ignore task ID 0
    {
        snprintf(buf, BUFSIZE+80, "$var wire 1 %s %s $end\n", sigsymbol(i), pxTCBName[i]);
        fwrite(buf, 1, strlen(buf), fp);
    }

    for(j=0; j<=g_max_tag; j++)
    {
        snprintf(buf, BUFSIZE+80, "$var wire 32 %s tag%02d $end\n", sigsymbol(i+j), j);
        fwrite(buf, 1, strlen(buf), fp);
    }

    #if defined(MEMORY_BANDWIDTH_ANALYSIS)
    snprintf(buf, BUFSIZE+80, "$var wire 32 %s %s $end\n", sigsymbol(i+j), "mem_usage");
    fwrite(buf, 1, strlen(buf), fp);
    #endif // MEMORY_BANDWIDTH_ANALYSIS

    snprintf(buf, BUFSIZE+80, "$upscope $end\n");         fwrite(buf, 1, strlen(buf), fp);
    snprintf(buf, BUFSIZE+80, "$enddefinitions $end\n");  fwrite(buf, 1, strlen(buf), fp);
    snprintf(buf, BUFSIZE+80, "$dumpvars\n");             fwrite(buf, 1, strlen(buf), fp);

    for(i=1; i<=g_max_id; i++) // ignore task ID 0
    {
        snprintf(buf, BUFSIZE+80, "x%s\n", sigsymbol(i));
        fwrite(buf, 1, strlen(buf), fp);
    }

    for(j=0; j<=g_max_tag; j++)
    {
        snprintf(buf, BUFSIZE+80, "bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx %s\n", sigsymbol(i+j));
        fwrite(buf, 1, strlen(buf), fp);
    }

    #if defined(MEMORY_BANDWIDTH_ANALYSIS)
    snprintf(buf, BUFSIZE+80, "bxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx %s\n", sigsymbol(i+j));
    fwrite(buf, 1, strlen(buf), fp);
    #endif // MEMORY_BANDWIDTH_ANALYSIS

    snprintf(buf, BUFSIZE+80, "$end\n");
    fwrite(buf, 1, strlen(buf), fp);

    outbuf = buf;
    for(i=0, j=0; i<g_buf_idx*2; i+=2)
    {
        int task_id = g_buf_ptr[i] & ~MODE_MASK;

        if (task_id == 0) continue; // ignore task ID 0

        if (((g_buf_ptr[i] & MODE_MASK) != MODE_TASK_IN)     &&
            ((g_buf_ptr[i] & MODE_MASK) != MODE_TASK_OUT)    &&
            ((g_buf_ptr[i] & MODE_MASK) != MODE_TASK_DELAY)  &&
            ((g_buf_ptr[i] & MODE_MASK) != MODE_TASK_DELETE) &&
            ((g_buf_ptr[i] & MODE_MASK) != MODE_TAG)         &&
            ((g_buf_ptr[i] & MODE_MASK) != MODE_MEM_USAGE))
        {
            printf("[RTOS][DUMP] unknown task stat 0x%08x.\n", g_buf_ptr[i]);
            continue;
        }

        snprintf(outbuf, (BUFSIZE+80-j), "#%d\n", g_buf_ptr[i+1] / TIMESCALE);
        j += strlen(outbuf);
        outbuf = &buf[j];

        if ((g_buf_ptr[i] & MODE_MASK) == MODE_TASK_IN)
        {
            snprintf(outbuf, (BUFSIZE+80-j), "1%s\n", sigsymbol(task_id));
            j += strlen(outbuf);
        }
        else if ((g_buf_ptr[i] & MODE_MASK) == MODE_TASK_OUT)
        {
            snprintf(outbuf, (BUFSIZE+80-j), "0%s\n", sigsymbol(task_id));
            j += strlen(outbuf);
        }
        else if ((g_buf_ptr[i] & MODE_MASK) == MODE_TASK_DELAY)
        {
            snprintf(outbuf, (BUFSIZE+80-j), "x%s\n", sigsymbol(task_id));
            j += strlen(outbuf);
        }
        else if ((g_buf_ptr[i] & MODE_MASK) == MODE_TASK_DELETE)
        {
            snprintf(outbuf, (BUFSIZE+80-j), "x%s\n", sigsymbol(task_id));
            j += strlen(outbuf);
        }
        else if ((g_buf_ptr[i] & MODE_MASK) == MODE_TAG)
        {
            int tag_id = (g_buf_ptr[i] >> 24) & 0xf;
            int value  = g_buf_ptr[i] & ~(MODE_MASK | 0x0f000000);
            snprintf(outbuf, (BUFSIZE+80-j), "b%s %s\n", dec2bin(value), sigsymbol((g_max_id+1)+tag_id));
            j += strlen(outbuf);
        }
        #if defined(MEMORY_BANDWIDTH_ANALYSIS)
        else if ((g_buf_ptr[i] & MODE_MASK) == MODE_MEM_USAGE)
        {
            snprintf(outbuf, (BUFSIZE+80-j), "b%s %s\n", dec2bin(g_buf_ptr[i] & ~MODE_MASK), sigsymbol((g_max_id+1)+(g_max_tag+1)));
            j += strlen(outbuf);
        }
        #endif // MEMORY_BANDWIDTH_ANALYSIS

        if (j >= BUFSIZE)
        {
            fwrite(buf, 1, j, fp);
            outbuf = buf;
            j = 0;
            printf("."); fflush(stdout);
        }
        else
        {
            outbuf = &buf[j];
        }
    }

    if (j != 0)
    {
        fwrite(buf, 1, j, fp);
        printf("."); fflush(stdout);
        printf("\n");
    }

    //xTaskResumeAll();
    printf("[RTOS][DUMP] Dump complete %d event....\n", g_buf_idx);

    /* stop trace after dump the trace */
end:
    if (fp)
    {
        fclose(fp);
    }

    if (buf)
    {
        free(buf);
    }

    itpTaskVcdClose();
}

static void
mem_trace(void)
{
#if defined(MEMORY_BANDWIDTH_ANALYSIS)
    if (g_buf_idx >= g_buf_size)
    {
        g_dump   = 1;
        g_enable = 0;
    }
    else
    {
        uint32_t servCount, cycle;

        ithMemStatServCounterDisable();

        servCount = ithMemStatGetAllServCount();
        cycle = ithMemStatGetServCount();

        //*g_last_ptr  = (int)((servCount * 8) / (cycle / (float)ithGetMemClock()) / 1000000) | MODE_MEM_USAGE;
        *g_last_ptr  = (int)((servCount * 100) / cycle) | MODE_MEM_USAGE;
        g_last_ptr   = g_buf_ptr;
        *g_buf_ptr++ = MODE_MEM_USAGE;
        *g_buf_ptr++ = getDuration(g_start_time);
        g_buf_idx++;

        ithMemStatServCounterEnable();
    }
#endif // MEMORY_BANDWIDTH_ANALYSIS
}

static int add_tasklist(int task_id)
{
    char* pch;
    int i;

    if (task_id == 0)
        task_id = ++g_max_id;

    snprintf(pxTCBName[task_id], configMAX_TASK_NAME_LEN+5, "%04d_%s", task_id, pcTaskGetTaskName(xTaskGetCurrentTaskHandle()));
    pch = pxTCBName[task_id];

    for(i=0; pch[i] != 0 && i < configMAX_TASK_NAME_LEN+5; i++)
    {
        if (pch[i] == ' ')
            pch[i] = '_';
    }

    if (g_max_id >= MAX_TASK_RECORD)
    {
        g_dump   = 1;
        g_enable = 0;
        g_max_id = MAX_TASK_RECORD-1;
        printf("[RTOS][DUMP] Out of task to record\n");
    }

    return task_id;
}

/***************************************************************************
 *                              Public Functions
 ***************************************************************************/
void portTASK_SWITCHED_IN(void)
{
    if (g_trace_buf && g_enable)
    {
        if (g_buf_idx >= g_buf_size)
        {
            g_dump   = 1;
            g_enable = 0;
        }
        else
        {
            int task_id = uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle());
            if (task_id == 0)
                task_id = add_tasklist(task_id);

            *g_buf_ptr++ = task_id | MODE_TASK_IN;
            *g_buf_ptr++ = getDuration(g_start_time);
            g_buf_idx++;
        }

        mem_trace();
    }
}

void portTASK_SWITCHED_OUT(void)
{
    if (g_trace_buf && g_enable)
    {
        if (g_buf_idx >= g_buf_size)
        {
            g_dump   = 1;
            g_enable = 0;
        }
        else
        {
            int task_id = uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle());
            if (task_id == 0)
                task_id = add_tasklist(task_id);

            *g_buf_ptr++ = task_id | MODE_TASK_OUT;
            *g_buf_ptr++ = getDuration(g_start_time);
            g_buf_idx++;
        }
    }
}

void portTASK_DELAY(void)
{
    if (g_trace_buf && g_enable)
    {
        if (g_buf_idx >= g_buf_size)
        {
            g_dump   = 1;
            g_enable = 0;
        }
        else
        {
            int task_id = uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle());
            if (task_id == 0)
                task_id = add_tasklist(task_id);

            *g_buf_ptr++ = task_id | MODE_TASK_DELAY;
            *g_buf_ptr++ = getDuration(g_start_time);
            g_buf_idx++;
        }
    }
}

void portTASK_CREATE(TaskHandle_t xTask)
{
    unsigned portBASE_TYPE task_id = ++g_max_id;
    char* pch;
    int i;

    vTaskSetTaskNumber(xTask, task_id);

    snprintf(pxTCBName[task_id], configMAX_TASK_NAME_LEN+5, "%04d_%s", task_id, pcTaskGetTaskName(xTask));
    pch = pxTCBName[task_id];

    for(i=0; pch[i] != 0 && i < configMAX_TASK_NAME_LEN+5; i++)
    {
        if (pch[i] == ' ')
            pch[i] = '_';
    }

    if (g_max_id >= MAX_TASK_RECORD)
    {
        g_dump   = 1;
        g_enable = 0;
        g_max_id = MAX_TASK_RECORD-1;
        printf("[RTOS][DUMP] Out of task to record\n");
    }
}

void portTASK_DELETE(TaskHandle_t xTask)
{
    if (g_trace_buf && g_enable)
    {
        if (g_buf_idx >= g_buf_size)
        {
            g_dump   = 1;
            g_enable = 0;
        }
        else
        {
            int task_id = uxTaskGetTaskNumber(xTask);
            *g_buf_ptr++ = task_id | MODE_TASK_DELETE;
            *g_buf_ptr++ = getDuration(g_start_time);
            g_buf_idx++;
        }
    }
}

void itpTaskVcdInit(void)
{
    ithTimerCtrlEnable(VCD_TIMER, ITH_TIMER_UPCOUNT);
    ithTimerSetCounter(VCD_TIMER, 0);
    ithTimerEnable(VCD_TIMER);
}

void itpTaskVcdOpen(const char* filename, int count)
{
    int i;
    g_buf_size   = count;
    g_buf_idx    = 0;
    g_start_time = getClock();
    g_fname      = filename;

    // Allocate Trace Buffer
    if (!g_trace_buf)
    {
        g_trace_buf = (unsigned int*)malloc(g_buf_size*sizeof(int)*2);
    }

    if (g_trace_buf == (unsigned int*)0)
    {
        printf("[RTOS][DUMP] Can not creat trace buffer\n");
    }
    g_buf_ptr = g_trace_buf;
    g_enable  = 1;
    g_dump    = 0;
    //ithPrintf("g_trace_buf=%d g_enable=%d\n", g_trace_buf, g_enable);

    snprintf(pxTCBName[0], configMAX_TASK_NAME_LEN+5, "%04d_untrack", 0);

#ifdef MEMORY_BANDWIDTH_ANALYSIS

    g_last_ptr   = g_buf_ptr;
    *g_buf_ptr++ = MODE_MEM_USAGE;
    *g_buf_ptr++ = getDuration(g_start_time);
    g_buf_idx++;

    ithMemStatSetServCountPeriod(0xFFFF);

#ifdef __arm__
    ithMemStatSetServ0Request(ITH_MEMSTAT_ARM);
#elif defined(__sm32__)
    ithMemStatSetServ0Request(ITH_MEMSTAT_RISC);
#else
#error "Unknown CPU type!"
#endif // __arm__

    ithMemStatServCounterEnable();

#endif // MEMORY_BANDWIDTH_ANALYSIS
}

int itpTaskVcdGetEventIndex(void)
{
    return g_buf_idx;
}

void itpTaskVcdSetTag(int id, int tag)
{
    ithEnterCritical();

    if (id > 15)
    {
        printf("[RTOS][DUMP] Out of tag id number.\n");
        goto end;
    }

    if (tag & 0xff000000)
    {
        printf("[RTOS][DUMP] tag value can not exceed 24-bits.\n");
    }

    if(id > g_max_tag)
        g_max_tag = id;

    if (g_trace_buf && g_enable)
    {
        if (g_buf_idx >= g_buf_size)
        {
            g_dump   = 1;
            g_enable = 0;
        }
        else
        {
            *g_buf_ptr++ = (tag & ~(MODE_MASK | 0x0f000000)) | MODE_TAG | (id << 24);
            *g_buf_ptr++ = getDuration(g_start_time);
            g_buf_idx++;
        }
    }

end:
    ithExitCritical();
}

void itpTaskVcdWrite(void)
{
    vcd_dump();
}

void itpTaskVcdClose(void)
{
    free((void*)g_trace_buf);
    g_trace_buf = (unsigned int*)0;
    g_buf_idx   = 0;
    g_enable    = 0;
    g_dump      = 0;
    g_max_tag   = 0;
    g_buf_ptr   = g_trace_buf;
}

#endif // CFG_DBG_TRACE_ANALYZER && CFG_DBG_VCD

