#include "library.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

static int wav_ext(const char *name){size_t n=strlen(name);return n>4&&name[n-4]=='.'&&
    tolower((unsigned char)name[n-3])=='w'&&tolower((unsigned char)name[n-2])=='a'&&tolower((unsigned char)name[n-1])=='v';}
static int cmp_track(const void *a,const void *b){return strcmp(((const TwLocalTrack*)a)->name,((const TwLocalTrack*)b)->name);}
int tw_library_scan(const char *directory,TwLocalLibrary *out){
    if(!directory||!out)return -1;
    memset(out,0,sizeof *out);
    DIR *dir=opendir(directory);if(!dir)return -1;
    struct dirent *e;
    while((e=readdir(dir)) && out->count<TW_LOCAL_MAX){
        const char *name=e->d_name;
        if(!wav_ext(name)||strchr(name,'/')||strchr(name,'\\'))continue;
        TwLocalTrack *song=&out->tracks[out->count];
        if(snprintf(song->path,sizeof song->path,"%s/%s",directory,name)>=(int)sizeof song->path)continue;
        if(snprintf(song->name,sizeof song->name,"%s",name)>=(int)sizeof song->name)continue;
        out->count++;
    }
    closedir(dir);
    qsort(out->tracks,out->count,sizeof out->tracks[0],cmp_track);
    return out->count;
}
