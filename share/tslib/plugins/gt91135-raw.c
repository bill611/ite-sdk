/*
   you have to sure 3 things before porting TP driver
   1).INT is work normal
   2).I2C BUS can read data from TP chip
   3).Parse the X,Y coordination correctly

   These function are customized.
   You Have to modify these function with "(*)" mark.
   These functions(3&4) are almost without modification
   Function(5~7) will be modified deponding on chip's feature.
   0._tpInitSpec_vendor()           //set control config(*)
   1._tpReadPointBuffer_vendor()    //read point buffer(*)
   2._tpParseRawPxy_vendor()        //parse the touch point(*)
   3._tpIntActiveRule_vendor()      //touch-down RULE
   4._tpIntNotActiveRule_vendor()   //touch-up RULE
   5._tpParseKey_vendor()           //depend on TP with key
   6._tpDoPowerOnSeq_vendor();      //depend on TP with power-on sequence
   7._tpDoInitProgram_vendor();         //depend on TP with initial programming
   */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <alloca.h>
#include <pthread.h>
#include "linux/os.h"
#include "ite/ith.h"
#include "ite/itp.h"
#include "config.h"
#include "tslib-private.h"

//#include "raw_api.h"
/****************************************************************************
 * initial Kconfig setting
 ****************************************************************************/

#if	defined(CFG_TOUCH_I2C0) || defined(CFG_TOUCH_I2C1)
#define TP_INTERFACE_I2C   (0)
#endif

#if	defined(CFG_TOUCH_SPI) || defined(CFG_TOUCH_SPI0) || defined(CFG_TOUCH_SPI1)
#define TP_INTERFACE_SPI   (1)
#endif

#define TP_INT_PIN	    CFG_GPIO_TOUCH_INT
#define TP_GPIO_MASK    (1<<(TP_INT_PIN%32))

#ifdef	CFG_GPIO_TOUCH_WAKE
#if (CFG_GPIO_TOUCH_WAKE<128)
#define TP_GPIO_WAKE_PIN	CFG_GPIO_TOUCH_WAKE
#endif
#endif

#ifdef	CFG_GPIO_TOUCH_RESET
#if (CFG_GPIO_TOUCH_RESET<128)
#define TP_GPIO_RESET_PIN	CFG_GPIO_TOUCH_RESET
#endif
#endif

#ifdef	CFG_LCD_ENABLE
#define	TP_SCREEN_WIDTH	    CFG_LCD_WIDTH
#define	TP_SCREEN_HEIGHT	CFG_LCD_HEIGHT
#else
#define	TP_SCREEN_WIDTH	    800
#define	TP_SCREEN_HEIGHT	480
#endif

#define TP_SAMPLE_RATE	(10)

#define	TOUCH_NO_CONTACT		(0)
#define	TOUCH_DOWN				(1)
#define	TOUCH_UP				(2)

#define	TP_ACTIVE_LOW           (0)
#define	TP_ACTIVE_HIGH          (1)

#define	TP_INT_LEVLE_TRIGGER    (1)
#define	TP_INT_EDGE_TRIGGER     (0)

#define	TP_INT_TYPE_KEEP_STATE  (0)
#define	TP_INT_TYPE_ZT2083      (0)
#define	TP_INT_TYPE_GT911      (1)
#define	TP_INT_TYPE_IT7260      (2)

#define	TP_WITHOUT_KEY          (0)
#define	TP_HAS_TOUCH_KEY        (1)
#define	TP_GPIO_PIN_NO_DEF      (-1)

#ifdef CFG_TOUCH_INTR
#define	TP_ENABLE_INTERRUPT     (1)
#else
#define	TP_ENABLE_INTERRUPT     (0)
#endif

/****************************************************************************
 * touch cofig setting
 ****************************************************************************/
#define TP_IDLE_TIME                (2000)
#define TP_IDLE_TIME_NO_INITIAL     (100000)

/****************************************************************************
 * ENABLE_TOUCH_POSITION_MSG :: just print X,Y coordination &
 * 								touch-down/touch-up
 * ENABLE_TOUCH_IIC_DBG_MSG  :: show the IIC command
 * ENABLE_TOUCH_PANEL_DBG_MSG:: show send-queue recieve-queue,
 *                              and the xy value of each INTr
 ****************************************************************************/
//#define ENABLE_TOUCH_POSITION_MSG
//#define ENABLE_TOUCH_RAW_POINT_MSG
//#define ENABLE_TOUCH_PANEL_DBG_MSG
//#define ENABLE_TOUCH_IIC_DBG_MSG
//#define ENABLE_SEND_FAKE_SAMPLE

/****************************************************************************
 * MACRO define of GT911
 ****************************************************************************/
#define TOUCH_DEVICE_ID1      (0x5d)
#define TOUCH_DEVICE_ID2      (0x14)

//Register define
#define GTP_READ_COOR_ADDR    0x814E
#define GTP_REG_SLEEP         0x8040
#define GTP_REG_SENSOR_ID     0x814A
#define GTP_REG_CONFIG_DATA   0x8047
#define GTP_REG_VERSION       0x8140
#define GTP_REG_READ_POINT       0x8150

#define RESOLUTION_LOC        3
#define TRIGGER_LOC           8

#define GTP_INT_TRIGGER  1    //0:Rising 1:Falling
#define GTP_MAX_TOUCH         1
#define GTP_ESD_CHECK_CIRCLE  2000
#define GTP_CONFIG_MAX_LENGTH 240
#define GTP_ADDR_LENGTH       2
#define CFG_GROUP_LEN(p_cfg_grp)  (sizeof(p_cfg_grp) / sizeof(p_cfg_grp[0]))

#define DIS_SHAKE_TIME 3
//  0x47 0x48 0x49 0x4A 0x4B 0x4C 0x4D 0x4E 0x4F 0x50
#if 0  // old touch
	//   x=0x400,y=0x258,         0x3c means x,y need exchange,trigger mode is rising
