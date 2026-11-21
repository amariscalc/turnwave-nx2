/* Turnwave NX2 — native Liquid library shell for Nintendo Switch 2.
 * Chromium NX2 is chain-loaded for Apple authentication and MusicKit sync.
 * User tokens/cookies never cross the bridge: only library resource metadata.
 */
#include <switch.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

#define WIDTH 1280
#define HEIGHT 720
#define MAX_ITEMS 180
#define TW_DIR "sdmc:/switch/TurnwaveNX2"
#define LIBRARY_PATH TW_DIR "/library.tsv"
#define CHROME_CONFIG TW_DIR "/chromium-nro.txt"
#define PLAY_REQUEST TW_DIR "/play-request.txt"

extern const u8 tw_home[];
extern const u8 tw_font16[], tw_font22[], tw_font32[];
extern const u8 tw_width16[], tw_width22[], tw_width32[];

typedef struct {
    char kind;
    char library_id[96];
    char catalog_id[64];
    char name[168];
    char detail[168];
} LibraryItem;

static LibraryItem library[MAX_ITEMS];
static int count, active_tab, selected, first_row;
static char status[176] = "";
static FILE *diag;
static const char tabs[] = { 'P', 'A', 'S', 'R', 'N' };
static const char *tab_names[] = { "Playlists", "Albums", "Songs", "Artists", "Recent" };

static void log_msg(const char *line, int code) {
    if (!diag) return;
    fprintf(diag,"%s: %d\n",line,code);
    fflush(diag);
}

static void set_status(const char *s) {
    snprintf(status,sizeof(status),"%s",s);
}

/* Copy a displayed field without risking an overflow on long metadata. */
static void copy_field(char *dst, size_t capacity, const char *src) {
    if (!capacity) return;
    size_t length=strlen(src);
    if (length>=capacity) length=capacity-1;
    memcpy(dst,src,length);
    dst[length]=0;
}

static void load_library(void) {
    FILE *f=fopen(LIBRARY_PATH,"r");
    count=0;selected=0;first_row=0;
    if(!f) {set_status("No library yet. Press X to connect Apple Music.");return;}
    char line[1200];
    if (!fgets(line,sizeof line,f)) {fclose(f);set_status("Empty library file.");return;}
    int v2=!strcmp(line,"TURNWAVE_LIBRARY_V2\n");
    if(!v2 && strcmp(line,"TURNWAVE_LIBRARY_V1\n")) {
        fclose(f);set_status("Library format invalid. Press X to synchronize.");return;
    }
    while(count<MAX_ITEMS && fgets(line,sizeof line,f)) {
        line[strcspn(line,"\r\n")]=0;
        if(!memchr(tabs,line[0],sizeof tabs) || line[1]!='\t')continue;
        char *parts[4]={0};int n=0;
        char *field=line+2;
        parts[n++]=field;
        while(n<(v2?4:2)) {
            char *tab=strchr(field,'\t');
            if(!tab)break;
            *tab=0;field=tab+1;parts[n++]=field;
        }
        if(n!=(v2?4:2))continue;
        LibraryItem *item=&library[count];memset(item,0,sizeof *item);
        item->kind=line[0];
        if(v2) {
            /* Never truncate resource IDs: an altered ID could select a different item. */
            if(strlen(parts[0])>=sizeof item->library_id ||
               strlen(parts[1])>=sizeof item->catalog_id)continue;
            copy_field(item->library_id,sizeof item->library_id,parts[0]);
            copy_field(item->catalog_id,sizeof item->catalog_id,parts[1]);
            copy_field(item->name,sizeof item->name,parts[2]);
            copy_field(item->detail,sizeof item->detail,parts[3]);
        } else {
            copy_field(item->name,sizeof item->name,parts[0]);
            copy_field(item->detail,sizeof item->detail,parts[1]);
        }
        if(item->name[0])count++;
    }
    fclose(f);
    if(count)snprintf(status,sizeof status,"%d library entries loaded. A: play selected song; X: sync",count);
    else set_status("Empty library. Press X to synchronize.");
    log_msg("loaded library items",count);
}

static unsigned glyph_advance(unsigned cp, const u8 *widths) {
    return widths[cp-32];
}

static int utf8_read(const char **p) {
    const unsigned char *s=(const unsigned char *)*p;
    if (!*s) return 0;
    if (*s<128) { *p=(const char *)(s+1); return *s; }
    if ((*s&0xe0)==0xc0 && (s[1]&0xc0)==0x80) {
        int c=((*s&31)<<6)|(s[1]&63);
        *p=(const char *)(s+2); return c;
    }
    if ((*s&0xf0)==0xe0 && (s[1]&0xc0)==0x80 && (s[2]&0xc0)==0x80) {
        int c=((*s&15)<<12)|((s[1]&63)<<6)|(s[2]&63);
        *p=(const char *)(s+3); return c;
    }
    *p=(const char *)(s+1);
    return '?';
}

