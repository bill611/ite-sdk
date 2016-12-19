/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 * Castor3 keypad module.
 *
 * @author Joseph Chang
 * @version 1.0
 */
#include "../itp_cfg.h"
#include "iic/mmp_iic.h"
#include <errno.h>
#include <pthread.h>

#include "openrtos/FreeRTOS.h"		//must "include FreeRTOS.h" before "include queue.h"
#include "openrtos/queue.h"			//for using xQueue function

/**************************************************************************
** MACRO defination                                                      **
***************************************************************************/
#ifndef	CFG_TOUCH_KEY_NUM
#define CFG_TOUCH_KEY_NUM	(5)
#endif

#if (CFG_TOUCH_KEY_NUM==5)
#define CBM7110_IIC_ADDR		(0x22)
#else
#define CBM7110_IIC_ADDR		(0x8C>>1)
#endif
#define CBM7110_ID              (0x01)
#define CBM7110_REG_KEYFIFO     (0xA8)

//#define ENABLE_KEYPAD_DBG_MODE
#define ENABLE_KEYPAD_INT
//#define ENABLE_SHARE_GPIOEXTENDER_INTR
//#define ENABLE_CUTOFF_ALL_INTR
// #define ENABLE_CLEAR_TP_INT
//#define ENABLE_SKIP_I2C_SLAVE_NACK


#ifdef	ENABLE_KEYPAD_INT
#define TK_GPIO_PIN	    CFG_GPIO_KEYPAD

#if (TK_GPIO_PIN<32)
#define TK_GPIO_MASK    (1<<TK_GPIO_PIN)
#else
#define TK_GPIO_MASK    (1<<(TK_GPIO_PIN-32))
#endif

#endif

#define QUEUE_LEN 256
/**************************************************************************
** global variable                                                      **
***************************************************************************/
//IT7235BR touch 16-key pad mapping table
//  x   80   40   02   01
//y	 ----------------------
//04 | [1]  [2]  [3]  [A] |
//08 | [4]  [5]  [6]  [B] |
//10 | [7]  [8]  [9]  [C] |
//20 | [*]  [0]  [#] [Cen]|
//	 ----------------------
//kay:: 	[0]  [1]  [2]  [3]  [4]  [5]  [6]  [7]  [8]  [9]  [A]  [B]  [C]  [Cen] [*]  [#]
//code::   	0x60,0x84,0x44,0x02,0x88,0x48,0x0a,0x90,0x50,0x12,0x05,0x09,0x11,0x21,0xa0,0x22
//SDL code: 13  ,0,  ,1   ,2   ,4   ,5   ,6   ,8   ,9   ,10  ,3   ,7   ,11  ,15   ,12, ,14
//x:0x80,0x40,0x02,0x01
//y:0x04,0x08,0x10,0x20

typedef struct cbm7110_fw_tag { /* Used in the IBM Arctic II */
	uint8_t fwCR;
	uint8_t fwStatus;
	uint8_t fwDevName[6];
	uint8_t fwVer[4];
	uint8_t fwOtpVer[2];
	uint16_t fwSramSize;
	uint16_t fwMtpSize;
	uint16_t fwCodeSize;
	uint8_t fwDummy[3];
}FW_Info;

static	pthread_mutex_t     keypad_mutex = PTHREAD_MUTEX_INITIALIZER;

static	const uint8_t gTotalTouchKeyNum = (uint8_t)(CFG_TOUCH_KEY_NUM);

static	uint8_t		gRegPage=0xFE;
static	uint8_t		gLastIicStatus=0;

#ifdef	ENABLE_SHARE_GPIOEXTENDER_INTR
static QueueHandle_t tkQueue;
static uint8_t		gLastKey = 0;
#endif

static	uint8_t		gTocuhDown = 0;
static	uint8_t		gLastIndex = 0xFF;
static	uint8_t		g_1stTch = 1;
static	FW_Info		gFwInfo;
static	const uint8_t		gFwVerCycle = 23;

static	uint8_t		g_kpI2cPort = IIC_PORT_0;

