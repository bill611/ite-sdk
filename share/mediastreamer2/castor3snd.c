#include <assert.h>
#include <time.h>

#include "mediastreamer2/mssndcard.h"
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msticker.h"
#include "mediastreamer2/msfileplayer.h"
#include "ite/itp.h"
#if (CFG_CHIP_FAMILY == 9910)
#include "i2s/i2s_it9910.h"
#else // 9850 & 9070
#include "i2s/i2s.h"
#endif
#ifdef CFG_FM2018_ENABLE 
#include "fm2018/fortemedia_fm2018_driver.h"
#endif
#include "ite/audio.h"

MSFilter *ms_castor3snd_read_new(MSSndCard *card);
MSFilter *ms_castor3snd_write_new(MSSndCard *card);

static STRC_I2S_SPEC spec_da = {0};
static STRC_I2S_SPEC spec_ad = {0};

typedef struct Castor3SndData{
    char *pcmdev;
    char *mixdev;
    int   devid;

    int rate;
    int datalength;
    //int bits;
    ms_thread_t txthread;
    ms_thread_t rxthread;
    ms_mutex_t ad_mutex;
	ms_mutex_t da_mutex;
    queue_t rq;
    MSBufferizer * bufferizer;
    bool_t read_started;
    bool_t write_started;
    bool_t write_EOF;
    bool_t stereo;

    //uint64_t bytes_read;
    //int32_t stat_input;
    //int32_t stat_output;
    //int32_t stat_notplayed;

    STRC_I2S_SPEC spec;
    // DAC
    //uint32_t dac_vmem;
    uint8_t *dac_buf;
    int dac_buf_len;
    int dac_vol;

    // ADC
    //uint32_t adc_vmem;
    uint8_t *adc_buf;
    int adc_buf_len;
    int adc_vol;
} Castor3SndData;

//static uint64_t castor3snd_get_cur_time( void *data){
//    Castor3SndData *d=(Castor3SndData*)data;
//    uint64_t curtime=(d->bytes_read*1000)/(d->rate*(d->bits/8)*((d->stereo==FALSE) ? 1 : 2));
//    ms_debug("castor3snd_get_cur_time: bytes_read=%lu, rate=%i, bits=%i, stereo=%i return %lu\n",
//    (unsigned long)d->bytes_read,d->rate,d->bits,d->stereo,(unsigned long)curtime);
//    return curtime;
//}

//static int castor3snd_open(Castor3SndData *device, int devnumber, int bits,int stereo, int rate, int *minsz)
//{
//    int channel = 1;
//    if (stereo>0)
//        channel = stereo;
//
//    *minsz=device->rate/8000 * 320;
//    return 0;
//}

static void castor3snd_set_level(MSSndCard *card, MSSndCardMixerElem e, int percent)
{
    Castor3SndData *d=(Castor3SndData*)card->data;

    if (d->mixdev==NULL) return;
    switch(e){
        case MS_SND_CARD_MASTER:
            return;
        break;

        case MS_SND_CARD_PLAYBACK:
/* #if (CFG_CHIP_FAMILY == 9910)
			if (d->write_started)
            {
                if (d->dac_vol == 0 && percent > 0)
                    i2s_mute_DAC(0);
                else if (d->dac_vol > 0 && percent == 0)
                    i2s_mute_DAC(1);

                i2s_set_direct_volperc(percent);
            }
#else */
            i2s_set_direct_volperc(percent);
//#endif
			d->dac_vol = percent;
            return;
        break;

        case MS_SND_CARD_CAPTURE:
            {
                unsigned int max = 0, normal = 0, min = 0;
                i2s_ADC_get_volstep_range(&max, &normal, &min);
                if (max > min)
                    i2s_ADC_set_direct_volstep((max - min) * percent / 100);
                else
                    i2s_ADC_set_direct_volstep((min - max) * percent / 100);
                d->adc_vol = percent;                
            }
            return;
        break;

        default:
            ms_warning("castor3snd_card_set_level: unsupported command.");
            return;
    }
}

