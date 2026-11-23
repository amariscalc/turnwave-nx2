#include "skin.hpp"
#include <cstdio>
#include <cstring>
#include <string_view>

namespace {
struct Glyph {int advance,x,y,w,h; const char* coverage;};
#include "font.inc"
using Color=uint32_t;
constexpr Color paper=0xd9d8ee, ink=0x141525, dark=0x22213c, lit=0xf8f7ff,
    green=0xc7bcff, dimGreen=0x9b91c8, well=0x24233e, gold=0xb6d9ff, muted=0xb3b0cc;
Color mix(Color a,Color b,int n,int d) {
    Color c=0;for(int s:{0,8,16}) c|=((((a>>s&255)*(d-n)+(b>>s&255)*n)/d)<<s);
    return c;
}
struct Canvas {
    uint32_t* px;
    void box(int x,int y,int w,int h,Color c) {
        int a=std::clamp(x,0,width),b=std::clamp(x+w,0,width);
        if(a>=b) return;
        for(int j=std::max(0,y*2);j<std::min(renderHeight,(y+h)*2);j++)
            std::fill(px+j*renderWidth+a*2,px+j*renderWidth+b*2,c|0xff000000);
    }
    void pixel(int x,int y,Color color) {
        px[y*renderWidth+x]=color|0xff000000;
    }
    void grad(int x,int y,int w,int h,Color a,Color b) {
        for(int i=0;i<h;i++) box(x,y+i,w,1,mix(a,b,i,std::max(1,h-1)));
    }
    void edge(int x,int y,int w,int h,Color hi,Color lo) {
        box(x,y,w,1,hi);box(x,y,1,h,hi);box(x,y+h-1,w,1,lo);box(x+w-1,y,1,h,lo);
    }
    void round(int x,int y,int w,int h,int r,Color top,Color bottom) {
        const int radius=r*2;
        for(int row=0;row<h*2;row++) {
            const int py=y*2+row;
            if(py<0 || py>=renderHeight) continue;
            int dy=std::max({0,radius-row,row-(h*2-1-radius)});
            int pad=dy?radius-int(std::sqrt(radius*radius-dy*dy)):0;
            int left=std::clamp(x*2+pad,0,renderWidth);
            int right=std::clamp((x+w)*2-pad,left,renderWidth);
            auto color=mix(top,bottom,row,std::max(1,h*2-1));
            std::fill(px+py*renderWidth+left,px+py*renderWidth+right,color|0xff000000);
        }
    }
    void inset(int x,int y,int w,int h,Color fill=well) {round(x,y,w,h,5,fill,fill);}
    void panel(int x,int y,int w,int h) {
        round(x,y,w,h,12,0x696285,0x383756);
        round(x+1,y+1,w-2,h-2,11,0x353250,0x24233c);
    }
    static unsigned glyph(std::string_view s,size_t& i) {
        unsigned c=(unsigned char)s[i++];
        if(c<128) return c<32?' ':c;
        unsigned n=c<224?1:c<240?2:3,code=c&((1u<<(6-n))-1);
        while(n-- && i<s.size() && ((unsigned char)s[i]&192)==128) code=code*64+((unsigned char)s[i++]&63);
        return code>=32 && code<=255?code:'?';
    }
    int measure(std::string_view s,const Glyph* font) {
        int w=0;
        for(size_t i=0;i<s.size();) w+=font[glyph(s,i)-32].advance;
        return (w+1)/2;
    }
    void text(int x,int y,std::string_view s,Color color,const Glyph* font=small,int limit=640) {
        const int end=(x+limit)*2;
        int pen=x*2;
        for(size_t i=0;i<s.size();) {
            const auto& g=font[glyph(s,i)-32];
            if(pen+g.advance>end) break;
            for(int row=0;row<g.h;row++) for(int col=0;col<g.w;col++) {
                int dx=pen+g.x+col,dy=y*2+g.y+row;
                if(dx<0 || dx>=renderWidth || dy<0 || dy>=renderHeight) continue;
                const int index=row*g.w+col;
                const unsigned coverage=static_cast<unsigned char>(g.coverage[index/2]);
                const int alpha=(coverage>>((index%2)*4))&15;
                if(alpha) pixel(dx,dy,mix(px[dy*renderWidth+dx],color,alpha,15));
            }
            pen+=g.advance;
        }
    }
    void center(int x,int y,int w,std::string_view s,Color c,const Glyph* f=small) {
        text(x+(w-measure(s,f))/2,y,s,c,f,w);
    }
    void title(int x,int y,int w,int h,std::string_view label) {
        text(x+5,y+(h-14)/2,label,lit,bold,w-10);
    }
    void title(int x,int y,int w,std::string_view label) {title(x,y,w,15,label);}
    void button(int x,int y,int w,int h,std::string_view label,bool active=false) {
        round(x,y,w,h,std::min(10,h/2),active?0xb3a0ed:0x575273,active?0x7d64c2:0x3b3858);
        center(x,y+(h-11)/2,w,label,lit,small);
    }
    void indicator(int x,int y,std::string_view label,bool on) {
        inset(x,y+4,6,5,on?green:0x244226); text(x+9,y,label,on?lit:muted);
    }
    void slider(int x,int y,int w,int val,Color tint) {
        inset(x,y+3,w,6,0x14151c);
        int n=(w-7)*std::clamp(val,0,100)/100;
        box(x+2,y+5,n,2,tint);
        grad(x+n,y,8,12,0xf3e0a4,0xb0873f);edge(x+n,y,8,12,0xffeabd,ink);
        box(x+n+3,y+3,1,6,0x70522e);
    }
    void digit(int x,int y,int num) {
        static constexpr int masks[]={0x3f,6,0x5b,0x4f,0x66,0x6d,0x7d,7,0x7f,0x6f};
        constexpr int seg[][4]={{3,0,12,3},{15,3,3,9},{15,15,3,9},{3,24,12,3},{0,15,3,9},{0,3,3,9},{3,12,12,3}};
        for(int i=0;i<7;i++) box(x+seg[i][0],y+seg[i][1],seg[i][2],seg[i][3],masks[num]&(1<<i)?green:0x102513);
    }
    void triangle(int x,int y,int size,bool left,Color c) {
        for(int i=0;i<size;i++) box(x+(left?size-1-i:i),y+i/2,1,size-(i/2)*2,c);
    }
    void transport(int x,int y,int type) {
        button(x,y,30,23,""); int cx=x+10,cy=y+6;
        if(type==1||type==4) triangle(cx,cy,10,false,ink);
        if(type==0) triangle(cx,cy,10,true,ink);
        if(type==0||type==4) box(cx+(type==0?-2:9),cy,2,10,ink);
        if(type==2) {box(cx,cy,3,10,ink);box(cx+6,cy,3,10,ink);}
        if(type==3) box(cx,cy+1,9,9,ink);
    }
};
std::string clockText(double seconds) {
    int n=std::max(0,int(seconds)); char s[24];std::snprintf(s,sizeof s,"%d:%02d",n/60,n%60);return s;
}
void mainDeck(Canvas& c,const Player& p) {
    c.panel(16,55,330,246);
    c.text(33,69,"Now playing",lit,bold);
    const int imageX=33,imageY=98,side=128;
    const auto* audio=p.audio.get();
    c.round(imageX,imageY,side,side,13,0x8170ce,0x393768);
    if(audio && audio->cover.size()==Decoder::coverSide*Decoder::coverSide) {
        for(int y=0;y<Decoder::coverSide;y++) for(int x=0;x<Decoder::coverSide;x++) {
            int dx=std::max({0,24-x,x-(Decoder::coverSide-25)}),dy=std::max({0,24-y,y-(Decoder::coverSide-25)});
            if(dx*dx+dy*dy<=576) c.pixel(imageX*2+x,imageY*2+y,audio->cover[y*Decoder::coverSide+x]);
        }
    } else {
        const int bars[]={28,49,66,45,25};
        for(int i=0;i<5;i++) c.round(64+i*13,162-bars[i]/2,7,bars[i],3,0xf6f3ff,0xb6a6ef);
    }
    std::string title=p.current>=0?p.queue[p.current].name:"Select a track";
    if(audio && !audio->title.empty()) title=audio->title;
    c.text(177,103,title,lit,bold,150);
    if(audio) {
        c.text(177,126,audio->artist,paper,body,150);
        c.text(177,145,audio->album,muted,small,150);
        c.text(177,171,upper(audio->format),green,small,150);
        c.text(177,189,clockText(p.position)+" / "+clockText(audio->duration),paper,small,150);
    }
    c.round(33,240,296,5,2,0x514a71,0x514a71);
    if(audio && audio->duration>0) {
        const int progress=int(296*std::clamp(p.position/audio->duration,0.,1.));
        if(progress>=4) c.round(33,240,progress,5,2,0xe1d5ff,0x9f83ea);
    }
    c.button(33,255,73,32,"Previous");
    c.button(113,255,92,32,audio && !p.paused?"Pause":"Play",true);
    c.button(212,255,52,32,"Next");
    c.button(271,255,58,32,"Actions");
}
void equalizer(Canvas& c,const Player& p) {
    c.panel(8,184,358,137);c.title(14,189,346,"GRAPHIC EQUALIZER");
    c.button(20,211,41,17,p.eq.on?"ON":"OFF",p.eq.on);
    c.button(65,211,43,17,"RESET");
    c.text(121,213,"PREAMP",muted);c.text(166,213,std::to_string(p.eq.preamp)+" dB",gold);
    int sel=p.band==0?p.eq.preamp:p.eq.gain[p.band-1];
    std::string status=p.view==View::Eq?(p.band?std::to_string(Equalizer::hz[p.band-1])+" Hz":"PREAMP")+"  "+std::to_string(sel)+" dB":p.eq.on?"EQ ENABLED":"FLAT / BYPASS";
    c.inset(232,210,121,19);c.center(232,214,121,status,p.eq.on?green:dimGreen);
    c.text(19,235,"+12",muted);c.text(24,261,"0",muted);c.text(19,286,"-12",muted);
    constexpr const char* names[]={"PRE","60","170","310","600","1K","3K","6K","12K","14K","16K"};
    for(int i=0;i<11;i++) {
        int x=i?76+(i-1)*29:47;
        int gain=i?p.eq.gain[i-1]:p.eq.preamp;
        c.inset(x-4,236,9,59,0x14151a);
        for(int j=0;j<24;j++) c.box(x-1,239+j*2,3,1,mix(0xd8663e,0x70b66a,j,23));
        int y=265-gain*2;
        c.grad(x-6,y-3,13,7,0xe6e5dc,0x9999a1);c.edge(x-6,y-3,13,7,lit,ink);
        c.box(x-4,y,9,1,0x615841);
        if(p.view==View::Eq && p.band==i) c.edge(x-9,233,19,64,gold,gold);
        c.center(x-11,300,23,names[i],i==0?gold:paper);
    }
}
void listing(Canvas& c,const Player& p) {
    bool browse=p.view==View::Files;
    auto& items=browse?p.browserEntries():p.queue;
    int active=browse?p.fileRow:p.selected,top=p.topRow();
    c.panel(363,10,264,311);c.title(380,15,246,browse?(p.libraryView?"LIBRARY":p.favoritesView?"FAVORITES":"LIBRARY"):"UP NEXT");
    c.inset(383,38,240,24,0x14171c);
    c.text(390,44,browse?(p.libraryView?"LIBRARY":p.favoritesView?"FAVORITES":p.folder):p.queueName,browse?gold:paper,small,185);
    c.text(586,44,std::to_string(items.size()),dimGreen,small,30);
    c.inset(383,65,240,192);
    if(items.empty()) {
        c.text(393,86,browse?"No supported files in this folder":"Your playlist is empty",muted,body,220);
        c.text(393,108,browse?"B  Parent folder":"X  Browse SD",green);
    }
    for(int row=0;row<Player::visible && row+top<int(items.size());row++) {
        int i=row+top, y=69+row*20;
        auto& e=items[i];bool playing=!browse && i==p.current;
        if(i==active) c.round(387,y,226,19,5,0x544878,0x3d355e);
        Color color=i==active?lit:paper;
        char num[16];std::snprintf(num,sizeof num,"%02d",i+1);
        if(playing) c.triangle(391,y+6,7,false,green);
        else c.text(390,y+4,e.directory?">":p.isFavorite(e)?"*":num,i==active?0xc3c4df:dimGreen);
        bool add=browse && e.track();
        c.text(409,y+1,e.name,color,body,add?146:160);
        std::string suffix=e.directory?"DIR":e.ext;
        c.text((add?588:608)-c.measure(suffix,small),y+4,suffix,i==active?0xd0cfdc:dimGreen);
        if(add) c.button(593,y,20,18,"+");
    }
    c.inset(615,70,5,181,0x171e19);
    int sh=std::max(9,180*std::min(Player::visible,int(items.size()))/std::max(1,int(items.size())));
    int pos=items.size()>Player::visible?(180-sh)*top/(int(items.size())-Player::visible):0;
    c.box(616,71+pos,3,sh,0x636a65);
    c.inset(383,263,240,20);
    std::string state=p.audio?(p.paused?"PAUSED":"PLAYING"):"STOPPED";
    c.text(390,268,p.notice.empty()?state:p.notice,p.notice.empty()?green:gold,small,p.notice.empty()?95:225);
    if(p.notice.empty()) {
        std::string times=clockText(p.position)+" / "+(p.audio?clockText(p.audio->duration):"--:--");
        c.text(616-c.measure(times,small),268,times,green);
    }
    c.button(383,291,62,20,"PLAYLIST",!browse);c.button(448,291,47,20,"FILES",browse);
    c.button(498,291,55,20,"Actions",p.menu);
    c.button(556,291,67,20,browse?"OPEN":"PLAY");
    if(p.menu) {
        c.panel(383,36,240,221);c.title(389,42,228,"Actions");
        for(int row=p.menuTop();row<p.menuTop()+Player::menuVisible;row++) {
            int y=60+(row-p.menuTop())*20;
            bool focus=row==p.menuRow;
            c.box(392,y,221,18,focus?0x252a71:well);
            c.text(401,y+1,p.menuLabel(static_cast<Player::MixAction>(row)),focus?lit:green,body,203);
        }
        c.text(392,240,std::to_string(p.menuRow+1)+"/"+std::to_string(Player::ActionCount)+"  MORE >",muted);
        c.button(544,240,68,14,"CLOSE");
    }

}
void fullScreen(Canvas& c,const Player& p) {
    using Overlay=Player::Overlay;
    if(p.keyboardMode()) {
        c.panel(8,10,624,314);c.title(14,15,612,p.tagField>=0?"EDIT LIBRARY TAG":p.naming?"RENAME MIX":"SEARCH MUSIC LIBRARY");
        c.inset(20,49,597,40);
        c.text(29,62,(p.tagField>=0?p.tags[p.tagField]:p.naming?p.editName:p.searchQuery)+"_",green,body,570);
        for(int i=0;i<40;i++) c.button(20+i%10*60,105+i/10*46,56,40,Player::keyLabel(i),i==p.keyboard);
        c.text(20,299,p.notice.empty()?"D-pad selects   A types   B cancels":p.notice,paper,small,475);
        c.button(530,289,86,24,"BACK");
        return;
    }
    if(p.overlay==Overlay::Speed || p.overlay==Overlay::Tags) {
        const bool tags=p.overlay==Overlay::Tags;
        c.panel(8,10,624,314);c.title(14,15,612,tags?"LIBRARY TAGS":"PLAYBACK SPEED");
        const char* fields[]={"Title","Artist","Album","Track","Save"};
        const char* rates[]={"1.00x","1.25x","1.50x"};
        for(int row=0;row<(tags?5:3);++row) {
            int y=48+row*40;
            c.box(24,y,592,34,row==p.panelRow?0x252a71:well);
            std::string label=tags?fields[row]:rates[row];
            if(tags && row<4) label+="  "+p.tags[row];
            c.text(33,y+8,label,paper,body,560);
        }
        c.text(25,298,p.notice,paper,small,490);
        c.button(530,289,86,24,"BACK");
        return;
    }
    if(p.overlay==Overlay::Queue || p.overlay==Overlay::Lyrics || p.overlay==Overlay::Cue) {
        bool queue=p.overlay==Overlay::Queue,lyrics=p.overlay==Overlay::Lyrics;
        c.panel(8,10,624,314);c.title(14,15,612,queue?"PLAYBACK QUEUE":lyrics?"SYNCED LYRICS":"CUE CHAPTERS");
        int count=queue?int(p.queue.size()):int(p.timedLines.size()),active=p.panelRow;
        if(lyrics) {
            auto it=std::upper_bound(p.timedLines.begin(),p.timedLines.end(),p.position,[](double t,const auto& row){return t<row.first;});
            active=int(it-p.timedLines.begin())-1;
        }
        int top=std::max(0,active-(lyrics?3:7));
        for(int row=top;row<std::min(top+8,count);row++) {
            int y=44+(row-top)*30;
            c.box(24,y,592,27,row==active?0x252a71:well);
            auto text=queue?p.queue[row].name:clockText(p.timedLines[row].first)+"  "+p.timedLines[row].second;
            c.text(33,y+6,text,row==active?green:paper,body,565);
        }
        if(queue) {
            c.button(24,289,76,24,"MOVE UP");
            c.button(105,289,91,24,"MOVE DOWN");
            c.button(201,289,80,24,"REMOVE");
            c.text(290,298,p.notice,paper,small,234);
        } else c.text(24,298,lyrics?"B returns":"A jumps to chapter   B returns",paper,small);
        c.button(530,289,86,24,"BACK");
        return;
    }
    if(p.overlay==Overlay::PlaybackOptions) {
        c.panel(8,10,624,314);c.title(14,15,612,"PLAYBACK OPTIONS");
        for(int i=0;i<8;i++) {
            c.box(25,48+i*29,590,26,i==p.optionRow?0x252a71:well);
            c.text(37,53+i*29,p.playbackOptionLabel(i),paper,body);
        }
        c.button(512,289,104,22,"BACK");
        return;
    }

    const bool presets=p.overlay==Overlay::Presets, info=p.overlay==Overlay::Info;
    c.panel(8,10,624,314);
    c.title(14,15,612,presets?"EQUALIZER PRESETS":info?"NOW PLAYING":p.overlay==Overlay::Wave?"OSCILLOSCOPE":"SPECTRUM ANALYZER");
    c.button(512,289,104,22,"BACK");
    if(presets) {
        for(int i=0;i<5;i++) {
            c.box(25,49+i*39,582,31,i==p.presetRow?0x252a71:well);
            c.text(36,57+i*39,Player::presetName(i),green,body);
        }
        if(!p.notice.empty()) c.text(26,294,p.notice,gold,small,405);
        return;
    }
    if(info) {
        if(!p.audio) {c.text(25,53,"Start playback to see track details",paper,body);return;}
        const auto& a=*p.audio;
        c.inset(24,45,132,132);
        if(a.cover.empty()) c.center(26,103,128,"NO JPEG COVER",muted);
        else for(int y=0;y<Decoder::coverSide;y++) for(int x=0;x<Decoder::coverSide;x++)
            c.pixel(52+x,94+y,a.cover[y*Decoder::coverSide+x]);
        c.text(178,51,a.title.empty()?p.queue[p.current].name:a.title,green,bold,424);
        c.text(178,77,a.artist.empty()?"Unknown artist":a.artist,lit,body,424);
        c.text(178,101,a.album.empty()?"Unknown album":a.album,paper,body,424);
        c.text(178,131,upper(a.format)+"  /  "+std::to_string(a.sourceRate)+" Hz  /  "+std::to_string(a.channels)+" ch",gold,body);
        c.text(178,153,clockText(p.position)+" / "+clockText(a.duration),muted,body);
        return;
    }
    c.inset(24,42,592,235);
    for(int y=57;y<274;y+=24) c.box(26,y,588,1,0x17291e);
    if(p.overlay==Overlay::Spectrum) {
        for(int i=0;i<32;i++) {
            int h=int(213*p.bars[i]);
            if(h) c.grad(33+i*18,267-h,14,h,0xe5b94b,0x2d7a43);
        }
    } else {
        int previous=156;
        for(int i=0;i<int(p.wave.size());i++) {
            int y=156-int(100*p.wave[i]);
            c.box(59+2*i,std::min(y,previous),2,1+std::abs(y-previous),green);
            previous=y;
        }
    }
    c.button(25,289,138,22,"CHANGE MODE");
}

}
void Skin::draw(const Player& p) {
    Canvas c{pixels.data()};
    static const auto background=[] {
        std::vector<uint32_t> result(renderWidth*renderHeight);
        for(int y=0;y<renderHeight;y++) for(int x=0;x<renderWidth;x++) {
            int glow=std::max(0,150-(std::abs(x-230)+std::abs(y-60))/6);
            int blue=std::max(0,110-(std::abs(x-1180)+std::abs(y-90))/6);
            result[y*renderWidth+x]=0xff000000|mix(mix(0x131526,0x4d356d,glow,150),0x283d70,blue,150);
        }
        return result;
    }();
    std::copy(background.begin(),background.end(),pixels.begin());
    if(p.keyboardMode() || p.overlay!=Player::Overlay::None) {
        fullScreen(c,p);
        c.text(13,339,p.overlay==Player::Overlay::Presets?"D-PAD  Select   A  Apply   B  Back":"B  Back",paper);
        return;
    }
    if(p.view==View::Eq) equalizer(c,p);
    else {
        c.text(18,12,"Turnwave",lit,bold);
        c.button(120,13,66,28,"Library",p.view==View::Files);
        c.button(192,13,64,28,"Search");
        c.button(262,13,77,28,"Favorites");
        mainDeck(c,p);
    }
    listing(c,p);
    c.text(18,339,p.view==View::Eq?"Y returns   D-pad adjusts bands":"A opens   X browser   - actions   L/R track",paper,small);

}
void Skin::touch(Player& p,const Input& e) {
    int x=e.x,y=e.y;bool tap=e.key==Key::Tap;
    if(p.keyboardMode()) {
        if(tap && x>=530 && y>=289 && y<=313) p.typeKey(Key::Back);
        else if(tap && x>=20 && x<620 && y>=105 && y<289) p.keyPress((y-105)/46*10+(x-20)/60);
        return;
    }
    if(p.overlay==Player::Overlay::Speed || p.overlay==Player::Overlay::Tags) {
        if(tap && x>=530 && y>=289 && y<=313) p.panelKey(Key::Back);
        else if(tap && x>=24 && x<616 && y>=48 && y<(p.overlay==Player::Overlay::Tags?248:168)) {
            p.panelRow=(y-48)/40;p.panelKey(Key::Accept);
        }
        return;
    }
    if(p.overlay==Player::Overlay::Queue || p.overlay==Player::Overlay::Lyrics || p.overlay==Player::Overlay::Cue) {
        if(tap && x>=530 && y>=289 && y<=313) p.panelKey(Key::Back);
        else if(tap && p.overlay==Player::Overlay::Queue && y>=289 && y<=313) {
            if(x>=24 && x<=100) p.panelKey(Key::Left);
            else if(x>=105 && x<=196) p.panelKey(Key::Right);
            else if(x>=201 && x<=281) p.panelKey(Key::Accept);
        } else if(tap && (p.overlay==Player::Overlay::Cue || p.overlay==Player::Overlay::Queue) && x>=24 && x<616 && y>=44 && y<284) {
            int row=std::max(0,p.panelRow-7)+(y-44)/30;
            if(p.overlay==Player::Overlay::Queue) {
                if(row<int(p.queue.size())) p.panelRow=row;
            } else if(row<int(p.timedLines.size())) {p.panelRow=row;p.panelKey(Key::Accept);}
        }
        return;
    }

    if(p.overlay!=Player::Overlay::None) {
        if(!tap) return;
        if(x>=512 && x<=616 && y>=289 && y<=311) p.input(Key::Back);
        else if(p.overlay==Player::Overlay::PlaybackOptions && x>=25 && x<615 && y>=48 && y<280)
            p.playbackOption((y-48)/29);
        else if(p.overlay==Player::Overlay::Presets && x>=25 && x<607 && y>=49 && y<244)
            p.applyPreset((y-49)/39);
        else if(x>=25 && x<=163 && y>=289 && y<=311) p.input(Key::Accept);
        return;
    }

    if(p.menu) {
        if(!tap) return;
        if(x>=392 && x<613 && y>=60 && y<60+Player::menuVisible*20) {
            p.mixAction(static_cast<Player::MixAction>(p.menuTop()+(y-60)/20));
        } else if(x>=392 && x<544 && y>=240 && y<256) p.menuRow=std::min(int(Player::ActionCount)-1,p.menuTop()+2*Player::menuVisible-1);
        else if(x>=389 && x<=617 && y>=42 && y<60) p.menuRow=std::max(0,p.menuTop()-1);
        else p.menu=false;
        return;
    }
    if(p.view!=View::Eq && x>=33 && x<=329 && y>=229 && y<=249 && p.audio) {
        p.seek((x-33)/296.*p.audio->duration);return;
    }
    if(!tap) return;
    if(p.view!=View::Eq && y>=13 && y<=41 && x>=120 && x<=339) {
        if(x<=186) {p.loadFolder(p.folder);p.view=View::Files;}
        else if(x<=256) p.mixAction(Player::Library);
        else p.mixAction(Player::Favorites);
    }
    else if(p.view!=View::Eq && x>=33 && x<=161 && y>=98 && y<=226) p.mixAction(Player::TrackInfo);
    else if(p.view!=View::Eq && y>=255 && y<=287 && x>=33 && x<=329) {
        if(x<=106) p.step(-1);
        else if(x<=205) p.toggle();
        else if(x<=264) p.step(1);
        else p.openMenu();
    }
    else if(p.view==View::Eq && y>=210 && y<=229 && x>=20 && x<=61) p.toggleEq();
    else if(p.view==View::Eq && y>=210 && y<=229 && x>=65 && x<=108) {p.eq.preamp=0;p.eq.gain.fill(0);p.eq.rebuild();}
    else if(x>=386 && x<=613 && y>=69 && y<249) {
        int index=p.topRow()+(y-69)/20;
        if(p.view==View::Files) {if(index<int(p.browserEntries().size())) {p.fileRow=index;if(x>=593 && p.browserEntries()[index].track()) p.mixAction(Player::Add);else p.chooseFile();}}
        else if(index<int(p.queue.size())) {p.view=View::Playlist;p.selected=index;p.enter();}
    } else if(x>=614 && x<=623 && y>=69 && y<=253) {
        bool browse=p.view==View::Files;int n=browse?p.browserEntries().size():p.queue.size();
        (browse?p.fileRow:p.selected)=std::clamp((y-69)*n/185,0,std::max(0,n-1));
    } else if(y>=290 && y<=312 && x>=383 && x<=445) p.view=View::Playlist;
    else if(y>=290 && y<=312 && x>=448 && x<=495) {if(p.favoritesView || p.libraryView) p.loadFolder(p.folder);p.view=View::Files;}
    else if(y>=290 && y<=312 && x>=498 && x<=553) p.openMenu();
    else if(y>=290 && y<=312 && x>=556 && x<=623) p.enter();
}
