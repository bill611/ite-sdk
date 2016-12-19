/*
 * Copyright (c) 2011 ITE Tech. Inc. All Rights Reserved.
 */
/** @file
 * PAL NAND functions.
 *
 * @author Jim Tan
 * @version 1.0
 *
 *
 *
 */
#include <errno.h>
#include "itp_cfg.h"
#include "ite/ite_nand.h"
#ifdef	CFG_SPI_NAND
#include "ssp/mmp_spi.h"

#ifdef	CFG_SPI_NAND_USE_SPI0
#define NAND_SPI_PORT	SPI_0
#endif

#ifdef	CFG_SPI_NAND_USE_SPI1
#define NAND_SPI_PORT	SPI_1
#endif

#ifndef NAND_SPI_PORT
    #error "ERROR: itp_nand.c MUST define the SPI NAND bus(SPI0 or SPI1)"
#endif

#endif
/********************
 * global variable *
 ********************/
static ITE_NF_INFO	itpNfInfo;
static uint8_t GoodBlkCodeMark[4]={'F','I', 'N','E'};

/********************
 * private function *
 ********************/
static int FlushBootRom()
{
    char *BlkBuf=NULL;
    int  res=true;
	uint32_t  i,j;
	uint32_t  BakBlkNum;
    
    if(!itpNfInfo.BootRomSize)
    {
    	errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | 1;	//nand initial fail
    	LOG_ERR "nand block initial error: \n" LOG_END
    	goto end;
    }
    
    //allocate block buffer
    BlkBuf = malloc( itpNfInfo.PageInBlk * (itpNfInfo.PageSize + itpNfInfo.SprSize) );
    if(!BlkBuf)
    {
    	errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | 1;	//nand initial fail
    	LOG_ERR "nand block initial error: \n" LOG_END
    	goto end;
    }
    
    //calculate block number of boot ROM
	BakBlkNum = 0;
    
    for(i=0; i<BakBlkNum; i++)
    {
    	char *tmpbuf=BlkBuf;
    	
    	//read a block data from bak area(include spare data)
    	for(j=0; j<itpNfInfo.PageInBlk;j++)
    	{
    		if( iteNfRead(i, j, tmpbuf)!=true )				
    		{
    			errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | 1;
    			//LOG_ERR "nand block initial error: \n" LOG_END
    			goto end;
    		}
    		tmpbuf+=(itpNfInfo.PageSize + itpNfInfo.SprSize);
    	}
    	
    	//write to boot area a block size
    	tmpbuf=BlkBuf;
    	for(j=0;j<itpNfInfo.PageInBlk;j++)
    	{
    		if(iteNfWrite(i, j, tmpbuf, tmpbuf+itpNfInfo.SprSize)!=true )
    		{
    			errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | 1;	//nand initial fail
    			LOG_ERR "nand block initial error: \n" LOG_END
    			goto end;
    		}
    		tmpbuf+=(itpNfInfo.PageSize + itpNfInfo.SprSize);
    	}    	
    }
     
end:
	/*
	if(SprBuf)
	{
		free(SprBuf);
		SprBuf = NULL;
	}		
	*/
	return 	res;
}

/********************
 * public function *
 ********************/
static int NandOpen(const char* name, int flags, int mode, void* info)
{
    // TODO: IMPLEMENT
    //switch(flags)
    //to get nand attribution
    //iteNfGetInfo(&itpNfInfo);
    
    //itpNfInfo.NumOfBlk   = 0;
    //itpNfInfo.PageSize   = 0;
    //itpNfInfo.PageInBlk  = 0;    
    //itpNfInfo.SprSize    = 0;
    
    //errno = ENOENT;
    return 0;
}

static int NandClose(int file, void* info)
{
	//itpNfInfo.NumOfBlk = 0;
    //errno = ENOENT;
    //errno = (ITP_DEVICE_FAT << ITP_DEVICE_ERRNO_BIT) | ret;
    return 0;
}