static	uint8_t		gKpInitFinished = false;
static const uint8_t single_buf[10]  = {0,1,0,1,0,1,0,0,1,1};

/**************************************************************************
** private function                                                      **
***************************************************************************/
static unsigned int cbm7110Init(void)
{
	return mmpIicSendData(g_kpI2cPort,
		   	IIC_MASTER_MODE,
		   	CBM7110_IIC_ADDR,
		   	0x22,
		   	single_buf,
		   	10);
	// msleep(20);
}
static uint8_t _checkSum(uint16_t sum)
{
	uint8_t	i;
	uint8_t	cnt=0;

	for(i=0; i<gTotalTouchKeyNum; i++)
	{
		if( (sum>>i)&0x01 )	cnt++;
	}
	return cnt;
}

#ifdef	ENABLE_KEYPAD_INT
static void _initTkGpioPin(void)
{
	ithGpioSetMode(TK_GPIO_PIN, ITH_GPIO_MODE1);
	ithGpioSetIn(TK_GPIO_PIN);
	ithGpioCtrlEnable(TK_GPIO_PIN, ITH_GPIO_PULL_ENABLE);
	ithGpioCtrlEnable(TK_GPIO_PIN, ITH_GPIO_PULL_UP);
	ithGpioEnable(TK_GPIO_PIN);
}
#endif

#ifdef	ENABLE_SKIP_I2C_SLAVE_NACK
static uint8_t _checkKeyPadExist(void)
{
	unsigned int result = 1;
	uint32_t	 reg32, cnt=0;
	uint8_t      pg=0;

	while(result)
	{
		result = mmpIicSendData(g_kpI2cPort, IIC_MASTER_MODE, CBM7110_IIC_ADDR, 0xF0, &pg, 1);
		if(result)
	    {
	    	mmpIicStop(g_kpI2cPort);
	    	usleep(1000);

	    	if( result==I2C_WAIT_TRANSMIT_TIME_OUT )
	    	{
	    		printf("[KEYPAD]warning: IIC hang up(1), reset keypad device, result=%x.\n",result);
	    		_resetDevice();
	    	}
	    	usleep(2000);
        }
        if(errCnt++>10)	return false;
    }

    gRegPage = 0;

    return true;
}
#endif

static void _resetDevice(void)
{
	//pull down iic_SCL for 4ms
#if (CFG_CHIP_FAMILY == 9850)
	#if (defined CFG_IIC0_GPIO_CONFIG_1)
	int sclPin = 23;
	int sdaPin = 24;
	#elif (defined CFG_IIC0_GPIO_CONFIG_2)
	int sclPin = 86;
	int sdaPin = 87;
	#endif
#elif (CFG_CHIP_FAMILY == 9070)
	#if (defined CFG_IIC0_GPIO_CONFIG_1)
	int sclPin = 2;
	int sdaPin = 3;
	#elif (defined CFG_IIC0_GPIO_CONFIG_2)
	int sclPin = 11;
	int sdaPin = 12;
	#endif
#endif

	#ifdef	ENABLE_KEYPAD_DBG_MODE
	printf("[Keypad error] CBM7110 hangup, and reset keypad device.\n");
	#endif

	//set GPIO3 as mode0
	ithGpioSetMode(sclPin, ITH_GPIO_MODE0);
	ithGpioSetMode(sdaPin, ITH_GPIO_MODE0);

	//set output mode
	ithGpioSetOut(sclPin);
	ithGpioSetOut(sdaPin);

	//set GPIO output 0
	ithGpioClear(sclPin);
	ithGpioClear(sdaPin);

	//for 4ms
	usleep(8000);
	ithGpioSet(sclPin);
	ithGpioSet(sdaPin);
	usleep(100);

	//set GPIO3 as mode1(IIC mode)
	ithGpioSetMode(sclPin, ITH_GPIO_MODE3);
	ithGpioSetMode(sdaPin, ITH_GPIO_MODE3);
	usleep(100);

	#ifdef	ENABLE_KEYPAD_DBG_MODE
	printf(" end of keypad reset flow\n");
	#endif
}

