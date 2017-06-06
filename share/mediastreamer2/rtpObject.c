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

#include "tc_include.h"
/* ---------------------------------------------------------------------------*
 *                  extern variables declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                  internal functions declare
 *----------------------------------------------------------------------------*/

/* ---------------------------------------------------------------------------*
 *                        macro define
 *----------------------------------------------------------------------------*/
typedef struct _RtpObjData {
	unsigned int max_encode_packet;
	unsigned int body_size;
	unsigned int head_size;
	unsigned int max_mtu;
	void *audio_data;
	void *heart_data;
}RtpObjData;

#define MAXENCODEPACKET_BZ (120*1024-16)
#define MAXENCODEPACKET_U9 (60*1024-16)

#define MAXMTUBZ 1400
#define MAXMTUU9 512

#define MAX_TIMEOUT_CNT 10

typedef struct _rec_body_bz{
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
    unsigned char       sdata[MAXENCODEPACKET_BZ]; //帧数据
}rec_body_bz;

typedef struct _rec_body_u9{
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
    unsigned char       sdata[MAXENCODEPACKET_U9]; //帧数据
}rec_body_u9;


/* ---------------------------------------------------------------------------*
 *                      variables define
 *----------------------------------------------------------------------------*/
static rec_body_bz audio_data_bz;
static rec_body_bz heart_data_bz;
static rec_body_u9 audio_data_u9;
static rec_body_u9 heart_data_u9;

static RtpObjData rtp[] = {
	// PROTOCOL_BZ
	{
		.max_encode_packet = MAXENCODEPACKET_BZ,
		.body_size = (unsigned int)sizeof(rec_body_bz),
		.head_size = (unsigned int)sizeof(rec_body_bz) - MAXENCODEPACKET_BZ,
		.max_mtu = MAXMTUBZ,
		.audio_data = &audio_data_bz,
		.heart_data = &heart_data_bz,
	},
	// PROTOCOL_U9
	{
		.max_encode_packet = MAXENCODEPACKET_U9,
		.body_size = (unsigned int)sizeof(rec_body_u9),
		.head_size = (unsigned int)sizeof(rec_body_u9) - MAXENCODEPACKET_U9,
		.max_mtu = MAXMTUU9,
		.audio_data = &audio_data_u9,
		.heart_data = &heart_data_u9,
	}
};

static int tcRtpSendBuffer(int socket,
		void *pBuf,
		int size,
		struct sockaddr_in *addr,
		int addr_len)
{
	int i,Cnt,Pos,Len;
	char *pData = (char*)pBuf;

	Cnt = (size - rtp[theConfig.protocol].head_size + rtp[theConfig.protocol].max_mtu - 1) / rtp[theConfig.protocol].max_mtu;
	if(Cnt==0) {
		Cnt = 1;
	}

	if (theConfig.protocol == PROTOCOL_BZ) {
		rec_body_bz *pbody = (rec_body_bz*)pBuf;
		pbody->packet_cnt = Cnt;
		pbody->packet_size = rtp[theConfig.protocol].max_mtu;
		pbody->dead = 0;			//不通过服务器转发
		Pos = rtp[theConfig.protocol].head_size;
		for(i=0;i<Cnt;i++) {
			pbody->packet_idx = i;
			if(i)
				memcpy(&pData[Pos-rtp[theConfig.protocol].head_size],pData,rtp[theConfig.protocol].head_size);

			//发送数据长度
			Len = ((size-Pos)>rtp[theConfig.protocol].max_mtu?rtp[theConfig.protocol].max_mtu:(size-Pos))+rtp[theConfig.protocol].head_size;
			sendto(socket, (unsigned char *)&pData[Pos-rtp[theConfig.protocol].head_size],
					Len, 0, addr, addr_len);
			Pos+=rtp[theConfig.protocol].max_mtu;
		}
	} else {
		rec_body_u9 *pbody = (rec_body_u9*)pBuf;
		pbody->packet_cnt = Cnt;
		pbody->packet_size = rtp[theConfig.protocol].max_mtu;
		pbody->dead = 0;			//不通过服务器转发
		Pos = rtp[theConfig.protocol].head_size;
		for(i=0;i<Cnt;i++) {
			pbody->packet_idx = i;
			if(i)
				memcpy(&pData[Pos-rtp[theConfig.protocol].head_size],pData,rtp[theConfig.protocol].head_size);

			//发送数据长度
			Len = ((size-Pos)>rtp[theConfig.protocol].max_mtu?rtp[theConfig.protocol].max_mtu:(size-Pos))+rtp[theConfig.protocol].head_size;
			sendto(socket, (unsigned char *)&pData[Pos-rtp[theConfig.protocol].head_size],
					Len, 0, addr, addr_len);
			Pos+=rtp[theConfig.protocol].max_mtu;
		}
	}
	return size;
}

