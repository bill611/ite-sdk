#if defined(HAVE_CONFIG_H)
#include "mediastreamer-config.h"
#endif

#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "ortp/b64.h"
#include "mediastreamer2/hwengine.h"

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#ifdef WIN32
#include <malloc.h> /* for alloca */
#endif

#include "ite/audio.h"
#include "type_def.h"
#include "aecm_core.h"
#include "basic_op.h"
#include "dft_filt_bank.h"
#include "hd_aec.h"
#include "howling_ctrl.h"
#include "rfft_256.h"

#ifdef ENABLE_DUMP_AEC_DATA
#define EC_DUMP_ITE
#endif
//#define AEC_INPLUSE_GENERATER
//#define INPLUSE_GENERATER_SEND
//#define AEC_INARM

typedef struct _AudioFlowController_{
    int target_samples;
    int total_samples;
    int current_pos;
    int current_dropped;
}AudioFlowController_;

static int drop;
 
void audio_flow_controller_init_(AudioFlowController_ *ctl){
    ctl->target_samples=0;
    ctl->total_samples=0;
    ctl->current_pos=0;
    ctl->current_dropped=0;
}
 
void audio_flow_controller_set_target_(AudioFlowController_ *ctl, int samples_to_drop, int total_samples){
    ctl->target_samples=samples_to_drop;
    ctl->total_samples=total_samples;
    ctl->current_pos=0;
    ctl->current_dropped=0;
}
 
