/*
 * =============================================================================
 *
 *       Filename:  adpcm.h
 *
 *    Description:  adpcm ±à½âÂë
 *
 *        Version:  1.0
 *        Created:  2016-11-25 10:40:33 
 *       Revision:  1.0
 *
 *         Author:  xubin
 *        Company:  Taichuan
 *
 * =============================================================================
 */
#ifndef _TC_ADPCM_H
#define _TC_ADPCM_H

#ifdef __cplusplus
extern "C" {
#endif  /* __cplusplus */

	void adpcm_dec_init();
	void adpcm_enc_init();
	int adpcm_coder(short *indata,char *outdata,int len);
	int adpcm_decoder(char *indata,short *outdata,int len);

#ifdef __cplusplus
}
#endif  /* __cplusplus */

#endif