static int castor3snd_get_level(MSSndCard *card, MSSndCardMixerElem e)
{
    Castor3SndData *d=(Castor3SndData*)card->data;

    if (d->mixdev==NULL) return -1;
    switch(e){
        case MS_SND_CARD_MASTER:
            return 60;
        break;

        case MS_SND_CARD_PLAYBACK:
            return d->dac_vol;
        break;

        case MS_SND_CARD_CAPTURE:
            return d->adc_vol;
        break;

        default:
            ms_warning("castor3snd_card_get_level: unsupported command.");
            return -1;
    }
    return -1;
}

static void castor3snd_set_source(MSSndCard *card, MSSndCardCapture source)
{
    Castor3SndData *d=(Castor3SndData*)card->data;
    if (d->mixdev==NULL) return;

    switch(source){
        case MS_SND_CARD_MIC:
        break;
        case MS_SND_CARD_LINE:
        break;
    }
}

static void castor3snd_init(MSSndCard *card)
{
    Castor3SndData *d=(Castor3SndData*)ms_new0(Castor3SndData,1);

    
    //d->bytes_read=0;
    d->pcmdev=NULL;
    d->mixdev=NULL;
    d->read_started=FALSE;
    d->write_started=FALSE;
    //d->bits=16;
    d->rate=CFG_AUDIO_SAMPLING_RATE;
    d->stereo=FALSE;
    d->datalength = -1;
    d->write_EOF = FALSE;
    qinit(&d->rq);
    d->bufferizer=ms_bufferizer_new();
    ms_mutex_init(&d->ad_mutex,NULL);
	ms_mutex_init(&d->da_mutex,NULL);
    card->data=d;

    //d->stat_input=0;
    //d->stat_output=0;
    //d->stat_notplayed=0;

#ifdef CFG_FM2018_ENABLE 
    /* init FM2018 */
    printf("--- Init FM2018 ---\n");
    itpFM2018Initialize();
#endif

    iteAudioOpenEngine(ITE_SBC_CODEC);

    /* init DAC */
    d->dac_buf_len = 128 * 1024;
    //d->dac_vmem = itpVmemAlloc(d->dac_buf_len);
    //d->dac_buf = ithMapVram(d->dac_vmem, d->dac_buf_len, ITH_VRAM_WRITE);
    d->dac_buf = (uint8_t*)malloc(d->dac_buf_len);
    assert(d->dac_buf);
    memset((void*)&spec_da, 0, sizeof(STRC_I2S_SPEC));
    spec_da.channels                 = d->stereo? 2: 1;
    spec_da.sample_rate              = d->rate;
    spec_da.buffer_size              = d->dac_buf_len;
#if (CFG_CHIP_FAMILY == 9910)
    spec_da.is_big_endian            = 1;
#else
	spec_da.is_big_endian            = 0;
#endif
    spec_da.base_i2s                 = d->dac_buf;

    spec_da.sample_size              = 16;
    spec_da.num_hdmi_audio_buffer    = 1;
    spec_da.is_dac_spdif_same_buffer = 1;
#if (CFG_CHIP_FAMILY == 9070)
    spec_da.base_hdmi[0]             = d->dac_buf;
    spec_da.base_hdmi[1]             = d->dac_buf;
    spec_da.base_hdmi[2]             = d->dac_buf;
    spec_da.base_hdmi[3]             = d->dac_buf;
    spec_da.base_spdif               = d->dac_buf;
#endif
    spec_da.enable_Speaker           = 1;
    spec_da.enable_HeadPhone         = 1;
    spec_da.postpone_audio_output    = 1;

    i2s_init_DAC(&spec_da);
    
    
    /* init ADC */
    d->adc_buf_len = 64 * 1024 - 8;
    //d->adc_vmem = itpVmemAlloc(d->adc_buf_len);
    //d->adc_buf = ithMapVram(d->adc_vmem, d->adc_buf_len, ITH_VRAM_WRITE);
    d->adc_buf = (uint8_t*)malloc(d->adc_buf_len);
    assert(d->adc_buf);

    memset((void*)&spec_ad, 0, sizeof(STRC_I2S_SPEC));
    /* ADC Spec */    
    spec_ad.channels      = d->stereo? 2: 1;
    spec_ad.sample_rate   = d->rate;
    spec_ad.buffer_size   = d->adc_buf_len;
#if (CFG_CHIP_FAMILY == 9910)
    spec_ad.is_big_endian = 1;
	spec_ad.base_in_i2s   = d->adc_buf;
#else
	spec_ad.is_big_endian = 0;
    spec_ad.base_i2s      = d->adc_buf;
#endif
    spec_ad.sample_size   = 16;
    spec_ad.record_mode   = 1;
#ifdef CFG_FM2018_ENABLE 
    spec_ad.from_LineIN   = 1;
    spec_ad.from_MIC_IN   = 0;
#else
    spec_ad.from_LineIN   = 0;
    spec_ad.from_MIC_IN   = 1;
#endif

#ifdef CFG_I2S_USE_GPIO_MODE_2
    i2s_init_ADC(&spec_ad);
#endif
    i2s_pause_ADC(1);
    
}