unsigned char * tcRtpInitRecBuffer(void)
{
	unsigned char *pBuf;
	pBuf = (unsigned char *)malloc(sizeof(unsigned char) * rtp[theConfig.protocol].body_size);
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
	char cTmpBuf[MAXMTUBZ+1024],*pData;
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
		Pos = udpReciveBuffer(socket,pBuf,rtp[theConfig.protocol].body_size,(struct sockaddr*)&remote_addr,&len,time_out);
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
	if(Pos<rtp[theConfig.protocol].head_size) {
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

	if (theConfig.protocol == PROTOCOL_BZ) {
		rec_body_bz *pbody = (rec_body_bz*)pBuf;
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
		pbody = (rec_body_bz*)cTmpBuf;
		for(i=1;i<Cnt;i++) {
			//如果IP不对，丢弃该包
			for(j=0;j<MAX_TIMEOUT_CNT;j++) {
				Len = udpReciveBuffer(socket,cTmpBuf,rtp[theConfig.protocol].max_mtu+rtp[theConfig.protocol].head_size,(struct sockaddr*)&remote_addr,&len,time_out);
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
			if(Len > rtp[theConfig.protocol].head_size)
				memcpy(&pData[rtp[theConfig.protocol].head_size+pbody->packet_idx*rtp[theConfig.protocol].max_mtu],&cTmpBuf[rtp[theConfig.protocol].head_size],Len-rtp[theConfig.protocol].head_size);
			Pos += Len-rtp[theConfig.protocol].head_size;
		}
	} else {
		rec_body_u9 *pbody = (rec_body_u9*)pBuf;
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
		pbody = (rec_body_u9*)cTmpBuf;
		for(i=1;i<Cnt;i++) {
			//如果IP不对，丢弃该包
			for(j=0;j<MAX_TIMEOUT_CNT;j++) {
				Len = udpReciveBuffer(socket,cTmpBuf,rtp[theConfig.protocol].max_mtu+rtp[theConfig.protocol].head_size,(struct sockaddr*)&remote_addr,&len,time_out);
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
			if(Len > rtp[theConfig.protocol].head_size)
				memcpy(&pData[rtp[theConfig.protocol].head_size+pbody->packet_idx*rtp[theConfig.protocol].max_mtu],&cTmpBuf[rtp[theConfig.protocol].head_size],Len-rtp[theConfig.protocol].head_size);
			Pos += Len-rtp[theConfig.protocol].head_size;
		}
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
	if (theConfig.protocol == PROTOCOL_BZ) {
		rec_body_bz *audio_data = (rec_body_bz*)rtp[theConfig.protocol].audio_data;
		audio_data->seq++;
		audio_data->vtype = 0;
		audio_data->slen = 0;
		audio_data->alen = size;
		memcpy(audio_data->sdata,buf,size);
		audio_data->tlen = rtp[theConfig.protocol].head_size + size;
		tcRtpSendBuffer(socket,
				(unsigned char *)audio_data,
				audio_data->tlen,
				addr,
				addr_len);
	} else {
		rec_body_u9 *audio_data = (rec_body_u9*)rtp[theConfig.protocol].audio_data;
		audio_data->seq++;
		audio_data->vtype = 0;
		audio_data->slen = 0;
		audio_data->alen = size;
		memcpy(audio_data->sdata,buf,size);
		audio_data->tlen = rtp[theConfig.protocol].head_size + size;
		tcRtpSendBuffer(socket,
				(unsigned char *)audio_data,
				audio_data->tlen,
				addr,
				addr_len);
	}
}
void tcRtpSendHeart(int socket,
		struct sockaddr_in *addr,
		int addr_len)
{
	if (theConfig.protocol == PROTOCOL_BZ) {
		rec_body_bz *heart_data = (rec_body_bz*)rtp[theConfig.protocol].heart_data;
		heart_data->seq = 0;
		heart_data->vtype = 0;
		heart_data->slen = 0;
		heart_data->alen = 0;
		heart_data->tlen = rtp[theConfig.protocol].head_size + 32;
		tcRtpSendBuffer(socket,
				(unsigned char *)heart_data,
				heart_data->tlen,
				addr,
				addr_len);
	} else {
		rec_body_u9 *heart_data = (rec_body_u9*)rtp[theConfig.protocol].heart_data;
		heart_data->seq = 0;
		heart_data->vtype = 0;
		heart_data->slen = 0;
		heart_data->alen = 0;
		heart_data->tlen = rtp[theConfig.protocol].head_size + 32;
		tcRtpSendBuffer(socket,
				(unsigned char *)heart_data,
				heart_data->tlen,
				addr,
				addr_len);
	}
}
int tcRtpGetAudioReady(void *buf)
{
	if (theConfig.protocol == PROTOCOL_BZ) {
		rec_body_bz *pbody = (rec_body_bz*)buf;
		return pbody->alen ? 1 : 0;
	} else {
		rec_body_u9 *pbody = (rec_body_u9*)buf;
		return pbody->alen ? 1 : 0;
	}
}
int tcRtpGetVideoReady(void *buf)
{
	if (theConfig.protocol == PROTOCOL_BZ) {
		rec_body_bz *pbody = (rec_body_bz*)buf;
		return pbody->slen ? 1 : 0;
	} else {
		rec_body_u9 *pbody = (rec_body_u9*)buf;
		return pbody->slen ? 1 : 0;
	}
}
unsigned char * tcRtpGetVideoData(void *buf)
{
	if (theConfig.protocol == PROTOCOL_BZ) {
		rec_body_bz *pbody = (rec_body_bz*)buf;
		return pbody->sdata;
	} else {
		rec_body_u9 *pbody = (rec_body_u9*)buf;
		return pbody->sdata;
	}
}

unsigned char * tcRtpGetAudioData(void *buf)
{
	return tcRtpGetVideoData(buf);
}
unsigned int  tcRtpGetVideoLen(void *buf)
{
	if (theConfig.protocol == PROTOCOL_BZ) {
		rec_body_bz *pbody = (rec_body_bz*)buf;
		return pbody->slen;
	} else {
		rec_body_u9 *pbody = (rec_body_u9*)buf;
		return pbody->slen;
	}
}
unsigned int  tcRtpGetAudioLen(void *buf)
{
	if (theConfig.protocol == PROTOCOL_BZ) {
		rec_body_bz *pbody = (rec_body_bz*)buf;
		return pbody->alen;
	} else {
		rec_body_u9 *pbody = (rec_body_u9*)buf;
		return pbody->alen;
	}
}
unsigned int  tcRtpGetHeadLen(void)
{
	return rtp[theConfig.protocol].head_size;
}
