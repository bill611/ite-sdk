/*
 * =============================================================================
 *
 *       Filename:  rtpObject.c
 *
 *    Description:  tc protocl rtp
 *
 *        Version:  1.0
 *        Created:  2016-11-25 09:39:49
 *       Revision:  none
 *
 *         Author:  xubin
 *        Company:  Taichuan
 *
 * =============================================================================
 */
/* ---------------------------------------------------------------------------*
 *                      include head files
 *----------------------------------------------------------------------------*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>

/* ---------------------------------------------------------------------------*
 *                  extern variables declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                  internal functions declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                        macro define
 *----------------------------------------------------------------------------*/
#if (defined TC3000_BZ)
#define MAXENCODEPACKET (120*1024-16)
#else
#define MAXENCODEPACKET (60*1024-16)
#endif

#define MAX_TIMEOUT_CNT 10

typedef struct _rec_body{
#if (defined TC3000_BZ)
    unsigned int        packet_cnt;     //分包数量
    unsigned int        packet_size;    //分包大小
    unsigned int        packet_idx;     //包索引
    unsigned int        alen;           //audio长度
    unsigned int        atype;
    unsigned int        tlen;           //数据长度
    unsigned int        dead;
    unsigned int        seq;            //帧序号
    unsigned int        slen;           //第一帧长度
    unsigned int        vtype;          //第一帧类型
    unsigned int        checksum;       // 校验和
    unsigned char       sdata[MAXENCODEPACKET]; //帧数据
#else
    unsigned int        packet_cnt;     //分包数量
    unsigned short      packet_size;    //分包大小
    unsigned short      packet_idx;     //包索引
    unsigned short      alen;           //audio长度
    unsigned short      atype;
    unsigned short      tlen;           //数据长度
    unsigned short      dead;
    unsigned int        seq;            //帧序号
    unsigned short      slen;           //第一帧长度
  	unsigned short      vtype;          //第一帧类型
    unsigned int        checksum;       // 校验和
    unsigned char       sdata[MAXENCODEPACKET]; //帧数据
#endif
}rec_body;

#define RECBODYSIZE ((unsigned int)sizeof(rec_body))
#define RECHEADSIZE ((unsigned int)(sizeof(rec_body)-MAXENCODEPACKET))

#if (defined TC3000_BZ)
#define MAXMTU (1400)  //xb modify 20150821
#else
#define MAXMTU (512)
#endif

/* ---------------------------------------------------------------------------*
 *                      variables define
 *----------------------------------------------------------------------------*/
static rec_body audio_data;
static rec_body heart_data;

static int tcRtpSendBuffer(int socket,
		void *pBuf,
		int size,
		struct sockaddr_in *addr,
		int addr_len)
{
	int i,Cnt,Pos,Len;
	// int PeerPort = This->PeerPort;
	char *pData = (char*)pBuf;

	Cnt = (size - RECHEADSIZE + MAXMTU - 1) / MAXMTU;
	if(Cnt==0) {
		Cnt = 1;
	}

	rec_body *pbody = (rec_body*)pBuf;
	pbody->packet_cnt = Cnt;
	pbody->packet_size = MAXMTU;
	pbody->dead = 0;			//不通过服务器转发
	Pos = RECHEADSIZE;
	for(i=0;i<Cnt;i++) {
		pbody->packet_idx = i;
		if(i) 
			memcpy(&pData[Pos-RECHEADSIZE],pData,RECHEADSIZE);
		
		//发送数据长度
		Len = ((size-Pos)>MAXMTU?MAXMTU:(size-Pos))+RECHEADSIZE;
		sendto(socket, (unsigned char *)&pData[Pos-RECHEADSIZE],
				Len, 0, addr, addr_len);
		Pos+=MAXMTU;
		//printf("cPeerIP=%s, PeerPort=%d\n", This->cPeerIP,PeerPort);
	}
	return size;
}

unsigned char * tcRtpInitRecBuffer(void)
{
	unsigned char *pBuf;
	pBuf = (unsigned char *)malloc(sizeof(unsigned char) * RECBODYSIZE);
	return pBuf;
}

void tcRtpDeInitRecBuffer(unsigned char **pBuf)
{
	if (*pBuf)
		free(*pBuf);
}