static int NandRead(int file, char *ptr, int len, void* info)
{
    // TODO: IMPLEMENT
    int  DoneLen = 0;
	uint32_t  blk = 0;
	uint32_t  pg = 0;
    char *databuf;
	char *tmpbuf;
	char *srcbuf=ptr;
	
    if( !ptr || !len )
    {
    	errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//ptr or len error
    	LOG_ERR "ptr or len error: \n" LOG_END
    	goto end;
    }
    
    if(!itpNfInfo.NumOfBlk)
    {
    	errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//nand initial fail
    	LOG_ERR "nand block initial error: \n" LOG_END
    	goto end;
    }

    databuf = (char *)malloc(itpNfInfo.PageInBlk*itpNfInfo.PageSize);
    if(databuf==NULL)
    {
    	errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//out of memory:
        LOG_ERR "out of memory:\n" LOG_END
    	goto end;  	
    }
	

   	for(blk=itpNfInfo.CurBlk; blk<(itpNfInfo.CurBlk+len); blk++)
   	{
		tmpbuf = databuf;
   		for( pg=0; pg<itpNfInfo.PageInBlk; pg++)
   		{ 
   			if( iteNfReadPart(blk, pg, tmpbuf, LL_RP_DATA)!=true )
   			{
   				errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//read fail
   				return -1;
   			}
   			tmpbuf += itpNfInfo.PageSize;
   		}

        if(blk)
   	{
            memcpy(srcbuf, databuf+4, itpNfInfo.PageInBlk*itpNfInfo.PageSize-4);
            srcbuf+=(itpNfInfo.PageInBlk*itpNfInfo.PageSize)-4;
        }
        else
        {
            memcpy(srcbuf, databuf, itpNfInfo.PageInBlk*itpNfInfo.PageSize);  		
            srcbuf+=(itpNfInfo.PageInBlk*itpNfInfo.PageSize);
        }

        DoneLen++;
   	}
   	itpNfInfo.CurBlk += DoneLen;
    
end:
	if(databuf)
	{
		free(databuf);
		databuf = NULL;
	}	
    return DoneLen;
}

static int NandWrite(int file, char *ptr, int len, void* info)
{
    int  DoneLen = 0;
    char *DataBuf;
	char *tmpDataBuf;
	char *tmpSrcBuf = ptr;
    char *SprBuf= NULL;
    uint32_t blk,pg;
    
    // TODO: IMPLEMENT
    
    if( !ptr || !len )
    {
    	errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//ptr or len error
    	LOG_ERR "ptr or len error: \n" LOG_END
    	goto end;
    }
    
    if(!itpNfInfo.NumOfBlk)
    {
    	errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//nand initial fail
    	LOG_ERR "nand block initial error: \n" LOG_END
    	goto end;
    }

    DataBuf = (char *)malloc(itpNfInfo.PageInBlk*itpNfInfo.PageSize);
    if(DataBuf==NULL)
    {
    	errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//out of memory:
        LOG_ERR "out of memory:\n" LOG_END
    	goto end;  	
    }

    if(!itpNfInfo.SprSize)
    {
    	errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//nand initial fail
        LOG_ERR "nand spare initial error: \n" LOG_END
    	goto end;
    }
    
    SprBuf = (char *)malloc(itpNfInfo.SprSize);
    if(SprBuf==NULL)
    {
    	errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//out of memory:
        LOG_ERR "out of memory:\n" LOG_END
    	goto end;  	
    }

   	for(blk=itpNfInfo.CurBlk; blk<itpNfInfo.CurBlk+len; blk++)
   	{
   		//erase current block
   		if( iteNfErase(blk)!=true )
   		{
   			errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//erase fail
   			LOG_ERR "erase block fail: %d\n", blk LOG_END
   			DoneLen = 0;
   			goto end;
   		}

        if(blk)
        {
            memcpy(DataBuf, GoodBlkCodeMark, 4);
            memcpy(DataBuf+4, tmpSrcBuf, itpNfInfo.PageInBlk*itpNfInfo.PageSize - 4);
        }
        else
        {
            memcpy(DataBuf, tmpSrcBuf, itpNfInfo.PageInBlk*itpNfInfo.PageSize);
        }
        tmpDataBuf = DataBuf;
   		
    	for( pg=0; pg<itpNfInfo.PageInBlk; pg++)
    	{
    		//SetSprBuff(blk, pg, SprBuf);
    		if( iteNfWrite(blk, pg, tmpDataBuf, SprBuf)!=true )
    		{
   				errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | __LINE__;	//erase fail
   				LOG_ERR "write fail:[%d,%d]\n", blk, pg LOG_END
   				DoneLen = 0;
   				goto end;
    		}
			tmpDataBuf+=itpNfInfo.PageSize;
    	}
        DoneLen++;

        if(blk)	tmpSrcBuf += itpNfInfo.PageInBlk*itpNfInfo.PageSize - 4;
        else	tmpSrcBuf += itpNfInfo.PageInBlk*itpNfInfo.PageSize;		
    }
    itpNfInfo.CurBlk += len;

end:
	if(SprBuf)
	{
		free(SprBuf);
		SprBuf = NULL;
	}		
	if(DataBuf)
	{
		free(DataBuf);
		DataBuf = NULL;
	}	
    return DoneLen;
}