static void castor3snd_uninit(MSSndCard *card)
{
    Castor3SndData *d=(Castor3SndData*)card->data;
    
    if (d==NULL)
        return;
    if (d->pcmdev!=NULL) ms_free(d->pcmdev);
    if (d->mixdev!=NULL) ms_free(d->mixdev);
    ms_bufferizer_destroy(d->bufferizer);
    flushq(&d->rq,0);

    ms_mutex_destroy(&d->ad_mutex);
	ms_mutex_destroy(&d->da_mutex);

    /* hook for Castor3 I2S */
    if (d->adc_buf != NULL) {
        //ithUnmapVram(d->adc_buf, d->adc_buf_len);
        //itpVmemFree(d->adc_vmem);
        free(d->adc_buf);
        d->adc_buf = NULL;
    }
    d->adc_buf_len = 0;

    if (d->dac_buf != NULL) {
        //ithUnmapVram(d->dac_buf, d->dac_buf_len);
        //itpVmemFree(d->dac_vmem);
        free(d->dac_buf);
        d->dac_buf = NULL;
    }
    d->dac_buf_len = 0;

    ms_free(d);
}

static void castor3snd_detect(MSSndCardManager *m);
static MSSndCard *castor3snd_dup(MSSndCard *obj);

MSSndCardDesc castor3snd_card_desc={
    "CASTOR3SND",
    castor3snd_detect,
    castor3snd_init,
    castor3snd_set_level,
    castor3snd_get_level,
    castor3snd_set_source,
    NULL,
    NULL,
    ms_castor3snd_read_new,
    ms_castor3snd_write_new,
    castor3snd_uninit,
    castor3snd_dup
};

static  MSSndCard *castor3snd_dup(MSSndCard *obj){
    MSSndCard *card=ms_snd_card_new(&castor3snd_card_desc);
    Castor3SndData *dcard=(Castor3SndData*)card->data;
    Castor3SndData *dobj=(Castor3SndData*)obj->data;
    dcard->pcmdev=ms_strdup(dobj->pcmdev);
    dcard->mixdev=ms_strdup(dobj->mixdev);
    dcard->devid=dobj->devid;
    card->name=ms_strdup(obj->name);
    //printf("CAUTION: called castor3snd_dup\n");
    return card;
}

static MSSndCard *castor3snd_card_new(const char *pcmdev, const char *mixdev, int id){
    MSSndCard *card=ms_snd_card_new(&castor3snd_card_desc);
    Castor3SndData *d=(Castor3SndData*)card->data;
    d->pcmdev=ms_strdup(pcmdev);
    d->mixdev=ms_strdup(mixdev);
    card->name=ms_strdup(pcmdev);
    d->devid=id;    
    return card;
}

