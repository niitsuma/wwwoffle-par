/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9g.
  Miscellaneous HTTP / HTML functions.
  ******************/ /******************
  Written by Andrew M. Bishop.
  Modified by Paul A. Rombouts.

  This file Copyright 1997-2010 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2005,2006,2007,2008 Paul A. Rombouts
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

#include <unistd.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <netinet/in.h>


/*+ The longest string needed to contain an integer. +*/
#define MAX_INT_STR 20  /* to hold a 64-bit unsigned long: 18446744073709551615ULL */
#define MAX_HEX_STR 16  /* to hold a 64-bit hex respresentation. */

typedef struct {
  uint32_t elem[4];
}
md5hash_t;

#if USE_IPV6
#define IPADDR struct in6_addr
#else /* use IPV4 */
#define IPADDR struct in_addr
#endif


/*+ A URL data type. +*/
typedef struct _URL
{
 char *original_name;           /*+ The original URL that was split to create this structure. +*/

 char *original_hostp;          /*+ A pointer to the host in the original URL. +*/
 char *original_pathp;          /*+ A pointer to the path in the original URL. +*/
 char *original_pathendp;       /*+ A pointer to end of the path in the original URL. +*/

 char *name;                    /*+ The canonical URL for the object without the username/password. +*/

 char *file;                    /*+ The URL that is used for generating the filename with the username/password (may point to name). +*/

 char *hostp;                   /*+ A pointer to the host in the URL. +*/
 char *pathp;                   /*+ A pointer to the path in the URL. +*/
 char *pathendp;                /*+ A pointer to the end of the path in the URL. +*/

 char *proto;                   /*+ The protocol. +*/
 char *hostport;                /*+ The host name and port number. +*/
 char *path;                    /*+ The path. +*/
 char *args;                    /*+ The arguments (also known as the query in RFCs). +*/

 char *user;                    /*+ The username if supplied (or else NULL). +*/
 char *pass;                    /*+ The password if supplied (or else NULL). +*/

 char *host;                    /*+ The host only part without the port (may point to hostport). +*/
 char *port;                    /*+ The port if supplied (points inside hostport, or is null) +*/
 int   portnum;                 /*+ The port (integer representation) +*/

 char *private_link;            /*+ A local URL for non-proxyable protocols (may point to name).  Private data. +*/

#ifdef __CYGWIN__
 char *private_dir;             /*+ The directory name for the host to avoid using ':' on Win32 (may point to hostport).  Private data. +*/
#endif

 md5hash_t hash;                /*+ The (binary) hash value used to make the filename for this URL. +*/
 IPADDR addr;                   /*+ The binary IP address belonging to the host name for this URL. +*/
 char hashvalid;                /*+ Set to true if the hash field contains the correct hash value. +*/
 char addrvalid;                /*+ Set to true if the addr field contains the correct IP address. +*/
}
URL;

inline static char *URL_get_original_args(URL *Url)
{ return *(Url->original_pathendp)=='?'? Url->original_pathendp+1: NULL; }


typedef struct {
  IPADDR addr;
  unsigned short port;
  unsigned short remotedns;
}
socksdata_t;


/*+ A request or reply header type. +*/
typedef struct _Header Header;


/*+ A request or reply body type. +*/
typedef struct _Body
{
 size_t length;                 /*+ The length of the content. +*/
 char content[0];               /*+ The content itself. +*/
}
Body;


/*+ A header value split into a list. +*/
typedef struct _HeaderList HeaderList;


/* in miscurl.c */

URL *SplitURL(const char *url);

#define REPLACEURLPROTO     1
#define REPLACEURLHOSTPORT  2
#define REPLACEURLPATH      4
#define REPLACEURLARGS      8
#define REPLACEURLUSER     16
#define REPLACEURLPASS     32
#define REPLACEURLALL (REPLACEURLPROTO|REPLACEURLHOSTPORT|REPLACEURLPATH|REPLACEURLARGS|REPLACEURLUSER|REPLACEURLPASS)

URL /*@special@*/ *MakeModifiedURL(const URL *Url,int modflags,const char *proto,const char *hostport,const char *path,/*@null@*/ const char *args,/*@null@*/ const char *user,/*@null@*/ const char *pass) /*@allocates result@*/ /*@defines result->name,result->link,result->dir,result->file,result->hostp,result->pathp,result->proto,result->hostport,result->path,result->args,result->user,result->pass,result->host,result->port,result->private_link,result->private_dir,result->private_file@*/;

#define CreateURL(proto,hostport,path,args,user,pass) MakeModifiedURL(NULL,REPLACEURLALL,proto,hostport,path,args,user,pass)
#define CopyURL(Url) MakeModifiedURL(Url,0,NULL,NULL,NULL,NULL,NULL,NULL)

void FreeURL(/*@special@*/ URL *Url) /*@releases Url@*/;