#define CTP_CFG_GROUP	{\
	0x41,0x00,0x04,0x58,0x02,0x0A,0x3c,0x00,0x01,0x08,\
	0x28,0x05,0x50,0x32,0x03,0x05,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x1A,0x1C,0x1E,0x14,0x8C,0x2E,0x0E,\
	0x2D,0x2A,0x56,0x09,0x00,0x00,0x00,0x81,0x03,0x1C,\
	0x00,0x00,0x00,0x00,0x00,0x03,0x64,0x32,0x00,0x00,\
	0x00,0x2D,0x63,0x94,0xC5,0x02,0x07,0x00,0x00,0x04,\
	0xA1,0x11,0x00,0x80,0x16,0x00,0x62,0x1E,0x00,0x50,\
	0x27,0x00,0x41,0x33,0x00,0x41,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x1C,0x1A,0x18,0x16,0x14,0x12,0x10,0x0E,\
	0x0C,0x0A,0x08,0x06,0x04,0x02,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0F,\
	0x10,0x12,0x13,0x14,0x16,0x18,0x1C,0x1D,0x1E,0x1F,\
	0x20,0x21,0x22,0x24,0x26,0x28,0x29,0x2A,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
	0x00,0x00,0x00,0x00,0x7A,0x01\
}
#else // new touch
#define CTP_CFG_GROUP	{\
0x00,0xF0,0x00,0x40,0x01,0x0A,0x05,0x00,0x01,0x08,\
0x28,0x05,0x50,0x32,0x03,0x05,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x87,0x27,0x0A,\
0x17,0x15,0x31,0x0D,0x00,0x00,0x02,0xB9,0x04,0x1D,\
0x00,0x00,0x00,0x00,0x00,0x03,0x64,0x32,0x00,0x00,\
0x00,0x0F,0x94,0x94,0xC5,0x02,0x07,0x00,0x00,0x04,\
0x8D,0x13,0x00,0x5D,0x1E,0x00,0x3D,0x30,0x00,0x2A,\
0x4C,0x00,0x20,0x78,0x00,0x20,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x0E,0x10,\
0x12,0x14,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x02,0x04,0x06,0x08,0x0A,0x0C,0x1D,\
0x1E,0x1F,0x20,0x21,0x22,0x24,0xFF,0xFF,0xFF,0xFF,\
0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x00,0x00,\
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,\
0x00,0x00,0x00,0x00,0x41,0x01\
}
#endif
/****************************************
 *
 ***************************************/
typedef struct
{
	char tpCurrINT;
	char tpStatus;
	char tpNeedToGetSample;
	char tpNeedUpdateSample;
	char tpFirstSampHasSend;
	char tpIntr4Probe;
	char tpIsInitFinished;
	int  tpDevFd;
	int  tpIntrCnt;
}tp_info_tag;//tp_gv_tag

typedef struct tp_spec_tag
{
	//TP H/W setting
	char tpIntPin;		    //INT signal GPIO pin number
	char tpIntActiveState;	//High=1, Low=0
	char tpIntTriggerType;  //interrupt trigger type. 0:edge trigger, 1:level trigger
	char tpWakeUpPin;		//Wake-Up pin GPIO pin number, -1: means NO Wake-Up pin.
	char tpResetPin;		//Reset pin GPIO pin number, -1: means NO reset pin.
	char tpIntrType;		//0:keep state when touch down(like ZT2083), 1:like FT5XXX type 2:like IT7260, 3:others....
	char tpInterface; 		//0:I2C, 1:SPI, 2:other...
	char tpI2cDeviceId; 	//I2C device ID(slave address) if TP has I2C interface
	char tpHasTouchKey;		//0: NO touch key, 1:touch key type I, 2:touch key type II, ...
	char tpIntUseIsr;	    //0:polling INT siganl, 1:INT use interrupt,

	//TP resolution
	int  tpMaxRawX;
	int  tpMaxRawY;
	int  tpScreenX;
	int  tpScreenY;

	//TP convert function
	char tpCvtSwapXY;		//0:Disable, 1:Enable
	char tpCvtReverseX;     //0:Disable, 1:Enable
	char tpCvtReverseY;     //0:Disable, 1:Enable
	char tpCvtScaleX;		//0:Disable, 1:Enable
	char tpCvtScaleY;		//0:Disable, 1:Enable

	//TP sample specification
	char tpEnTchPressure;	//0:disable pressure info, 1:enable pressure info
	char tpSampleNum;		//0:NO scense, 1: single touch 2~10:multi-touch
	char tpSampleRate;		//UNIT: mill-second, range 8~16 ms(60~120 samples/per second)

	//TP idle time
	int  tpIdleTime;		//sleep time for polling INT signal(even if interrupt mode).
	int  tpIdleTimeB4Init;	//sleep time if TP not initial yet.
	int  tpReadChipRegCnt;	//read register count for getting touch xy coordination

	//TP specific function
	char tpHasPowerOnSeq;	//0:NO power-on sequence, 1:TP has power-on sequence
	char tpNeedProgB4Init;	//0:TP IC works well without programe flow, 1:TP IC need program before operation.
} TP_SPEC;

/***************************
 * global variable
 **************************/
static struct ts_sample g_sample;

static char g_TouchDownIntr = false;
static char  g_IsTpInitialized = false;
static char  creat_init_thread = false;
static pthread_mutex_t 	gTpMutex;

static TP_SPEC     gTpSpec;
static tp_info_tag gTpInfo = { 0,TOUCH_NO_CONTACT,1,0,0,0,0,0,0};

static unsigned int dur=0;
struct timeval startT, endT;
struct timeval T1, T2;
static int g_tpCntr = 0;

//for the function "_tpFixIntHasNoResponseIssue()"
static int  g_IntrLowCnt = 0;
struct timeval tv1, tv2;
static int shake_time = 0; //抖动时间
static int no_touch_time = 0; // unit 2ms

/*##################################################################################
 *                         the protocol of private function
###################################################################################*/
#ifdef CFG_TOUCH_INTR
static void _tp_isr(void* data);
static void _initTouchIntr(void);
#endif

/* *************************************************************** */
static void _tpInitSpec_vendor(void);
static int  _tpReadPointBuffer_vendor(unsigned char *buf, int cnt);
static int  _tpParseRawPxy_vendor(struct ts_sample *s, unsigned char *buf);
static void _tpParseKey_vendor(struct ts_sample *s, unsigned char *buf);

static void _tpIntActiveRule_vendor(struct ts_sample *tpSmp);
static void _tpIntNotActiveRule_vendor(struct ts_sample *tpSmp);
static bool _initGt911(void);


/* *************************************************************** */

/* *************************************************************** */
static int  _tpDoInitial(void);
static void _tpInitWakeUpPin(void);
static void _tpInitResetPin(void);
static void _tpInitIntPin(void);
static void _tpInitTouchGpioPin(void);

static void _initSample(struct ts_sample *s, int nr);
static char _tpGetIntr(void);
/* *************************************************************** */

/* *************************************************************** */
static void _tpGetRawPoint(struct ts_sample *samp, int nr);
static void _tpConvertRawPoint(struct ts_sample *samp, int nr);
static void _tpUpdateLastXY(struct ts_sample *smp);

static bool _tpNeedToGetSample(void);
static void _tpGetSample(struct ts_sample *samp, int nr);
static void _tpUpdate(struct ts_sample *tpSmp);
/* *************************************************************** */