static void shade_rect(u8 *pixels,u32 stride,int x0,int y0,int w,int h,
                       u8 r,u8 g,u8 b,u8 alpha) {
    for (int y=y0;y<y0+h && y<HEIGHT;y++) {
        if (y<0) continue;
        for(int x=x0;x<x0+w && x<WIDTH;x++) {
            if(x<0) continue;
            u8 *px=pixels+(size_t)y*stride+(size_t)x*4;
            px[0]=(u8)((r*alpha+px[0]*(255-alpha))/255);
            px[1]=(u8)((g*alpha+px[1]*(255-alpha))/255);
            px[2]=(u8)((b*alpha+px[2]*(255-alpha))/255);
        }
    }
}

static void draw_text(u8 *pixels,u32 stride,int x,int y,int max_x,
                      const char *s,int size,u8 r,u8 g,u8 b) {
    const u8 *atlas=size==32?tw_font32:size==22?tw_font22:tw_font16;
    const u8 *widths=size==32?tw_width32:size==22?tw_width22:tw_width16;
    const int cw=size==32?50:size==22?34:27;
    const int ch=size==32?52:size==22?38:30;
    for(const char *p=s;*p;) {
        int cp=utf8_read(&p);
        if(cp<32 || cp>255) cp='?';
        unsigned adv=glyph_advance((unsigned)cp,widths);
        if(x+(int)adv>max_x) break;
        const u8 *glyph=atlas+(size_t)(cp-32)*cw*ch;
        for(int gy=0;gy<ch && y+gy<HEIGHT;gy++) {
            if(y+gy<0)continue;
            for(int gx=0;gx<cw && x+gx<WIDTH;gx++) {
                if(x+gx<0)continue;
                u8 a=glyph[gy*cw+gx];
                if(!a)continue;
                u8 *px=pixels+(size_t)(y+gy)*stride+(size_t)(x+gx)*4;
                px[0]=(u8)((r*a+px[0]*(255-a))/255);
                px[1]=(u8)((g*a+px[1]*(255-a))/255);
                px[2]=(u8)((b*a+px[2]*(255-a))/255);
            }
        }
        x+=(int)adv;
    }
}

static int visible_count(void) {
    int n=0;
    for(int i=0;i<count;i++) if(library[i].kind==tabs[active_tab]) n++;
    return n;
}

static const LibraryItem *visible_item(int position) {
    for(int i=0;i<count;i++) if(library[i].kind==tabs[active_tab]) {
        if(!position--)return &library[i];
    }
    return NULL;
}

static int render(Framebuffer *fb) {
    u32 stride=0;
    u8 *pixels=framebufferBegin(fb,&stride);
    if(!pixels)return 0;
    for(int y=0;y<HEIGHT;y++)
        memcpy(pixels+(size_t)y*stride,tw_home+(size_t)y*WIDTH*4,WIDTH*4);

    /* Layout v0.6: fixed, non-overlapping text bands. */
    const int available=visible_count();
    draw_text(pixels,stride,554,173,694,tab_names[active_tab],16,224,217,255);
    for(int row=0;row<7;row++) {
        const LibraryItem *item=visible_item(first_row+row);
        if(!item)break;
        int y=247+47*row;
        if(first_row+row==selected)
            shade_rect(pixels,stride,94,y,610,41,115,100,221,105);
        draw_text(pixels,stride,152,y+5,686,item->name,16,255,252,255);
    }
    draw_text(pixels,stride,795,255,1185,
              count?"Your music is connected":"Connect to Apple Music",
              22,245,241,255);
    char counter[96];
    snprintf(counter,sizeof counter,"%d in %s",available,tab_names[active_tab]);
    draw_text(pixels,stride,795,307,1185,counter,16,200,193,238);

    const LibraryItem *cur=visible_item(selected);
    if(cur) {
        draw_text(pixels,stride,795,368,1185,cur->name,16,249,245,255);
        draw_text(pixels,stride,795,407,1185,cur->detail,16,211,204,243);
        if(cur->kind=='S')
            draw_text(pixels,stride,795,445,1185,
                      cur->catalog_id[0]?"A  Try song in Chromium":"No playable catalog ID",
                      16,198,192,237);
    } else {
        draw_text(pixels,stride,795,368,1185,
                  "Press X to connect or sync",16,211,204,243);
    }
    draw_text(pixels,stride,833,521,1162,
              count?"X  Sync Apple Music":"X  Connect Apple Music",
              22,255,253,255);
    draw_text(pixels,stride,98,672,1190,status,16,228,223,253);
    framebufferEnd(fb);
    return 1;
}