static void _checkPage(uint8_t page)
{
	unsigned int result = 0;
	uint8_t		buf[2];
	uint32_t	reg32, cnt=0;

	if(gRegPage!=page)
	{
		result = mmpIicSendData(g_kpI2cPort, IIC_MASTER_MODE, CBM7110_IIC_ADDR, 0xF0, &page, 1);
		if(result)
	    {
	    	mmpIicStop(g_kpI2cPort);
	    	usleep(1000);

	    	if( result==I2C_WAIT_TRANSMIT_TIME_OUT )
	    	{
	    		printf("[KEYPAD]warning: IIC hang up(1), reset keypad device, result=%x.\n",result);
	    		_resetDevice();
	    	}

			#ifdef	ENABLE_KEYPAD_DBG_MODE
			printf("[KEYPAD]check iic status(busy/ready).01\n");
	   		while(1)
			{
				reg32 = ithReadRegA((ITH_IIC_BASE + 0x04));
				if( !(reg32&0x0C) )	break;
				if( (cnt&0xFFFF)==0xFFFF )	printf(" iic busy.01,cnt=%X,reg=%x\n",cnt,reg32);
				if(++cnt>0xFFFFF){printf("STOP!!\n");	break;}
			}
		    printf("[KEYPAD]warning:: _checkPage() try again!!\n");
			#endif

		    result = mmpIicSendData(g_kpI2cPort, IIC_MASTER_MODE, CBM7110_IIC_ADDR, 0xF0, &page, 1);
		    if(result)
		    {
	    		mmpIicStop(g_kpI2cPort);
	    		usleep(1000);

	    		if( result==I2C_WAIT_TRANSMIT_TIME_OUT )
	    		{
	    			printf("[KEYPAD]warning: IIC hang up(2), reset keypad device, result=%x.\n",result);
	    			_resetDevice();
	    		}

		       	printf("%s[%d]ERROR = %x \n", __FILE__, __LINE__, result);

		       	#ifdef	ENABLE_KEYPAD_DBG_MODE
		       	printf("[KEYPAD]check iic status(busy/ready).02\n");
		   		while(1)
				{
					reg32 = ithReadRegA((ITH_IIC_BASE + 0x04));
					if( !(reg32&0x0C) )	break;
					if( (cnt&0xFFFF)==0xFFFF )	printf(" iic busy.02,cnt=%X,reg=%x\n",cnt,reg32);
					if(++cnt>0xFFFFF){printf("STOP!!\n");	break;}
				}
				printf("[KEYPAD]warning::for checking IIC busy status.\n");
				#endif

		    	return;
		    }
        }

		#ifdef	ENABLE_KEYPAD_DBG_MODE
		printf("write page=0x%X to regAddr 0x%X\n", page, 0xF0);
		#endif

		gRegPage = page;
	}
}