static void* _tpProbeHandler(void* arg);
static int  _tpProbeSample(struct ts_sample *samp, int nr);

#define mdelay(x)	            msleep(x) //ithDelay(x*1000)

static void _tpGpioOutPut(int port,int value)
{
	ithGpioSetMode(port,ITH_GPIO_MODE0);
	ithGpioSetOut(port);
	if (value)
		ithGpioSet(port);
	else
		ithGpioClear(port);
	ithGpioEnable(port);
}
static void _tpGpioInPut(int port)
{
	ithGpioSetMode(port, ITH_GPIO_MODE0);
	ithGpioSetIn(port);
	ithGpioCtrlEnable(port, ITH_GPIO_PULL_ENABLE);
	ithGpioCtrlEnable(port, ITH_GPIO_PULL_UP);

	ithGpioEnable(port);
}
/*##################################################################################
 *                        the private function implementation
###################################################################################*/
static void _tpInitSpec_vendor(void)
{
	gTpSpec.tpIntPin          	= (char)TP_INT_PIN;           //from Kconfig setting
	gTpSpec.tpWakeUpPin         = (char)TP_GPIO_PIN_NO_DEF;   //from Kconfig setting
	gTpSpec.tpResetPin          = (char)TP_GPIO_RESET_PIN;    //from Kconfig setting
	gTpSpec.tpIntUseIsr         = (char)TP_ENABLE_INTERRUPT;  //from Kconfig setting
	gTpSpec.tpIntActiveState    = (char)TP_ACTIVE_LOW;        //from Kconfig setting
	gTpSpec.tpIntTriggerType    = (char)TP_INT_EDGE_TRIGGER; //from Kconfig setting   level/edge

	gTpSpec.tpInterface         = (char)TP_INTERFACE_I2C;	  //from Kconfig setting

	gTpSpec.tpMaxRawX           = (int)CFG_TOUCH_X_MAX_VALUE; //from Kconfig setting
	gTpSpec.tpMaxRawY           = (int)CFG_TOUCH_Y_MAX_VALUE; //from Kconfig setting
	gTpSpec.tpScreenX           = (int)TP_SCREEN_WIDTH;       //from Kconfig setting
	gTpSpec.tpScreenY           = (int)TP_SCREEN_HEIGHT;      //from Kconfig setting

	gTpSpec.tpCvtSwapXY        = (char)1;                    //from Kconfig setting
	gTpSpec.tpCvtReverseX      = (char)0;                    //from Kconfig setting
	gTpSpec.tpCvtReverseY      = (char)0;                    //from Kconfig setting
	gTpSpec.tpCvtScaleX        = (char)1;                    //from Kconfig setting
	gTpSpec.tpCvtScaleY        = (char)1;                    //from Kconfig setting

	gTpSpec.tpI2cDeviceId       = (char)TOUCH_DEVICE_ID1;	  //from this driver setting
	gTpSpec.tpEnTchPressure     = (char)0;                    //from this driver setting
	gTpSpec.tpSampleNum         = (char)1;                    //from this driver setting
	gTpSpec.tpSampleRate        = (char)TP_SAMPLE_RATE;       //from this driver setting
	gTpSpec.tpIntrType          = (char)TP_INT_TYPE_GT911;	  //from this driver setting
	gTpSpec.tpHasTouchKey       = (char)TP_WITHOUT_KEY;       //from this driver setting
	gTpSpec.tpIdleTime          = (int)TP_IDLE_TIME;          //from this driver setting
	gTpSpec.tpIdleTimeB4Init    = (int)TP_IDLE_TIME_NO_INITIAL;//from this driver setting
	gTpSpec.tpReadChipRegCnt    = (int)4;

	//special initial flow
	gTpSpec.tpHasPowerOnSeq     = (char)0;
	gTpSpec.tpNeedProgB4Init    = (char)0;

	/*
	   printf("gTpSpec.tpIntPin         = %d\n",gTpSpec.tpIntPin);
	   printf("gTpSpec.tpIntActiveState = %x\n",gTpSpec.tpIntActiveState);
	   printf("gTpSpec.tpWakeUpPin      = %d\n",gTpSpec.tpWakeUpPin);
	   printf("gTpSpec.tpResetPin       = %d\n",gTpSpec.tpResetPin);
	   printf("gTpSpec.tpIntrType       = %x\n",gTpSpec.tpIntrType);
	   printf("gTpSpec.tpInterface      = %x\n",gTpSpec.tpInterface);
	   printf("gTpSpec.tpI2cDeviceId    = %x\n",gTpSpec.tpI2cDeviceId);
	   printf("gTpSpec.tpHasTouchKey    = %x\n",gTpSpec.tpHasTouchKey);
	   printf("gTpSpec.tpIntUseIsr      = %x\n",gTpSpec.tpIntUseIsr);
	   printf("gTpSpec.tpMaxRawX        = %d\n",gTpSpec.tpMaxRawX);
	   printf("gTpSpec.tpMaxRawY        = %d\n",gTpSpec.tpMaxRawY);
	   printf("gTpSpec.tpScreenX        = %d\n",gTpSpec.tpScreenX);
	   printf("gTpSpec.tpScreenY        = %d\n",gTpSpec.tpScreenY);
	   printf("gTpSpec.tpCvtSwapXY     = %x\n",gTpSpec.tpCvtSwapXY);
	   printf("gTpSpec.tpCvtReverseX   = %x\n",gTpSpec.tpCvtReverseX);
	   printf("gTpSpec.tpCvtReverseY   = %x\n",gTpSpec.tpCvtReverseY);
	   printf("gTpSpec.tpCvtScaleX     = %x\n",gTpSpec.tpCvtScaleX);
	   printf("gTpSpec.tpCvtScaleY     = %x\n",gTpSpec.tpCvtScaleY);
	   printf("gTpSpec.tpEnTchPressure  = %x\n",gTpSpec.tpEnTchPressure);
	   printf("gTpSpec.tpSampleNum      = %x\n",gTpSpec.tpSampleNum);
	   printf("gTpSpec.tpSampleRate     = %x\n",gTpSpec.tpSampleRate);
	   printf("gTpSpec.tpIdleTime       = %d\n",gTpSpec.tpIdleTime);
	   printf("gTpSpec.tpIdleTimeB4Init = %d\n",gTpSpec.tpIdleTimeB4Init);
	   printf("gTpSpec.tpHasPowerOnSeq  = %x\n",gTpSpec.tpHasPowerOnSeq);
	   printf("gTpSpec.tpNeedProgB4Init = %x\n",gTpSpec.tpNeedProgB4Init);
	   */

	//initial global variable "gTpInfo"
	/*
	   printf("gTpInfo.tpCurrINT              = %x\n",gTpInfo.tpCurrINT);
	   printf("gTpInfo.tpStatus               = %x\n",gTpInfo.tpStatus);
	   printf("gTpInfo.tpNeedToGetSample      = %x\n",gTpInfo.tpNeedToGetSample);
	   printf("gTpInfo.tpNeedUpdateSample     = %x\n",gTpInfo.tpNeedUpdateSample);
	   printf("gTpInfo.tpFirstSampHasSend     = %x\n",gTpInfo.tpFirstSampHasSend);
	   printf("gTpInfo.tpIsInitFinished     = %x\n",gTpInfo.tpIsInitFinished);
	   printf("gTpInfo.tpIntr4Probe           = %x\n",gTpInfo.tpIntr4Probe);
	   printf("gTpInfo.tpDevFd                = %x\n",gTpInfo.tpDevFd);
	   printf("gTpInfo.tpIntrCnt              = %x\n",gTpInfo.tpIntrCnt);
	   */
}

