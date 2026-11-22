#include "catalog.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#define TW_JSON_TOKENS 5500
#define TW_JSON_DEPTH 40
typedef struct { int start,end,next;char kind; } Token;
typedef struct { const char *s;size_t len,pos;Token *t;int used,cap; } Parser;
static void ws(Parser *p){while(p->pos<p->len && (p->s[p->pos]==' '||p->s[p->pos]=='\r'||p->s[p->pos]=='\t'||p->s[p->pos]=='\n'))p->pos++;}
static int parse(Parser *p,int depth) {
    ws(p);
    if(depth>TW_JSON_DEPTH||p->used>=p->cap||p->pos>=p->len)return -1;
    int i=p->used++;
    Token *t=&p->t[i];t->start=(int)p->pos;
    char c=p->s[p->pos++];t->kind=c;
    if(c=='"'){
        while(p->pos<p->len){
            unsigned char b=(unsigned char)p->s[p->pos++];
            if(b=='"'){t->end=(int)p->pos;t->next=p->used;return i;}
            if(b<32)return -1;
            if(b=='\\'){
                if(p->pos>=p->len)return -1;
                char esc=p->s[p->pos++];
                if(esc=='u'){
                    for(int n=0;n<4;n++){
                        if(p->pos>=p->len||!isxdigit((unsigned char)p->s[p->pos++]))return -1;
                    }
                } else if(!strchr("\\\"/bfnrt",esc))return -1;
            }
        }
        return -1;
    }
    if(c=='{'||c=='['){
        char close=(c=='{'?'}':']');ws(p);
        if(p->pos<p->len&&p->s[p->pos]==close){p->pos++;t->end=(int)p->pos;t->next=p->used;return i;}
        while(p->pos<p->len){
            int k=parse(p,depth+1);if(k<0)return -1;
            if(c=='{'){
                if(p->t[k].kind!='"')return -1;
                ws(p);if(p->pos>=p->len||p->s[p->pos++]!=':')return -1;
                if(parse(p,depth+1)<0)return -1;
            }
            ws(p);
            if(p->pos>=p->len)return -1;
            if(p->s[p->pos]==close){p->pos++;t->end=(int)p->pos;t->next=p->used;return i;}
            if(p->s[p->pos++]!=',')return -1;
        }
        return -1;
    }
    if(c=='-'||(c>='0'&&c<='9')||c=='t'||c=='f'||c=='n'){
        while(p->pos<p->len && p->s[p->pos]!=',' && p->s[p->pos]!=']' &&
              p->s[p->pos]!='}' && !isspace((unsigned char)p->s[p->pos]))p->pos++;
        t->end=(int)p->pos;t->next=p->used;
        return i;
    }
    return -1;
}
static int key_eq(Parser *p,int i,const char *key){
    Token *t=&p->t[i];size_t n=strlen(key);
    return t->kind=='"'&&t->end-t->start==(int)n+2&&!memcmp(p->s+t->start+1,key,n);
}
static int get(Parser *p,int obj,const char *key){
    if(obj<0||p->t[obj].kind!='{')return -1;
    for(int i=obj+1;i<p->t[obj].next;){
        int value=i+1;
        if(value>=p->t[obj].next)return -1;
        if(key_eq(p,i,key))return value;
        i=p->t[value].next;
    }
    return -1;
}
static int utf8(char *dest,size_t cap,size_t *pos,uint32_t cp){
    unsigned char b[4];int n=0;
    if(cp<0x80)b[n++]=(unsigned char)cp;
    else if(cp<0x800){b[n++]=0xc0|(cp>>6);b[n++]=0x80|(cp&63);}
    else if(cp<0x10000){b[n++]=0xe0|(cp>>12);b[n++]=0x80|((cp>>6)&63);b[n++]=0x80|(cp&63);}
    else if(cp<=0x10ffff){b[n++]=0xf0|(cp>>18);b[n++]=0x80|((cp>>12)&63);b[n++]=0x80|((cp>>6)&63);b[n++]=0x80|(cp&63);}
    if(!n||*pos+(size_t)n>=cap)return -1;
    for(int j=0;j<n;j++) dest[(*pos)++]=(char)b[j];
    return 0;
}
static int hex4(const char *p){int n=0;for(int i=0;i<4;i++){
    int h=p[i]>='0'&&p[i]<='9'?p[i]-'0':p[i]>='a'&&p[i]<='f'?p[i]-'a'+10:p[i]>='A'&&p[i]<='F'?p[i]-'A'+10:-1;
    if(h<0)return -1;
    n=(n<<4)|h;
}return n;}
static void string(Parser *p,int i,char *dest,size_t cap){
    if(!cap)return;
    dest[0]=0;
    if(i<0||p->t[i].kind!='"')return;
    const char *s=p->s+ p->t[i].start+1;const char *end=p->s+p->t[i].end-1;
    size_t at=0;
    while(s<end && at+1<cap){
        unsigned char c=(unsigned char)*s++;
        if(c=='\\'&&s<end){
            c=(unsigned char)*s++;
            if(c=='u'&&end-s>=4){
                int cp=hex4(s);s+=4;
                if(cp>=0xD800&&cp<=0xDBFF&&end-s>=6&&s[0]=='\\'&&s[1]=='u'){
                    int lo=hex4(s+2);
                    if(lo>=0xDC00&&lo<=0xDFFF){cp=0x10000+((cp-0xD800)<<10)+(lo-0xDC00);s+=6;}
                }
                if(cp>=0xD800&&cp<=0xDFFF)cp='?';
                if(utf8(dest,cap,&at,(uint32_t)cp)!=0)break;
                continue;
            }
            if(c=='n'||c=='r'||c=='t')c=' ';
            else if(c=='b'||c=='f')c=' ';
        }
        dest[at++]=(char)c;
    }
    dest[at]=0;
}
int tw_catalog_parse(const char *json,size_t length,TwCatalog *out){
    if(!json||!out||length>524288)return -1;
    memset(out,0,sizeof *out);
    Token *tokens=calloc(TW_JSON_TOKENS,sizeof *tokens);
    if(!tokens)return -1;
    Parser p={.s=json,.len=length,.t=tokens,.cap=TW_JSON_TOKENS};
    int root=parse(&p,0);
    ws(&p);
    if(root<0||p.pos!=length){free(tokens);return -1;}
    int results=get(&p,root,"results"),songs=get(&p,results,"songs"),data=get(&p,songs,"data");
    if(results<0 || p.t[results].kind!='{'){free(tokens);return -1;}
    if(songs<0){free(tokens);return 0;}
    if(data<0||p.t[data].kind!='['){free(tokens);return -1;}
    for(int i=data+1;i<p.t[data].next && out->count<TW_MAX_SONGS;){
        if(p.t[i].kind=='{'){
            int att=get(&p,i,"attributes");
            TwSong song={0};
            string(&p,get(&p,i,"id"),song.id,sizeof song.id);
            string(&p,get(&p,att,"name"),song.name,sizeof song.name);
            string(&p,get(&p,att,"artistName"),song.artist,sizeof song.artist);
            string(&p,get(&p,att,"albumName"),song.album,sizeof song.album);
            if(song.id[0]&&song.name[0])out->songs[out->count++]=song;
        }
        i=p.t[i].next;
    }
    free(tokens);
    return 0;
}
