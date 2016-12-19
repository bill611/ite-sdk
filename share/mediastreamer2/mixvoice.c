
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/dtmfgen.h"

static unsigned char* pcm_buf;
static int flag = 0;
static int end = 0;
static int data_size; 

static int wav_file_open(MSFilter *f, void *arg){
    const char *filepath=(const char *)arg;  
    char* buf;
    int i;
    FILE *fp;
    
    if(!pcm_buf){
        fp=fopen(filepath,"rb");
        buf=malloc(50);
        fread(buf, 1, 44, fp);
        data_size = *((int*)&buf[40]);
#if (CFG_CHIP_FAMILY == 9910)
        data_size = (((data_size) & 0x000000FF) << 24) | \
                    (((data_size) & 0x0000FF00) <<  8) | \
                    (((data_size) & 0x00FF0000) >>  8) | \
                    (((data_size) & 0xFF000000) >> 24);    
#endif
        pcm_buf=(unsigned char*)malloc(data_size);
        fread(pcm_buf, 1, data_size, fp);
#if (CFG_CHIP_FAMILY == 9910)    
        for (i=0;i<data_size;i+=2)
        {
            pcm_buf[i]  = pcm_buf[i]^pcm_buf[i+1];
            pcm_buf[i+1]= pcm_buf[i]^pcm_buf[i+1];
            pcm_buf[i]  = pcm_buf[i]^pcm_buf[i+1];
        }//big endian
#endif
        fclose(fp);
        free(buf);
    }
    
    flag = 1;
    return 0;
} 

static void mix_voice_process(MSFilter *f){
    
    mblk_t *m;
    if(flag){
        while((m=ms_queue_get(f->inputs[0]))!=NULL){
            mblk_t *o;
            msgpullup(m,-1);
            o=allocb(m->b_wptr-m->b_rptr,0);
            mblk_meta_copy(m, o);
            if(flag){
                for(;m->b_rptr<m->b_wptr;m->b_rptr+=2,o->b_wptr+=2,end+=2){
                    if(end < data_size){//mixsound
                    *((int16_t*)(o->b_wptr))=(*((int16_t*)(pcm_buf+end)))*(0.3)+(0.7)*((int)*(int16_t*)m->b_rptr);
                    }else{//mixsound over
                        for(;m->b_rptr<m->b_wptr;m->b_rptr+=2,o->b_wptr+=2){
                            *((int16_t*)(o->b_wptr))=(int)*(int16_t*)m->b_rptr;
                        }
                    }
                }
            }else{//no mixsound
                for(;m->b_rptr<m->b_wptr;m->b_rptr+=2,o->b_wptr+=2){
                    *((int16_t*)(o->b_wptr))=(int)*(int16_t*)m->b_rptr;
                }              
            }
            if(end > data_size){
                end=0;
                flag=0;
            };
            freemsg(m);
            ms_queue_put(f->outputs[0],o);
        }
    }else{
        while((m=ms_queue_get(f->inputs[0]))!=NULL){
        ms_queue_put(f->outputs[0],m);
         }
    }
    
}

static void mix_voice_postprocess(MSFilter *f){
    if(flag){
            end=0;
            flag=0;
            //free(pcm_buf);
    }
}

 static MSFilterMethod mixvoice_methods[]={
    {   MS_WAV_FILE_OPEN        ,   wav_file_open},
    {   0               ,   NULL        }
}; 

#ifdef _MSC_VER

MSFilterDesc ms_mix_voice_desc={
    MS_MIXVOICE_ID,
    "MSMixVoice",
    N_("Mix the wave"),
    MS_FILTER_OTHER,
    NULL,
    1,
    1,
    NULL,
    NULL,
    mix_voice_process,
    mix_voice_postprocess,
    NULL,
    mixvoice_methods
};

#else

MSFilterDesc ms_mix_voice_desc={
    .id=MS_MIXVOICE_ID,
    .name="MSMixVoice",
    .text=N_("Mix the wave"),
    .category=MS_FILTER_OTHER,
    .ninputs=1,
    .noutputs=1,
    .process=mix_voice_process,
    .postprocess=mix_voice_postprocess,
    .methods=mixvoice_methods
};

#endif

MS_FILTER_DESC_EXPORT(ms_mix_voice_desc)