void gtp_reset_guitar(int ms)
{
	_tpGpioOutPut(gTpSpec.tpResetPin, 0);   // begin select I2C slave addr

	mdelay(ms);                         // T2: > 10ms
	// HIGH: 0x28/0x29, LOW: 0xBA/0xBB
	_tpGpioOutPut(gTpSpec.tpIntPin, gTpSpec.tpI2cDeviceId == TOUCH_DEVICE_ID2);

	mdelay(2);                          // T3: > 100us
	_tpGpioOutPut(gTpSpec.tpResetPin, 1);

	mdelay(6);                          // T4: > 5ms

	_tpGpioInPut(gTpSpec.tpResetPin);    // end select I2C slave addr

	mdelay(50);

}

bool gtp_i2c_read(  uint8_t* cmd, uint8_t *buf, uint32_t len)
{
	int i2cret;
	ITPI2cInfo *ft5306_evt;
	int retries = 0;

	ft5306_evt = alloca(sizeof(*ft5306_evt));

	ft5306_evt->slaveAddress   = gTpSpec.tpI2cDeviceId;
	ft5306_evt->cmdBuffer      = cmd;
	ft5306_evt->cmdBufferSize  = 2;
	ft5306_evt->dataBuffer     = buf;
	ft5306_evt->dataBufferSize = len;

	while (retries < 5) {
		i2cret = read(gTpInfo.tpDevFd, ft5306_evt, 1);
		if (i2cret == len)
			break;
		retries++;
	}
	if (retries >= 5) {
		printf("Err : read retry times > 5,reset tp chip\n");
		gtp_reset_guitar(10);
	}
	return i2cret;
}

bool gtp_i2c_write( uint8_t* cmd, uint32_t cmdlen)
{
	int i2cret;
	ITPI2cInfo *ft5306_evt;
	int retries = 0;

	ft5306_evt = alloca(sizeof(*ft5306_evt));

	ft5306_evt->slaveAddress   = gTpSpec.tpI2cDeviceId;
	ft5306_evt->cmdBuffer      = cmd;
	ft5306_evt->cmdBufferSize  = cmdlen;
	while (retries < 5) {
		i2cret = write(gTpInfo.tpDevFd, ft5306_evt, 1);
		if (i2cret == cmdlen)
			break;
		retries++;
	}
	if (retries >= 5) {
		printf("Err : write retry times > 5,reset tp chip\n");
		gtp_reset_guitar(10);
	}

	return i2cret;
}

static int _tpReadPointBuffer_vendor(unsigned char *buf, int cnt)
{
	int recv = -1;
	uint8_t cmd_read_config[2]={0x81, 0x4e};
	uint8_t cmd_read_point[2]={0x81, 0x50};
	uint8_t cmd_read_end[3] = {0x81, 0x4e, 0};
	int ret = gtp_i2c_read(cmd_read_config,buf, 1);
	if (ret < 0) {
		printf("I2C transfer error. errno:%d\n ", ret);
		goto end;
	}
	if((buf[0] & 0x80) == 0)
		goto end;

	uint8_t touch_num = buf[0] & 0x0f;
	if (touch_num > GTP_MAX_TOUCH || buf[0] == 0x80)
		goto exit_work_func;

	gtp_i2c_read(cmd_read_point,buf, cnt);
	recv = 1;

exit_work_func:
	if (gtp_i2c_write(cmd_read_end,3) < 0)
		printf("I2C write cmd_read_end  error!");

end:
	return recv;
}

static int _tpParseRawPxy_vendor(struct ts_sample *s, unsigned char *buf)
{
	// if(gTpInfo.tpStatus==TOUCH_DOWN ) {
		// s->x = (int)0;
		// s->y = (int)0;
		// s->pressure = 0;		//?? how to get ft5xxx's presure?
		// printf("pressure\n");
	// } else {
		s->x = (((unsigned int)buf[1]<<8) |buf[0]);
		s->y = (((unsigned int)buf[3]<<8) |buf[2]);
		s->pressure = 1;		//?? how to get gt911's presure?
		// s->x = (int)((((unsigned int)buf[3]<<8)&0x0F00) | ((unsigned int)buf[4]));
		// s->y = (int)((((unsigned int)buf[5]<<8)&0x0F00) | ((unsigned int)buf[6]));
		// s->pressure = 1;			//?? how to get ft5xxx's presure?
	// }
	/*
	   if( buf[2] || ((buf[3]&0xf0)==0x40) )
	   {
	   if( (buf[2]==0) && (gTpInfo.tpStatus==TOUCH_DOWN) && ((buf[3]&0xf0)==0x40) )
	   {
	   s->x = (int)0;
	   s->y = (int)0;
	   s->pressure = 0;		//?? how to get ft5xxx's presure?
	   }
	   else
	   {
	   s->x = (int)((((unsigned int)buf[3]<<8)&0x0F00) | ((unsigned int)buf[4]));
	   s->y = (int)((((unsigned int)buf[5]<<8)&0x0F00) | ((unsigned int)buf[6]));
	   s->pressure = 1;			//?? how to get ft5xxx's presure?
	   }

#ifdef	ENABLE_TOUCH_RAW_POINT_MSG
printf("	RAW->---------> %d %d %d\n", s->pressure, s->x, s->y);
#endif

	//gettimeofday(&s->tv,NULL);
	return 0;
	}
	else
	{
#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
printf("tch inv buf3=%x\r\n",buf[3]);
#endif
return -1;
}
*/
}

static void _tpParseKey_vendor(struct ts_sample *s, unsigned char *buf)
{
	//TODO: get key information and input to xy sample...? as a special xy?
	//maybe define a special area for key
	//(like touch is 800x480, for example, y>500 for key, x=0~100 for keyA, x=100~200 for keyB... )
	//SDL layer could parse this special defination xy into key event(but this layer is not ready yet).
}