void ChangePasswordURL(URL *Url,/*@null@*/ const char *user,/*@null@*/ const char *pass);
URL /*@special@*/ *LinkURL(const URL *Url,const char *link) /*@allocates result@*/ /*@defines result->name,result->link,result->dir,result->file,result->hostp,result->pathp,result->proto,result->hostport,result->path,result->args,result->user,result->pass,result->host,result->port,result->private_link,result->private_dir,result->private_file@*/;

char /*@special@*/ *CanonicaliseHost(const char *host) /*@allocates result@*/;
void CanonicaliseName(char *name);

inline static void SplitHostPort(char *hostport,char **host,size_t *hostlen,char **port)
  __attribute__((always_inline));
inline static void SplitHostPort(char *hostport,char **host,size_t *hostlen,char **port)
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


/* In miscencdec.c */

char /*@only@*/ *URLDecodeGeneric(const char *str);
char /*@only@*/ *URLDecodeFormArgs(const char *str);

char /*@only@*/ *URLRecodeFormArgs(const char *str);

char /*@only@*/ *URLEncodePath(const char *str);
char /*@only@*/ *URLEncodeFormArgs(const char *str);
char /*@only@*/ *URLEncodePassword(const char *str);

char /*@only@*/ **SplitFormArgs(const char *str);

char *TrimArgs(/*@returned@*/ char *str);

inline static int md5_cmp(md5hash_t *a, md5hash_t *b)
  __attribute__((always_inline));
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

void MakeHash(const char *args, unsigned len, /*@out@*/ md5hash_t *h);
inline static void MakeStrHash(const char *args, md5hash_t *h)
  __attribute__((always_inline));
inline static void MakeStrHash(const char *args, md5hash_t *h) {MakeHash(args,strlen(args),h);}
char *hashbase64encode(md5hash_t *h, unsigned char *buf, unsigned buflen);
char *GetHash(URL *Url,char *buf, unsigned buflen);

/* Added by Paul Rombouts:
   Get the binary hash value of a URL. */
inline static md5hash_t *geturlhash(URL *Url)  __attribute__((always_inline));
inline static md5hash_t *geturlhash(URL *Url)
{
  if(!(Url->hashvalid)) {
    MakeStrHash(Url->file,&Url->hash);
    Url->hashvalid=1;
  }
  return &Url->hash;
}


#define MAXDATESIZE 32
#define MAXDURATIONSIZE 64
void RFC822Date_r(time_t t,int utc,char *buf);
char /*@observer@*/ *RFC822Date(time_t t,int utc);
time_t DateToTimeT(const char *date);
void DurationToString_r(const time_t duration,char *buf);

unsigned char /*@only@*/ *Base64Decode(const unsigned char *str,/*@out@*/ size_t *lp, unsigned char *buf, size_t buflen);
unsigned char /*@only@*/ *Base64Encode(const unsigned char *str,size_t l, unsigned char *buf, size_t buflen);
#define base64declenmax(n) ((3*(n))/4)
#define base64enclen(n) ((((n)+2)/3)*4)
/*+ The length of the hash created for a cached name +*/
#define CACHE_HASHED_NAME_LEN ((4*sizeof(md5hash_t)+2)/3)

char /*@only@*/ *FixHTMLLinkURL(const char *str);

char /*@only@*/ *HTMLString(const char* str,int nbsp, size_t *lenp);
char /*@only@*/ *HTML_url(char *url);
char *HTMLcommentstring(char *str, size_t *lenp);

char *strunescapechr(const char *str, char c);
char *strunescapepbrk(const char *str, const char *stopset);
#if 0
size_t strunescapelen(const char *str);
char *strunescapecpy(char *dst, const char *src);
char *strunescapedup(const char *str);
#endif
char *strunescapechr2(const char *str, const char *end, char c);
size_t strunescapelen2(const char *str, const char *end);
char *strunescapecpy2(char *dst, const char *src, const char *end);
#if 0
char *strunescapedup2(const char *str, const char *end);
#endif


/* following inline functions and macros were added by Paul Rombouts */

inline static void upcase(char *p)  __attribute__((always_inline));
inline static void upcase(char *p)
{
  for(;*p;++p) *p=toupper(*p);
}

inline static void downcase(char *p)  __attribute__((always_inline));
inline static void downcase(char *p)
{
  for(;*p;++p) *p=tolower(*p);
}

inline static void str_append(char **dst, const char *src)
  __attribute__((always_inline));
inline static void str_append(char **dst, const char *src)
{
  size_t strlen_dst=(*dst)?strlen(*dst):0;
  size_t sizeof_src=strlen(src)+1;
  *dst= (char *)realloc(*dst, strlen_dst+sizeof_src);
  memcpy(*dst+strlen_dst,src,sizeof_src);
}

inline static void strn_append(char **dst, size_t *lendst, const char *src)
  __attribute__((always_inline));
inline static void strn_append(char **dst, size_t *lendst, const char *src)
{
  size_t lensrc=strlen(src);
  size_t newlen=*lendst+lensrc;
  *dst= (char *)realloc(*dst, newlen+1);
  memcpy(*dst+*lendst,src,lensrc+1);
  *lendst=newlen;
}

inline static void chomp_str(char *str)  __attribute__((always_inline));
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