static int read_chromium_path(char *out,size_t cap) {
    FILE *f=fopen(CHROME_CONFIG,"r");
    if(f) {
        if(fgets(out,(int)cap,f)) {
            out[strcspn(out,"\r\n")]=0;
            if(!strncmp(out,"sdmc:/switch/",13) && strstr(out,".nro") && !strchr(out,' ')) {
                FILE *check=fopen(out,"rb");
                if(check) { fclose(check); fclose(f); return 1; }
            }
        }
        fclose(f);
    }
    /* The user's custom GX build comes first. Can be overridden with chromium-nro.txt. */
    const char *paths[]={
        "sdmc:/switch/chromium-nx-diagnostico/chromium-nx2-diagnostico.nro",
        "sdmc:/switch/chromium-nx/chromium-nx2.nro"
    };
    for(unsigned i=0;i<sizeof(paths)/sizeof(paths[0]);i++) {
        FILE *probe=fopen(paths[i],"rb");
        if(probe) { fclose(probe); snprintf(out,cap,"%s",paths[i]); return 1; }
    }
    return 0;
}

static int save_play_request(const char *catalogid) {
    if(!catalogid || strlen(catalogid)<2 || strlen(catalogid)>30)return 0;
    for(const char *p=catalogid;*p;p++)if(*p<'0'||*p>'9')return 0;
    FILE *f=fopen(PLAY_REQUEST ".part","w");
    if(!f)return 0;
    int ok=fprintf(f,"TURNWAVE_PLAY_V1\n%s\n",catalogid)>0;
    ok &= fflush(f)==0;
    ok &= fclose(f)==0;
    if(!ok){remove(PLAY_REQUEST ".part");return 0;}
    remove(PLAY_REQUEST);
    if(rename(PLAY_REQUEST ".part",PLAY_REQUEST)!=0)return 0;
    FILE *log=fopen(TW_DIR "/playback.log","w");
    if(log) {fprintf(log,"REQUEST|%s\n",catalogid);fclose(log);}
    return 1;
}

static int login_with_chromium(int playback) {
    char path[280]={0};
    if(!read_chromium_path(path,sizeof path)) {
        set_status("Chromium NX2 not found. Configure chromium-nro.txt on SD.");
        log_msg("Chromium NRO missing",-1);
        return 0;
    }
    char args[620];
    snprintf(args,sizeof args,"%s --browser %s",path, playback?"--turnwave-play":"--turnwave-login");
    Result rc=envSetNextLoad(path,args);
    log_msg(playback?"envSetNextLoad Chromium playback":"envSetNextLoad Chromium login",(int)rc);
    if(R_FAILED(rc)) {
        snprintf(status,sizeof status,"Cannot start Chromium (0x%08X)",(unsigned)rc);
        return 0;
    }
    return 1;
}

int main(void) {
    mkdir("sdmc:/switch",0777);
    mkdir(TW_DIR,0777);
    diag=fopen(TW_DIR "/native.log","a");
    log_msg("Turnwave native started",0);
    PadState pad;
    padConfigureInput(8,HidNpadStyleSet_NpadStandard);
    padInitializeAny(&pad);
    load_library();
    Framebuffer fb={0};
    Result rc=framebufferCreate(&fb,nwindowGetDefault(),WIDTH,HEIGHT,PIXEL_FORMAT_RGBA_8888,2);
    if(R_FAILED(rc)){log_msg("framebufferCreate",(int)rc);if(diag)fclose(diag);return 1;}
    rc=framebufferMakeLinear(&fb);
    if(R_FAILED(rc)){log_msg("framebufferMakeLinear",(int)rc);framebufferClose(&fb);if(diag)fclose(diag);return 1;}
    int redraw=1,login=0;
    while(appletMainLoop()) {
        if(redraw) {if(!render(&fb))break;redraw=0;}
        padUpdate(&pad);
        u64 pressed=padGetButtonsDown(&pad);
        if(pressed&(HidNpadButton_Plus|HidNpadButton_B))break;
        if(pressed&HidNpadButton_X) {
            remove(PLAY_REQUEST);login=login_with_chromium(0);redraw=1;if(login)break;
        }
        if(pressed&HidNpadButton_A) {
            const LibraryItem *item=visible_item(selected);
            if(item && item->kind=='S') {
                if(!save_play_request(item->catalog_id)) {
                    set_status("Song has no usable catalog ID. Try another track.");
                } else {
                    set_status("Opening MusicKit playback probe in Chromium...");
                    login=login_with_chromium(1);
                }
            } else {
                remove(PLAY_REQUEST);login=login_with_chromium(0);
            }
            redraw=1;if(login)break;
        }
        if(pressed&HidNpadButton_Y) {load_library();redraw=1;}
        if(pressed&HidNpadButton_L) {active_tab=(active_tab+4)%5;selected=first_row=0;redraw=1;}
        if(pressed&HidNpadButton_R) {active_tab=(active_tab+1)%5;selected=first_row=0;redraw=1;}
        if(pressed&HidNpadButton_AnyUp) {if(selected>0)selected--;if(selected<first_row)first_row=selected;redraw=1;}
        if(pressed&HidNpadButton_AnyDown) {
            if(selected+1<visible_count())selected++;
            if(selected>=first_row+7)first_row=selected-6;
            redraw=1;
        }
        svcSleepThread(16666666ULL);
    }
    framebufferClose(&fb);
    log_msg(login?"Handing off to Chromium":"Native exit",login);
    if(diag)fclose(diag);
    return 0;
}
