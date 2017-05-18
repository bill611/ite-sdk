/*
 * =============================================================================
 *
 *       Filename:  rtpObject.h
 *
 *    Description:  tc rtp protocl
 *
 *        Version:  1.0
 *        Created:  2016-11-25 10:35:27 
 *       Revision:  1.0
 *
 *         Author:  xubin
 *        Company:  Taichuan
 *
 * =============================================================================
 */
#ifndef _TC_RTP_OBJECT_H
#define _TC_RTP_OBJECT_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

	unsigned char * tcRtpInitRecBuffer(void);
	void tcRtpDeInitRecBuffer(unsigned char **pBuf);
	int tcRtpRecvVideoAudioBuffer(int socket,char *ip,void *pBuf,int time_out);
	int tcRtpSendVideo(int socket,unsigned char *buf,unsigned int size);
	void tcRtpSendAudio(int socket,
			unsigned char *buf,
			unsigned int size,
			struct sockaddr_in *addr,
			int addr_len);
	void tcRtpSendHeart(int socket,
			struct sockaddr_in *addr,
			int addr_len);
	int tcRtpGetAudioReady(void *buf);
	int tcRtpGetVideoReady(void *buf);
	unsigned char * tcRtpGetVideoData(void *buf);
	unsigned char * tcRtpGetAudioData(void *buf);
	unsigned int  tcRtpGetVideoLen(void *buf);
	unsigned int  tcRtpGetAudioLen(void *buf);
	unsigned int  tcRtpGetHeadLen(void);


#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
