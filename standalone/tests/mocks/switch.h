#ifndef MOCK_SWITCH_H
#define MOCK_SWITCH_H
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8;typedef uint32_t u32;typedef uint64_t u64;typedef uint32_t Result;
#define R_FAILED(x) ((x)!=0)
#define R_SUCCEEDED(x) ((x)==0)
#define PIXEL_FORMAT_RGBA_8888 0
#define HidNpadStyleSet_NpadStandard 0
#define HidNpadButton_Plus (1ull<<0)
#define HidNpadButton_B (1ull<<1)
#define HidNpadButton_L (1ull<<2)
#define HidNpadButton_R (1ull<<3)
#define HidNpadButton_A (1ull<<4)
#define HidNpadButton_X (1ull<<5)
#define HidNpadButton_Y (1ull<<6)
#define HidNpadButton_AnyUp (1ull<<7)
#define HidNpadButton_AnyDown (1ull<<8)
typedef struct {int reserved;} PadState;
typedef struct {int reserved;} Framebuffer;
typedef struct {u32 x,y;} HidTouchState;
typedef struct {int count;HidTouchState touches[16];} HidTouchScreenState;
typedef struct AudioOutBuffer AudioOutBuffer;
struct AudioOutBuffer { AudioOutBuffer *next;void *buffer;u64 buffer_size,data_size,data_offset; };
void padConfigureInput(int,int);void padInitializeAny(PadState*);void padUpdate(PadState*);
u64 padGetButtonsDown(PadState*);
int appletMainLoop(void);void svcSleepThread(u64);void *nwindowGetDefault(void);
Result framebufferCreate(Framebuffer*,void*,int,int,int,int);
Result framebufferMakeLinear(Framebuffer*);u8 *framebufferBegin(Framebuffer*,u32*);
void framebufferEnd(Framebuffer*);void framebufferClose(Framebuffer*);
Result socketInitializeDefault(void);void socketExit(void);
void hidInitializeTouchScreen(void);size_t hidGetTouchScreenStates(HidTouchScreenState*,size_t);
Result audoutInitialize(void);void audoutExit(void);u32 audoutGetSampleRate(void);u32 audoutGetChannelCount(void);
Result audoutStartAudioOut(void);Result audoutStopAudioOut(void);
Result audoutAppendAudioOutBuffer(AudioOutBuffer*);
Result audoutGetReleasedAudioOutBuffer(AudioOutBuffer**,u32*);
Result audoutContainsAudioOutBuffer(AudioOutBuffer*,bool*);
Result audoutFlushAudioOutBuffers(bool*);
Result audoutGetAudioOutPlayedSampleCount(u64*);
#endif