// this function will be called while booting
static void castor3snd_detect(MSSndCardManager *m){
    char pcmdev[1024];
    char mixdev[1024];
    MSSndCard *card;
    snprintf(pcmdev,sizeof(pcmdev),"%s","default");
    snprintf(mixdev,sizeof(mixdev),"%s","default");
    card=castor3snd_card_new(pcmdev,mixdev, 1);
    ms_snd_card_manager_add_card(m,card);    
    //printf("CAUTION: called castor3snd_detect\n");
}

static void *castor3snd_txthread(void *p)
{
    MSSndCard *card=(MSSndCard*)p;
    Castor3SndData *d=(Castor3SndData*)card->data;
    int bsize;
    uint8_t *wtmpbuff=NULL;
    int err;
    int totaldata = 0;
    uint32_t DA_r_pre;
    bool the_first_write = TRUE;
    if(d->rate == CFG_AUDIO_SAMPLING_RATE && d->read_started) 
        bsize=(256*d->rate)/8000;//16K call
    else
        bsize=(320*d->rate)/8000;// 8K call & wav play

    i2s_pause_DAC(0);

    i2s_set_direct_volperc(d->dac_vol);
    //I2S_DA32_SET_WP(I2S_DA32_GET_RP());

    wtmpbuff=(uint8_t*)malloc(bsize);
#if ENABLE_AUDIO_ENGENEER_MODEL    
    usleep(100000);
#endif
    while (d->write_started)
    {
        uint32_t DA_r, DA_w, DA_free;

        ms_mutex_lock(&d->da_mutex);
        err=ms_bufferizer_read(d->bufferizer,wtmpbuff,bsize);
        ms_mutex_unlock(&d->da_mutex);
        if (err!=bsize) // with ms_bufferizer_read only two values are allowed for return err, 0 or bsize
        {
            if (d->read_started)
            {
                usleep(1000); // for calling case
            }
            else
            {
                usleep(100000); // for playing case, wait for data readed from file
            }
            continue;
        }

        if (the_first_write)
        {

            #if CFG_MP3_RING
                if (linphonec_mp3_is_ringing()==0)
                {
                    I2S_DA32_SET_WP(I2S_DA32_GET_RP());
                }

            #else
                    I2S_DA32_SET_WP(I2S_DA32_GET_RP());
            #endif
            DA_r_pre = I2S_DA32_GET_RP();
            the_first_write = FALSE;
        }
        else
        {    
            if (d->read_started)
            {
                // for calling case, do nothing because of too much sleep makes more delays
            }
            else
            {
                // for playing case, add sleep time here to avoid filling too much data into i2s dac buffer shortly
                // sleep 20000 will get broken sound
                usleep(18000);
            }
        }

        while (d->write_started)
        {
            DA_r = I2S_DA32_GET_RP();
            DA_w = I2S_DA32_GET_WP();            
            DA_free = (DA_w >= DA_r) ? ((d->dac_buf_len - DA_w) + DA_r): (DA_r - DA_w);

            if (!d->read_started && DA_free < (d->dac_buf_len/4))//workaround 
                usleep(18000);//when DA_free almost full add sleep time avoid audio miss
            
            if (DA_free >= (uint32_t)bsize)
                break;

            printf("i2s buffer full, rpt = %d, wpt = %d\n", DA_r, DA_w);
            #if CFG_MP3_RING
                if (linphonec_mp3_is_ringing()==0)
                {
                    I2S_DA32_SET_WP(I2S_DA32_GET_RP());
                }

            #else
                    I2S_DA32_SET_WP(I2S_DA32_GET_RP());
            #endif

            
    //        I2S_DA32_SET_WP(I2S_DA32_GET_RP());
        }
        
        if (d->write_started == FALSE)
            break;

        //ms_mutex_lock(&d->da_mutex);

        //printf("bsize=%d dac_buf_len=%d DA_r=%d DA_w=%d DA_free=%d\n", bsize, d->dac_buf_len, DA_r, DA_w, DA_free);

        /* write to sound device! */
        if ((DA_w + bsize) > (uint32_t)d->dac_buf_len)
        {
            int szbuf = d->dac_buf_len - DA_w;

            if (szbuf > 0)
            {
                memcpy(d->dac_buf + DA_w, wtmpbuff, szbuf);
                ithFlushDCacheRange(d->dac_buf+DA_w, szbuf);
            }
            DA_w = bsize - szbuf;
            memcpy(d->dac_buf, wtmpbuff + szbuf, DA_w);
            ithFlushDCacheRange(d->dac_buf, DA_w);
        }
        else
        {
            memcpy(d->dac_buf + DA_w, wtmpbuff, bsize);
            ithFlushDCacheRange(d->dac_buf+DA_w, bsize);
            DA_w += bsize;
        }
        
        if (DA_w == d->dac_buf_len)
            DA_w = 0;

    #if CFG_MP3_RING
        if (linphonec_mp3_is_ringing()==0)
        {
            I2S_DA32_SET_WP(DA_w);
        }

    #else
        I2S_DA32_SET_WP(DA_w);
    #endif
        if(d->datalength != -1){
            int shift;
            DA_r = I2S_DA32_GET_RP();
            shift = (DA_r >= DA_r_pre) ? (DA_r-DA_r_pre): (DA_r + d->dac_buf_len-DA_r_pre);
            totaldata += shift; 
            DA_r_pre = DA_r;
            if(totaldata >= d->datalength ){//play EOF 
                d->datalength = -1;
                d->write_EOF = TRUE;
            }
        }
        //ms_mutex_unlock(&d->da_mutex);
    }

    /* close sound card */
    //ms_error("Shutting down sound device (input-output: %i) (notplayed: %i)", d->stat_input - d->stat_output, d->stat_notplayed);   
    free(wtmpbuff);
    i2s_pause_DAC(1);

    //d->stat_output=0;
    //d->stat_notplayed=0;

    return NULL;
}

