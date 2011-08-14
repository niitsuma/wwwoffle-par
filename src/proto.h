/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Information about the protocols that wwwoffle supports.
  ******************/ /******************
  Originally written by Andrew M. Bishop.
  Extensively modified by Paul A. Rombouts.

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2004,2006,2008,2009 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef PROTO_H
#define PROTO_H    /*+ To stop multiple inclusions. +*/

#include "misc.h"

typedef struct _Connection Connection;

/*+ A type to contain the information that is required for a particular protocol. +*/
typedef struct _Protocol
{
 char *name;                    /*+ The protocol name. +*/

 int defport;                   /*+ The default port number. +*/

 int proxyable;                 /*+ Set to true if any known proxies can understand this. +*/
 int postable;                  /*+ Set to true if the POST method works. +*/
 int putable;                   /*+ Set to true if the PUT method works. +*/
 int allowkeepalive;            /*+ Set to true if the protocal allows persistent connections. +*/

 char*   (*open)(Connection*,URL*);                  /*+ A function to open a connection to a remote server. +*/
 char*   (*request)(Connection*,URL*,Header*,Body*); /*+ A function to make a request to the remote server. +*/
 Header* (*readhead)(Connection*);                   /*+ A function to read the header from the remote server. +*/
 ssize_t (*readbody)(Connection*,char*,size_t);      /*+ A function to read the body data from the remote server. +*/
 int     (*finishbody)(Connection*);                 /*+ A function to clean up after reading the body data from the remote server. +*/
 int     (*close)(Connection*);                      /*+ A function to close the connection to the remote server. +*/
}
Protocol;


typedef struct hint_s {unsigned int line; int field;} hint_t;

struct _Connection
{
  const Protocol *protocol;
  URL  *Url;
  int ctrlfd,fd;
  URL *proxyUrl;
  socksdata_t *socksproxy;
  Header *bufferhead;   /*+ A header to contain the reply. (ftp) +*/
  char *buffer,         /*+ A buffer to contain the reply body (ftp). +*/
       *buffertail;     /*+ A buffer to contain the reply tail. (ftp) +*/
  /*+ The number of characters in the buffer +*/
  int nbuffer,          /*+ in total for the body part. +*/
      nread,            /*+ that have been read for the body. +*/
      nbuffertail,      /*+ in total for the tail . +*/
      nreadtail;        /*+ that have been read from the tail. +*/
  hint_t *hint;         /*+ Heuristic information for parsing ftp directory listings. +*/
  unsigned long rbytes, /*+ The number of raw bytes read. +*/
                wbytes; /*+ The number of raw bytes written. +*/
  short int loggedin;   /*+ Indicates whether we have logged in (ftp). +*/
  short int tryepsv;    /*+ Indicates whether the EPSV command should be tried first. +*/
  short int expectmore; /*+ Indicates whether to expect a further response from the control socket (ftp). +*/
};


/* In proto.c */

/*+ The list of protocols. +*/
extern const Protocol Protocols[];

/*+ The number of protocols. +*/
extern int NProtocols;

/* Definitions for SOCKS protocol */
#define DEFSOCKSPORT 1080

const Protocol *GetProtocol(const URL *Url);

int IsProtocolHandled(const URL *Url);

int DefaultPort(const URL *Url);

int IsPOSTHandled(const URL *Url);
int IsPUTHandled(const URL *Url);
int IsProxyHandled(const URL *Url);

char /*@observer@*/ *ProxyableLink(URL *Url);

Connection *ConnectionOpen(URL *Url, char **errmsg);

inline static char *ConnectionRequest(Connection *connection,URL *Url,Header *request_head,Body *request_body)
{ return (connection->protocol->request)(connection,Url,request_head,request_body);}

inline static Header *ConnectionReadHead(Connection *connection)
{ return (connection->protocol->readhead)(connection);}

inline static ssize_t ConnectionReadBody(Connection *connection,char *s,size_t n)
{ return (connection->protocol->readbody)(connection,s,n);}

inline static int ConnectionFinishBody(Connection *connection)
{ return (connection->protocol->finishbody)(connection);}

int ConnectionClose(Connection *connection);
int ConnectionIsReusable(Connection* connection, URL *Url2);

inline static int ConnectionAllowKeepAlive(Connection *connection)
{
  return (connection &&
	  ((connection->proxyUrl)?ConfigBooleanURL(ProxyKeepAlive,connection->proxyUrl):
				  connection->protocol->allowkeepalive));
}

inline static int ConnectionFd(Connection *connection)
{ return connection? (connection->fd==-1?connection->ctrlfd:connection->fd): -1;}

/* In http.c */

char    /*@null@*/ /*@only@*/ *HTTP_Open(Connection *connection,URL *Url);
char    /*@null@*/ /*@only@*/ *HTTP_Request(Connection *connection,URL *Url,Header *request_head,Body *request_body);
Header *HTTP_ReadHead(Connection *connection);
ssize_t HTTP_ReadBody(Connection *connection,char *s,size_t n);
int     HTTP_FinishBody(Connection *connection);
int     HTTP_Close(Connection *connection);

#if USE_GNUTLS

/* In https.c */

char    /*@null@*/ /*@only@*/ *HTTPS_Open(Connection *connection,URL *Url);
char    /*@null@*/ /*@only@*/ *HTTPS_Request(Connection *connection,URL *Url,Header *request_head,Body *request_body);
Header *HTTPS_ReadHead(Connection *connection);
ssize_t HTTPS_ReadBody(Connection *connection,char *s,size_t n);
int     HTTPS_FinishBody(Connection *connection);
int     HTTPS_Close(Connection *connection);

#endif

/* In ftp.c */

char    /*@null@*/ /*@only@*/ *FTP_Open(Connection *connection,URL *Url);
char    /*@null@*/ /*@only@*/ *FTP_Request(Connection *connection,URL *Url,Header *request_head,Body *request_body);
Header *FTP_ReadHead(Connection *connection);
ssize_t FTP_ReadBody(Connection *connection,char *s,size_t n);
int     FTP_FinishBody(Connection *connection);
int     FTP_Close(Connection *connection);

/* In finger.c */

char    /*@null@*/ /*@only@*/ *Finger_Open(Connection *connection,URL *Url);
char    /*@null@*/ /*@only@*/ *Finger_Request(Connection *connection,URL *Url,Header *request_head,Body *request_body);
Header *Finger_ReadHead(Connection *connection);
ssize_t Finger_ReadBody(Connection *connection,char *s,size_t n);
int     Finger_FinishBody(Connection *connection);
int     Finger_Close(Connection *connection);

/* In ssl.c */

char    /*@null@*/ /*@only@*/ *SSL_Open(URL *Url);
char    /*@null@*/ /*@only@*/ *SSL_Request(int client,URL *Url,Header *request_head);
void    SSL_Transfer(int client);
int     SSL_Close(void);

#endif /* PROTO_H */
