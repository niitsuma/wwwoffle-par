/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/proto.h 1.14 2002/08/04 10:24:43 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7e.
  Information about the protocols that wwwoffle supports.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef PROTO_H
#define PROTO_H    /*+ To stop multiple inclusions. +*/

#include "misc.h"

/*+ A type to contain the information that is required for a particular protocol. +*/
typedef struct _Protocol
{
 int number;                    /*+ The protocol number. +*/
 char *name;                    /*+ The protocol name. +*/

 int defport;                   /*+ The default port number. +*/

 int proxyable;                 /*+ Set to true if any known proxies can understand this. +*/
 int postable;                  /*+ Set to true if the POST method works. +*/
 int putable;                   /*+ Set to true if the PUT method works. +*/

 char*(*open)(URL*);            /*+ A function to open a connection to a remote server. +*/
 char*(*request)(URL*,Header*,Body*); /*+ A function to make a request to the remote server. +*/
 int  (*readhead)(Header**);    /*+ A function to read the header from the remote server. +*/
 int  (*readbody)(char*,int);   /*+ A function to read the body data from the remote server. +*/
 int  (*close)(void);           /*+ A function to close the connection to the remote server. +*/
}
Protocol;

#define Protocol_HTTP    0      /*+ The http protocol. +*/
#define Protocol_FTP     1      /*+ The ftp protocol. +*/
#define Protocol_Finger  2      /*+ The finger protocol. +*/

/* In proto.c */

/*+ The list of protocols. +*/
extern Protocol Protocols[];

/*+ The number of protocols. +*/
extern int NProtocols;

/* In http.c */

char /*@null@*/ /*@observer@*/ *HTTP_Open(URL *Url);
char /*@null@*/ /*@observer@*/ *HTTP_Request(URL *Url,Header *request_head,Body *request_body);
int   HTTP_ReadHead(/*@out@*/ Header **reply_head);
int   HTTP_ReadBody(char *s,int n);
int   HTTP_Close(void);

/* In ftp.c */

char /*@null@*/ /*@observer@*/ *FTP_Open(URL *Url);
char /*@null@*/ /*@observer@*/ *FTP_Request(URL *Url,Header *request_head,Body *request_body);
int   FTP_ReadHead(/*@out@*/ Header **reply_head);
int   FTP_ReadBody(char *s,int n);
int   FTP_Close(void);

/* In finger.c */

char /*@null@*/ /*@observer@*/ *Finger_Open(URL *Url);
char /*@null@*/ /*@observer@*/ *Finger_Request(URL *Url,Header *request_head,Body *request_body);
int   Finger_ReadHead(/*@out@*/ Header **reply_head);
int   Finger_ReadBody(char *s,int n);
int   Finger_Close(void);

/* In ssl.c */

char /*@null@*/ /*@observer@*/ *SSL_Open(URL *Url);
char /*@null@*/ /*@observer@*/ *SSL_Request(int client,URL *Url,Header *request_head);
void  SSL_Transfer(int client);
int   SSL_Close(void);

#endif /* PROTO_H */
