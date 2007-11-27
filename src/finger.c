/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/finger.c 1.30 2007/04/01 10:38:28 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  Functions for getting URLs using Finger.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1998,99,2000,01,02,03,04,05,06,07 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "headbody.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


/*+ Set to the name of the proxy if there is one. +*/
static URL /*@null@*/ /*@only@*/ *proxyUrl=NULL;

/*+ The file descriptor of the server. +*/
static int server=-1;


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using Finger.

  char *Finger_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *Finger_Open(URL *Url)
{
 char *msg=NULL;
 char *proxy=NULL,*sproxy=NULL;
 char *server_host=NULL;
 int server_port=0;
 char *socks_host=NULL;
 int socks_port=0;
 int socksremotedns=0;

 /* Sort out the host. */

 if(!IsLocalNetHost(Url->host)) {
   proxy=ConfigStringURL(Proxies,Url);
   sproxy=ConfigStringURL(SocksProxy,Url);
   socksremotedns=ConfigBooleanURL(SocksRemoteDNS,Url);
 }

 if(proxyUrl) {
   FreeURL(proxyUrl);
   proxyUrl=NULL;
 }
 if(proxy) {
   proxyUrl=CreateURL("http",proxy,"/",NULL,NULL,NULL);
   server_host=proxyUrl->host;
   server_port=proxyUrl->portnum;
 }
 else {
   server_host=Url->host;
   server_port=Url->portnum;
 }

 if(sproxy)
   SETSOCKSHOSTPORT(sproxy,server_host,server_port,socks_host,socks_port);

 /* Open the connection. */

 server=OpenClientSocket(server_host,server_port,socks_host,socks_port,socksremotedns,NULL);

 if(server==-1)
    msg=GetPrintMessage(Warning,"Cannot open the Finger connection to %s port %d; [%!s].",server_host,server_port);
 else
   {
    init_io(server);
    configure_io_timeout_rw(server,ConfigInteger(SocketTimeout));
   }

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL.

  char *Finger_Request Returns NULL on success, a useful message on error.

  URL *Url The URL to get.

  Header *request_head The head of the Finger request for the URL.

  Body *request_body The body of the Finger request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *Finger_Request(URL *Url,Header *request_head,/*@unused@*/ Body *request_body)
{
 char *msg=NULL;
 char *user;

 /* Take a simple route if it is proxied. */

 if(proxyUrl)
   {
    char *head; size_t head_len;

    MakeRequestProxyAuthorised(proxyUrl,request_head);

    head=HeaderString(request_head,&head_len);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to proxy)\n%s",head);

    if(write_data(server,head,head_len)<0)
       msg=GetPrintMessage(Warning,"Failed to write to remote Finger proxy; [%!s].");

    free(head);

    return(msg);
   }

 /* Else Sort out the path. */

 user=Url->path+1;
 {char *slash=strchr(user,'/'); if(slash) user=STRDUPA2(user,slash);}

 if(*user)
   {
    if(write_formatted(server,"/W %s\r\n",user)<0)
       msg=GetPrintMessage(Warning,"Failed to write to remote Finger server; [%!s].");
   }
 else
   {
    if(write_string(server,"/W\r\n")<0)
       msg=GetPrintMessage(Warning,"Failed to write to remote Finger server; [%!s].");
   }

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a line from the header of the reply for the URL.

  int Finger_ReadHead Returns the server socket file descriptor.

  Header **reply_head Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

int Finger_ReadHead(Header **reply_head)
{
 *reply_head=NULL;

 /* Take a simple route if it is proxied. */

 if(proxyUrl)
   {
    ParseReply(server,reply_head);

    return(server);
   }

 /* Else send the header. */

 CreateHeader("HTTP/1.0 200 Finger OK",0,reply_head);

 AddToHeader(*reply_head,"Content-Type","text/plain");

 return(server);
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  ssize_t Finger_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  size_t n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t Finger_ReadBody(char *s,size_t n)
{
 return(read_data(server,s,n));
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using Finger.

  int Finger_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int Finger_Close(void)
{
 unsigned long r,w;

 if(finish_tell_io(server,&r,&w)<0)
   PrintMessage(Inform,"Error finishing IO on socket with remote host [%!s].");

 PrintMessage(Inform,"Server bytes; %lu Read, %lu Written.",r,w); /* Used in audit-usage.pl */

 if(proxyUrl) {
   FreeURL(proxyUrl);
   proxyUrl=NULL;
 }

 return(CloseSocket(server));
}
