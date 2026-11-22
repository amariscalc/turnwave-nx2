#include "audio.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static int started;
static AudioOutBuffer *queue[32];
static int pending;
static u64 played;
Result audoutInitialize(void){return 0;}
void audoutExit(void){}
u32 audoutGetSampleRate(void){return 48000;}
u32 audoutGetChannelCount(void){return 2;}
Result audoutStartAudioOut(void){started=1;return 0;}
Result audoutStopAudioOut(void){started=0;return 0;}
Result audoutAppendAudioOutBuffer(AudioOutBuffer *b){
    assert(started && b && pending<32 && b->data_size<=b->buffer_size);
    queue[pending++]=b;
    return 0;
}
Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer **b,u32 *count){*b=NULL;*count=0;return 0;}
Result audoutContainsAudioOutBuffer(AudioOutBuffer *b,bool *contains){
    *contains=0;
    for(int i=0;i<pending;i++)if(queue[i]==b){*contains=1;break;}
    return 0;
}
Result audoutFlushAudioOutBuffers(bool *flushed){pending=0;*flushed=1;return 0;}
Result audoutGetAudioOutPlayedSampleCount(u64 *count){*count=played;return 0;}
static void advance(void){
    assert(pending);
    played+=queue[0]->data_size/4;
    for(int i=1;i<pending;i++)queue[i-1]=queue[i];
    pending--;
}
int main(int argc,char **argv){
    assert(argc==2);
    TwAudio audio;assert(tw_audio_init(&audio)==0);
    char err[200];assert(tw_audio_open(&audio,argv[1],err,sizeof err)==0);
    assert(audio.playing && pending>=2);
    assert(tw_audio_pause(&audio)==0 && audio.paused && !started);
    assert(tw_audio_pause(&audio)==0 && !audio.paused && started);
    int runs=0;
    while(audio.playing && runs++<1000){
        advance();tw_audio_pump(&audio);
    }
    assert(runs<1000 && audio.finished && !audio.playing);
    assert(played==6*48000u);
    assert(tw_audio_open(&audio,argv[1],err,sizeof err)==0);
    assert(audio.playing);
    tw_audio_stop(&audio);
    assert(!audio.playing && !pending);
    tw_audio_shutdown(&audio);
    assert(!audio.initialized);
    puts("PASS: mock audout queue, sample accounting, pause/resume, EOF, reopen, stop, shutdown");
    return 0;
}