static void discard_well_choosed_samples_(mblk_t *m, int nsamples, int todrop){
    int i;
    int16_t *samples=(int16_t*)m->b_rptr;
    int min_diff=32768;
    int pos=0;
 
 
#ifdef TWO_SAMPLES_CRITERIA
    for(i=0;i<nsamples-1;++i){
        int tmp=abs((int)samples[i]- (int)samples[i+1]);
#else
    for(i=0;i<nsamples-2;++i){
        int tmp=abs((int)samples[i]- (int)samples[i+1])+abs((int)samples[i+1]- (int)samples[i+2]);
#endif
        if (tmp<=min_diff){
            pos=i;
            min_diff=tmp;
        }
    }
    /*ms_message("min_diff=%i at pos %i",min_diff, pos);*/
#ifdef TWO_SAMPLES_CRITERIA
    memmove(samples+pos,samples+pos+1,(nsamples-pos-1)*2);
#else
    memmove(samples+pos+1,samples+pos+2,(nsamples-pos-2)*2);
#endif
 
    todrop--;
    m->b_wptr-=2;
    nsamples--;
    if (todrop>0){
        /*repeat the same process again*/
        discard_well_choosed_samples_(m,nsamples,todrop);
    }
}
 
mblk_t * audio_flow_controller_process_(AudioFlowController_ *ctl, mblk_t *m){
    if (ctl->total_samples>0 && ctl->target_samples>0){
        int nsamples=(m->b_wptr-m->b_rptr)/2;
        if (ctl->target_samples*16>ctl->total_samples){
            ms_warning("Too many samples to drop, dropping entire frames");
            m->b_wptr=m->b_rptr;
            ctl->current_pos+=nsamples;
        }else{
            int th_dropped;
            int todrop;
 
            ctl->current_pos+=nsamples;
            th_dropped=(ctl->target_samples*ctl->current_pos)/ctl->total_samples;
            todrop=th_dropped-ctl->current_dropped;
            if (todrop>0){
                if (todrop>nsamples) todrop=nsamples;
                discard_well_choosed_samples_(m,nsamples,todrop);
                /*ms_message("th_dropped=%i, current_dropped=%i, %i samples dropped.",th_dropped,ctl->current_dropped,todrop);*/
                ctl->current_dropped+=todrop;
            }
        }
        if (ctl->current_pos>=ctl->total_samples) ctl->target_samples=0;/*stop discarding*/
    }
    return m;
}
 
static const int flow_control_interval_ms=5000;
 
typedef struct SbcAECState{
    MSBufferizer delayed_ref;
    MSBufferizer ref;
    MSBufferizer echo;
    int framesize;
    int samplerate;
    int delay_ms;
    int nominal_ref_samples;
    int min_ref_samples;
    AudioFlowController_ afc;
    bool_t echostarted;
    bool_t bypass_mode;
    bool_t using_zeroes;
    queue_t echo_q;
    queue_t ref_q;
#ifdef EC_DUMP_ITE
    queue_t echo_copy_q;
    queue_t ref_copy_q;
    queue_t clean_copy_q;
#endif
    ms_mutex_t mutex;
}SbcAECState;
 
static void sbc_aec_init(MSFilter *f){
    SbcAECState *s=(SbcAECState *)ms_new(SbcAECState,1);
 
    s->samplerate=CFG_AUDIO_SAMPLING_RATE;
    ms_bufferizer_init(&s->delayed_ref);
    ms_bufferizer_init(&s->echo);
    ms_bufferizer_init(&s->ref);
    s->delay_ms=CFG_AEC_DELAY_MS;
    s->using_zeroes=FALSE;
    s->echostarted=FALSE;
    s->bypass_mode=FALSE;
#if (CFG_CHIP_FAMILY == 9910)
    s->framesize=144; // MUST assign value for win32, otherwise break at alloca(nbytes)
#else
    #if(CFG_AUDIO_SAMPLING_RATE == 16000)
    s->framesize=256;
    #else
    s->framesize=128;//16K : s->framesize=256
    #endif  
#endif
    qinit(&s->echo_q);
    qinit(&s->ref_q);
#ifdef EC_DUMP_ITE
    qinit(&s->echo_copy_q);
    qinit(&s->ref_copy_q);
    qinit(&s->clean_copy_q);
#endif
    ms_mutex_init(&s->mutex,NULL);
 
    f->data=s;
#if (CFG_CHIP_FAMILY == 9070 || CFG_CHIP_FAMILY == 9850)
    init_uexp();
    init_udiv();
    init_ursqrt();
    init_nr_core(&NR_STATE_TX, 1);
    NLP_Init(&nlp_state);
    PBFDSR_Init(&anc_state);
    PBFDAF_Init(&aec_state);
#else //9910
    iteAecCommand(AEC_CMD_INIT, 0, 0, 0, 0, &s->framesize);
#endif

}
 
static void sbc_aec_uninit(MSFilter *f){
    SbcAECState *s=(SbcAECState*)f->data;
 
    ms_bufferizer_uninit(&s->delayed_ref);
    ms_mutex_destroy(&s->mutex);
    ms_free(s);
}

static void hw_ec_process(void *arg) {
    MSFilter *f=(MSFilter*)arg;
    SbcAECState *s=(SbcAECState*)f->data;
    mblk_t *ref;
    mblk_t *echo;
    int nbytes = s->framesize*2;

	echo = ref = NULL;
	ms_mutex_lock(&s->mutex);
	echo = getq(&s->echo_q);
	ref = getq(&s->ref_q);
	ms_mutex_unlock(&s->mutex);

	if (echo && ref) {
		mblk_t *oecho = allocb(nbytes, 0);
		memset(oecho->b_wptr, 0, nbytes);
#if (CFG_CHIP_FAMILY != 9910)
    //memmove(oecho->b_wptr, echo->b_rptr, sizeof(short)*s->framesize);   //bypass
        #if(CFG_AUDIO_SAMPLING_RATE == 16000)
            //PAES_Process_Block(&aecm,echo->b_rptr, ref->b_rptr, oecho->b_wptr); //16K
            FreqWarpping(echo->b_rptr, ref->b_rptr, oecho->b_wptr, &anc_state);
        #else
            aecm_core(echo->b_rptr, ref->b_rptr, oecho->b_wptr, &anc_state);  //8K
        #endif
#else //AEC run in RISC
		iteAecCommand(AEC_CMD_PROCESS,(unsigned int) echo->b_rptr,(unsigned int) ref->b_rptr, (unsigned int) oecho->b_wptr,nbytes, 0);
#endif
        oecho->b_wptr += nbytes;
#ifdef EC_DUMP_ITE
        putq(&s->clean_copy_q, dupmsg(oecho));
#endif
        ms_queue_put(f->outputs[1],oecho);
    }
    
    if (echo) freemsg(echo);
    if (ref) freemsg(ref);

}
 
static void sbc_aec_preprocess(MSFilter *f){
    SbcAECState *s=(SbcAECState*)f->data;
    mblk_t *m;
    int delay_samples=0;
    
    s->echostarted=FALSE;
    delay_samples=s->delay_ms*s->samplerate/1000;
 
    /* fill with zeroes for the time of the delay*/
    m=allocb(delay_samples*2,0);
    memset(m->b_wptr,0,delay_samples*2);
    m->b_wptr+=delay_samples*2;
    ms_bufferizer_put(&s->delayed_ref,m);
    s->min_ref_samples=-1;
    s->nominal_ref_samples=delay_samples;
    audio_flow_controller_init_(&s->afc);
    drop = 0;
    
    //hw_engine_init();
    //hw_engine_link_filter(HW_EC_ID, f);
}
 
/*  inputs[0]= reference signal from far end (sent to soundcard)
 *  inputs[1]= near speech & echo signal    (read from soundcard)
 *  outputs[0]=  is a copy of inputs[0] to be sent to soundcard
 *  outputs[1]=  near end speech, echo removed - towards far end
*/
static void sbc_aec_process(MSFilter *f){
    SbcAECState *s=(SbcAECState*)f->data;
    int nbytes=s->framesize*2;
    mblk_t *refm;
    uint8_t *ref,*echo;
    if (s->bypass_mode) {
        while((refm=ms_queue_get(f->inputs[0]))!=NULL){
            ms_queue_put(f->outputs[0],refm);
        }
        while((refm=ms_queue_get(f->inputs[1]))!=NULL){
            ms_queue_put(f->outputs[1],refm);
        }
        return;
    }
    
#ifdef AEC_INPLUSE_GENERATER
    if (f->ticker->time %  2000 == 0){
        mblk_t *refm_tmp;
        refm_tmp=allocb(nbytes,0);
        memset(refm_tmp->b_wptr,40,nbytes);
        refm_tmp->b_wptr+=nbytes;
#ifdef INPLUSE_GENERATER_SEND
        ms_queue_put(f->outputs[1],refm_tmp); //send to far-end
#else
        ms_queue_put(f->inputs[0],refm_tmp);  //send to near-end
#endif
    }
#endif 
      
    if (f->inputs[0]!=NULL){
        if (s->echostarted){
            while((refm=ms_queue_get(f->inputs[0]))!=NULL){
                mblk_t *cp=dupmsg(audio_flow_controller_process_(&s->afc,refm));
                ms_mutex_lock(&s->mutex);
                ms_bufferizer_put(&s->delayed_ref,cp);
                ms_bufferizer_put(&s->ref,refm);
                ms_mutex_unlock(&s->mutex);
            }
        }else{
            ms_warning("Getting reference signal but no echo to synchronize on.");
            ms_queue_flush(f->inputs[0]);
        }
    }
 
    ms_bufferizer_put_from_queue(&s->echo,f->inputs[1]);
 
    ref=(uint8_t*)alloca(nbytes);
    echo=(uint8_t*)alloca(nbytes);
    while (ms_bufferizer_read(&s->echo,echo,nbytes)>=nbytes){
        int avail;
        int avail_samples;
        mblk_t *echo_to_ec;
        mblk_t *ref_to_ec;
 
        if (!s->echostarted) s->echostarted=TRUE;
        if ((avail=ms_bufferizer_get_avail(&s->delayed_ref))<((s->nominal_ref_samples*2)+nbytes)){
            /*we don't have enough to read in a reference signal buffer, inject silence instead*/
            refm=allocb(nbytes,0);
            memset(refm->b_wptr,0,nbytes);
            refm->b_wptr+=nbytes;
            ms_mutex_lock(&s->mutex);
            ms_bufferizer_put(&s->delayed_ref,refm);
            ms_queue_put(f->outputs[0],dupmsg(refm));
            ms_mutex_unlock(&s->mutex);
            if (!s->using_zeroes){
                ms_warning("Not enough ref samples, using zeroes");
                s->using_zeroes=TRUE;
            }
        }else{
            if (s->using_zeroes){
                ms_message("Samples are back.");
                s->using_zeroes=FALSE;
            }
            /* read from our no-delay buffer and output */
            refm=allocb(nbytes,0);
            if (ms_bufferizer_read(&s->ref,refm->b_wptr,nbytes)==0){
                ms_fatal("Should never happen");
            }
            refm->b_wptr+=nbytes;
            ms_queue_put(f->outputs[0],refm);
        }
 
        /*now read a valid buffer of delayed ref samples*/
        if (ms_bufferizer_read(&s->delayed_ref,ref,nbytes)==0){
            ms_fatal("Should never happen");
        }
        avail-=nbytes;
        avail_samples=avail/2;
        if (avail_samples<s->min_ref_samples || s->min_ref_samples==-1){
            s->min_ref_samples=avail_samples;
        }
 
        // put near-end and far-end data to queue
        echo_to_ec = allocb(nbytes, 0);        
        memcpy(echo_to_ec->b_wptr, echo, nbytes);
        echo_to_ec->b_wptr += nbytes;
        ref_to_ec = allocb(nbytes, 0);
        memcpy(ref_to_ec->b_wptr, ref, nbytes);
        ref_to_ec->b_wptr += nbytes;
        
        ms_mutex_lock(&s->mutex);
        putq(&s->echo_q, echo_to_ec);
        putq(&s->ref_q, ref_to_ec);
#ifdef EC_DUMP_ITE
        putq(&s->echo_copy_q, dupmsg(echo_to_ec));
        putq(&s->ref_copy_q, dupmsg(ref_to_ec));
#endif
        ms_mutex_unlock(&s->mutex);
    }
 
    /*verify our ref buffer does not become too big, meaning that we are receiving more samples than we are sending*/
    if (f->ticker->time % flow_control_interval_ms == 0 && s->min_ref_samples!=-1){
        int diff=s->min_ref_samples-s->nominal_ref_samples;
        if (diff>(nbytes/1)){
            int purge=diff-(nbytes/1);
            ms_warning("echo canceller: we are accumulating too much reference signal, need to throw out %i samples",purge);
            audio_flow_controller_set_target_(&s->afc,purge,(flow_control_interval_ms*s->samplerate)/1000);
        }
        s->min_ref_samples=-1;
    }
}
 
static void sbc_aec_postprocess(MSFilter *f){
    SbcAECState *s=(SbcAECState*)f->data;
 
    ms_bufferizer_flush (&s->delayed_ref);
    ms_bufferizer_flush (&s->echo);
    ms_bufferizer_flush (&s->ref);
 
    //hw_engine_uninit();
    
    flushq(&s->echo_q,0);
    flushq(&s->ref_q,0);
 
#ifdef EC_DUMP_ITE
    FILE *echofile;
    FILE *reffile;
    FILE *cleanfile;
    mblk_t *echo;
    mblk_t *ref;
    mblk_t *clean;
    int nbytes=s->framesize*2;
    static int index = 0;
    char *fname;
    fname=ms_strdup_printf("d:/echo%03d.raw", index);
    echofile=fopen(fname,"w");
    ms_free(fname);
    fname=ms_strdup_printf("d:/ref%03d.raw", index);
    reffile=fopen(fname,"w");
    ms_free(fname);
    fname=ms_strdup_printf("d:/clean%03d.raw", index);
    cleanfile=fopen(fname,"w");
    ms_free(fname);
    index++;
    while (1)
    {
        echo=ref=NULL;
        echo=getq(&s->echo_copy_q);
        ref=getq(&s->ref_copy_q);
        clean=getq(&s->clean_copy_q);
        if (echo && ref && clean)
        {
            fwrite(echo->b_rptr,nbytes,1,echofile);
            freemsg(echo);            
            fwrite(ref->b_rptr,nbytes,1,reffile);
            freemsg(ref);
            fwrite(clean->b_rptr,nbytes,1,cleanfile);
            freemsg(clean);
        }
        else
        {
            flushq(&s->echo_copy_q,0);
            flushq(&s->ref_copy_q,0);
            flushq(&s->clean_copy_q,0);
            fclose(echofile);
            fclose(reffile);
            fclose(cleanfile);
            break;
        }
    }
#endif
}

static int sbc_aec_method_1(MSFilter *f, void *arg){
    printf("do nothing for method 1, fix it later\n");
    return 0;
}
 
static int sbc_aec_method_2(MSFilter *f, void *arg){
    printf("do nothing for method 2, fix it later\n");
    return 0;
}
 
static int echo_canceller_set_delay(MSFilter *f, void *arg){
	SbcAECState *s=(SbcAECState*)f->data;
	s->delay_ms= *(int*)arg;
    return 0;
}
 
static int sbc_aec_method_4(MSFilter *f, void *arg){
    printf("do nothing for method 4, fix it later\n");
    return 0;
}
 
static int sbc_aec_method_5(MSFilter *f, void *arg){
    printf("do nothing for method 5, fix it later\n");
    return 0;
}
 
static int sbc_aec_method_6(MSFilter *f, void *arg){
    printf("do nothing for method 6, fix it later\n");
    return 0;
}
 
static int sbc_aec_method_7(MSFilter *f, void *arg){
    printf("do nothing for method 7, fix it later\n");
    return 0;
}
 
static int sbc_aec_method_8(MSFilter *f, void *arg){
    printf("do nothing for method 8, fix it later\n");
    return 0;
}
 
static MSFilterMethod sbc_aec_methods[]={
    {MS_FILTER_SET_SAMPLE_RATE          , sbc_aec_method_1},
    {MS_ECHO_CANCELLER_SET_TAIL_LENGTH    , sbc_aec_method_2},
    {MS_ECHO_CANCELLER_SET_DELAY        , echo_canceller_set_delay},
    {MS_ECHO_CANCELLER_SET_FRAMESIZE    , sbc_aec_method_4},
    {MS_ECHO_CANCELLER_SET_BYPASS_MODE    , sbc_aec_method_5},
    {MS_ECHO_CANCELLER_GET_BYPASS_MODE    , sbc_aec_method_6},
    {MS_ECHO_CANCELLER_GET_STATE_STRING    , sbc_aec_method_7},
    {MS_ECHO_CANCELLER_SET_STATE_STRING    , sbc_aec_method_8}
};
 
#ifdef _MSC_VER
 
MSFilterDesc ms_sbc_aec_desc={
    MS_SBC_AEC_ID,
    "MSSbcAEC",
    N_("Echo canceller using sbc aec"),
    MS_FILTER_OTHER,
    NULL,
    2,
    2,
    sbc_aec_init,
    sbc_aec_preprocess,
    sbc_aec_process,
    sbc_aec_postprocess,
    sbc_aec_uninit,
    sbc_aec_methods
};
 
HWEngineDesc hw_ec_engine_desc={
    HW_EC_ID,
    hw_ec_process
};
 
#else
 
MSFilterDesc ms_sbc_aec_desc={
    .id=MS_SBC_AEC_ID,
    .name="MSSbcAEC",
    .text=N_("Echo canceller using sbc aec"),
    .category=MS_FILTER_OTHER,
    .ninputs=2,
    .noutputs=2,
    .init=sbc_aec_init,
    .preprocess=sbc_aec_preprocess,
    .process=sbc_aec_process,
    .postprocess=sbc_aec_postprocess,
    .uninit=sbc_aec_uninit,
    .methods=sbc_aec_methods
};
 
HWEngineDesc hw_ec_engine_desc={
    .id=HW_EC_ID,
    .process=hw_ec_process
};
 
#endif
 
MS_FILTER_DESC_EXPORT(ms_sbc_aec_desc)
