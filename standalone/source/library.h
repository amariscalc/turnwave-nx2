#ifndef TW_LIBRARY_H
#define TW_LIBRARY_H
#define TW_LOCAL_MAX 160
#define TW_LOCAL_PATH 512

typedef struct {char name[144];char path[TW_LOCAL_PATH];} TwLocalTrack;
typedef struct {TwLocalTrack tracks[TW_LOCAL_MAX];int count;} TwLocalLibrary;
int tw_library_scan(const char *directory,TwLocalLibrary *out);
#endif