static void *castor3snd_rxthread(void *p)
{
    MSSndCard *card = (MSSndCard*)p;
    Castor3SndData *d = (Castor3SndData*)card->data;

    //d->stat_input=0;
    //d->stat_notplayed=0;
    i2s_pause_ADC(0);
#if ENABLE_AUDIO_ENGENEER_MODEL        
    usleep(100000);
#endif
    while (d->read_started)
    {
        mblk_t *rm = NULL;
#if (CFG_CHIP_FAMILY == 9910 || CFG_CHIP_FAMILY == 9850)
		uint32_t AD_r = I2S_AD32_GET_RP();
        uint32_t AD_w = I2S_AD32_GET_WP();
#else
        uint16_t AD_r = I2S_AD16_GET_RP();
        uint16_t AD_w = I2S_AD16_GET_WP();
#endif
        uint32_t bsize = 0;
        uint32_t sleep_time = 20000;

    #ifdef CFG_I2S_USE_GPIO_MODE_2
        if (AD_r <= AD_w)
        {
            bsize = AD_w - AD_r;
            if (bsize)
            {
                //printf("AD_r %u, AD_w %u bsize %u\n", AD_r, AD_w, bsize);
                rm = allocb(bsize, 0);
                ithInvalidateDCacheRange(d->adc_buf + AD_r, bsize);
                memcpy(rm->b_wptr, d->adc_buf + AD_r, bsize);
                rm->b_wptr += bsize;
                AD_r += bsize;
#if (CFG_CHIP_FAMILY == 9910 || CFG_CHIP_FAMILY == 9850)
				I2S_AD32_SET_RP(AD_r);
#else
                I2S_AD16_SET_RP(AD_r);
#endif
                //d->bytes_read += bsize;
            }
        }
        else
        { // AD_r > AD_w
            bsize = (d->adc_buf_len - AD_r) + AD_w;
            if (bsize)
            {
                //printf("AD_r %u, AD_w %u bsize %u adc_buf_len %u\n", AD_r, AD_w, bsize, d->adc_buf_len);
                uint32_t szsec0 = d->adc_buf_len - AD_r;
                uint32_t szsec1 = bsize - szsec0;
                rm = allocb(bsize, 0);
                if (szsec0)
                {
                    ithInvalidateDCacheRange(d->adc_buf + AD_r, szsec0);
                    memcpy(rm->b_wptr, d->adc_buf + AD_r, szsec0);
                }
                ithInvalidateDCacheRange(d->adc_buf, szsec1);
                memcpy(rm->b_wptr + szsec0, d->adc_buf, szsec1);
                rm->b_wptr += bsize;
                AD_r = szsec1;
#if (CFG_CHIP_FAMILY == 9910 || CFG_CHIP_FAMILY == 9850)
				I2S_AD32_SET_RP(AD_r);
#else
                I2S_AD16_SET_RP(AD_r);
#endif
                //d->bytes_read += bsize;
            }
        }
    #else
        // dummy data
        bsize = 160;
        rm = allocb(bsize, 0);
        rm->b_wptr += bsize;
        //d->bytes_read += bsize;
    #endif // CFG_I2S_USE_GPIO_MODE_2
        if (rm)
        {
            if (d->read_started == FALSE)
            {
                freeb(rm);
                break;
            }
            ms_mutex_lock(&d->ad_mutex);
            putq(&d->rq, rm);
            ms_mutex_unlock(&d->ad_mutex);
            //d->stat_input++;
            rm = NULL;
        }
        //else
        //{
            //sleep_time = (320 - bsize)*20000/320; // only for sample rate 8000
        //}
#ifdef CFG_CHIP_PKG_IT9910
        usleep(1000);
#else
        usleep(sleep_time);
#endif        
    }

    i2s_pause_ADC(1);
#if (CFG_CHIP_FAMILY == 9910 || CFG_CHIP_FAMILY == 9850)
				I2S_AD32_SET_RP(I2S_AD32_GET_WP());
#else
                I2S_AD16_SET_RP(I2S_AD16_GET_WP());
#endif    
    
    //d->stat_input=0;
    //d->stat_notplayed=0;

    return NULL;
}

