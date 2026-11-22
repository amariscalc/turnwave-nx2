#include "wav.h"
#include <string.h>
#include <limits.h>

static uint16_t le16(const unsigned char *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static uint32_t le32(const unsigned char *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                                                        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static int fail(TwWav *w, char *s, size_t n, const char *reason) {
    if(s && n) { strncpy(s, reason, n-1); s[n-1] = '\0'; }
    if(w->file) fclose(w->file);
    memset(w,0,sizeof *w);
    return -1;
}
int tw_wav_open(TwWav *w, const char *path, char *err, size_t errsz) {
    if(!w || !path) return -1;
    memset(w,0,sizeof *w);
    w->file = fopen(path,"rb");
    if(!w->file) return fail(w,err,errsz,"Cannot open WAV file");
    if(fseek(w->file,0,SEEK_END)!=0) return fail(w,err,errsz,"Cannot size WAV file");
    long file_len=ftell(w->file);
    if(file_len<44 || fseek(w->file,0,SEEK_SET)!=0)
        return fail(w,err,errsz,"Invalid WAV length");
    unsigned char riff[12];
    if(fread(riff,1,12,w->file)!=12 || memcmp(riff,"RIFF",4) || memcmp(riff+8,"WAVE",4))
        return fail(w,err,errsz,"Expected RIFF/WAVE");
    /* Clamp to the declared RIFF size; reject chunk overflow and truncation. */
    uint64_t declared=(uint64_t)le32(riff+4)+8;
    if(declared> (uint64_t)file_len || declared<44)
        return fail(w,err,errsz,"WAV RIFF size is truncated or invalid");
    long bound=(long)declared;
    int found_fmt=0,found_data=0;
    uint16_t format=0,channels=0,bits=0,align=0;
    uint32_t rate=0,byte_rate=0,databytes=0;
    long data_start=0;
    while(ftell(w->file)>=0 && ftell(w->file)<=bound-8) {
        unsigned char h[8];
        if(fread(h,1,8,w->file)!=8) return fail(w,err,errsz,"Truncated WAV chunk header");
        long start=ftell(w->file);
        uint32_t sz=le32(h+4);
        uint64_t after=(uint64_t)(unsigned long)start+sz+(sz&1u);
        if(after>(uint64_t)bound || after>LONG_MAX)
            return fail(w,err,errsz,"Invalid WAV chunk bounds");
        if(!memcmp(h,"fmt ",4) && !found_fmt) {
            unsigned char fmt[16];
            if(sz<16 || fread(fmt,1,16,w->file)!=16)
                return fail(w,err,errsz,"Truncated WAV format chunk");
            format=le16(fmt);channels=le16(fmt+2);rate=le32(fmt+4);
            byte_rate=le32(fmt+8);align=le16(fmt+12);bits=le16(fmt+14);
            found_fmt=1;
        }
        if(!memcmp(h,"data",4) && !found_data) {
            data_start=start;databytes=sz;found_data=1;
        }
        if(found_fmt && found_data) break;
        if(fseek(w->file,(long)after,SEEK_SET)!=0)
            return fail(w,err,errsz,"WAV seek failed");
    }
    if(!found_fmt || !found_data) return fail(w,err,errsz,"WAV missing fmt/data");
    if(format!=1 || channels!=2 || rate!=48000 || bits!=16 || align!=4 || byte_rate!=192000)
        return fail(w,err,errsz,"Requires PCM 48kHz stereo 16-bit WAV");
    if(databytes==0 || (databytes%4)!=0 || fseek(w->file,data_start,SEEK_SET)!=0)
        return fail(w,err,errsz,"Invalid WAV PCM data");
    w->data_offset=data_start;w->data_bytes=databytes;w->sample_rate=rate;
    if(err && errsz) err[0]=0;
    return 0;
}
size_t tw_wav_read(TwWav *w,void *out,size_t bytes) {
    if(!w || !w->file || !out || w->bytes_read>=w->data_bytes) return 0;
    uint32_t remains=w->data_bytes-w->bytes_read;
    if(bytes>remains) bytes=remains;
    bytes-=bytes%4;
    size_t got=fread(out,1,bytes,w->file);
    got-=got%4;
    w->bytes_read+=(uint32_t)got;
    return got;
}
int tw_wav_rewind(TwWav *w) {
    if(!w||!w->file||fseek(w->file,w->data_offset,SEEK_SET))return -1;
    w->bytes_read=0;return 0;
}
void tw_wav_close(TwWav *w) {if(w){if(w->file)fclose(w->file);memset(w,0,sizeof *w);}}
