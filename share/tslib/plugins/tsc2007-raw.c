#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>
#include <pthread.h>
#include "ite/ith.h"   // for
#include "ite/itp.h"
#include "config.h"
#include "tslib-private.h"

#ifdef __OPENRTOS__
#include "openrtos/FreeRTOS.h"		//must "include FreeRTOS.h" before "include queue.h"
#include "openrtos/queue.h"			//for using xQueue function
#endif


//#define EN_DISABLE_ALL_INTR
#define	ENABLE_PATCH_Y_SHIFT_ISSUE
/****************************************************************************
 * ENABLE_TOUCH_POSITION_MSG :: just print X,Y coordination &
 * 								touch-down/touch-up
 * ENABLE_TOUCH_IIC_DBG_MSG  :: show the IIC command
 * ENABLE_TOUCH_PANEL_DBG_MSG:: show send-queue recieve-queue,
 *                              and the xy value of each INTr
 ****************************************************************************/
// #define ENABLE_TOUCH_POSITION_MSG
//#define ENABLE_TOUCH_PANEL_DBG_MSG
//#define ENABLE_TOUCH_IIC_DBG_MSG
//#define ENABLE_SEND_FAKE_SAMPLE


/****************************************************************************
 * 0.pre-work(build a test project, set storage for tslib config file)
 * 1.INT pin response(OK)
 * 2.IIC send read/write command, register address, and response
 * 3.get x,y coordiantion(palse data buffer)
 * 4.celibration(rotate, scale, reverse...)
 * 5.get 4 corner touch point(should be (0,0)(800,0)(0,600)(800,600) )
 * 6.add interrupt, thread, Xqueue, Timer..
 * 7.check interrupt response time(NO event loss)
 * 8.check sample rate (33ms)
 * 9.check touch down/up event
 * 10.draw line test
 * 11.check IIC 400kHz and IIC acess procedure(sleep 300~500us)
 ****************************************************************************/

// #define ENABLE_SWITCH_XY
#define ENABLE_SCALE_X
#define ENABLE_SCALE_Y
#define ENABLE_REVERSE_X
// #define ENABLE_REVERSE_Y
/***************************
 *
 **************************/
#define TSC2007_MEASURE_TEMP0       (0x0 << 4)
#define TSC2007_MEASURE_AUX     (0x2 << 4)
#define TSC2007_MEASURE_TEMP1       (0x4 << 4)
#define TSC2007_ACTIVATE_XN     (0x8 << 4)
#define TSC2007_ACTIVATE_YN     (0x9 << 4)
#define TSC2007_ACTIVATE_YP_XN      (0xa << 4)
#define TSC2007_SETUP           (0xb << 4)
#define TSC2007_MEASURE_X       (0xc << 4)
#define TSC2007_MEASURE_Y       (0xd << 4)
#define TSC2007_MEASURE_Z1      (0xe << 4)
#define TSC2007_MEASURE_Z2      (0xf << 4)

#define TSC2007_POWER_OFF_IRQ_EN    (0x0 << 2)
#define TSC2007_ADC_ON_IRQ_DIS0     (0x1 << 2)
#define TSC2007_ADC_OFF_IRQ_EN      (0x2 << 2)
#define TSC2007_ADC_ON_IRQ_DIS1     (0x3 << 2)

#define TSC2007_12BIT           (0x0 << 1)
#define TSC2007_8BIT            (0x1 << 1)

#define MAX_12BIT           ((1 << 12) - 1)

#define ADC_ON_12BIT    (TSC2007_12BIT | TSC2007_ADC_ON_IRQ_DIS0)

#define READ_Y      (ADC_ON_12BIT | TSC2007_MEASURE_Y)
#define READ_Z1     (ADC_ON_12BIT | TSC2007_MEASURE_Z1)
#define READ_Z2     (ADC_ON_12BIT | TSC2007_MEASURE_Z2)
#define READ_X      (ADC_ON_12BIT | TSC2007_MEASURE_X)
#define PWRDOWN     (TSC2007_12BIT | TSC2007_POWER_OFF_IRQ_EN)

struct ts_event {
	unsigned int x;
	unsigned int y;
	unsigned int z1, z2;
};

#define TP_GPIO_PIN	    CFG_GPIO_TOUCH_INT
#define TP_GPIO_MASK    (1<<(TP_GPIO_PIN%32))

#ifdef	CFG_GPIO_TOUCH_WAKE
#define TP_GPIO_WAKE_PIN	CFG_GPIO_TOUCH_WAKE
#endif

#define TOUCH_DEVICE_ID     (0x48)
#define QUEUE_LEN 			(256)
#define TOUCH_SAMPLE_RATE	(7)

