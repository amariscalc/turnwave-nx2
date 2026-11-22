#include "audio.h"
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <stdio.h>

int tw_audio_init(TwAudio *a) {
    memset(a,0,sizeof *a);
    Result rc=audoutInitialize();
    if(R_FAILED(rc)){a->result=rc;return -1;}
    a->initialized=1;
    if(audoutGetSampleRate()!=48000 || audoutGetChannelCount()!=2){a->result=0xFFFFFFFFu;tw_audio_shutdown(a);return -1;}
    for(int i=0;i<TW_AUDIO_BUFFERS;i++){
        a->memory[i]=memalign(0x1000,TW_AUDIO_BUFFER_BYTES);
        if(!a->memory[i]){a->result=0xFFFFFFFEu;tw_audio_shutdown(a);return -1;}
        memset(a->memory[i],0,TW_AUDIO_BUFFER_BYTES);
        a->buffers[i].buffer=a->memory[i];
        a->buffers[i].buffer_size=TW_AUDIO_BUFFER_BYTES;
    }
    return 0;
}
static int inflight(TwAudio *a){int n=0;for(int i=0;i<TW_AUDIO_BUFFERS;i++)n+=a->queued[i];return n;}
void tw_audio_stop(TwAudio *a) {
    if(!a) return;
    if(a->initialized){
        audoutStopAudioOut();
        bool flushed=false;
        audoutFlushAudioOutBuffers(&flushed);
        /* Buffers remain allocated until shutdown, even if the service is slow to release. */
        for(int i=0;i<TW_AUDIO_BUFFERS;i++)a->queued[i]=0;
    }
    tw_wav_close(&a->wav);
    a->playing=a->paused=a->eof=a->finished=0;
    a->submitted_frames=0;
}
int tw_audio_open(TwAudio *a,const char *filename,char *error,size_t esz) {
    if(!a || !a->initialized){if(error&&esz)snprintf(error,esz,"Audio service unavailable");return -1;}
    tw_audio_stop(a);
    if(tw_wav_open(&a->wav,filename,error,esz))return -1;
    Result rc=audoutStartAudioOut();
    if(R_FAILED(rc)){a->result=rc;tw_audio_stop(a);if(error&&esz)snprintf(error,esz,"audoutStart failed: %08x",(unsigned)rc);return -1;}
    a->playing=1;
    audoutGetAudioOutPlayedSampleCount(&a->played_origin);
    tw_audio_pump(a);
    if(error&&esz)error[0]=0;
    return 0;
}
void tw_audio_pump(TwAudio *a) {
    if(!a||!a->initialized||!a->playing||a->paused)return;
    AudioOutBuffer *released=NULL;u32 count=0;
    Result rc=audoutGetReleasedAudioOutBuffer(&released,&count);
    if(R_FAILED(rc)){a->result=rc;return;}
    /* Some service versions return a list; others expose only the first.
       Check ownership with audoutContainsAudioOutBuffer before reusing memory. */
    for(int i=0;i<TW_AUDIO_BUFFERS;i++){
        if(!a->queued[i])continue;
        bool contains=true;
        if(R_SUCCEEDED(audoutContainsAudioOutBuffer(&a->buffers[i],&contains))&&!contains)
            a->queued[i]=0;
    }
    /* Even if the service returned a count >1, never reuse DMA memory still queued. */
    for(int i=0;i<TW_AUDIO_BUFFERS&&!a->eof;i++){
        if(a->queued[i])continue;
        size_t bytes=tw_wav_read(&a->wav,a->memory[i],TW_AUDIO_BUFFER_BYTES);
        if(!bytes){a->eof=1;break;}
        a->buffers[i].data_offset=0;
        a->buffers[i].data_size=bytes;
        rc=audoutAppendAudioOutBuffer(&a->buffers[i]);
        if(R_FAILED(rc)){a->result=rc;a->eof=1;break;}
        a->queued[i]=1;
        a->submitted_frames+=bytes/4;
        if(a->wav.bytes_read>=a->wav.data_bytes)a->eof=1;
    }
    if(a->eof&&!inflight(a)) {
        tw_wav_close(&a->wav);a->playing=0;a->finished=1;
        audoutStopAudioOut();
    }
}
int tw_audio_pause(TwAudio *a) {
    if(!a||!a->playing)return -1;
    Result rc=a->paused?audoutStartAudioOut():audoutStopAudioOut();
    if(R_FAILED(rc)){a->result=rc;return -1;}
    a->paused=!a->paused;
    return 0;
}
uint32_t tw_audio_position_seconds(const TwAudio *a) {
    if(!a||!a->playing)return 0;
    u64 played=0;
    if(R_SUCCEEDED(audoutGetAudioOutPlayedSampleCount(&played)) && played>=a->played_origin)
        return (uint32_t)((played-a->played_origin)/48000u);
    return 0;
}
uint32_t tw_audio_duration_seconds(const TwAudio *a) {
    return a && a->wav.data_bytes ? a->wav.data_bytes/192000u : 0;
}
void tw_audio_shutdown(TwAudio *a){
    if(!a)return;
    tw_audio_stop(a);
    if(a->initialized)audoutExit();
    for(int i=0;i<TW_AUDIO_BUFFERS;i++){free(a->memory[i]);a->memory[i]=NULL;}
    a->initialized=0;
}
