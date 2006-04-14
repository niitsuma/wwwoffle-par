/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/misc.h 2.44 2004/01/17 16:22:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Miscellaneous HTTP / HTML functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef MISC_H
#define MISC_H    /*+ To stop multiple inclusions. +*/

#include <stdio.h>
#include <stdlib.h>
#include <alloca.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

/*+ A forward definition of the protocol type. +*/
typedef struct _Protocol *ProtocolP;

typedef struct {
  uint32_t elem[4];
}
md5hash_t;


/*+ A URL data type. +*/
typedef struct _URL
{
 char *name;                    /*+ The canonical URL for the object. +*/

 char *link;                    /*+ A URL that will work for browsers (may point to name). +*/

 char *file;                    /*+ The URL that is used for generating the filename (may point to name). +*/

 char *hostp;                   /*+ A pointer to the host in the url. +*/
 char *pathp;                   /*+ A pointer to the path in the url. +*/
 char *pathendp;                /*+ A pointer to the end of the path in url +*/

 char *proto;                   /*+ The protocol. +*/
 char *hostport;                /*+ host and port combination +*/
 char *host;                    /*+ The host (may point to hostport) +*/
 char *port;                    /*+ The port (points inside hostport, or is null) +*/
 int   portnum;                 /*+ The port (integer representation) +*/
 char *path;                    /*+ The path. +*/
 char *params;                  /*+ The parameters. +*/
 char *args;                    /*+ The arguments (also known as the query in RFCs). +*/

 ProtocolP Protocol;            /*+ The protocol. +*/

 char *user;                    /*+ The username if supplied. +*/
 char *pass;                    /*+ The password if supplied. +*/

 char *dir;                     /*+ The directory name for the host to avoid using ':' on Win32 (may point to host). +*/

 md5hash_t hash;                /*+ The (binary) hash value used to make the filename for this URL +*/
 char hashvalid;                /*+ Set to true if the hash field contains the correct hash value. +*/

 char local;                    /*+ Set to true if the host is the localhost. +*/
}
URL;

/*+ A request or reply header type. +*/
typedef struct _Header Header;

/*+ A request or reply body type. +*/
typedef struct _Body
{
 int length;                    /*+ The length of the content. +*/
 char content[0];               /*+ The content itself. +*/
}
Body;

/*+ A header value split into a list. +*/
typedef struct _HeaderList HeaderList;


/* in miscurl.c */

URL /*@only@*/ /*@unique@*/ *SplitURL(const char *url);
void ChangeURLPassword(URL *Url,/*@null@*/ char *user,/*@null@*/ char *pass);
URL *CopyURL(URL *Url);
void FreeURL(/*@special@*/ URL *Url) /*@releases Url@*/;

char *LinkURL(URL *Url,char *link);

char *CanonicaliseHost(char *host);
void CanonicaliseName(char *name);

inline static void SplitHostPort(char *hostport,char **host,int *hostlen,char **port)
{
  *port=NULL;
  if(*hostport=='[') {   /* IPv6 */
    char *square=strchrnul(hostport,']');
    if(*square) {
      ++hostport;
      if(*(square+1)==':') *port=square+2;
    }
    *host=hostport;
    *hostlen=square-hostport;
  }
  else {
    char *colon=strchrnul(hostport,':');
    if(*colon) {
      *port=colon+1;
    }
    *host=hostport;
    *hostlen=colon-hostport;
  }
}

inline static char *ExtractPort(char *hostport)
{
  if(*hostport=='[') {   /* IPv6 */
    char *square=strchr(hostport,']');
    if(square) {
      if(*(square+1)==':') return square+2;
    }
  }
  else {
    char *colon=strchr(hostport,':');
    if(colon)
      return colon+1;
  }

  return NULL;
}


/* In miscencdec.c */

char /*@only@*/ *URLDecodeGeneric(const char *str);
char /*@only@*/ *URLDecodeFormArgs(const char *str);

char /*@only@*/ *URLRecodeFormArgs(const char *str);

char /*@only@*/ *URLEncodePath(const char *str);
char /*@only@*/ *URLEncodeFormArgs(const char *str);
char /*@only@*/ *URLEncodePassword(const char *str);

char /*@only@*/ **SplitFormArgs(char *str);

char *TrimArgs(/*@returned@*/ char *str);

inline static int md5_cmp(md5hash_t *a, md5hash_t *b)
{
  if(a->elem[0] < b->elem[0]) return -1;
  if(a->elem[0] > b->elem[0]) return 1;
  if(a->elem[1] < b->elem[1]) return -1;
  if(a->elem[1] > b->elem[1]) return 1;
  if(a->elem[2] < b->elem[2]) return -1;
  if(a->elem[2] > b->elem[2]) return 1;
  if(a->elem[3] < b->elem[3]) return -1;
  if(a->elem[3] > b->elem[3]) return 1;

  return 0;
}

