/*
  This file is part of iic_sw
  
  Information here.
*/
/**
 * @file iic.c
 * @brief  
 * @author
 * @author
 */

/********************************************
 * Include Header 
 ********************************************/
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <string.h>
#include "mmp_iic.h"
#include "iic_sw.h"

/********************************************
 * MACRO defination
 ********************************************/
#define IF_ERROR_GOTO_END(result)	{ if (result == IIC_RESULT_ERROR) { IIC_ERROR_MSG(" at %s[%d]\n", __func__, __LINE__); goto end; } }
 
 /********************************************
 * Definition
 ********************************************/


/********************************************
 * Global variable
 ********************************************/
static const uint8_t IicIo[] = { CFG_SW_I2C_GPIO };	//[0]: SCLK, [1]: SDA
static pthread_mutex_t IicExternalMutex  = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t IicInternalMutex  = PTHREAD_MUTEX_INITIALIZER;

/********************************************
 * Prinvate function 
 ********************************************/
/**
 * Function Brief Here
 *
 * @param XXX XXX
 * @return XXX
 */


/********************************************
 * Public function 
 ********************************************/
/**
 * Function Brief Here
 *
 * @param XXX XXX
 * @return XXX
 */
uint32_t
mmpIicInitialize(
    uint32_t delay,
    NOR_BOOL bModuleLock)
{
	IIC_RESULT iicResult;
	
	pthread_mutex_lock(&IicInternalMutex);
	iicResult = IIC_Initial(IicIo[0], IicIo[1], delay);
	pthread_mutex_unlock(&IicInternalMutex);
	
	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

uint32_t
mmpIicTerminate()
{
	IIC_RESULT iicResult;
	
	pthread_mutex_lock(&IicInternalMutex);
	iicResult = IIC_Terminate();
	pthread_mutex_unlock(&IicInternalMutex);
	
	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

uint32_t
mmpIicStart()
{
	IIC_RESULT iicResult;
	
	pthread_mutex_lock(&IicInternalMutex);
	iicResult = IIC_Start();
	pthread_mutex_unlock(&IicInternalMutex);

	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

uint32_t
mmpIicStop()
{
	IIC_RESULT iicResult;
	
	pthread_mutex_lock(&IicInternalMutex);
	iicResult = IIC_Stop();
	pthread_mutex_unlock(&IicInternalMutex);

	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

uint32_t
mmpIicRecieve(
    uint32_t* data)
{
	IIC_RESULT iicResult;
	
	pthread_mutex_lock(&IicInternalMutex);
	iicResult = IIC_RecieveByte((uint8_t*)data, true);
	pthread_mutex_unlock(&IicInternalMutex);

	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

uint32_t
mmpIicSend(
    uint32_t data)
{
	IIC_RESULT iicResult;
	
	pthread_mutex_lock(&IicInternalMutex);
	iicResult = IIC_SendByte((uint8_t)data);
	pthread_mutex_unlock(&IicInternalMutex);

	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

uint32_t
mmpIicSendData(
    IIC_OP_MODE mode,
    uint8_t		slaveAddr,
	uint8_t		regAddr,
    uint8_t*	pbuffer,
    uint16_t	size)
{
	IIC_RESULT iicResult        = IIC_RESULT_OK;
	uint8_t*   sendBuffer       = NULL;
	uint32_t   sendBufferLength = size + 2;

	pthread_mutex_lock(&IicInternalMutex);
	sendBuffer = (uint8_t*)malloc(sendBufferLength);
	if ( sendBuffer != NULL )
	{
		uint32_t i = 0;
		
		sendBuffer[0] = slaveAddr << 1;
		sendBuffer[1] = regAddr;
		memcpy(sendBuffer + 2, pbuffer, size);
		
		IIC_Start();
		for ( i = 0; i < sendBufferLength; i++ )
		{
			iicResult = IIC_SendByte(sendBuffer[i]);
			if ( iicResult == IIC_RESULT_ERROR )
			{
				goto end;
			}
		}
		IIC_Stop();
		
		free(sendBuffer);
		sendBuffer = NULL;
	}

end:
	if ( sendBuffer )
	{
		free(sendBuffer);
		sendBuffer = NULL;
	}
	pthread_mutex_unlock(&IicInternalMutex);
	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

uint32_t
mmpIicSendDataEx(
    IIC_OP_MODE	mode,
    uint8_t		slaveAddr,
    uint8_t*	pbuffer,
    uint16_t	size)
{
	IIC_RESULT iicResult = IIC_RESULT_OK;
	uint32_t   i         = 0;
	
	pthread_mutex_lock(&IicInternalMutex);
	IIC_Start();
	iicResult = IIC_SendByte(slaveAddr << 1);
	if ( iicResult == IIC_RESULT_ERROR )
	{
		goto end;
	}
	for ( i = 0; i < size; i++ )
	{
		iicResult = IIC_SendByte(pbuffer[i]);
		if ( iicResult == IIC_RESULT_ERROR )
		{
			goto end;
		}
	}
	IIC_Stop();

end:
	pthread_mutex_unlock(&IicInternalMutex);
	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

uint32_t
mmpIicSendDataTest(
    IIC_OP_MODE	mode,
    uint8_t		slaveAddr,
    uint8_t*	pbuffer,
    uint16_t	size)
{
	IIC_RESULT iicResult = IIC_RESULT_OK;
	uint32_t   i         = 0;
	
	pthread_mutex_lock(&IicInternalMutex);
	IIC_Start();
	printf("addr\n");
	iicResult = IIC_SendByte(slaveAddr << 1);
	if ( iicResult == IIC_RESULT_ERROR )
	{
		printf("%s[%d]\n", __FILE__, __LINE__);
		goto end;
	}
#if 1
	printf("data\n");
	for ( i = 0; i < size; i++ )
	{
		iicResult = IIC_SendByte(pbuffer[i]);
		if ( iicResult == IIC_RESULT_ERROR )
		{
			printf("%s[%d]\n", __FILE__, __LINE__);
			goto end;
		}
	}
#endif
	IIC_Stop();

end:
	pthread_mutex_unlock(&IicInternalMutex);
	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

uint32_t
mmpIicReceiveData(
    IIC_OP_MODE mode,
    uint8_t		slaveAddr,
	uint8_t		regAddr,
    uint8_t*	pbuffer,
    uint16_t	size)
{
	IIC_RESULT iicResult = IIC_RESULT_OK;
	uint32_t   i         = 0;

	pthread_mutex_lock(&IicInternalMutex);
	IIC_Start();
	iicResult = IIC_SendByte(slaveAddr << 1);	IF_ERROR_GOTO_END(iicResult);
	iicResult = IIC_SendByte(regAddr);			IF_ERROR_GOTO_END(iicResult);
	
	IIC_Start();
	iicResult = IIC_SendByte((slaveAddr << 1) | 1);	IF_ERROR_GOTO_END(iicResult);

    if (size > 1)
    {
        for ( i = 0; i < (size - 1); i++ )
        {
            uint8_t receiveData = 0;
            
            /*
            if ( i == (size - 1) )
            {
                iicResult = IIC_RecieveByte(&receiveData, true);	IF_ERROR_GOTO_END(iicResult);
            }
            else
            {
                iicResult = IIC_RecieveByte(&receiveData, true);	IF_ERROR_GOTO_END(iicResult);
            }
            */
            iicResult = IIC_RecieveByte(&receiveData, true);	IF_ERROR_GOTO_END(iicResult);
            pbuffer[i] = receiveData;
        }
    }

    // The last read data send NACK instead of ACK
    iicResult = IIC_RecieveByte(&receiveData, false);	IF_ERROR_GOTO_END(iicResult);
    pbuffer[i] = receiveData;

	IIC_Stop();
	
end:
	pthread_mutex_unlock(&IicInternalMutex);
	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

uint32_t
mmpIicReceiveDataEx(
    IIC_OP_MODE	mode,
    uint8_t		slaveAddr,
    uint8_t*	pwbuffer,
    uint16_t	wsize,   
    uint8_t*	prbuffer,
    uint16_t	rsize)
{
	IIC_RESULT iicResult = IIC_RESULT_OK;
	uint32_t   i         = 0;

	pthread_mutex_lock(&IicInternalMutex);
	IIC_Start();
	iicResult = IIC_SendByte(slaveAddr << 1);	IF_ERROR_GOTO_END(iicResult);
	
	for ( i = 0; i < wsize; i++ )
	{
		iicResult = IIC_SendByte(pwbuffer[i]);	IF_ERROR_GOTO_END(iicResult);
	}
	
	IIC_Start();
	iicResult = IIC_SendByte((slaveAddr << 1) | 1);	IF_ERROR_GOTO_END(iicResult);

    if (rsize > 1)
    {
        for ( i = 0; i < (rsize - 1); i++ )
        {
            uint8_t receiveData = 0;
            
            /*
            if ( i == (rsize - 1) )
            {
                iicResult = IIC_RecieveByte(&receiveData, true);	IF_ERROR_GOTO_END(iicResult);
            }
            else
            {
                iicResult = IIC_RecieveByte(&receiveData, true);	IF_ERROR_GOTO_END(iicResult);
            }
            */
            iicResult = IIC_RecieveByte(&receiveData, true);	IF_ERROR_GOTO_END(iicResult);
            prbuffer[i] = receiveData;
        }
    }

    // The last read data send NACK instead of ACK
    iicResult = IIC_RecieveByte(&receiveData, false);	IF_ERROR_GOTO_END(iicResult);
    pbuffer[i] = receiveData;

	IIC_Stop();
end:
	pthread_mutex_unlock(&IicInternalMutex);
	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}


uint32_t
mmpIicSetClockRate(
    uint32_t clock)
{
	IIC_ERROR_MSG("Not support %s()!\n", __func__);
	
    return 0;
}    
    
uint32_t
mmpIicGetClockRate(
    void)
{
	IIC_ERROR_MSG("Not support %s()!\n", __func__);
	
    return 0;
}

uint32_t
mmpIicGenStop(void)
{
	IIC_RESULT iicResult;
	
	pthread_mutex_lock(&IicInternalMutex);
	iicResult = IIC_Stop();
	pthread_mutex_unlock(&IicInternalMutex);

	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}

void
mmpIicLockModule(
    void)
{
	pthread_mutex_lock(&IicExternalMutex);
}

void
mmpIicReleaseModule(
    void)
{
	pthread_mutex_unlock(&IicExternalMutex);
}

void
mmpIicSetSlaveMode(
	uint32_t slaveAddr)
{
	IIC_ERROR_MSG("Not support %s()!\n", __func__);
}

uint32_t
mmpIicSlaveRead(
	uint32_t slaveAddr,
	uint8_t* inutBuffer,
	uint32_t inputBufferLength)
{
	IIC_ERROR_MSG("Not support %s()!\n", __func__);
	
    return 0;
}

uint32_t
mmpIicSlaveReadWithCallback(
	uint32_t           slaveAddr,
	IicReceiveCallback receiveCallBack,
	IicWriteCallback   writeCallback)
{
	IIC_ERROR_MSG("Not support %s()!\n", __func__);
	
    return 0;
}

uint32_t
mmpIicSlaveWrite(
	uint32_t slaveAddr,
	uint8_t* outputBuffer,
	uint32_t outputBufferLength)
{
	IIC_ERROR_MSG("Not support %s()!\n", __func__);
	
    return 0;
}

uint32_t
mmpIicReceiveDataRetry(
    IIC_OP_MODE mode,
    uint8_t		slaveAddr,
	uint8_t		regAddr,
    uint8_t*	pbuffer,
    uint16_t	size,
    uint16_t    retryCount)
{
	IIC_RESULT  iicResult = IIC_RESULT_OK;
	uint32_t    i = 0;
    uint16_t    tryBoundary = 0;
    uint16_t    tryCount = 0;
    uint8_t     receiveData = 0;

    if (retryCount)
    {
        tryBoundary = retryCount;
    }
    else
    {
        tryBoundary = 1;
    }

	pthread_mutex_lock(&IicInternalMutex);

	IIC_Start();

    tryCount = tryBoundary;
    while (tryCount)
    {
        iicResult = IIC_SendByte(slaveAddr << 1);
        if (iicResult == IIC_RESULT_OK)
        {
            break;
        }
        else
        {
            usleep(500);
        }
        tryCount--;
    }

    if (iicResult != IIC_RESULT_OK)
    {
        IF_ERROR_GOTO_END(iicResult);
    }

    tryCount = tryBoundary;
    while (tryCount)
    {
        iicResult = IIC_SendByte(regAddr);
        if (iicResult == IIC_RESULT_OK)
        {
            break;
        }
        else
        {
            usleep(500);
        }
        tryCount--;
    }

    if (iicResult != IIC_RESULT_OK)
    {
        IF_ERROR_GOTO_END(iicResult);
    }

	IIC_Start();

    tryCount = tryBoundary;
    while (tryCount)
    {
        iicResult = IIC_SendByte((slaveAddr << 1) | 1);
        if (iicResult == IIC_RESULT_OK)
        {
            break;
        }
        else
        {
            usleep(500);
        }
        tryCount--;
    }

    if (iicResult != IIC_RESULT_OK)
    {
        IF_ERROR_GOTO_END(iicResult);
    }

    if (size > 1)
    {
        for ( i = 0; i < (size - 1); i++ )
        {
            tryCount = tryBoundary;
            while (tryCount)
            {
                iicResult = IIC_RecieveByte(&receiveData, true);
                if (iicResult == IIC_RESULT_OK)
                {
                    break;
                }
                else
                {
                    usleep(500);
                }
                tryCount--;
            }

            if (iicResult != IIC_RESULT_OK)
            {
                IF_ERROR_GOTO_END(iicResult);
            }
            pbuffer[i] = receiveData;
        }
    }

    // The last read data send NACK instead of ACK
    tryCount = tryBoundary;
    while (tryCount)
    {
        iicResult = IIC_RecieveByte(&receiveData, false);
        if (iicResult == IIC_RESULT_OK)
        {
            break;
        }
        else
        {
            usleep(500);
        }
        tryCount--;
    }

    if (iicResult != IIC_RESULT_OK)
    {
        IF_ERROR_GOTO_END(iicResult);
    }
    pbuffer[i] = receiveData;
	
end:
	IIC_Stop();
	pthread_mutex_unlock(&IicInternalMutex);
	return (iicResult == IIC_RESULT_OK) ? 0 : 1;
}