static int NandLseek(int file, int ptr, int dir, void* info)
{
    switch(dir)
    {
    default:
    case SEEK_SET:
        itpNfInfo.CurBlk = ptr;
        break;
    case SEEK_CUR:
        itpNfInfo.CurBlk += ptr;
        break;
    case SEEK_END:
        break;
    }
    return itpNfInfo.CurBlk;
}

static int NandIoctl(int file, unsigned long request, void* ptr, void* info)
{
	int res = -1;
	
    switch (request)
    {
	    case ITP_IOCTL_INIT:
	    	#ifdef	CFG_SPI_NAND
	    	mmpSpiInitialize(NAND_SPI_PORT, SPI_OP_MASTR, CPO_0_CPH_0, SPI_CLK_20M);
	    	#endif

	    	itpNfInfo.BootRomSize = (uint32_t)CFG_NAND_RESERVED_SIZE;
	    	#ifdef	CFG_NOR_SHARE_SPI_NAND
	    	itpNfInfo.enSpiCsCtrl = 1;
	    	#else
	    	itpNfInfo.enSpiCsCtrl = 0;
	    	#endif
	    	
	    	#ifdef	CFG_SPI_NAND_ADDR_HAS_DUMMY_BYTE
	    	LOG_INFO "set AHDB=1\n" LOG_END
	    	itpNfInfo.addrHasDummyByte = 1;
	    	#else
	    	LOG_INFO "set AHDB=0\n" LOG_END
	    	itpNfInfo.addrHasDummyByte = 0;
	    	#endif
	    	
	    	printf("ITP_IOCTL_INIT[01](%x,%x)(%x,%x)\n",&itpNfInfo,itpNfInfo.NumOfBlk,itpNfInfo.BootRomSize,itpNfInfo.enSpiCsCtrl);
	        if( iteNfInitialize(&itpNfInfo)==true )
	        {
	        	printf("ITP_IOCTL_INIT[02](%x,%x)(%x)\n",&itpNfInfo, itpNfInfo.NumOfBlk, itpNfInfo.BootRomSize);
	        	res = 0;
	        }
	        #ifdef	CFG_NOR_SHARE_SPI_NAND
	        mmpSpiSetControlMode(SPI_CONTROL_NOR);
	        mmpSpiResetControl();
	        #endif
	        break;
	
	    case ITP_IOCTL_GET_BLOCK_SIZE:
	        if(itpNfInfo.NumOfBlk)
	        {
	        	*(unsigned long*)ptr = (itpNfInfo.PageInBlk)*(itpNfInfo.PageSize)-4;
	        	res = 0;
	        }
	        break;
	        
	    case ITP_IOCTL_FLUSH:
	        // TODO: IMPLEMENT
	        if(FlushBootRom()==true)
	        {
	        	res = 0;
	        }
	        break;

	    case ITP_IOCTL_SCAN:
	    	{
	    		uint32_t *blk=(uint32_t *)ptr;
	        	if(iteNfIsBadBlockForBoot(*blk)==true )
	        	{
	        		//*(unsigned long*)ptr = itpNfInfo.PageInBlk * itpNfInfo.PageSize;
	        		res = 0;
	        	}
	        }
	        break;

        case ITP_IOCTL_GET_GAP_SIZE:
            {
                *(unsigned long*)ptr = 4;
                res = 0;
            }
            break;

	    default:
	        errno = (ITP_DEVICE_NAND << ITP_DEVICE_ERRNO_BIT) | 1;
	        break;
    }
    return res;
}

const ITPDevice itpDeviceNand =
{
    ":nand",
    NandOpen,
    NandClose,
    NandRead,
    NandWrite,
    NandLseek,
    NandIoctl,
    NULL
};