static void castor3snd_start_r(MSSndCard *card){
    Castor3SndData *d=(Castor3SndData*)card->data;
    if (d->read_started==FALSE){
        pthread_attr_t attr;
        //struct sched_param param;
        d->read_started=TRUE;
        pthread_attr_init(&attr);
        //param.sched_priority = sched_get_priority_min(0) + 1;
        //pthread_attr_setschedparam(&attr, &param);
        ms_thread_create(&d->rxthread, &attr, castor3snd_rxthread, card);
    }
}

static void castor3snd_stop_r(MSSndCard *card){
    Castor3SndData *d=(Castor3SndData*)card->data;
    d->read_started=FALSE;
    ms_thread_join(d->rxthread,NULL);
}

static void castor3snd_start_w(MSSndCard *card){
    Castor3SndData *d=(Castor3SndData*)card->data;
    if (d->write_started==FALSE){
        pthread_attr_t attr;
        //struct sched_param param;
        d->write_started=TRUE;
        pthread_attr_init(&attr);
        //param.sched_priority = sched_get_priority_min(0) + 1;
        //pthread_attr_setschedparam(&attr, &param);
        ms_thread_create(&d->txthread, &attr, castor3snd_txthread, card);
    }
}

static void castor3snd_stop_w(MSSndCard *card){
    Castor3SndData *d=(Castor3SndData*)card->data;
    d->write_started=FALSE;
    ms_thread_join(d->txthread,NULL);
}

static mblk_t *castor3snd_get(MSSndCard *card){
    Castor3SndData *d=(Castor3SndData*)card->data;
    mblk_t *m;
    ms_mutex_lock(&d->ad_mutex);
    m=getq(&d->rq);
    ms_mutex_unlock(&d->ad_mutex);
    return m;
}