/***************************
 *
 **************************/
struct zt2083_ts_event { /* Used in the IBM Arctic II */
	signed short pressure;
	signed int x;
	signed int y;
	int millisecs;
	int flags;
};

//#ifdef CFG_TOUCH_INTR
static char g_TouchDownIntr = false;
//#endif

static char g_IsTpInitialized = false;
static char g_IsTpFirstPoint = 1;

static int lastp=0;
static int g_MaxRawX = MAX_12BIT;
static int g_MinRawX = 0;
static int g_MaxRawY = MAX_12BIT;
static int g_MinRawY = 0;

#ifdef	ENABLE_SCALE_X
static int g_xResolution=1024;
#else
static int g_xResolution=CFG_TOUCH_X_MAX_VALUE;
#endif

#ifdef	ENABLE_SCALE_Y
static int g_yResolution=600;
#else
static int g_yResolution=CFG_TOUCH_Y_MAX_VALUE;
#endif

static unsigned int dur=0;
static unsigned char	gLastBuf[8]={0};
struct timeval startT, endT;
static int gInvalidZ1Z2 = 0;


static int g_tchDevFd=0;

#ifdef __OPENRTOS__
static QueueHandle_t tpQueue;
static pthread_mutex_t 	gTpMutex;
#endif

static struct ts_event tc;
static struct ts_event last_tc;
/*##################################################################################
 *                        the private function implementation
###################################################################################*/
void _tp_isr(void* data)
{
	unsigned int regValue;

	regValue = ithGpioGet(TP_GPIO_PIN);
	if ( (regValue & TP_GPIO_MASK) )
		g_TouchDownIntr = false;
	else
		g_TouchDownIntr = true;

	ithGpioClearIntr(TP_GPIO_PIN);

}

static void _initTouchIntr(void)
{
	ithGpioClearIntr(TP_GPIO_PIN);
	ithGpioRegisterIntrHandler(TP_GPIO_PIN, _tp_isr, NULL);
	ithGpioCtrlEnable(TP_GPIO_PIN, ITH_GPIO_INTR_BOTHEDGE);
	ithIntrEnableIrq(ITH_INTR_GPIO);
	ithGpioEnableIntr(TP_GPIO_PIN);
}

void _initTouchGpioPin(void)
{
	ithEnterCritical();
	_initTouchIntr();
	ithGpioSetMode(TP_GPIO_PIN, ITH_GPIO_MODE0);
	ithGpioSetIn(TP_GPIO_PIN);
	ithGpioCtrlEnable(TP_GPIO_PIN, ITH_GPIO_PULL_ENABLE);
	ithGpioCtrlEnable(TP_GPIO_PIN, ITH_GPIO_PULL_UP);
	ithGpioEnable(TP_GPIO_PIN);
	ithExitCritical();
}

/******************************************************************************
 * the read flow for reading the zt2083's register by using iic repead start
 ******************************************************************************/
static int _tsc2007_xfer(int fd, unsigned char regAddr)
{
	ITPI2cInfo evt;
	unsigned char	I2cCmd;
	int 			i2cret;
	unsigned int val;
	unsigned char dBuf[2];

	I2cCmd = regAddr;	//1000 0010
	evt.slaveAddress   = TOUCH_DEVICE_ID;
	evt.cmdBuffer      = &I2cCmd;
	evt.cmdBufferSize  = 1;
	evt.dataBuffer     = dBuf;
	evt.dataBufferSize = sizeof(dBuf);

	i2cret = read(fd, &evt, 1);

	if(i2cret<0) {
		printf("[TOUCH ERROR].iic read fail\n");
		return -1;
	}
	val = ((unsigned int)dBuf[0] << 8) + (unsigned int)dBuf[1];
	val >>= 4;
	return val;
}


/******************************************************************************
 * to read the Point Buffer by reading the register "0xE0"
 ******************************************************************************/
