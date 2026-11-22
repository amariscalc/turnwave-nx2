#ifndef TW_CATALOG_H
#define TW_CATALOG_H
#include <stddef.h>
#define TW_MAX_SONGS 30

typedef struct {
    char id[72];
    char name[144];
    char artist[112];
    char album[112];
} TwSong;
typedef struct {
    TwSong songs[TW_MAX_SONGS];
    int count;
} TwCatalog;

/* Pure JSON parser: available to Linux host tests without console SDK. */
int tw_catalog_parse(const char *json,size_t length,TwCatalog *out);
/* HTTPS read-only lookup; requires a manually provisioned, signed developer JWT.
   Never returns protected audio or the user's library without authorization. */
int tw_catalog_search(const char *jwt_file,const char *query_file,
                      const char *ca_file,TwCatalog *out,char *error,size_t error_size);
#endif