static uint16_t _getTouchKey(void)
{
	uint8_t		page = 0x60;
	uint16_t	KeyValue;
	unsigned int result = 0;
	uint8_t		buf[2]={0};
	uint8_t 	cmd;
    uint32_t	regData1,regData2;
    uint32_t	reg32, cnt=0;

	// _checkPage(page);

	cmd = CBM7110_REG_KEYFIFO;

	result = mmpIicReceiveData(g_kpI2cPort, IIC_MASTER_MODE, CBM7110_IIC_ADDR, &cmd, 1, buf, 2);
    if(result)
    {
    	mmpIicStop(g_kpI2cPort);
    	usleep(1000);

    	if( result==I2C_WAIT_TRANSMIT_TIME_OUT )
    	{
    		printf("[KEYPAD]warning: IIC hang up(3), reset keypad device, result=%x.\n",result);
    		_resetDevice();
    		return 0;
    	}

		#ifdef	ENABLE_KEYPAD_DBG_MODE
    	printf("[KEYPAD]warning:: retry _getTouchKey(), result=%x,buf=[%x,%x]\n",result,buf[0],buf[1]);
    	printf("[KEYPAD]check iic status(busy/ready).03\n");
   		while(1)
		{
			reg32 = ithReadRegA((ITH_IIC_BASE + 0x04));
			if( !(reg32&0x0C) )	break;
			if( (cnt&0xFFFF)==0xFFFF )	printf(" iic busy.03,cnt=%X,reg=%x\n",cnt,reg32);
			if(++cnt>0xFFFFF){printf("STOP!!\n");	break;}
		}
		printf("[KEYPAD]warning::.03\n");
		#endif

        // buf[0] = 0xFE;
		// buf[1] = 0xFE;
		// buf[0] = CBM7110_REG_KEYFIFO;

    	result = mmpIicReceiveData(g_kpI2cPort, IIC_MASTER_MODE, CBM7110_IIC_ADDR, buf, 2, &cmd, 1);
    	if(result)
    	{
        	mmpIicStop(g_kpI2cPort);
        	usleep(1000);

	    	if( result==I2C_WAIT_TRANSMIT_TIME_OUT )
	    	{
	    		printf("[KEYPAD]warning: IIC hang up(4), reset keypad device, result=%x.\n",result);
	    		_resetDevice();
	    	}

	    	printf("[KEYPAD][%x,%x]%s[%d]ERROR = %x \n",buf[0],buf[1] , __FILE__, __LINE__, result);
	    	gRegPage = 0xFE;

	    	#ifdef	ENABLE_KEYPAD_DBG_MODE
       		printf("[KEYPAD]check iic status(busy/ready).04\n");
       		while(1)
			{
				reg32 = ithReadRegA((ITH_IIC_BASE + 0x04));
				if( !(reg32&0x0C) )	break;
				if( (cnt&0xFFF)==0xFFF )	printf(" iic busy.04,cnt=%X,reg=%x\n",cnt,reg32);
				if(++cnt>0xFFFF){printf("STOP!!\n");	break;}
			}
			printf("_getTouchKey.4\n");
			#endif

			return 0;
       	}
    }

    #ifdef	ENABLE_CLEAR_TP_INT
    {
    	uint8_t		cbuf[2];

    	cbuf[0]=0x00;

    	result = mmpIicSendData(g_kpI2cPort, IIC_MASTER_MODE, CBM7110_IIC_ADDR, 0xF3, cbuf, 1);

    }
    #endif

    #ifdef	ENABLE_KEYPAD_DBG_MODE
    //if( buf[0] || buf[1])
		//printf("buf=[0x%X,0x%X], R=%x\n", buf[0], buf[1], result);
	#endif

		// printf("buf=[0x%X,0x%X], R=%x\n", buf[0], buf[1], result);
	KeyValue = (uint16_t)buf[0] & 0x00FF;

	return KeyValue;
}

static void _getFirmwareVersion(FW_Info* fwInfo)
{
	uint8_t		page = 0x00;
	uint16_t	KeyValue;
	unsigned int result = 0;
	uint8_t		buf[24];
	uint8_t		cbuf[4];
	uint8_t     regAddr;
    uint32_t	regData1,regData2;
    uint32_t	reg32, cnt=0;
    uint32_t	FwVer=0;

    // printf("get FW version:fwInfo=%x, size=%d:\n",fwInfo, gFwVerCycle);

	// _checkPage(page);

	result = cbm7110Init();
	if(result)
		printf("Err1\n");
	usleep(1000);
}