static void _tpIntActiveRule_vendor(struct ts_sample *tpSmp)
{
	gTpInfo.tpIntrCnt = 0;
	gTpInfo.tpNeedUpdateSample = 0;

	if(!gTpSpec.tpIntUseIsr)
	{
		//for prevent from the issue that polling INT signal will get the same sample.
		if(!gTpInfo.tpNeedToGetSample)
			return;
		else
			gTpInfo.tpNeedToGetSample = 0;
	}

	no_touch_time = 0;
	//status rule for TOUCH_DOWN/TOUCH_UP/TOUCH_NO_CONTACT
	switch(gTpInfo.tpStatus)
	{
		case TOUCH_NO_CONTACT:
			if(tpSmp->pressure)
			{
				// if (++shake_time >= DIS_SHAKE_TIME) {
					gTpInfo.tpStatus = TOUCH_DOWN;
					gTpInfo.tpIntr4Probe = 1;
					gTpInfo.tpNeedUpdateSample = 1;
					gTpInfo.tpFirstSampHasSend = 0;
				// } else {
					// _initSample(tpSmp,1);
				// }
			}
			break;

		case TOUCH_DOWN:
			if(!tpSmp->pressure)
			{
				gTpInfo.tpStatus = TOUCH_UP;
			}
			if(gTpInfo.tpFirstSampHasSend)
				gTpInfo.tpNeedUpdateSample = 1;
			break;

		case TOUCH_UP:
			if(!tpSmp->pressure)
			{
				gTpInfo.tpStatus = TOUCH_NO_CONTACT;
				gTpInfo.tpIntr4Probe = 0;
			}
			else
			{
				gTpInfo.tpStatus = TOUCH_DOWN;
				gTpInfo.tpIntr4Probe = 1;
				gTpInfo.tpNeedUpdateSample = 1;
			}
			break;

		default:
			printf("ERROR touch STATUS, need to check it!!\n");
			break;
	}

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG

	printf("	tpStatus=%x, NSQ=%x, cINT=%x, send=%x\n",
			gTpInfo.tpStatus, gTpInfo.tpNeedUpdateSample, gTpInfo.tpIntr4Probe, gTpInfo.tpFirstSampHasSend);
#endif

	//use this flag to judge if update the touch sample
	//1.have to update the first TOUCH_DOWN event
	//2.don't update the touch event if UI does not get the first event
	//3.real-time update the X,Y point after send the 1st event
	//4.must send the touch event if last status is touch-up, and INT active again in this time.
	//  to handle the quickly touch case.
	//5.others...
	if(gTpInfo.tpNeedUpdateSample)
	{
		gettimeofday(&startT,NULL);
		_tpUpdateLastXY(tpSmp);
	}

	if(gTpSpec.tpIntUseIsr)
	{
		//clear INT flag and enable interrupt if use ISR to handle INT signal
		g_TouchDownIntr = 0;
		ithGpioEnableIntr(TP_INT_PIN);
	}
}

static void _tpIntNotActiveRule_vendor(struct ts_sample *tpSmp)
{

	if(!gTpSpec.tpIntUseIsr)
	{
		//if INT not active, then set this flag to call _tpGetSample() if next INT active
		gTpInfo.tpNeedToGetSample = 1;
	}

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	if( (gTpInfo.tpStatus != TOUCH_NO_CONTACT) )
		printf("	UpdateSmp0:INT=%x, ss=(%d,%d)\n",
				gTpInfo.tpCurrINT, gTpInfo.tpStatus, gTpInfo.tpFirstSampHasSend);
#endif
	if (++no_touch_time > 8)
		shake_time = 0;

	//In order to prevent from loss of the first touch event
	//Need To set "status=TOUCH_NO_CONTACT" if "last status=TOUCH_UP" + "first sample has send"
	if( (gTpInfo.tpStatus == TOUCH_UP) && (gTpInfo.tpFirstSampHasSend) )
	{
		_tpUpdateLastXY(NULL);
		gTpInfo.tpStatus = TOUCH_NO_CONTACT;
		gTpInfo.tpIntr4Probe = 0;

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
		printf("INT=0, force to set status=0!!\n");
#endif
	}

	//For prevent from lossing the touch-up event
	//sometimes, S/W can not get TOUCH-UP event when INT is actived
	//So, this code will force to set touch-status as TOUCH_UP after INT is not actived for a specific time(16ms)
	if( (gTpInfo.tpStatus == TOUCH_DOWN) && (_tpGetIntr()==false) )
	{
		//printf("	UdSmp:s=%d, int=%x, ic=%d\n",gTpInfo.tpStatus,currINT,gTpInfo.tpIntrCnt);

		if(!gTpInfo.tpIntrCnt)	
			gettimeofday(&T1,NULL);
		gettimeofday(&T2,NULL);
		dur = (unsigned int)itpTimevalDiff(&T1, &T2);

		if( dur > gTpSpec.tpSampleRate && gTpInfo.tpFirstSampHasSend )
		{
			gTpInfo.tpStatus = TOUCH_UP;
			gTpInfo.tpIntr4Probe = 0;
			_tpUpdateLastXY(NULL);

// #ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
			printf("INT=0, and dur%d>%dms, so force to set status=2!!\n",dur,gTpSpec.tpSampleRate);
// #endif

			gettimeofday(&startT,NULL);
		}

		gTpInfo.tpIntrCnt++;
	}

}

#ifdef CFG_TOUCH_INTR
static void _tp_isr(void* data)
{
	unsigned int regValue;

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	//ithPrintf("$in\n");
#endif

	g_TouchDownIntr = true;

	ithGpioClearIntr(TP_INT_PIN);

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	//ithPrintf("$out(%x,%x)\n",g_TouchDownIntr,regValue);
#endif
}

static void _initTouchIntr(void)
{
#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	printf("TP init in\n");
#endif

	ithEnterCritical();

	ithGpioClearIntr(TP_INT_PIN);
	ithGpioRegisterIntrHandler(TP_INT_PIN, _tp_isr, NULL);

	if(gTpSpec.tpIntTriggerType==TP_INT_LEVLE_TRIGGER)
		ithGpioCtrlEnable(TP_INT_PIN, ITH_GPIO_INTR_LEVELTRIGGER);
	else
		ithGpioCtrlDisable(TP_INT_PIN, ITH_GPIO_INTR_LEVELTRIGGER);

	if(gTpSpec.tpIntTriggerType==TP_INT_EDGE_TRIGGER)  //if edge trigger
		ithGpioCtrlDisable(TP_INT_PIN, ITH_GPIO_INTR_BOTHEDGE);		//set as single edge

	if(gTpSpec.tpIntActiveState)
		ithGpioCtrlDisable(TP_INT_PIN, ITH_GPIO_INTR_TRIGGERFALLING);	//set as rising edge
	else
		ithGpioCtrlEnable(TP_INT_PIN, ITH_GPIO_INTR_TRIGGERFALLING);	//set as falling edge

	ithIntrEnableIrq(ITH_INTR_GPIO);
	ithGpioEnableIntr(TP_INT_PIN);

	ithExitCritical();

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	printf("TP init out\n");
#endif
}
#endif

