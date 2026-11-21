#ifndef MOCK_SWITCH_H
#define MOCK_SWITCH_H
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int Result;
typedef struct {int mock;} Framebuffer;
typedef struct {int mock;} PadState;
#define PIXEL_FORMAT_RGBA_8888 1
#define HidNpadStyleSet_NpadStandard 1
#define HidNpadButton_Plus (1ULL<<1)
#define HidNpadButton_B (1ULL<<2)
#define HidNpadButton_X (1ULL<<3)
#define HidNpadButton_A (1ULL<<4)
#define HidNpadButton_Y (1ULL<<5)
#define HidNpadButton_L (1ULL<<6)
#define HidNpadButton_R (1ULL<<7)
#define HidNpadButton_AnyUp (1ULL<<8)
#define HidNpadButton_AnyDown (1ULL<<9)
#define R_FAILED(x) ((x)!=0)
void padConfigureInput(int,int);
void padInitializeAny(PadState *);
void padUpdate(PadState *);
u64 padGetButtonsDown(PadState *);
void *nwindowGetDefault(void);
Result framebufferCreate(Framebuffer *,void*,int,int,int,int);
Result framebufferMakeLinear(Framebuffer *);
u8 *framebufferBegin(Framebuffer *,u32 *);
void framebufferEnd(Framebuffer *);
void framebufferClose(Framebuffer *);
int appletMainLoop(void);
void svcSleepThread(unsigned long long);
Result envSetNextLoad(const char *,const char *);
#endif
