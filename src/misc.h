/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/misc.h 2.39 2002/08/04 08:38:03 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7e.
  Miscellaneous HTTP / HTML functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef MISC_H
#define MISC_H    /*+ To stop multiple inclusions. +*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*+ A forward definition of the protocol type. +*/
typedef struct _Protocol *ProtocolP;

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

 char *hash;                    /*+ The hash value used to make the filename for this URL (may be null) +*/

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
void AddURLPassword(URL *Url,char *user,/*@null@*/ char *pass);
void FreeURL(/*@only@*/ URL *Url);

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

char /*@only@*/ *MakeHash(const char *args);

/* Added by Paul Rombouts:
   The following function can be used as a mnemonic version of MakeHash. */
inline static char *GetHash(URL *Url)
{
  if(!(Url->hash)) Url->hash=MakeHash(Url->file);
  return Url->hash;
}

#define MAXDATESIZE 32
#define MAXDURATIONSIZE 64
void RFC822Date_r(time_t t,int utc,char *buf);
char /*@observer@*/ *RFC822Date(time_t t,int utc);
time_t DateToTimeT(const char *date);
void DurationToString_r(const long duration,char *buf);

char /*@only@*/ *Base64Decode(const char *str,/*@out@*/ int *l);
char /*@only@*/ *Base64Encode(const char *str,int l);

char /*@only@*/ *HTMLString(const char* c,int nbsp);
char /*@only@*/ *HTML_url(char *url);



/* In io.c */

/*+ The buffer size for reading lines. +*/
#define BUFSIZE 256

void set_read_timeout(int timeout);

char /*@null@*/ /*@only@*/ *fgets_realloc(/*@out@*/ /*@returned@*/ /*@null@*/ /*@only@*/ char *buffer,FILE *file);

void init_buffer(int fd);

int /*@alt void@*/ empty_buffer(int fd);

int read_data(int fd,/*@out@*/ char *buffer,int n);
int read_data_or_timeout(int fd,/*@out@*/ char *buffer,int n);

char /*@null@*/ *read_line(int fd,/*@out@*/ /*@returned@*/ /*@null@*/ char *line);
char /*@null@*/ *read_line_or_timeout(int fd,/*@out@*/ /*@returned@*/ /*@null@*/ char *line);

int /*@alt void@*/ write_data(int fd,const char *data,int n);

int /*@alt void@*/ write_string(int fd,const char *str);

#ifdef __GNUC__
int /*@alt void@*/ write_formatted(int fd,const char *fmt,...) /*@printflike@*/ __attribute__ ((format (printf,2,3)));
#else
int /*@alt void@*/ write_formatted(int fd,const char *fmt,...) /*@printflike@*/;
#endif

#if USE_ZLIB
int init_zlib_buffer(int fd,int direction);
int finish_zlib_buffer(int fd);
#endif

int write_all(int fd,const char *data,int n);  /* placed here by Paul Rombouts */

off_t buffered_seek_cur(int fd);  /* added by Paul Rombouts */



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
  
#define sprintf_strdupa(format,str)  \
 ({ char *_str=(str); char *_result= (char *)alloca((sizeof(format)-2)+strlen(_str)); sprintf(_result,(format),_str); _result; })

#define STRDUP2(p,q)  strndup(p, (q)-(p))
#define STRDUPA2(p,q)  strndupa(p, (q)-(p))

/* STRSLICE is similar to STRDUPA but doesn't necessarily make a fresh copy */
#define STRSLICE(p,q)  (*(q)?STRDUPA2(p,q):(p))
#define STRDUP3(p,q,f) ({size_t _templen=(q)-(p); char _temp[_templen+1]; *((char *)mempcpy(_temp, (p), _templen))=0; f(_temp); })

#define local_strdup(str,copy) size_t _str_size=strlen(str)+1; char copy[_str_size]; memcpy(copy,str,_str_size);

#endif /* MISC_H */
