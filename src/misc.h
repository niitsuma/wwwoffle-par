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

URL /*@special@*/ *SplitURL(char *url) /*@allocates result@*/ /*@defines result->name,result->link,result->file,result->hostp,result->pathp,result->proto,result->host,result->path,result->params,result->args,result->Protocol,result->user,result->pass,result->dir,result->local@*/;
void AddURLPassword(URL *Url,char *user,/*@null@*/ char *pass);
void FreeURL(/*@special@*/ URL *Url) /*@releases Url@*/;

char *LinkURL(URL *Url,char *link);

char *CanonicaliseHost(char *host);
void CanonicaliseName(char *name);

void SplitHostPort(char *hostport,/*@out@*/ char **host,/*@out@*/ char **port);
void RejoinHostPort(char *hostport,char *host,/*@null@*/ char *port);


/* In miscencdec.c */

char /*@only@*/ *URLDecodeGeneric(const char *str);
char /*@only@*/ *URLDecodeFormArgs(const char *str);

char /*@only@*/ *URLRecodeFormArgs(const char *str);

char /*@only@*/ *URLEncodePath(const char *str);
char /*@only@*/ *URLEncodeFormArgs(const char *str);
char /*@only@*/ *URLEncodePassword(const char *str);

char /*@only@*/ **SplitFormArgs(char *str);

char *TrimArgs(/*@returned@*/ char *str);

char /*@only@*/ *MakeHash(const char *args);

char /*@observer@*/ *RFC822Date(long t,int utc);
long DateToTimeT(const char *date);
char /*@observer@*/ *DurationToString(const long duration);

char /*@only@*/ *Base64Decode(const char *str,/*@out@*/ int *l);
char /*@only@*/ *Base64Encode(const char *str,int l);

void URLReplaceAmp(char *string);

char /*@only@*/* HTMLString(const char* c,int nbsp);


/* In headbody.c */

Header /*@special@*/ *CreateHeader(char *line,int type) /*@allocates result@*/ /*@defines result->method,result->url,result->note,result->version,result->key,result->val@*/;

void AddToHeader(Header *head,/*@null@*/ char *key,char *val);
int AddToHeaderRaw(Header *head,char *line);

void ChangeURLInHeader(Header *head,char *url);
void ChangeNoteInHeader(Header *head,char *note);
void RemovePlingFromHeader(Header *head,char *url);
void ChangeVersionInHeader(Header *head,char *version);

void RemoveFromHeader(Header *head,char* key);
void RemoveFromHeader2(Header *head,char* key,char *val);

char /*@null@*/ /*@observer@*/ *GetHeader(Header *head,char* key);
char /*@null@*/ /*@observer@*/ *GetHeader2(Header *head,char* key,char *val);

char /*@only@*/ *HeaderString(Header *head);

void FreeHeader(/*@special@*/ Header *head) /*@releases head@*/;

Body /*@only@*/ *CreateBody(int length);
void FreeBody(/*@special@*/ Body *body) /*@releases body@*/;

HeaderList /*@only@*/ *SplitHeaderList(char *val);
void FreeHeaderList(/*@special@*/ HeaderList *hlist) /*@releases hlist@*/;


#endif /* MISC_H */
