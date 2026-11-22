#include "catalog.h"
#include <curl/curl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define TW_MAX_RESPONSE (512u*1024u)
#define TW_MAX_JWT 8192
static int read_line(const char *path,char *out,size_t cap) {
    FILE *f=fopen(path,"rb");if(!f)return -1;
    if(!fgets(out,(int)cap,f)){fclose(f);return -1;}
    fclose(f);
    size_t n=strcspn(out,"\r\n");out[n]=0;
    return n ? 0 : -1;
}
static void seterr(char *dst,size_t cap,const char *text) {
    if(dst&&cap)snprintf(dst,cap,"%s",text);
}
typedef struct { char *data;size_t size;int overflow; } Body;
static size_t callback(void *ptr,size_t size,size_t count,void *userdata){
    Body *body=userdata;
    if(!size||count>SIZE_MAX/size)return 0;
    size_t len=size*count;
    if(len>TW_MAX_RESPONSE-body->size){body->overflow=1;return 0;}
    memcpy(body->data+body->size,ptr,len);body->size+=len;body->data[body->size]=0;
    return len;
}
int tw_catalog_search(const char *jwt_file,const char *query_file,
                      const char *ca_file,TwCatalog *out,char *error,size_t errsz){
    if(!jwt_file||!out){seterr(error,errsz,"Invalid catalog configuration");return -1;}
    memset(out,0,sizeof *out);
    char token[TW_MAX_JWT],query[192]="Daft Punk";
    if(read_line(jwt_file,token,sizeof token)){
        seterr(error,errsz,"Missing developer-token.txt (signed JWT)");return -1;
    }
    if(strchr(token,' ')||strchr(token,'\t')||strchr(token,'\r')||strchr(token,'\n')){
        memset(token,0,sizeof token);seterr(error,errsz,"Developer token contains invalid whitespace");return -1;
    }
    if(query_file){char user_query[sizeof query];if(!read_line(query_file,user_query,sizeof user_query))strcpy(query,user_query);}
    if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK){seterr(error,errsz,"Cannot initialize TLS");return -1;}
    CURL *curl=curl_easy_init();
    char *body=malloc(TW_MAX_RESPONSE+1);
    if(!curl||!body){seterr(error,errsz,"Out of memory");free(body);if(curl)curl_easy_cleanup(curl);curl_global_cleanup();return -1;}
    Body result={.data=body};
    char *escaped=curl_easy_escape(curl,query,0);
    if(!escaped){seterr(error,errsz,"URL encoding failed");free(body);curl_easy_cleanup(curl);curl_global_cleanup();return -1;}
    size_t urlsize=strlen(escaped)+120;
    char *url=malloc(urlsize);
    if(!url){seterr(error,errsz,"Out of memory");curl_free(escaped);free(body);curl_easy_cleanup(curl);curl_global_cleanup();return -1;}
    snprintf(url,urlsize,"https://api.music.apple.com/v1/catalog/es/search?types=songs&limit=20&term=%s",escaped);
    curl_free(escaped);
    struct curl_slist *headers=NULL;
    size_t authsize=strlen(token)+24;
    char *auth=malloc(authsize);
    if(!auth){seterr(error,errsz,"Out of memory");free(url);free(body);curl_easy_cleanup(curl);curl_global_cleanup();return -1;}
    snprintf(auth,authsize,"Authorization: Bearer %s",token);
    memset(token,0,sizeof token);
    headers=curl_slist_append(headers,auth);
    memset(auth,0,authsize);free(auth);
    if(!headers){seterr(error,errsz,"HTTP header allocation failed");free(url);free(body);curl_easy_cleanup(curl);curl_global_cleanup();return -1;}
    struct curl_slist *accepted=curl_slist_append(headers,"Accept: application/json");
    if(!accepted){seterr(error,errsz,"HTTP header allocation failed");curl_slist_free_all(headers);free(url);free(body);curl_easy_cleanup(curl);curl_global_cleanup();return -1;}
    headers=accepted;
    curl_easy_setopt(curl,CURLOPT_URL,url);
    curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);
    curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,callback);
    curl_easy_setopt(curl,CURLOPT_WRITEDATA,&result);
    curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,0L);
    curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT,6L);
    curl_easy_setopt(curl,CURLOPT_TIMEOUT,12L);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYPEER,1L);
    curl_easy_setopt(curl,CURLOPT_SSL_VERIFYHOST,2L);
    curl_easy_setopt(curl,CURLOPT_USERAGENT,"TurnwaveNX2-Native/0.7");
    if(ca_file){FILE *f=fopen(ca_file,"rb");if(f){fclose(f);curl_easy_setopt(curl,CURLOPT_CAINFO,ca_file);}}
    CURLcode rc=curl_easy_perform(curl);
    long code=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&code);
    int ok=0;
    if(rc!=CURLE_OK){seterr(error,errsz,result.overflow?"Apple response exceeded 512 KiB":"HTTPS request failed; check TLS/connection");ok=-1;}
    else if(code!=200){char detail[80];snprintf(detail,sizeof detail,"Apple Music API returned HTTP %ld",code);seterr(error,errsz,detail);ok=-1;}
    else if(tw_catalog_parse(result.data,result.size,out)){seterr(error,errsz,"Cannot parse Apple Music response");ok=-1;}
    else if(error&&errsz)error[0]=0;
    curl_slist_free_all(headers);curl_easy_cleanup(curl);free(url);free(body);curl_global_cleanup();
    return ok;
}
