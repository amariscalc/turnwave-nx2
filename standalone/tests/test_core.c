#include "wav.h"
#include "catalog.h"
#include "library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void test_wav(const char *filename){
    TwWav w;char error[160];
    assert(tw_wav_open(&w,filename,error,sizeof error)==0);
    assert(w.sample_rate==48000 && w.data_bytes==6*48000*4);
    char chunk[16384];
    assert(tw_wav_read(&w,chunk,sizeof chunk)==sizeof chunk);
    assert(w.bytes_read==sizeof chunk);
    assert(tw_wav_rewind(&w)==0);
    assert(w.bytes_read==0);
    size_t total=0,n;
    while((n=tw_wav_read(&w,chunk,sizeof chunk)))total+=n;
    assert(total==w.data_bytes);
    tw_wav_close(&w);
    assert(tw_wav_open(&w,"/missing/wav/file.wav",error,sizeof error)<0);
}
static void test_json(void){
    const char *json="{\"results\":{\"songs\":{\"data\":[{\"id\":\"1299\",\"attributes\":{\"name\":\"D\\u00e9j\\u00e0 Vu\",\"artistName\":\"Artiste\",\"albumName\":\"Album\"}},{\"id\":\"9988\",\"attributes\":{\"name\":\"Song B\",\"artistName\":\"B\"}}]}}}";
    TwCatalog cat={0};
    assert(tw_catalog_parse(json,strlen(json),&cat)==0);
    assert(cat.count==2);
    assert(!strcmp(cat.songs[0].name,"Déjà Vu"));
    assert(!strcmp(cat.songs[0].id,"1299"));
    assert(!strcmp(cat.songs[1].name,"Song B"));
    assert(tw_catalog_parse("{\"results\":[]}",strlen("{\"results\":[]}"),&cat)<0);
    assert(tw_catalog_parse("{\"results\":{}}",strlen("{\"results\":{}}"),&cat)==0 && cat.count==0);
    assert(tw_catalog_parse("{\"results\":",11,&cat)<0);
    assert(tw_catalog_parse("not json",8,&cat)<0);
}
static void test_library(const char *directory){
    TwLocalLibrary lib;
    assert(tw_library_scan(directory,&lib)==1);
    assert(strstr(lib.tracks[0].name,"Turnwave Test Tone.wav")!=NULL);
    assert(strstr(lib.tracks[0].path,".wav")!=NULL);
}
int main(int argc,char **argv){
    assert(argc==3);
    test_wav(argv[1]);test_json();test_library(argv[2]);
    puts("PASS: WAV parsing, EOF, rewind, malformed input; Apple catalog JSON, unicode, local file scanning");
    return 0;
}
