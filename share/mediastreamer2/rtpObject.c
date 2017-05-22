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

#ifdef TC3000_Q1
#include "q1_mh/config.h"
#include "q1_mh/tc_protocol/public.h"
#endif
#ifdef TC3000_Z1
#include "z1_mh/config.h"
#include "z1_mh/tc_protocol/public.h"
#endif
#ifdef TC3000_T1
#include "t1_mh/config.h"
#include "t1_mh/tc_protocol/public.h"
#endif
#ifdef TC3000_18D1
#include "18d1_mh/config.h"
#include "18d1_mh/tc_protocol/public.h"
#endif
#ifdef TC3000_X1
#include "x1_mh/config.h"
#include "x1_mh/tc_protocol/public.h"
#endif
/* ---------------------------------------------------------------------------*
 *                  extern variables declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                  internal functions declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                        macro define
 *----------------------------------------------------------------------------*/
// #define TC3000_BZ
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

static int udpReciveBuffer(int socket,
		void *pBuf,
		size_t len,
		struct sockaddr *src_addr,
	   	socklen_t *addrlen,
		int time_out)
{
	struct timeval timeout;
	fd_set fdR;

	if(socket < 0)
		return -1;
    if(time_out < 0)
    	return recvfrom(socket,(char*)pBuf,len,0,src_addr,addrlen);
    else {
		FD_ZERO(&fdR);
		FD_SET(socket, &fdR);
        timeout.tv_sec = time_out / 1000;
        timeout.tv_usec = (time_out % 1000)<<10;
        if(select(socket+1, &fdR,NULL, NULL, &timeout)<=0 || !FD_ISSET(socket,&fdR))
            return -1;
    	return recvfrom(socket,(char*)pBuf,len,0,src_addr,addrlen);
    }
}
static int udpSendBuffer(int socket,const char *IP,int port,const void *pBuf,int size)
{
	struct sockaddr_in *p;
	struct sockaddr addr;
	struct hostent *hostaddr;
	if(socket < 0)
		return -1;

	if(IP[0]==0 || port==0)
		return -2;

	memset(&addr,0,sizeof(addr));
	p=(struct sockaddr_in *)&addr;
	p->sin_family=AF_INET;
	p->sin_port=htons(port);
	p->sin_addr.s_addr = inet_addr(IP);
	if((p->sin_addr.s_addr == INADDR_NONE) && (strcmp(IP,"255.255.255.255")!=0)) {
		printf("IP Address %s Error\n",IP);
		return -4;
	}
	return sendto(socket,(char*)pBuf,size,0,&addr,sizeof(struct sockaddr_in));
}
int tcRtpRecvVideoAudioBuffer(int socket,char *ip,void *pBuf,int time_out)
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
		Pos = udpReciveBuffer(socket,pBuf,RECBODYSIZE,(struct sockaddr*)&remote_addr,&len,time_out);
		//接收错误，由下面过程处理
		if(Pos<=0)
			break;

		RecvIP = remote_addr.sin_addr.s_addr;
		// 转发分机发的数据到门口机
		if(theConfig.master_ip[0] == 0) {
			int k;
			for(k=0; k<MAXFJCOUNT; k++) {
				if(Public.RegDev[k].bTalk && (Public.RegDev[k].dwIP == RecvIP)) {
					udpSendBuffer(socket,ip,8800,pBuf,Pos);
					break;
				}
			}
		}
		if (RecvIP == inet_addr(ip))
			break;
	}
	if(i == MAX_TIMEOUT_CNT)
		return 0;

	//接收错误处理
	if(Pos<RECHEADSIZE) {
		if(Pos<=0) {
			RecvTimeOut++;
			printf("[1]Rtp recv data timeout %d\n",RecvTimeOut);
			if(RecvTimeOut < MAX_TIMEOUT_CNT)
				return 0;
			return -1;
		} else {
			return 0;
		}
	}

	RecvTimeOut = 0;
	//首包索引必须为0
	if(pbody->packet_idx != 0)
		return 0;

	//转发门口机发的数据到分机
	int k;
	for(k=0; k<MAXFJCOUNT; k++) {
		if(Public.RegDev[k].bCalling)
			udpSendBuffer(socket,Public.RegDev[k].IP,8800,pBuf,Pos);

	}

	//保存有多少个数据包
	Cnt = pbody->packet_cnt;
	PackID = pbody->seq;
	PackSize = pbody->packet_size;
	if(Cnt == 1)
		return Pos;

	//接收剩余的分包
	pbody = (rec_body*)cTmpBuf;
	for(i=1;i<Cnt;i++) {
		//如果IP不对，丢弃该包
		for(j=0;j<MAX_TIMEOUT_CNT;j++) {
			Len = udpReciveBuffer(socket,cTmpBuf,MAXMTU+RECHEADSIZE,(struct sockaddr*)&remote_addr,&len,time_out);
			if(Len <= 0)
				break;

			unsigned int RecvIP2 = remote_addr.sin_addr.s_addr;
			// 转发分机的数据到门口机
			if(theConfig.master_ip[0] == 0) {
				int k;
				for(k=0; k<MAXFJCOUNT; k++) {
					if(Public.RegDev[k].bTalk && (Public.RegDev[k].dwIP == RecvIP2)) {
						udpSendBuffer(socket,ip,8800,cTmpBuf,Len);
						break;
					}
				}
			}
			if(RecvIP2 == RecvIP)
				break;

		}
		if(j == MAX_TIMEOUT_CNT)
			Len = 0;

		if(Len <= 0) {
			RecvTimeOut++;
			printf("[2]Rtp recv data timeout %d\n",RecvTimeOut);
			if(RecvTimeOut<MAX_TIMEOUT_CNT)
				return 0;
			return -1;
		}

		if(pbody->seq != PackID) 				//帧号错误
			return 0;

		if(pbody->packet_idx >= Cnt) 			//分包索引错误
			return 0;


		// 转发门口机的数据到分机
		int k;
		for(k=0; k<MAXFJCOUNT; k++) {
			if(Public.RegDev[k].bCalling)
				udpSendBuffer(socket,Public.RegDev[k].IP,8800,cTmpBuf,Len);

		}
		if(Len > RECHEADSIZE)
			memcpy(&pData[RECHEADSIZE+pbody->packet_idx*MAXMTU],&cTmpBuf[RECHEADSIZE],Len-RECHEADSIZE);
		Pos += Len-RECHEADSIZE;
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