static void _tpInitWakeUpPin(void)
{
	if( (gTpSpec.tpWakeUpPin>0) && (gTpSpec.tpWakeUpPin<128) )
	{
		ithGpioSetMode(gTpSpec.tpWakeUpPin,ITH_GPIO_MODE0);
		ithGpioSetOut(gTpSpec.tpWakeUpPin);
		ithGpioSet(gTpSpec.tpWakeUpPin);
		ithGpioEnable(gTpSpec.tpWakeUpPin);
	}
	else
	{
		printf("NOT initial TOUCH_GPIO_WAKE_PIN\n");
	}
}

static void _tpInitResetPin(void)
{
	if( (gTpSpec.tpResetPin>0) && (gTpSpec.tpResetPin<128) )
	{
		ithGpioSetMode(gTpSpec.tpResetPin,ITH_GPIO_MODE0);
		ithGpioSetOut(gTpSpec.tpResetPin);
		ithGpioSet(gTpSpec.tpResetPin);
		ithGpioEnable(gTpSpec.tpResetPin);
	}
	else
	{
		printf("NOT initial TOUCH_RESET_PIN\n");
	}
}

static void _tpInitIntPin(void)
{
	ithGpioSetMode(TP_INT_PIN, ITH_GPIO_MODE0);
	ithGpioSetIn(TP_INT_PIN);
	// ithGpioCtrlEnable(TP_INT_PIN, ITH_GPIO_PULL_ENABLE);

	// if(gTpSpec.tpIntActiveState)
		ithGpioCtrlDisable(TP_INT_PIN, ITH_GPIO_PULL_UP);
	// else
		// ithGpioCtrlEnable(TP_INT_PIN, ITH_GPIO_PULL_UP);

	ithGpioEnable(TP_INT_PIN);
}

static void _tpInitTouchGpioPin(void)
{
#ifdef	CFG_GPIO_TOUCH_WAKE
	_tpInitWakeUpPin();
#endif

#ifdef	CFG_GPIO_TOUCH_RESET
	_tpInitResetPin();
#endif

	_tpInitIntPin();

#ifdef CFG_TOUCH_INTR
	_initTouchIntr();
#endif
}

static char _tpGetIntr(void)
{
	if(gTpSpec.tpIntUseIsr)
	{
		//printf("GetInt1:%x\n",g_TouchDownIntr);
		return g_TouchDownIntr;
	}
	else
	{
		unsigned int regValue = ithGpioGet(TP_INT_PIN);

		//printf("GetInt2:%x,%x\n",regValue,TP_GPIO_MASK);
		if(gTpSpec.tpIntActiveState)
		{
			if ( regValue & TP_GPIO_MASK )	return true;
			else	return false;
		}
		else
		{
			if ( !(regValue & TP_GPIO_MASK) )	return true;
			else	return false;
		}
	}
}

static void _initSample(struct ts_sample *s, int nr)
{
	int i;
	struct ts_sample *samp=s;

	for(i = 0; i < nr; i++)
	{
		samp->x = 0;
		samp->y = 0;
		samp->pressure = 0;
		gettimeofday(&(samp->tv),NULL);
		samp++;
	}
}

static void _tpGetRawPoint(struct ts_sample *samp, int nr)
{
	int real_nr=0;
	struct ts_sample *s=samp;
	unsigned char *buf = (unsigned char *)malloc(gTpSpec.tpReadChipRegCnt);

	_initSample(s, nr);
	memset(buf, 0, gTpSpec.tpReadChipRegCnt);

	while(real_nr++<nr)
	{
		if(_tpReadPointBuffer_vendor(buf, gTpSpec.tpReadChipRegCnt)<0)
			break;

		if(_tpParseRawPxy_vendor(s, buf)<0)
			break;

		if(gTpSpec.tpHasTouchKey)
			_tpParseKey_vendor(s, buf);

		s++;
	}
	free(buf);
}

static void _tpConvertRawPoint(struct ts_sample *samp, int nr)
{
	int real_nr=0;
	struct ts_sample *s=samp;

#ifdef ENABLE_TOUCH_RAW_POINT_MSG
	printf("	CvtRawPnt:%x,%d,%d\n",s->pressure,s->x,s->y);
#endif

	while(real_nr++<nr)
	{
		if(!s->pressure)
		{
			s++;
			real_nr++;
			continue;
		}

		if(gTpSpec.tpCvtSwapXY)
		{
			int tmp = s->x;
			s->x = s->y;
			s->y = tmp;
		}

		if( (gTpSpec.tpHasTouchKey) && (s->y > gTpSpec.tpMaxRawX) )
		{
			s++;
			real_nr++;
			continue;//NEED TO VERIFY THIS CODE
		}

		if(gTpSpec.tpCvtReverseX)
		{
			if(s->x>=gTpSpec.tpMaxRawX)	s->x = 0;
			if(gTpSpec.tpMaxRawX>s->x)	s->x = gTpSpec.tpMaxRawX - s->x;
		}

		if(gTpSpec.tpCvtReverseY)
		{
			if(s->y >= gTpSpec.tpMaxRawY)	s->y = 0;
			if(gTpSpec.tpMaxRawY>s->y)	s->y = gTpSpec.tpMaxRawY - s->y;
		}

		if(gTpSpec.tpCvtScaleX)
		{
			//s->x = (short)(((uint32_t)s->x*gTpSpec.tpScreenX)/gTpSpec.tpMaxRawX);
			s->x = (short)(((uint32_t)s->x*gTpSpec.tpScreenX)/gTpSpec.tpMaxRawY);
		}

		if(gTpSpec.tpCvtScaleY)
		{
			//s->y = (short)(((uint32_t)s->y*gTpSpec.tpScreenY)/gTpSpec.tpMaxRawY);
			s->y = (short)(((uint32_t)s->y*gTpSpec.tpScreenY)/gTpSpec.tpMaxRawX);
		}

		/*
		   if( (s->x>=gTpSpec.tpScreenX) || (s->y>=gTpSpec.tpScreenY) || (s->x<0) || (s->y<0) )
		   printf("[TP warning] XY are abnormal, x=%d,%d y=%d,%d\n",s->x,gTpSpec.tpScreenX,s->y,gTpSpec.tpScreenY);

		   if(s->x>=gTpSpec.tpScreenX)	s->x = gTpSpec.tpScreenX - 1;
		   if(s->y>=gTpSpec.tpScreenY)	s->y = gTpSpec.tpScreenY - 1;

		   if(s->x<0)	s->x = 0;
		   if(s->y<0)	s->y = 0;

		//printf("modify x,y = %d, %d -##\n",s->x,s->y);
		*/

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
		printf("	cvtPXY->--------> %d %d %d\n", 
				s->pressure, s->x, s->y);
#endif

		s++;
	}
}