int tcRtpRecvVideoAudioBuffer(int socket,void *pBuf)
{
	int i,j,Pos,Len;
	unsigned int Cnt;
    unsigned int PackID;
	char cTmpBuf[MAXMTU+RECHEADSIZE],*pData;
	rec_body *pbody = (rec_body*)pBuf;
	char sockaddr[64];
	unsigned int RecvIP;
	int Size = sizeof(sockaddr);
	pData = (char*)pBuf;
	int PackSize = 0;
	static int RecvTimeOut = 0;

	struct sockaddr_in remote_addr;
	int len = sizeof(struct sockaddr_in);

	//如果IP不对，丢弃该包
	for(i=0;i<MAX_TIMEOUT_CNT;i++) {
		// unsigned int PeerIP;
		// Pos = This->udp->RecvBuffer(This->udp,pBuf,size,TimeOut,sockaddr,&Size);
		Pos = recvfrom(socket,pBuf,RECBODYSIZE,0,(struct sockaddr*)&remote_addr,&len);
		if(Pos<=0) {
			//接收错误，由下面过程处理
			break;
		}
		// RecvIP = GetIPBySockAddr(sockaddr);
		// if(PeerIP==This->dwPeerIP)
		// {
			// //if((PeerIP==This->dwPeerIP) || (PeerIP==This->dwMasterDevIP)) {
			// //已经接收到直传的包
			// //This->bRecvLocalPacket = TRUE;
			// RecvIP = PeerIP;
			break;
		// }
	}
	if(i==MAX_TIMEOUT_CNT) {
		return 0;
	}
	//接收错误处理
	if(Pos<RECHEADSIZE) {
		if(Pos<=0) {
			RecvTimeOut++;
			if (RecvTimeOut > 1) {
				printf("[1]Rtp recv data timeout %d\n",RecvTimeOut);
			}
			if(RecvTimeOut<MAX_TIMEOUT_CNT)
				return 0;
			return -1;
		} else {
			return 0;
		}
	}
	RecvTimeOut=0;
	//首包索引必须为0
	if(pbody->packet_idx!=0) {
		return 0;
	}

	//保存有多少个数据包
	Cnt = pbody->packet_cnt;
	PackID = pbody->seq;
	PackSize = pbody->packet_size;
	if(Cnt==1) {
		return Pos;
	}
	//接收剩余的分包
	pbody = (rec_body*)cTmpBuf;
	for(i=1;i<Cnt;i++) {
		//如果IP不对，丢弃该包
		for(j=0;j<MAX_TIMEOUT_CNT;j++) {
			// Len = This->udp->RecvBuffer(This->udp,cTmpBuf,MAXMTU+RECHEADSIZE,TimeOut,sockaddr,&Size);
			Len = recvfrom(socket,cTmpBuf,MAXMTU+RECHEADSIZE,0,(struct sockaddr*)&remote_addr,&len);
			if(Len<=0) {
				break;
			}
			// unsigned int RecvIP2 = GetIPBySockAddr(sockaddr);
			// if(RecvIP2==RecvIP) {
				break;
			// }
		}
		if(j==MAX_TIMEOUT_CNT)
			Len = 0;

		if(Len<=0) {
			RecvTimeOut++;
			printf("[2]Rtp recv data timeout %d\n",RecvTimeOut);
			if(RecvTimeOut<MAX_TIMEOUT_CNT)
				return 0;
			return -1;
		}

		if(pbody->seq!=PackID) {				//帧号错误
			return 0;
		}
		if(pbody->packet_idx>=Cnt) {			//分包索引错误
			return 0;
		}

		if(Len>RECHEADSIZE)
			memcpy(&pData[RECHEADSIZE+pbody->packet_idx*MAXMTU],&cTmpBuf[RECHEADSIZE],Len-RECHEADSIZE);
		Pos+=Len-RECHEADSIZE;
	}
	return Pos;
}

int tcRtpSendVideo(int socket,unsigned char *buf,unsigned int size)
{
	
}
void tcRtpSendAudio(int socket,
		unsigned char *buf,
		unsigned int size,
		struct sockaddr_in *addr,
		int addr_len)
{
	audio_data.seq++;
	audio_data.vtype = 0;
	audio_data.slen = 0;
	audio_data.alen = size;
	memcpy(audio_data.sdata,buf,size);
	audio_data.tlen = RECHEADSIZE + size;
	tcRtpSendBuffer(socket,
		   	(unsigned char *)&audio_data,
		   	audio_data.tlen,
		   	addr,
		   	addr_len);
}
void tcRtpSendHeart(int socket,
		struct sockaddr_in *addr,
		int addr_len)
{
	heart_data.seq = 0;
	heart_data.vtype = 0;
	heart_data.slen = 0;
	heart_data.alen = 0;
	heart_data.tlen = RECHEADSIZE + 32;
	tcRtpSendBuffer(socket,
		   	(unsigned char *)&audio_data,
		   	audio_data.tlen,
		   	addr,
		   	addr_len);
}
int tcRtpGetAudioReady(void *buf)
{
	rec_body *pbody = (rec_body*)buf;
	return pbody->alen ? 1 : 0;
}
int tcRtpGetVideoReady(void *buf)
{
	rec_body *pbody = (rec_body*)buf;
	return pbody->slen ? 1 : 0;
}
unsigned char * tcRtpGetVideoData(void *buf)
{
	rec_body *pbody = (rec_body*)buf;
	return pbody->sdata;
}

unsigned char * tcRtpGetAudioData(void *buf)
{
	return tcRtpGetVideoData(buf);
}
unsigned int  tcRtpGetVideoLen(void *buf)
{
	rec_body *pbody = (rec_body*)buf;
	return pbody->slen;
}
unsigned int  tcRtpGetAudioLen(void *buf)
{
	rec_body *pbody = (rec_body*)buf;
	return pbody->alen;
}
unsigned int  tcRtpGetHeadLen(void)
{
	return RECHEADSIZE;
}
