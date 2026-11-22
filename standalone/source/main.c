/* Turnwave NX2 — standalone Horizon OS player.
 * Browser-free: local PCM output + Apple Music catalog metadata.
 * Full Apple Music protected playback is NOT implemented.
 */
#include <switch.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include "audio.h"
#include "library.h"
#include "catalog.h"

#define TW_DIR "sdmc:/switch/TurnwaveNX2"
#define MUSIC_DIR TW_DIR "/music"
#define WIDTH 1280
#define HEIGHT 720

extern const u8 tw_home[],tw_font16[],tw_font22[],tw_font32[];
extern const u8 tw_width16[],tw_width22[],tw_width32[];
static TwAudio audio;
static TwLocalLibrary library;
static TwCatalog catalog;
static int view=0, selection=0, first=0, now_index=-1;
static int net_ready=0,prev_touch=0;
static char status[232]="Ready";
static FILE *logfile;

static void log_status(const char *value,unsigned code) {
    if(logfile){fprintf(logfile,"%s result=%08x\n",value,code);fflush(logfile);}
}
static void set_status(const char *value){snprintf(status,sizeof status,"%s",value);}
static unsigned glyph_width(unsigned cp,const u8 *widths){return widths[cp-32];}
static int read_utf8(const char **p){
    const unsigned char *s=(const unsigned char *)*p;
    if(!s[0])return 0;
    if(s[0]<128){*p=(const char*)s+1;return s[0];}
    if((s[0]&0xe0)==0xc0 && s[1] && (s[1]&0xc0)==0x80){int c=((s[0]&31)<<6)|(s[1]&63);*p=(const char*)s+2;return c;}
    if((s[0]&0xf0)==0xe0 && s[1] && s[2] && (s[1]&0xc0)==0x80){int c=((s[0]&15)<<12)|((s[1]&63)<<6)|(s[2]&63);*p=(const char*)s+3;return c;}
    *p=(const char*)s+1;return '?';
}
static void text_at(u8 *pixels,u32 stride,int x,int y,int max_x,const char *s,int pts,u8 r,u8 g,u8 b){
    const u8 *glyphs=pts==32?tw_font32:pts==22?tw_font22:tw_font16;
    const u8 *widths=pts==32?tw_width32:pts==22?tw_width22:tw_width16;
    int cw=pts==32?50:pts==22?34:27,ch=pts==32?52:pts==22?38:30;
    if(!s)return;
    for(const char *p=s;*p;){
        int c=read_utf8(&p);if(c<32||c>255)c='?';
        int advance=(int)glyph_width((unsigned)c,widths);
        if(x+advance>max_x)break;
        const u8 *glyph=glyphs+(size_t)(c-32)*cw*ch;
        for(int yy=0;yy<ch && y+yy<HEIGHT;yy++){
            if(y+yy<0)continue;
            for(int xx=0;xx<cw && x+xx<WIDTH;xx++){
                if(x+xx<0)continue;
                u8 a=glyph[yy*cw+xx];if(!a)continue;
                u8 *dst=pixels+(size_t)(y+yy)*stride+(size_t)(x+xx)*4;
                dst[0]=(u8)((r*a+dst[0]*(255-a))/255);
                dst[1]=(u8)((g*a+dst[1]*(255-a))/255);
                dst[2]=(u8)((b*a+dst[2]*(255-a))/255);
            }
        }
        x+=advance;
    }
}
static void shade(u8 *pixels,u32 stride,int x,int y,int w,int h,u8 r,u8 g,u8 b,u8 alpha){
    for(int yy=y;yy<y+h&&yy<HEIGHT;yy++)if(yy>=0)
        for(int xx=x;xx<x+w&&xx<WIDTH;xx++)if(xx>=0){
            u8 *p=pixels+(size_t)yy*stride+(size_t)xx*4;
            p[0]=(u8)((r*alpha+p[0]*(255-alpha))/255);
            p[1]=(u8)((g*alpha+p[1]*(255-alpha))/255);
            p[2]=(u8)((b*alpha+p[2]*(255-alpha))/255);
        }
}
static int items(void){return view?catalog.count:library.count;}
static const char *item_title(int i){return view?catalog.songs[i].name:library.tracks[i].name;}
static int render(Framebuffer *fb){
    u32 stride;u8 *p=framebufferBegin(fb,&stride);if(!p)return -1;
    for(int y=0;y<HEIGHT;y++)memcpy(p+(size_t)y*stride,tw_home+(size_t)y*WIDTH*4,WIDTH*4);
    shade(p,stride,94,154,view?191:175,45,134,100,215,77);
    text_at(p,stride,106,163,320,view?"APPLE CATALOG":"LOCAL MUSIC",16,255,253,255);
    text_at(p,stride,322,163,700,view?"L/R  switch to local":"L/R  switch to catalog",16,207,200,237);
    char count_text[80];snprintf(count_text,sizeof count_text,"%d items",items());
    text_at(p,stride,105,213,700,count_text,16,196,196,224);
    for(int i=0;i<7;i++){
        int at=first+i;if(at>=items())break;
        int y=255+i*47;
        if(at==selection)shade(p,stride,94,y,613,40,132,87,226,122);
        text_at(p,stride,152,y+5,697,item_title(at),16,254,250,255);
    }
    text_at(p,stride,797,166,1176,view?"APPLE MUSIC SEARCH":"NATIVE PLAYER",22,250,247,255);
    text_at(p,stride,797,223,1176,view?"Catalog metadata":"48 kHz 16-bit stereo",16,191,201,232);
    if(items()){
        text_at(p,stride,797,305,1176,item_title(selection),22,254,251,255);
        if(view){
            text_at(p,stride,797,363,1176,catalog.songs[selection].artist,16,213,211,235);
            text_at(p,stride,797,405,1176,catalog.songs[selection].album,16,191,200,226);
            text_at(p,stride,797,467,1176,"Catalog preview only",16,250,190,203);
            text_at(p,stride,797,497,1176,"No protected streaming",16,203,192,224);
        }else{
            text_at(p,stride,797,363,1176,now_index==selection && audio.playing?
                     (audio.paused?"PAUSED":"PLAYING"):"Press A to play",16,220,208,248);
            if(now_index==selection && audio.playing){
                uint32_t pos=tw_audio_position_seconds(&audio),total=tw_audio_duration_seconds(&audio);
                char timer[90];snprintf(timer,sizeof timer,"%02lu:%02lu / %02lu:%02lu",
                    (unsigned long)(pos/60),(unsigned long)(pos%60),
                    (unsigned long)(total/60),(unsigned long)(total%60));
                text_at(p,stride,797,414,1170,timer,22,243,236,255);
                if(total){int w=(int)(338u*(pos>total?total:pos)/total);shade(p,stride,798,468,w,7,195,130,255,240);}
            }
        }
    }else{
        text_at(p,stride,797,315,1176,view?"Press X to search":"Add WAV files to /music",16,217,206,238);
    }
    text_at(p,stride,101,677,1190,status,16,230,223,246);
    framebufferEnd(fb);return 0;
}
static void clamp_selection(void){
    if(selection>=items())selection=items()?items()-1:0;
    if(selection<0)selection=0;
    if(selection<first)first=selection;
    if(selection>first+6)first=selection-6;
}
static void select_item(int direction){selection+=direction;clamp_selection();}
static void switch_view(int next){view=next;selection=0;first=0;
    set_status(view?"Catalog: press X to search (developer JWT required)":"Local: A play/pause, X next, Y rescan");}
