#include "platform.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#ifdef __SWITCH2__
#include <switch.h>
#include <malloc.h>
static void require(Result rc,const char* what) {
    if (R_FAILED(rc)) { char msg[96]; std::snprintf(msg,sizeof msg,"%s (0x%08x)",what,unsigned(rc)); throw std::runtime_error(msg); }
}
struct Platform::Impl {
    PadState pad{};
    Framebuffer fb{};
    bool video=false,audio=false,playing=false,touch=false;
    AudioOutBuffer buf[bufferCount]{};
    bool busy[bufferCount]{};
    void reap() {
        if(!audio) return;
        for(int i=0;i<bufferCount;i++) {
            AudioOutBuffer* released=nullptr; u32 count=0;
            require(audoutGetReleasedAudioOutBuffer(&released,&count),"Audio buffer status");
            if(!count || !released) break;
            for(int k=0;k<bufferCount;k++) if(released==&buf[k]) busy[k]=false;
        }
    }
    void start() {
        if(!audio) { require(audoutInitialize(),"Open audio output"); audio=true; }
        if(!playing) { require(audoutStartAudioOut(),"Start audio output"); playing=true; }
    }
};
Platform::Platform():p(new Impl) {}
void Platform::open() {
    padConfigureInput(1,HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&p->pad);
    hidInitializeTouchScreen();
    require(framebufferCreate(&p->fb,nwindowGetDefault(),1280,720,PIXEL_FORMAT_BGRA_8888,2),"Open display");
    p->video=true;
    require(framebufferMakeLinear(&p->fb),"Prepare display");
    for(auto& b:p->buf) {
        b.buffer_size=blockFrames*4;
        b.buffer=memalign(4096,b.buffer_size);
        if(!b.buffer) throw std::bad_alloc();
    }
}
Platform::~Platform() {
    clearAudio();
    for(auto& b:p->buf) std::free(b.buffer);
    if(p->video) framebufferClose(&p->fb);
    delete p;
}
void Platform::clearAudio() {
    if(p->audio) {
        if(p->playing) audoutStopAudioOut();
        audoutExit();
        p->audio=p->playing=false;
    }
    std::fill_n(p->busy,bufferCount,false);
}
void Platform::pause(bool paused) {
    if(paused && p->playing) { require(audoutStopAudioOut(),"Pause playback"); p->playing=false; }
    if(!paused) p->start();
}
int Platform::queued() {
    p->reap();
    int count=0; for(int i=0;i<bufferCount;i++) if(p->busy[i]) count+=p->buf[i].data_size/4;
    return count;
}
bool Platform::canQueue() {p->reap();return std::find(p->busy,p->busy+bufferCount,false)!=p->busy+bufferCount;}
void Platform::submit(const int16_t* pcm,int frames) {
    p->start();
    for(int i=0;i<bufferCount;i++) if(!p->busy[i]) {
        auto& b=p->buf[i]; b.data_size=frames*4;
        std::memcpy(b.buffer,pcm,b.data_size);
        armDCacheFlush(b.buffer,b.data_size);
        require(audoutAppendAudioOutBuffer(&b),"Queue audio");
        p->busy[i]=true; return;
    }
    throw std::runtime_error("Audio queue full");
}
std::vector<Input> Platform::poll() {
    if(!appletMainLoop()) return {{Key::Quit}};
    std::vector<Input> events;
    padUpdate(&p->pad);
    u64 keys=padGetButtonsDown(&p->pad);
    const std::pair<u64,Key> map[]={{HidNpadButton_Up,Key::Up},{HidNpadButton_Down,Key::Down},
        {HidNpadButton_Left,Key::Left},{HidNpadButton_Right,Key::Right},{HidNpadButton_A,Key::Accept},
        {HidNpadButton_B,Key::Back},{HidNpadButton_X,Key::Browse},{HidNpadButton_Y,Key::Eq},
        {HidNpadButton_L,Key::Prev},{HidNpadButton_R,Key::Next},{HidNpadButton_ZL,Key::VolDown},
        {HidNpadButton_ZR,Key::VolUp},{HidNpadButton_StickL,Key::Shuffle},{HidNpadButton_StickR,Key::Repeat},{HidNpadButton_Minus,Key::Mix},{HidNpadButton_Plus,Key::Quit}};
    for(auto [mask,key]:map) if(keys&mask) events.push_back({key});
    HidTouchScreenState t{};
    bool down=hidGetTouchScreenStates(&t,1)>0 && t.count>0;
    if(down) events.push_back({p->touch?Key::Drag:Key::Tap,int(t.touches[0].x)*width/1280,int(t.touches[0].y)*height/720});
    p->touch=down; return events;
}
void Platform::present(const uint32_t* pixels) {
    u32 stride=0;
    auto* base=static_cast<uint8_t*>(framebufferBegin(&p->fb,&stride));
    if(!base) throw std::runtime_error("Display buffer unavailable");
    for(int y=0;y<height;y++) {
        auto* line=reinterpret_cast<uint32_t*>(base+2*y*stride);
        for(int x=0;x<width;x++) line[2*x]=line[2*x+1]=pixels[y*width+x];
        std::memcpy(base+(2*y+1)*stride,line,width*8);
    }
    framebufferEnd(&p->fb);
}
#else
#include <SDL2/SDL.h>
struct Platform::Impl {SDL_Window* window=nullptr; SDL_Renderer* renderer=nullptr; SDL_Texture* texture=nullptr; SDL_AudioDeviceID device=0;};
Platform::Platform():p(new Impl) {}
static void sdl(bool ok) {if(!ok) throw std::runtime_error(SDL_GetError());}
void Platform::open() {
    sdl(SDL_Init(SDL_INIT_AUDIO|SDL_INIT_VIDEO)==0);
    p->window=SDL_CreateWindow("Turnwave",SDL_WINDOWPOS_CENTERED,SDL_WINDOWPOS_CENTERED,width*2,height*2,0);
    sdl(p->window);
    p->renderer=SDL_CreateRenderer(p->window,-1,SDL_RENDERER_SOFTWARE); sdl(p->renderer);
    p->texture=SDL_CreateTexture(p->renderer,SDL_PIXELFORMAT_ARGB8888,SDL_TEXTUREACCESS_STREAMING,width,height);
    sdl(p->texture);
    SDL_AudioSpec spec{}; spec.freq=48000; spec.format=AUDIO_S16SYS; spec.channels=2; spec.samples=1024;
    p->device=SDL_OpenAudioDevice(nullptr,0,&spec,nullptr,0); sdl(p->device);
}
Platform::~Platform() {
    if(p->device) SDL_CloseAudioDevice(p->device);
    SDL_DestroyTexture(p->texture); SDL_DestroyRenderer(p->renderer); SDL_DestroyWindow(p->window); SDL_Quit(); delete p;
}
void Platform::clearAudio(){if(p->device) SDL_ClearQueuedAudio(p->device);}
void Platform::pause(bool paused){SDL_PauseAudioDevice(p->device,paused);}
int Platform::queued(){return SDL_GetQueuedAudioSize(p->device)/4;}
bool Platform::canQueue(){return queued()<(bufferCount-1)*blockFrames;}
void Platform::submit(const int16_t* pcm,int frames){sdl(SDL_QueueAudio(p->device,pcm,frames*4)==0);}
void Platform::present(const uint32_t* pixels) {
    sdl(SDL_UpdateTexture(p->texture,nullptr,pixels,width*4)==0);
    SDL_RenderCopy(p->renderer,p->texture,nullptr,nullptr); SDL_RenderPresent(p->renderer); SDL_Delay(16);
}
std::vector<Input> Platform::poll() {
    std::vector<Input> out;
    SDL_Event e;
    while(SDL_PollEvent(&e)) {
        if(e.type==SDL_QUIT) out.push_back({Key::Quit});
        if(e.type==SDL_MOUSEBUTTONDOWN && e.button.button==SDL_BUTTON_LEFT) out.push_back({Key::Tap,e.button.x/2,e.button.y/2});
        if(e.type==SDL_MOUSEMOTION && (e.motion.state&SDL_BUTTON_LMASK)) out.push_back({Key::Drag,e.motion.x/2,e.motion.y/2});
        if(e.type!=SDL_KEYDOWN) continue;
        const std::pair<int,Key> map[]={{SDLK_UP,Key::Up},{SDLK_DOWN,Key::Down},{SDLK_LEFT,Key::Left},{SDLK_RIGHT,Key::Right},
            {SDLK_RETURN,Key::Accept},{SDLK_SPACE,Key::Accept},{SDLK_ESCAPE,Key::Back},{SDLK_x,Key::Browse},{SDLK_y,Key::Eq},
            {SDLK_LEFTBRACKET,Key::Prev},{SDLK_RIGHTBRACKET,Key::Next},{SDLK_MINUS,Key::VolDown},{SDLK_EQUALS,Key::VolUp},{SDLK_s,Key::Shuffle},{SDLK_r,Key::Repeat},{SDLK_m,Key::Mix},{SDLK_q,Key::Quit}};
        for(auto [code,key]:map) if(e.key.keysym.sym==code) out.push_back({key});
    }
    return out;
}
#endif