static void castor3snd_put(MSSndCard *card, mblk_t *m){
    Castor3SndData *d=(Castor3SndData*)card->data;
    ms_mutex_lock(&d->da_mutex);
    ms_bufferizer_put(d->bufferizer,m);
    ms_mutex_unlock(&d->da_mutex);
}

static void castor3snd_read_preprocess(MSFilter *f){
    MSSndCard *card=(MSSndCard*)f->data;
//printf("### [%s] preprocess\n", f->desc->name);
    castor3snd_start_r(card);
    //ms_ticker_set_time_func(f->ticker,castor3snd_get_cur_time,card->data);
}

static void castor3snd_read_postprocess(MSFilter *f){
    MSSndCard *card=(MSSndCard*)f->data;
    Castor3SndData *d=(Castor3SndData*)card->data;
//printf("### [%s] postprocess\n", f->desc->name);
    //ms_ticker_set_time_func(f->ticker,NULL,NULL);
    castor3snd_stop_r(card);
    memset(d->adc_buf, 0, d->adc_buf_len);
    flushq(&d->rq,0);
}

static void castor3snd_read_process(MSFilter *f){
    MSSndCard *card=(MSSndCard*)f->data;
    mblk_t *m;
//printf("### [%s] process\n", f->desc->name);
    while((m=castor3snd_get(card))!=NULL){
        ms_queue_put(f->outputs[0],m);
    }
}

static void castor3snd_write_preprocess(MSFilter *f){
    MSSndCard *card=(MSSndCard*)f->data;
//printf("### [%s] preprocess\n", f->desc->name);
    castor3snd_start_w(card);
}

static void castor3snd_write_postprocess(MSFilter *f){
    MSSndCard *card=(MSSndCard*)f->data;
    Castor3SndData *d=(Castor3SndData*)card->data;
//printf("### [%s] postprocess\n", f->desc->name);
    castor3snd_stop_w(card);
    memset(d->dac_buf, 0, d->dac_buf_len);
    d->datalength = -1;
    d->write_EOF = FALSE;
    ms_bufferizer_flush(d->bufferizer);    
}

static void castor3snd_write_process(MSFilter *f){
    MSSndCard *card=(MSSndCard*)f->data;
    Castor3SndData *d=(Castor3SndData*)card->data;
    mblk_t *m;
//printf("### [%s] process\n", f->desc->name);
    while((m=ms_queue_get(f->inputs[0]))!=NULL){
        castor3snd_put(card,m);
    }
    if(d->write_EOF){
        ms_filter_notify_no_arg(f,MS_FILE_PLAYER_EOF);
        d->write_EOF = FALSE ;//play EOF reset to FALSE;
    }
}

static int set_rate(MSFilter *f, void *arg){
    MSSndCard *card=(MSSndCard*)f->data;
    Castor3SndData *d=(Castor3SndData*)card->data;
    d->rate=*((int*)arg);
//printf("### [%s] set sample rate %d\n", f->desc->name, d->rate);
    return 0;
}

static int set_nchannels(MSFilter *f, void *arg){
    MSSndCard *card=(MSSndCard*)f->data;
    Castor3SndData *d=(Castor3SndData*)card->data;
    d->stereo=(*((int*)arg)==2);
//printf("### [%s] set channels %d\n", f->desc->name, *(int*)arg);
    return 0;
}

static int set_datalength(MSFilter *f, void *arg){
    MSSndCard *card=(MSSndCard*)f->data;
    Castor3SndData *d=(Castor3SndData*)card->data;
    d->datalength = *((int*)arg) ;
//    printf("### [%s] set datalength = %d\n", f->desc->name, d->datalength);
    return 0;
}

static int get_rate(MSFilter *f, void *arg)
{
    MSSndCard *card = (MSSndCard*)f->data;
    Castor3SndData *d = (Castor3SndData*)card->data;
    *(int*)arg = d->rate;
    return 0;
}

static int get_nchannels(MSFilter *f, void *arg)
{
    MSSndCard *card = (MSSndCard*)f->data;
    Castor3SndData *d = (Castor3SndData*)card->data;
    *(int*)arg = d->stereo? 2: 1;
    return 0;
}