static void rescan(void){int n=tw_library_scan(MUSIC_DIR,&library);if(n<0)set_status("No music directory. Create /switch/TurnwaveNX2/music");
    else{char msg[108];snprintf(msg,sizeof msg,"Found %d WAV files. Select with D-pad, press A.",n);set_status(msg);}clamp_selection();}
static void open_track(int index){
    if(index<0||index>=library.count)return;
    char err[160];
    if(tw_audio_open(&audio,library.tracks[index].path,err,sizeof err)){
        set_status(err);log_status("WAV open/audout failed",audio.result);now_index=-1;return;
    }
    now_index=index;set_status("Native audio active: A pause/resume, X next");
    log_status("PCM playback started",0);
}
static void play_or_pause(void){
    if(view){set_status("Apple catalog does not grant audio playback rights");return;}
    if(!library.count){set_status("Copy a 48kHz stereo PCM WAV into /music");return;}
    if(now_index==selection && audio.playing){if(tw_audio_pause(&audio))set_status("Audio pause/resume failed");else set_status(audio.paused?"Paused":"Playing");}
    else open_track(selection);
}
static void next_song(void){
    if(view)return;
    if(!library.count){set_status("No local WAV tracks");return;}
    selection=now_index<0?0:(now_index+1)%library.count;clamp_selection();open_track(selection);
}
static void search_catalog(void){
    if(!net_ready){set_status("Network service unavailable");return;}
    int resume=audio.playing&&!audio.paused;
    if(resume)tw_audio_pause(&audio);
    set_status("Searching Apple Music API (HTTPS)...");
    char err[172];
    if(tw_catalog_search(TW_DIR "/developer-token.txt",TW_DIR "/search.txt",TW_DIR "/cacert.pem",&catalog,err,sizeof err))
        set_status(err);
    else {char msg[96];snprintf(msg,sizeof msg,"Apple Music catalog: %d songs",catalog.count);set_status(msg);}
    selection=0;first=0;
    if(resume)tw_audio_pause(&audio);
}
static void touch_input(void){
    HidTouchScreenState touch={0};
    if(!hidGetTouchScreenStates(&touch,1))return;
    int touching=touch.count>0;
    if(touching&&!prev_touch){
        u32 x=touch.touches[0].x,y=touch.touches[0].y;
        if(y>=153&&y<=210){switch_view(x>310?1:0);}
        else if(x<739&&y>=245&&y<595){selection=first+(int)((y-245)/47);clamp_selection();}
        else if(x>785&&y>=330&&y<570)play_or_pause();
    }
    prev_touch=touching;
}
int main(void){
    mkdir("sdmc:/switch",0777);mkdir(TW_DIR,0777);mkdir(MUSIC_DIR,0777);
    logfile=fopen(TW_DIR "/standalone.log","a");log_status("Turnwave 0.7 native startup",0);
    int have_audio=tw_audio_init(&audio)==0;
    if(!have_audio){char error[120];snprintf(error,sizeof error,"audout unavailable (0x%08x)",audio.result);set_status(error);log_status("audoutInitialize failed",audio.result);}
    Result sock=socketInitializeDefault();net_ready=R_SUCCEEDED(sock);
    if(!net_ready)log_status("socketInitializeDefault failed",sock);
    rescan();
    if(!have_audio){char msg[110];snprintf(msg,sizeof msg,"audout unavailable (0x%08x)",audio.result);set_status(msg);}
    hidInitializeTouchScreen();
    PadState pad;padConfigureInput(8,HidNpadStyleSet_NpadStandard);padInitializeAny(&pad);
    Framebuffer fb={0};
    Result rc=framebufferCreate(&fb,nwindowGetDefault(),WIDTH,HEIGHT,PIXEL_FORMAT_RGBA_8888,2);
    if(R_FAILED(rc)){log_status("framebufferCreate failed",rc);goto done;}
    rc=framebufferMakeLinear(&fb);
    if(R_FAILED(rc)){log_status("framebufferMakeLinear failed",rc);framebufferClose(&fb);goto done;}
    int redraw=1;
    while(appletMainLoop()){
        if(have_audio && audio.playing)tw_audio_pump(&audio);
        if(have_audio && audio.finished){
            audio.finished=0;
            if(!view && library.count>1)next_song();
            else{now_index=-1;set_status("Track finished");}
        }
        padUpdate(&pad);u64 keys=padGetButtonsDown(&pad);
        if(keys&(HidNpadButton_Plus|HidNpadButton_B))break;
        if(keys&HidNpadButton_L)switch_view(0);
        if(keys&HidNpadButton_R)switch_view(1);
        if(keys&HidNpadButton_AnyUp)select_item(-1);
        if(keys&HidNpadButton_AnyDown)select_item(1);
        if(keys&HidNpadButton_A)play_or_pause();
        if(keys&HidNpadButton_X){if(view)search_catalog();else next_song();}
        if(keys&HidNpadButton_Y){if(view)search_catalog();else rescan();}
        touch_input();
        if(redraw||keys||audio.playing){render(&fb);redraw=0;}
        svcSleepThread(16666666ULL);
    }
    framebufferClose(&fb);
done:
    tw_audio_shutdown(&audio);
    if(net_ready)socketExit();
    log_status("Turnwave standalone exit",0);
    if(logfile)fclose(logfile);
    return 0;
}