static uint8_t _tanslateTouchValue(uint16_t value, uint8_t totalKeyNum)
{
	// uint8_t ChkSum = _checkSum(value);
	uint8_t	i=0;
	uint16_t	flag;

	#ifdef	ENABLE_KEYPAD_DBG_MODE
	printf("parseTchKey(%x,%x), chkSum=%x\n",value,totalKeyNum,ChkSum);
	#endif
	// printf("parseTchKey(%x,%x)\n",value,totalKeyNum);

	// if(ChkSum==0)	return -1;

	switch(totalKeyNum)
	{
	case 5:
		// if(ChkSum>1)	return -1;
		switch (value)
		{
			case 0x04: return 0;
			case 0x02: return 1;
			case 0x01: return 2;
			case 0x40: return 3;
			case 0x20: return 4;
			default : return -1;
		}
		// while(1)
		// {
			// flag = 0x01<<i;
			// if(value & flag)	return i;//index_table[i];
			// if(i++>totalKeyNum)	return -1;
		// }
	case 16:
		{
			uint8_t	BtnX,BtnY;

			// if(ChkSum==1)	return -1;

			// if(ChkSum>2)
			// {
				// printf("[KEYPAD]warning:: multi-key, skip it(value=%x, chk=%d)\n",value, ChkSum);
				// return -1;
			// }
		    switch(value&0xC3)
		    {
		    	case 0x80:	BtnX = 0;	break;
		    	case 0x40:	BtnX = 1;	break;
		    	case 0x02:	BtnX = 2;	break;
		    	case 0x01:  BtnX = 3;	break;
		    	default:
		    		printf("[KEYPAD]warning:: incorrect X, skip it(value=%x, chk=%d)\n",value, value&0xC3);
		    		return -1;
		    }

		    switch(value&0x3C)
		    {
		    	case 0x04:	BtnY = 0;	break;
		    	case 0x08:	BtnY = 1;	break;
		    	case 0x10:	BtnY = 2;	break;
		    	case 0x20:  BtnY = 3;	break;
		    	default:
		    		printf("[KEYPAD]warning:: incorrect Y, skip it(value=%x, chk=%d)\n",value, value&0x3C);
		    		return -1;
		    }
		    return (BtnY*4 + BtnX);
	    }
	default:
		printf("touch keypad error: totalKeyNum incorrect, keyNum=%d\n",totalKeyNum);
		break;
	}
	return (uint8_t)-1;
}











/**************************************************************************
** public function(keypad API)                                           **
***************************************************************************/
#ifdef	ENABLE_SHARE_GPIOEXTENDER_INTR
void tkGpioExtHandler()
{
	uint8_t	tempKey=0;
	uint8_t	value;

	//1.i2c read key status
	#ifdef	ENABLE_CUTOFF_ALL_INTR
	portSAVEDISABLE_INTERRUPTS();
	#else
    pthread_mutex_lock(&keypad_mutex);
    #endif

    if(g_1stTch)
    {
    	_getFirmwareVersion(&gFwInfo);
    	printf("The %d-key FW version of CBM7110:(%02x.%02x.%02x.%02x)\n", gTotalTouchKeyNum, gFwInfo.fwVer[0], gFwInfo.fwVer[1], gFwInfo.fwVer[2], gFwInfo.fwVer[3] );
    	g_1stTch = 0;
    }

    value = _getTouchKey();

    #ifdef	ENABLE_CUTOFF_ALL_INTR
    portRESTORE_INTERRUPTS();
    #else
    pthread_mutex_unlock(&keypad_mutex);
    #endif

	//send xqueue
    tempKey = value&0xFF;
    xQueueSend(tkQueue, &tempKey, 0);

}
#endif

