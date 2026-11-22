#ifndef TW_AUDIO_H
#define TW_AUDIO_H
#include <switch.h>
#include <stdint.h>
#include "wav.h"
#define TW_AUDIO_BUFFERS 4
#define TW_AUDIO_BUFFER_BYTES 16384u

typedef struct {
    TwWav wav;
    AudioOutBuffer buffers[TW_AUDIO_BUFFERS];
    void *memory[TW_AUDIO_BUFFERS];
    int queued[TW_AUDIO_BUFFERS];
    int initialized, playing, paused, eof, finished;
    uint32_t result;
    uint64_t played_origin, submitted_frames;
} TwAudio;

int tw_audio_init(TwAudio *a);
int tw_audio_open(TwAudio *a,const char *filename,char *error,size_t error_size);
void tw_audio_pump(TwAudio *a);
int tw_audio_pause(TwAudio *a);
void tw_audio_stop(TwAudio *a);
void tw_audio_shutdown(TwAudio *a);
uint32_t tw_audio_position_seconds(const TwAudio *a);
uint32_t tw_audio_duration_seconds(const TwAudio *a);
#endif