static void _tpGetSample(struct ts_sample *samp, int nr)
{
	int real_nr=0;
	struct ts_sample *s=samp;

	while(real_nr++<nr)
	{
		_tpGetRawPoint(s, 1);

		_tpConvertRawPoint(s, 1);

		s++;
	}
}

static void _tpUpdateLastXY(struct ts_sample *smp)
{
	pthread_mutex_lock(&gTpMutex);
	if(smp!=NULL)
	{
		memcpy((void *)&g_sample ,(void *)smp, sizeof(struct ts_sample));
	}
	else
	{
		memset((void *)&g_sample , 0, sizeof(struct ts_sample));
	}

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	if(smp!=NULL)	
		printf("	EnQue:p=%x,xy=%d,%d\n", 
			smp->pressure, smp->x, smp->y);
	else       
   		printf("	EnQue:p=%x,xy=%d,%d\n", 0, 0, 0);
#endif
	pthread_mutex_unlock(&gTpMutex);
}

/**
 * to judge if S/W needs to get touch point
 *
 * @return: true for YES, false for NO
 *
 * [NOTE]:return true if need to get touch sample via i2c bus(ex: when TP's INT signal actived, or other special rules)
 return false if NO need to get sample(NO TP's INT actived, or other special rules)
 in usual case, return true for INT actived, false for INT not actived..
 special rule1: To prevent from getting the same sample, when use "pulling INT" without interrupt.
 special rule2: DO NOT wanna loss the first point when finger just touch down(or quickly touch)
 */
static bool _tpNeedToGetSample(void)
{
	if(g_IsTpInitialized != true)
		return false;

	if(_tpGetIntr()==true) {
		gTpInfo.tpCurrINT = 1;

		if(gTpSpec.tpIntUseIsr) {
			ithGpioDisableIntr(TP_INT_PIN);
			return true;
		} else {
			if( gTpInfo.tpNeedToGetSample )
				return true;
			else
				return false;
		}
	} else {
		gTpInfo.tpCurrINT = 0;
		return false;
	}
}

/**
 * to update the touch status
 *
 * [HINT 1]: when use "pulling INT", remember to prevent from getting the same sample
 * [HINT 2]: DO NOT loss the first point(each point) in quickly clicking case.
 * [HINT 3]: if not initial yet, TP will sleep 100ms
 * [HINT 4]: Basically, INT is active, then report "TOUCH DOWN" event; INT is not active, then report "TOUCH UP" event
 * [HINT 5]: INT has 2 action type:
 1). pull low until finger is not touch(contact) on TP
 2). pull low as a pulse which width about 2~20 micro-second until finger is not touch(contact) on TP
 */
static void _tpUpdate(struct ts_sample *tpSmp)
{
	if(g_IsTpInitialized!=true)
	{
		// printf("WARNING:: TP has not initial, yet~~~\n");
		usleep(TP_IDLE_TIME_NO_INITIAL);
		return;
	}

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	if(gTpInfo.tpStatus != TOUCH_NO_CONTACT)
		printf("	UpdateSmp:INT=%x, s=%x pxy=(%d,%d,%d)\n",
				gTpInfo.tpCurrINT, gTpInfo.tpStatus, tpSmp->pressure,tpSmp->x,tpSmp->y);
#endif

	if(gTpInfo.tpCurrINT)
	{
		_tpIntActiveRule_vendor(tpSmp);
	}
	else
	{
		_tpIntNotActiveRule_vendor(tpSmp);
	}

	usleep(TP_IDLE_TIME);	//sleep 2ms
}

/**
 * the thread for handling real-time touch event(<2ms)
 *
 * [HINT]: use g_sample for comunication with function "_tpProbeSample()"
 */
static void* _tpProbeHandler(void* arg)
{
	struct ts_sample tpSmp;

	while(1)
	{
		if(_tpNeedToGetSample()) {
			_tpGetSample(&tpSmp, 1);
		}

		_tpUpdate(&tpSmp);
	}
	return NULL;
}

static void* _tpInitHandler(void* arg)
{
	printf("Guitar reset\n");
	gtp_reset_guitar(10);

	printf("try to init GPIO pin\n");
	_tpInitTouchGpioPin();
	_initGt911();
	g_IsTpInitialized = true;
	printf("TP initial has finished\n");

	return NULL;
}
/******************************************************************************
 * do initial flow of gt911
 ******************************************************************************/

static bool _initGt911(void)
{
	int  ret = -1;
	int i;
	uint8_t check_sum;
	uint8_t cfg_buf[3];
	uint8_t cfg_config[GTP_CONFIG_MAX_LENGTH+GTP_ADDR_LENGTH];

	uint8_t cfg_info_group[] = CTP_CFG_GROUP;
	uint8_t cfg_info_len = CFG_GROUP_LEN(cfg_info_group);


	cfg_buf[0] = 0x80;                                ///read config information   check i2c address 5d/14
	cfg_buf[1] = 0x47;
	memset(cfg_config, 0, (GTP_CONFIG_MAX_LENGTH+GTP_ADDR_LENGTH));
	ret = gtp_i2c_read(cfg_buf,cfg_config,GTP_CONFIG_MAX_LENGTH);
	if(ret == 0) {
		gTpSpec.tpI2cDeviceId = TOUCH_DEVICE_ID2;
		ret = gtp_i2c_read(cfg_buf,cfg_config,GTP_CONFIG_MAX_LENGTH);
		if(ret == 0)
			return false;
	}

	printf("gt911 i2c-id %#02X \n",gTpSpec.tpI2cDeviceId);
#if 0
	for(i=0;i<GTP_CONFIG_MAX_LENGTH;i++)
	{
		printf("%02x,",cfg_config[i]);
		if ((i+1) % 10 == 0)
			printf("\n");
	}
#endif

	memset(cfg_config, 0, (GTP_CONFIG_MAX_LENGTH+GTP_ADDR_LENGTH));               ///write config information
	cfg_config[0] = 0x80;
	cfg_config[1] = 0x47;
	memcpy(&cfg_config[2], cfg_info_group, cfg_info_len);

#if 1  /// setting Resolution
	cfg_config[RESOLUTION_LOC]     = (uint8_t)CFG_TOUCH_X_MAX_VALUE;
	cfg_config[RESOLUTION_LOC + 1] = (uint8_t)(CFG_TOUCH_X_MAX_VALUE>>8);
	cfg_config[RESOLUTION_LOC + 2] = (uint8_t)CFG_TOUCH_Y_MAX_VALUE;
	cfg_config[RESOLUTION_LOC + 3] = (uint8_t)(CFG_TOUCH_Y_MAX_VALUE>>8);
#endif

#if 0
	if (GTP_INT_TRIGGER == 0)  //RISING
	{
		cfg_config[TRIGGER_LOC] &= 0xfe;
	}
	else if (GTP_INT_TRIGGER == 1)  //FALLING
	{
		cfg_config[TRIGGER_LOC] |= 0x01;
	}
#endif

#if 1                                                                                               ///Check Digit
	check_sum = 0;
	for (i = GTP_ADDR_LENGTH; i < cfg_info_len; i++)
	{
		check_sum += cfg_config[i];
	}
	cfg_config[cfg_info_len] = (~check_sum) + 1;
#endif

	gtp_i2c_write(cfg_config,(cfg_info_len+GTP_ADDR_LENGTH));

	return true;
}