static int _readPointBuffer(int fd, struct ts_event *tmp_ts)
{
	ITPI2cInfo evt;
	unsigned char	I2cCmd;
	int 			i2cret;
	char pwrdown[2] = {0};
	unsigned int Z;
	unsigned int rt = 0;

	while(1)
	{
		tmp_ts->x = _tsc2007_xfer(fd, READ_X);
		tmp_ts->y = _tsc2007_xfer(fd, READ_Y);
		tmp_ts->z1 = _tsc2007_xfer(fd, READ_Z1);
		tmp_ts->z2 = _tsc2007_xfer(fd, READ_Z2);
		_tsc2007_xfer(fd, PWRDOWN);

		if ( (ithGpioGet(TP_GPIO_PIN) & TP_GPIO_MASK) ) {
			return (-1);
			//break;
		}


		/* range filtering */
		// if (tmp_ts->x == MAX_12BIT)
				// tmp_ts->x = 0;

		// if (likely(tmp_ts->x && tmp_ts->z1)) {
		   // [> compute touch pressure resistance using equation #1 <]
			// rt = tmp_ts->z2 - tmp_ts->z1;
			// rt *= tmp_ts->x;
			// rt *= tsc->x_plate_ohms;
			// rt /= tmp_ts->z1;
			// rt = (rt + 2047) >> 12;
		// }

		//check Z1 & Z2 for valid point
		// if(!g_IsTpFirstPoint) {
			// if(tmp_ts->z1) {
				// Z = (tmp_ts->x*(tmp_ts->z2-tmp_ts->z1)) / (tmp_ts->z1);
				// if( ((Z/16)<200) || ((Z/16)>2200) )
				// {
					// printf("z1 z2 err---->\n");
					// //continue;
					// gInvalidZ1Z2 = 1;
					// return (-1);
				// }
			// }
			// else
			// {
				// printf("z1  err---->\n");
				// gInvalidZ1Z2 = 1;
				// return (-1);
			// }
		// }

		break;
	}


	gInvalidZ1Z2 = 0;

	return 0;
}


static void* _tpProbeHandler(void* arg)
{
	unsigned int regValue;
	int 			i2cret;
	char pwrdown[8] = {0};

	printf("_tpProbeHandler.start~~\n");

	_tsc2007_xfer(g_tchDevFd, PWRDOWN);
	while(1)
	{
		if(g_IsTpInitialized==true) {
			if ( g_TouchDownIntr ) {
				struct ts_sample tmpSamp;

				gettimeofday(&endT,NULL);
				dur = (unsigned int)itpTimevalDiff(&startT, &endT);

				if( dur>TOUCH_SAMPLE_RATE ) {
					if( !_getTouchSample(&tmpSamp, 1) ) {
#ifdef __OPENRTOS__
						if (xQueueSend(tpQueue, &tmpSamp, 0) != pdPASS)
							printf("queue full------------------------>\n");
#endif
						gettimeofday(&startT,NULL);
						lastp = 1;
					}
				}
				usleep(2000);
			}
			usleep(1000);
		} else {
			printf("touch has not init, yet~~~\n");
			usleep(100000);
		}
	}
	return NULL;
}

/******************************************************************************
 * do initial flow of zt2083
 * 1.indentify zt2083
 * 2.get 2D resolution
 * 3.set interrupt information
 ******************************************************************************/
static int _initTouchChip(int fd)
{
	int i=0;

	//identify FW
	//TODO:

	//create thread
	if(g_IsTpInitialized==0)
	{
		int res;
		pthread_t task;
		pthread_attr_t attr;
		printf("Create xQueue & pthread~~\n");

#ifdef __OPENRTOS__
		tpQueue = xQueueCreate(QUEUE_LEN, (unsigned portBASE_TYPE) sizeof(struct ts_sample));

		pthread_attr_init(&attr);
		res = pthread_create(&task, &attr, _tpProbeHandler, NULL);

		if(res) {
			printf( "[TouchPanel]%s() L#%ld: ERROR, create _tpProbeHandler() thread fail! res=%ld\n", res );
			return -1;
		}

		res = pthread_mutex_init(&gTpMutex, NULL);
		if(res) {
			printf("[AudioLink]%s() L#%ld: ERROR, init touch mutex fail! res=%ld\r\n", __FUNCTION__, __LINE__, res);
			return -1;
		}
#endif
		return 0;
	}

	return -1;
}

static void _initSample(struct ts_sample *s, int nr)
{
	int i;
	struct ts_sample *samp=s;

	//samp=s;

	for(i = 0; i < nr; i++)
	{
		samp->x = 0;
		samp->y = 0;
		samp->pressure = 0;
		gettimeofday(&(samp->tv),NULL);
		samp++;
	}
}

