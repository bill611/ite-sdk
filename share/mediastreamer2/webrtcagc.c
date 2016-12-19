                                                                 /*
 * webrtc_agc.c
 *
 *  Created on: 2016¦~4¤ë26¤é
 *      Author: ite01527
 */
#if 1

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "ortp/b64.h"

#include "digital_agc.h"
#include "gain_control.h"

#ifdef WIN32
#include <malloc.h>
#endif

static const int32_t framesize = 80;

typedef struct WebRTCAGCState {
	void *agcInst;
	MSBufferizer ref;
	int32_t framesize;
	int32_t samplerate;

}WebRTCAGCState;

static void webrtc_agc_init(MSFilter *f) {
	WebRTCAGCState *s = (WebRTCAGCState*)ms_new(WebRTCAGCState, 1);
	s->samplerate = 8000;
	ms_bufferizer_init(&s->ref);
	s->agcInst = NULL;
	f->data = s;
}

static void webrtc_agc_uninit(MSFilter *f) {
	WebRTCAGCState *s = (WebRTCAGCState *) f->data;
	ms_bufferizer_uninit(&s->ref);
	ms_free(s);
}

static void webrtc_agc_preprocess(MSFilter *f) {
	WebRTCAGCState *s = (WebRTCAGCState *) f->data;

	s->framesize = (framesize*s->samplerate)/8000;
	s->agcInst = (DigitalAgc*)ms_new(DigitalAgc, 1);
	WebRtcAgc_InitDigital((DigitalAgc*)s->agcInst, kAgcModeAdaptiveDigital);
	WebRtcAgc_CalculateGainTable(((DigitalAgc*)s->agcInst)->gainTable, 48, 0, 1, 24);
}

static void webrtc_agc_process(MSFilter *f) {
	WebRTCAGCState *s = (WebRTCAGCState *) f->data;
	int32_t nbytes = s->framesize * 2;
	uint8_t *ref;
  //mblk_t *refm;

  /*while ((refm = ms_queue_get(f->inputs[0])) != NULL) {
			ms_queue_put(f->outputs[0], refm);
	}

  return;*/

	ms_bufferizer_put_from_queue(&s->ref, f->inputs[0]);
	ref = alloca(nbytes);
	while(ms_bufferizer_read(&s->ref, ref, nbytes) >= nbytes) {
		mblk_t *oref = allocb(nbytes, 0);

		WebRtcAgc_ProcessDigital(s->agcInst, (int16_t**)&ref, 1, (int16_t**)&oref->b_wptr,
				s->samplerate, 0);
		oref->b_wptr += nbytes;
		ms_queue_put(f->outputs[0], oref);
	}

}

static void webrtc_agc_postprocess(MSFilter *f) {
	WebRTCAGCState *s = (WebRTCAGCState *) f->data;

	ms_bufferizer_flush(&s->ref);

	if(s->agcInst != NULL) {
		ms_free(s->agcInst);
		s->agcInst = NULL;
	}

}

static int webrtc_agc_set_sr(MSFilter *f, void *arg) {  
	return (0);
}

static int webrtc_agc_get_sr(MSFilter *f, void *arg) {
	return (0);
}

static MSFilterMethod webrtc_agc_methods[] = {
	{	MS_FILTER_SET_SAMPLE_RATE, webrtc_agc_set_sr},
	{	MS_FILTER_GET_SAMPLE_RATE, webrtc_agc_get_sr}
};

#define MS_WEBRTC_AGC_NAME        "MSWebRTCAGC"
#define MS_WEBRTC_AGC_DESCRIPTION "AGC using WebRTC library."
#define MS_WEBRTC_AGC_CATEGORY    MS_FILTER_OTHER
#define MS_WEBRTC_AGC_ENC_FMT     NULL
#define MS_WEBRTC_AGC_NINPUTS     1
#define MS_WEBRTC_AGC_NOUTPUTS    1
#define MS_WEBRTC_AGC_FLAGS       0

#ifdef _MSC_VER

MSFilterDesc ms_webrtc_agc_desc = {
	MS_FILTER_AGC_ID,
	"MSWebRTCAGC",
	N_("AGC using WebRTC library."),
	MS_FILTER_OTHER,
	NULL,
	1,
	1,
	webrtc_agc_init,
	webrtc_agc_preprocess,
	webrtc_agc_process,
	webrtc_agc_postprocess,
	webrtc_agc_uninit,
	webrtc_agc_methods
};

#else

MSFilterDesc ms_webrtc_agc_desc = {
	.id = MS_FILTER_AGC_ID,
	.name = MS_WEBRTC_AGC_NAME,
	.text = N_(MS_WEBRTC_AGC_DESCRIPTION),
	.category = MS_WEBRTC_AGC_CATEGORY,
	.ninputs = MS_WEBRTC_AGC_NINPUTS,
	.noutputs = MS_WEBRTC_AGC_NOUTPUTS,
	.init = webrtc_agc_init,
	.preprocess = webrtc_agc_preprocess,
	.process = webrtc_agc_process,
	.postprocess = webrtc_agc_postprocess,
	.uninit = webrtc_agc_uninit,
	.methods = webrtc_agc_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_webrtc_agc_desc)

#endif