/******************************************************************************
 * do initial flow
 ******************************************************************************/
/**
 * TP initial flow
 *
 * @return: 0 for success, -1 for failure
 *
 */
static int _tpDoInitial(void)
{
	int ret;
	int res;
	pthread_t task;
	pthread_attr_t attr;

	pthread_t task_init;
	pthread_attr_t attr_init;
	//initialize the TP SPEC first.
	_tpInitSpec_vendor();

	gettimeofday(&startT,NULL);

	//create touch mutex
	res = pthread_mutex_init(&gTpMutex, NULL);
	if(res) {
		printf("[Touch Panel]%s() L#%ld: ERROR, init touch mutex fail! res=%ld\r\n", __FUNCTION__, __LINE__, res);
		return -1;
	}

	printf("Create touch pthread~~\n");
	//create thread
	pthread_attr_init(&attr_init);
	res = pthread_create(&task_init, &attr_init, _tpInitHandler, NULL);

	pthread_attr_init(&attr);
	res = pthread_create(&task, &attr, _tpProbeHandler, NULL);
	if(res) {
		printf( "[TouchPanel]%s() L#%ld: ERROR, create _tpProbeHandler() thread fail! res=%ld\n", res );
		return -1;
	}
	creat_init_thread = true;
	return 0;
}

/**
 * Send touch sample(samp->pressure, samp->x, samp->y, and samp->tv)
 *
 * @param samp: the touch samples
 * @param nr: the sample count that upper layer wanna get.
 * @return: the really total touch sample count
 *
 * [HINT 1]:this function will be called by it7260_read().
 * [HINT 2]:get the samples from the global variable(g_sample).
 */
static int _tpProbeSample(struct ts_sample *samp, int nr)
{
	int ret=0;
	struct ts_sample *s=samp;

	pthread_mutex_lock(&gTpMutex);

	if(gTpInfo.tpIntr4Probe)
	{
		while(ret++<nr)
		{
			memcpy((void *)s,(void *)&g_sample, sizeof(struct ts_sample));
			gettimeofday(&s->tv,NULL);
			if(s->pressure)	
				gTpInfo.tpFirstSampHasSend = 1;
		}
	}

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	printf("gfQ, r=%x, INT=%x, s=%x, pxy=(%d,%d,%d)\n", 
			ret, gTpInfo.tpIntr4Probe, gTpInfo.tpStatus, s->pressure, s->x, s->y);
#endif

	pthread_mutex_unlock(&gTpMutex);

	return ret;
}

#ifdef	ENABLE_SEND_FAKE_SAMPLE
int _getFakeSample(struct ts_sample *samp, int nr)
{
	_initSample(samp, nr);

	printf("tp_getXY::cnt=%x\n",g_tpCntr);

	if(g_tpCntr++>0x100)
	{
		if( !(g_tpCntr&0x07) )
		{
			unsigned int i;
			i = (g_tpCntr>>3)&0x1F;
			if(i<MAX_FAKE_NUM)
			{
				samp->pressure = 1;
				samp->x = gFakeTableX[i];
				samp->y = gFakeTableY[i];
				printf("sendXY.=%d,%d\n",samp->x,samp->y);
			}
		}
	}

	return nr;
}
#endif
/*##################################################################################
 *                           private function above
###################################################################################*/

/*##################################################################################
#                       public function implementation
###################################################################################*/

/**
 * Send touch sample(samp->pressure, samp->x, samp->y, and samp->tv)
 *
 * @param inf: the module information of tslibo(just need to care "inf->dev")
 * @param samp: the touch samples
 * @param nr: the sample count that upper layer wanna get.
 * @return: the total touch sample count
 *
 * [HINT 1]:this function will be called by SDL every 33 ms.
 * [HINT 2]:Upper layer(SDL) will judge finger-down(contact on TP) if samp->pressure>0,
 *          finger-up(no touch) if samp->pressure=0.
 */
static int ft5xxx_read(struct tslib_module_info *inf, struct ts_sample *samp, int nr)
{
	struct tsdev *ts = inf->dev;
	unsigned int regValue;
	int ret;
	int total = 0;
	int tchdev = ts->fd;
	// struct ts_sample *s=samp;

#ifdef	ENABLE_SEND_FAKE_SAMPLE
	return _getFakeSample(samp,nr);
#endif

	if(creat_init_thread == false)
	{
		printf("TP first init(INT is GPIO %d)\n",TP_INT_PIN);
		gTpInfo.tpDevFd = tchdev;
		if(!_tpDoInitial())
			return -1;
	}

	_initSample(samp, nr);

	//to probe touch sample
	ret = _tpProbeSample(samp, nr);

#ifdef	ENABLE_TOUCH_PANEL_DBG_MSG
	if(ret)	printf("	deQue-O:%x=(%d,%d,%d)r=%d\n", 
			samp, samp->pressure, samp->x, samp->y, ret);
#endif

	return nr;
}

static const struct tslib_ops ft5xxx_ops =
{
	ft5xxx_read,
};

TSAPI struct tslib_module_info *castor3_mod_init(struct tsdev *dev, const char *params)
{
	struct tslib_module_info *m;

	m = malloc(sizeof(struct tslib_module_info));
	if (m == NULL)
		return NULL;

	m->ops = &ft5xxx_ops;
	return m;
}

#ifndef TSLIB_STATIC_CASTOR3_MODULE
TSLIB_MODULE_INIT(castor3_mod_init);
#endif