static int _palseTouchSample(struct ts_event *tmp_ts, struct ts_sample *s, int nr)
{
	int i;
	int nRet=0;
	int tmpX=0;
	int tmpY=0;
	struct ts_sample *samp=s;

	for(i = 0; i < nr; i++)
	{
		//if(buf[0] & (1 << i))
		{
			nRet++;

			tmpX = tmp_ts->x;
			tmpY = tmp_ts->y;

			// printf("	##-Raw x,y = %d, %d ::\n",tmpX,tmpY);

#ifdef	ENABLE_SWITCH_XY
			{
				int	tmp = tmpX;

				tmpX = tmpY;
				tmpY = tmp;
			}
#endif

#ifdef	ENABLE_SCALE_X
			tmpX = ((tmpX - g_MinRawX) * g_xResolution)/(g_MaxRawX - g_MinRawX);
#endif

#ifdef	ENABLE_SCALE_Y
			tmpY = ((tmpY - g_MinRawY) * g_yResolution)/(g_MaxRawY - g_MinRawY);
#endif

#ifdef	ENABLE_REVERSE_X
			tmpX = g_xResolution - tmpX - 1;
#else
			tmpX = tmpX;
#endif

#ifdef	ENABLE_REVERSE_Y
			tmpY = g_yResolution - tmpY - 1;
#else
			tmpY = tmpY;
#endif

			if(tmpX>=g_xResolution)	tmpX = g_xResolution - 1;
			if(tmpY>=g_yResolution)	tmpY = g_yResolution - 1;

			if(tmpX<0)	tmpX = 0;
			if(tmpY<0)	tmpY = 0;

			if( (tmpX>=g_xResolution) || (tmpY>=g_yResolution) || (tmpX<0) || (tmpY<0) )
				printf("[TP warning] XY are abnormal, x=%d,%d y=%d,%d\n",tmpX,g_xResolution,tmpY,g_yResolution);


			samp->pressure = 1;
			samp->x = tmpX;
			samp->y = tmpY;
			gettimeofday(&(samp->tv),NULL);

			//printf("modify x,y = %d, %d -##\n",samp->x,samp->y);
		}
		/*
		   else
		   {
		   samp[i]->x = 0;
		   samp[i]->y = 0;
		   samp[i]->pressure = 0;
		   }
		   */
		samp++;
	}
	return nRet;
}

int _getTouchSample(struct ts_sample *samp, int nr)
{
	int i2cret;
	int real_nr=0;
	unsigned char buf[14]={0};

	_initSample(samp, nr);
#ifdef __OPENRTOS__
	pthread_mutex_lock(&gTpMutex);
#endif
	i2cret = _readPointBuffer(g_tchDevFd,&tc);
	if(i2cret<0)
	{
		//if INT active and get data Z1/Z2 error
		//then keep last x/y sample
		if(gInvalidZ1Z2)
			_palseTouchSample(&last_tc, samp, nr);

#ifdef __OPENRTOS__
		pthread_mutex_unlock(&gTpMutex);
#endif
		return (-1);
	}

	_palseTouchSample(&tc, samp, nr);
	memcpy(&last_tc, &tc, sizeof(struct ts_event));

#ifdef __OPENRTOS__
	pthread_mutex_unlock(&gTpMutex);
#endif

	return (0);
}

/*##################################################################################
#                       public function implementation
###################################################################################*/
static int zt2083_read(struct tslib_module_info *inf, struct ts_sample *samp, int nr)
{
	struct tsdev *ts = inf->dev;
	unsigned int regValue;
	int ret;
	int total = 0;
	int tchdev = ts->fd;
	struct ts_sample *s=samp;


	if(g_IsTpInitialized==false) {
		printf("TP first init(INT is GPIO %d)\n",TP_GPIO_PIN);

		gettimeofday(&startT,NULL);

		//init touch GPIO pin
		_initTouchGpioPin();

		ret = _initTouchChip(tchdev);
		if(ret<0) {
			printf("[TOUCH]warning:: touch panel initial fail\n");
			return -1;
		}
		usleep(100000);		//sleep 100ms for get the 1st touch event

		g_tchDevFd = tchdev;
		g_IsTpInitialized = true;
		printf("## TP init OK, gTpIntr = %x\n",g_TouchDownIntr);
	}

	_initSample(s, nr);

	//to receive queue
	for(ret=0; ret<nr; ret++)
	{
#ifdef __OPENRTOS__
		if( !xQueueReceive(tpQueue, s, 0) ) {
			if(g_TouchDownIntr) {
				if(lastp)
					_palseTouchSample(&last_tc, s, 1);
			}
		}
#endif

		if(nr>1)
			s++;
	}

	if( lastp && !samp->pressure) {
		if(g_IsTpFirstPoint)
			g_IsTpFirstPoint=0;
		lastp = 0;
	}
	return nr;
}

static const struct tslib_ops zt2083_ops =
{
	zt2083_read,
};

TSAPI struct tslib_module_info *castor3_mod_init(struct tsdev *dev, const char *params)
{
	struct tslib_module_info *m;

	m = malloc(sizeof(struct tslib_module_info));
	if (m == NULL)
		return NULL;

	m->ops = &zt2083_ops;
	return m;
}

#ifndef TSLIB_STATIC_CASTOR3_MODULE
TSLIB_MODULE_INIT(castor3_mod_init);
#endif