//static int castor3snd_get_stat_input(MSFilter *f, void *arg){
//    MSSndCard *card=(MSSndCard*)f->data;
//    Castor3SndData *d=(Castor3SndData*)card->data;
//
//    return d->stat_input;
//}

//static int castor3snd_get_stat_ouptut(MSFilter *f, void *arg){
//    MSSndCard *card=(MSSndCard*)f->data;
//    Castor3SndData *d=(Castor3SndData*)card->data;
//
//    return d->stat_output;
//}

//static int castor3snd_get_stat_discarded(MSFilter *f, void *arg){
//    MSSndCard *card=(MSSndCard*)f->data;
//    Castor3SndData *d=(Castor3SndData*)card->data;
//
//    return d->stat_notplayed;
//}

static MSFilterMethod castor3snd_methods[]={
    {    MS_FILTER_SET_SAMPLE_RATE    , set_rate    },
    {    MS_FILTER_SET_NCHANNELS        , set_nchannels    },
    {    MS_FILTER_GET_SAMPLE_RATE    , get_rate    },
    {    MS_FILTER_GET_NCHANNELS        , get_nchannels    },
    {    MS_FILTER_SET_DATALENGTH        , set_datalength    },
    //{    MS_FILTER_GET_STAT_INPUT, castor3snd_get_stat_input },
    //{    MS_FILTER_GET_STAT_OUTPUT, castor3snd_get_stat_ouptut },
    //{    MS_FILTER_GET_STAT_DISCARDED, castor3snd_get_stat_discarded },
    {    0                , NULL        }
};

MSFilterDesc castor3snd_read_desc={
    MS_CASTOR3SND_READ_ID,
    "MSCastor3SndRead",
    "Sound capture filter for Windows Sound drivers",
    MS_FILTER_OTHER,
    NULL,
    0,
    1,
    NULL,
    castor3snd_read_preprocess,
    castor3snd_read_process,
    castor3snd_read_postprocess,
    NULL,
    castor3snd_methods
};

MSFilterDesc castor3snd_write_desc={
    MS_CASTOR3SND_WRITE_ID,
    "MSCastor3SndWrite",
    "Sound playback filter for Castor3 Sound drivers",
    MS_FILTER_OTHER,
    NULL,
    1,
    0,
    NULL,
    castor3snd_write_preprocess,
    castor3snd_write_process,
    castor3snd_write_postprocess,
    NULL,
    castor3snd_methods
};

MSFilter *ms_castor3snd_read_new(MSSndCard *card){
    MSFilter *f=ms_filter_new_from_desc(&castor3snd_read_desc);
    f->data=card;
    return f;
}

MSFilter *ms_castor3snd_write_new(MSSndCard *card){
    MSFilter *f=ms_filter_new_from_desc(&castor3snd_write_desc);
    f->data=card;
    return f;
}

MS_FILTER_DESC_EXPORT(castor3snd_read_desc)
MS_FILTER_DESC_EXPORT(castor3snd_write_desc)

void castor3snd_deinit_for_video_memo_play(void)
{
    i2s_deinit_ADC();
    i2s_deinit_DAC();
}
void castor3snd_reinit_for_video_memo_play(void)
{
    iteAudioOpenEngine(ITE_SBC_CODEC);
    i2s_init_DAC(&spec_da);
    i2s_init_ADC(&spec_ad);
    i2s_pause_ADC(1);
}

void Castor3snd_reinit_for_diff_rate(int rate,int bitsize)
{
    if (spec_da.sample_rate == rate && spec_da.sample_size == bitsize)
        return ;

    i2s_deinit_ADC();
    i2s_deinit_DAC();
    spec_da.sample_rate = rate ;
    spec_da.sample_size = bitsize ;
    // channel bit etc. can be changed here;
    i2s_init_DAC(&spec_da);
    i2s_init_ADC(&spec_ad);
    i2s_pause_ADC(1);

}