void MakeHash(const unsigned char *args, /*@out@*/ md5hash_t *h);
char *hashbase64encode(md5hash_t *h, unsigned char *buf, unsigned buflen);

/* Added by Paul Rombouts:
   Get the binary hash value of a URL. */
inline static md5hash_t *geturlhash(URL *Url)
{
  if(!(Url->hashvalid)) {
    MakeHash((unsigned char *)Url->file,&Url->hash);
    Url->hashvalid=1;
  }
  return &Url->hash;
}

/* Added by Paul Rombouts:
   Get the base64 encoded string version of the URL hash. */
inline static char *GetHash(URL *Url,char *buf, unsigned buflen)
{
  if(!(Url->hashvalid)) {
    MakeHash((unsigned char *)Url->file,&Url->hash);
    Url->hashvalid=1;
  }
  return hashbase64encode(&Url->hash,(unsigned char *)buf,buflen);
}

#define MAXDATESIZE 32
#define MAXDURATIONSIZE 64
void RFC822Date_r(time_t t,int utc,char *buf);
char /*@observer@*/ *RFC822Date(time_t t,int utc);
time_t DateToTimeT(const char *date);
void DurationToString_r(const long duration,char *buf);

unsigned char /*@only@*/ *Base64Decode(const unsigned char *str,/*@out@*/ unsigned *lp, unsigned char *buf, unsigned buflen);
unsigned char /*@only@*/ *Base64Encode(const unsigned char *str,unsigned l, unsigned char *buf, unsigned buflen);
#define base64enclen(n) ((((n)+2)/3)*4)

void URLReplaceAmp(char *string);

char /*@only@*/ *HTMLString(const char* c,int nbsp);
char /*@only@*/ *HTML_url(char *url);
char *HTMLcommentstring(char *c);




/* following inline functions and macros were added by Paul Rombouts */

inline static void upcase(char *p)
{
  for(;*p;++p) *p=toupper(*p);
}

inline static void downcase(char *p)
{
  for(;*p;++p) *p=tolower(*p);
}

inline static void str_append(char **dst, const char *src)
{
  size_t strlen_dst=(*dst)?strlen(*dst):0;
  size_t sizeof_src=strlen(src)+1;
  *dst= (char *)realloc(*dst, strlen_dst+sizeof_src);
  memcpy(*dst+strlen_dst,src,sizeof_src);
}

inline static void strn_append(char **dst, size_t *lendst, const char *src)
{
  size_t lensrc=strlen(src);
  size_t newlen=*lendst+lensrc;
  *dst= (char *)realloc(*dst, newlen+1);
  memcpy(*dst+*lendst,src,lensrc+1);
  *lendst=newlen;
}

inline static void chomp_str(char *str)
{
  char *p=strchrnul(str,0);
  while(--p>=str && (*p=='\n' || *p=='\r')) *p=0;
}

/* I need a macro that gives the length of a string literal.
   This should preferably yield an integer constant.
   If the compiler is capable of constant folding and 
   recognizes strlen() as a builtin function (like gcc),
   the first definition is better (better type safety),
   otherwise the second alternative should be used. */
#define strlitlen(strlit) strlen(strlit)
/* #define strlitlen(strlit) (sizeof(strlit)-1) */

#define strcmp_beg(str,beg) strncmp(str,beg,strlen(beg))
#define strcasecmp_beg(str,beg) strncasecmp(str,beg,strlen(beg))

#define strcmp_litbeg(str,strlit) strncmp(str,strlit,strlitlen(strlit))
#define strcasecmp_litbeg(str,strlit) strncasecmp(str,strlit,strlitlen(strlit))

#define strcmp_litend(str,strlit) \
 ({ char *_endpart=strchrnul(str,0)-strlitlen(strlit); (_endpart>=(str))?strcmp(_endpart,strlit):-1; })
#define strcasecmp_litend(str,strlit) \
 ({ char *_endpart=strchrnul(str,0)-strlitlen(strlit); (_endpart>=(str))?strcasecmp(_endpart,strlit):-1; })

#define x_asprintf(...)  \
 ({ char *_result; (asprintf(&_result, __VA_ARGS__) >= 0) ? _result : NULL; })
  
#define STRDUP2(p,q)  strndup(p, (q)-(p))
#define STRDUPA2(p,q)  strndupa(p, (q)-(p))

/* STRSLICE is similar to STRDUPA but doesn't necessarily make a fresh copy */
#define STRSLICE(p,q)  (*(q)?STRDUPA2(p,q):(p))
#define STRDUP3(p,q,f) ({size_t _templen=(q)-(p); char _temp[_templen+1]; *((char *)mempcpy(_temp, (p), _templen))=0; f(_temp); })

#define local_strdup(str,copy) size_t _str_size=strlen(str)+1; char copy[_str_size]; memcpy(copy,str,_str_size);

#endif /* MISC_H */
