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

#define CBM7021_IIC_ADDR		(0x22)

#define CBM7021_ID              (0x02)
#define CBM7021_REG_KEYFIFO     (0xA8)

#define ENABLE_KEYPAD_INT


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

static	pthread_mutex_t     keypad_mutex = PTHREAD_MUTEX_INITIALIZER;

static	const uint8_t gTotalTouchKeyNum = (uint8_t)(CFG_TOUCH_KEY_NUM);

static	uint8_t		gTocuhDown = 0;
static	uint8_t		gLastIndex = 0xFF;
static	uint8_t		g_1stTch = 1;

static	uint8_t		g_kpI2cPort = IIC_PORT_0;

static const uint8_t single_buf[18]  = {
	1,4,3,2,2,
	1,1,4,1,3,
	0,2,2,1,1,
	1,2,48
};

/**************************************************************************
 ** private function                                                      **
 ***************************************************************************/
static unsigned int _cbm7021Init(void)
{
	uint8_t 	cmd;
	cmd = CBM7021_ID;

	int i;

	char buf[4];
	int result = mmpIicReceiveData(g_kpI2cPort, IIC_MASTER_MODE, CBM7021_IIC_ADDR, &cmd, 1, buf, 4);
	
	if (result) {
		if (	   buf[0] == 0x70 
				&& buf[1] == 0x26
				&& buf[2] == 0x04
				&& buf[3] == 0x30) {
			return mmpIicSendData(g_kpI2cPort,
					IIC_MASTER_MODE,
					CBM7021_IIC_ADDR,
					0x22,
					single_buf,
					18);
		} else {
			return 0;
		}
	}
	return result;
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

}

static uint32_t _getTouchKey(void)
{
	uint32_t	KeyValue;
	unsigned int result = 0;
	uint8_t		buf[4]={0};
	uint8_t 	cmd;
	int i;


	cmd = CBM7021_REG_KEYFIFO;

	result = mmpIicReceiveData(g_kpI2cPort, IIC_MASTER_MODE, CBM7021_IIC_ADDR, &cmd, 1, buf, 4);
	if(result <= 0) 
		goto get_touch_key_end;
	mmpIicStop(g_kpI2cPort);
	usleep(1000);

	if( result == I2C_WAIT_TRANSMIT_TIME_OUT ) {
		printf("[KEYPAD]warning: IIC hang up(3), reset keypad device, result=%x.\n",result);
		_resetDevice();
		return 0;
	}

	result = mmpIicReceiveData(g_kpI2cPort, IIC_MASTER_MODE, CBM7021_IIC_ADDR, &cmd, 1, buf, 4);
	if(result <= 0) 
		goto get_touch_key_end;

	mmpIicStop(g_kpI2cPort);
	usleep(1000);

	if( result==I2C_WAIT_TRANSMIT_TIME_OUT ) {
		printf("[KEYPAD]warning: IIC hang up(4), reset keypad device, result=%x.\n",result);
		_resetDevice();
	}

	printf("[KEYPAD][%x,%x,%x,%x]%s[%d]ERROR = %x \n",
			buf[0],buf[1] ,buf[2],buf[3], __FILE__, __LINE__, result);
	return 0;


get_touch_key_end:
	KeyValue = (uint32_t)buf[3] << 24 | (uint32_t)buf[2] << 16  | (uint32_t)buf[1] << 8 | (uint32_t)buf[0];
	KeyValue &= 0x000FFFFFF;

	return KeyValue;
}


static uint8_t _tanslateTouchValue(uint32_t value, uint8_t totalKeyNum)
{
	switch (value)
	{
		case 0x000080: return 0;
		case 0x000200: return 1;
		case 0x000400: return 2;
		case 0x008000: return 3;
		case 0x010000: return 4;
		default: return -1;
	}
}

/**************************************************************************
 ** public function(keypad API)                                           **
 ***************************************************************************/

int itpKeypadProbe(void)
{
	static int init_interval = 0;
	unsigned int i;
	uint32_t value=0;
	uint8_t ChkSum=0;
	uint8_t index;
	uint32_t	kpIntr = 0;

	pthread_mutex_lock(&keypad_mutex);

#ifdef	ENABLE_KEYPAD_INT
	kpIntr = ithGpioGet(TK_GPIO_PIN);
#endif

	if(g_1stTch) {
		if (++init_interval > 3527) { // 60s for 17ms interval
			init_interval = 0;
			int ret = _cbm7021Init();
			if(ret)
				printf("cbm7021 init fail\n");
			else {
				printf("cbm7021 init ok\n");
				g_1stTch = 0;
			}
		}
	}

	if(!kpIntr) {
		value = _getTouchKey();
		if(!value) {
			gTocuhDown = 0;
			gLastIndex = 0xFF;
		}
	} else {
		gTocuhDown = 0;
		gLastIndex = 0xFF;
		return -1;
	}

	pthread_mutex_unlock(&keypad_mutex);

	index = _tanslateTouchValue(value, gTotalTouchKeyNum);
	if(index==0xFF)
		return -1;

	if(!kpIntr) {
		if(gTocuhDown && gLastIndex == index)
			return -1;
		gTocuhDown = 1;
		gLastIndex = index;
	}
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

	if(g_1stTch) {
		int ret = _cbm7021Init();
		if(ret)
			printf("cbm7021 init fail\n");
		else {
			printf("cbm7021 init ok\n");
			g_1stTch = 0;
		}
	}


}

int itpKeypadGetMaxLevel(void)
{
	return gTotalTouchKeyNum;
}