int itpKeypadProbe(void)
{
    unsigned int i;
    uint16_t value=0;
    uint8_t ChkSum=0;
    uint8_t index;
    uint32_t	kpIntr = 0;

    //printf("itpKeyPadProbe\n");
    #ifdef	ENABLE_SHARE_GPIOEXTENDER_INTR
    if( !xQueueReceive(tkQueue, &ChkSum, 0) )
    {
    	if(gLastKey)
    	{
    		#ifdef	ENABLE_KEYPAD_DBG_MODE
    		printf("	___key-down,lastKey=%x\n",gLastKey);
    		#endif
    		ChkSum = gLastKey;
    	}
    	else
    	{
    		#ifdef	ENABLE_KEYPAD_DBG_MODE
    		if(gLastKey)	printf("	___key-up,lastKey=%x\n",gLastKey);
    		#endif
    		gLastKey = 0;
    	}
    }
    else
    {
        if(ChkSum)
        {
        	#ifdef	ENABLE_KEYPAD_DBG_MODE
        	printf("	___key-down,key=%x,lastKey=%x\n",ChkSum,gLastKey);
        	#endif
        	gLastKey = ChkSum;
        }
    	else
    	{
    		#ifdef	ENABLE_KEYPAD_DBG_MODE
    		printf("	___key-up,key=%x,lastKey=%x\n",ChkSum,gLastKey);
    		#endif
    		gLastKey = 0;
    	}
    }
    value = (uint16_t)ChkSum;
    #else

	#ifdef	ENABLE_CUTOFF_ALL_INTR
	portSAVEDISABLE_INTERRUPTS();
	#else
    pthread_mutex_lock(&keypad_mutex);
    #endif

    #ifdef	ENABLE_KEYPAD_INT
    kpIntr = ithGpioGet(TK_GPIO_PIN);
    #endif

    #ifdef ENABLE_SKIP_I2C_SLAVE_NACK
    if(gKpInitFinished!=true)
    {
        return (int)-1;
    }
    #endif

    if(g_1stTch)
    {
		int ret = cbm7110Init();
		if(ret)
			printf("cbm7110 init fail\n");
		else {
			printf("cbm7110 init ok\n");
			g_1stTch = 0;
		}

        // _getFirmwareVersion(&gFwInfo);
        // printf("The %d-key FW version of CBM7110:(%02x.%02x.%02x.%02x)\n",
				// gTotalTouchKeyNum, gFwInfo.fwVer[0], gFwInfo.fwVer[1], gFwInfo.fwVer[2], gFwInfo.fwVer[3] );
    }

	// static int b=0;
    if(!kpIntr)
    {
    	value = _getTouchKey();
		// printf("[%d]interrupt:%d\n",++,value);
    	if(!value)
    	{
    		gTocuhDown = 0;
    		gLastIndex = 0xFF;
    	}
    }
    #ifdef	ENABLE_CUTOFF_ALL_INTR
    portRESTORE_INTERRUPTS();
    #else
    pthread_mutex_unlock(&keypad_mutex);
    #endif

    #endif	//#ifdef	ENABLE_SHARE_GPIOEXTENDER_INTR


    index = _tanslateTouchValue(value, gTotalTouchKeyNum);
    if(index==0xFF)	return -1;

    #ifdef	ENABLE_KEYPAD_DBG_MODE
   	printf("[KEYPAD]key=%d, value = %x(x=%x,y=%x)\n",index, value,value&0xC3,value&0x3C);
   	#endif
   	if(!kpIntr)
   	{
           // if(gTocuhDown && gLastIndex==index)
           // {
               // #ifdef	ENABLE_KEYPAD_DBG_MODE
               // printf("[KEYPAD] key repead, ignore this event(%x,%x)(%x,%x)\n",kpIntr,gTocuhDown,index,gLastIndex);
               // #endif
               // return -1;
           // }
   		gTocuhDown = 1;
   		gLastIndex = index;
   	}
    #ifdef	ENABLE_KEYPAD_DBG_MODE
   	printf("[KEYPAD] send_key=%d, value = %x\n",index, value);
   	#endif
    return (int)index;
}

void itpKeypadInit(void)
{
	//TODO::
	//skip i2c init flow
	#ifdef	CFG_TOUCH_KEYPAD_I2C1
	g_kpI2cPort = IIC_PORT_1;
	#endif

	#ifdef	ENABLE_KEYPAD_INT
	_initTkGpioPin();
	#endif

	#ifdef	ENABLE_SKIP_I2C_SLAVE_NACK
	if(_checkKeyPadExist()==false)
	{
	    printf("[KeyPad] ERROR: KEYPAD CANNOT be recognized\n");
	    return;
	}
	#endif

	#ifdef	ENABLE_SHARE_GPIOEXTENDER_INTR
	tkQueue = xQueueCreate(QUEUE_LEN, (unsigned portBASE_TYPE) sizeof(unsigned char));
	#endif

	#ifdef	ENABLE_SHARE_GPIOEXTENDER_INTR
	itpRegisterGpioExtenderIntrHandler(1, tkGpioExtHandler);
	#endif

	#ifdef	ENABLE_SKIP_I2C_SLAVE_NACK
	gKpInitFinished = true;
	#endif
}

int itpKeypadGetMaxLevel(void)
{
    return gTotalTouchKeyNum;
}
