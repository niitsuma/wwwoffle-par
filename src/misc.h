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

 char *proto;                   /*+ The protocol. +*/
 char *host;                    /*+ The host. +*/
 char *path;                    /*+ The path. +*/
 char *params;                  /*+ The parameters. +*/
 char *args;                    /*+ The arguments (also known as the query in RFCs). +*/

 ProtocolP Protocol;            /*+ The protocol. +*/

 char *user;                    /*+ The username if supplied. +*/
 char *pass;                    /*+ The password if supplied. +*/

 char *dir;                     /*+ The directory name for the host to avoid using ':' on Win32 (may point to host). +*/

 char local;                    /*+ Set to true if the host is the localhost. +*/
}
URL;

/*+ A request or reply header type. +*/
typedef struct _Header
{
 int type;                      /*+ The type of header, request=1 or reply=0. +*/

 char *method;                  /*+ The request method used. +*/
 char *url;                     /*+ The requested URL. +*/
 int status;                    /*+ The reply status. +*/
 char *note;                    /*+ The reply string. +*/
 char *version;                 /*+ The HTTP version. +*/

 int n;                         /*+ The number of header entries. +*/
 char **key;                    /*+ The name of the header line. +*/
 char **val;                    /*+ The value of the header line. +*/

 int size;                      /*+ The size of the header as read from the file/socket. +*/
}
Header;

/*+ A request or reply body type. +*/
typedef struct _Body
{
 int length;                    /*+ The length of the content. +*/

 char *content;                 /*+ The content itself. +*/
}
Body;


/*+ A header list item. +*/
typedef struct _HeaderListItem
{
 char *val;                     /*+ The string value. +*/
 float qval;                    /*+ The quality value. +*/
}
HeaderListItem;

/*+ A header value split into a list. +*/
typedef struct _HeaderList
{
 int n;                         /*+ The number of items in the list. +*/

 HeaderListItem *item;          /*+ The individual items (sorted into q value preference order). +*/
}
HeaderList;


/* in miscurl.c */

URL /*@only@*/ /*@unique@*/ *SplitURL(char *url);
void AddURLPassword(URL *Url,char *user,/*@null@*/ char *pass);
void FreeURL(/*@only@*/ URL *Url);

char *LinkURL(URL *Url,char *link);

char *CanonicaliseHost(char *host);
void CanonicaliseName(char *name);

void SplitHostPort(char *hostport,/*@out@*/ char **host,/*@out@*/ /*@null@*/ char **port);
void RejoinHostPort(char *hostport,char *host,/*@null@*/ char *port);


/* In miscencdec.c */

char /*@only@*/ *URLDecodeGeneric(const char *str);
char /*@only@*/ *URLDecodeFormArgs(const char *str);

char /*@only@*/ *URLRecodeFormArgs(const char *str);

char /*@only@*/ *URLEncodePath(const char *str);
char /*@only@*/ *URLEncodeFormArgs(const char *str);
char /*@only@*/ *URLEncodePassword(const char *str);

char /*@only@*/ **SplitFormArgs(char *str);

char /*@only@*/ *MakeHash(const char *args);

char /*@observer@*/ *RFC822Date(long t,int utc);
long DateToTimeT(const char *date);
char /*@observer@*/ *DurationToString(const long duration);

char /*@only@*/ *Base64Decode(const char *str,/*@out@*/ int *l);
char /*@only@*/ *Base64Encode(const char *str,int l);

char /*@only@*/* HTMLString(const char* c,int nbsp);


/* In headbody.c */

Header /*@only@*/ *CreateHeader(char *line,int type);

void AddToHeader(Header *head,/*@null@*/ char *key,char *val);
int AddToHeaderRaw(Header *head,char *line);

void ChangeURLInHeader(Header *head,char *url);
void ChangeNoteInHeader(Header *head,char *note);
void RemovePlingFromHeader(Header *head,char *url);

void RemoveFromHeader(Header *head,char* key);
void RemoveFromHeader2(Header *head,char* key,char *val);

char /*@null@*/ /*@observer@*/ *GetHeader(Header *head,char* key);
char /*@null@*/ /*@observer@*/ *GetHeader2(Header *head,char* key,char *val);

char /*@only@*/ *HeaderString(Header *head);

void FreeHeader(/*@only@*/ Header *head);

Body /*@only@*/ *CreateBody(int length);
void FreeBody(/*@only@*/ Body *body);

HeaderList /*@only@*/ *SplitHeaderList(char *val);
void FreeHeaderList(/*@only@*/ HeaderList *hlist);


/* In io.c */

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


#endif /* MISC_H */
