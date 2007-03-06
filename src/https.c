/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/https.c 1.4 2006/01/20 19:01:29 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Functions for getting URLs using HTTPS.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>

#if USE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#endif

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "headbody.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


#if USE_GNUTLS

#include "certificates.h"

/*+ Set to the name of the proxy if there is one. +*/
static URL /*@null@*/ /*@only@*/ *proxyUrl=NULL;

/*+ The file descriptor of the server. +*/
static int server=-1;


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using HTTPS.

  char *HTTPS_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTPS_Open(URL *Url)
{
 char *msg=NULL;
 char *proxy=NULL,*sproxy=NULL;;
 char *server_host=NULL;
 int server_port=0;
 char *socks_host=NULL;
 int socks_port=0;
 int socksremotedns=0;

 /* Sort out the host. */

 if(!IsLocalNetHost(Url->host)) {
   proxy=ConfigStringURL(SSLProxy,Url);
   sproxy=ConfigStringURL(SocksProxy,Url);
   socksremotedns=ConfigBooleanURL(SocksRemoteDNS,Url);
 }

 if(proxyUrl) {
   FreeURL(proxyUrl);
   proxyUrl=NULL;
 }
 if(proxy)
   {
    proxyUrl=CreateURL("http",proxy,"/",NULL,NULL,NULL);
    server_host=proxyUrl->host;
    server_port=proxyUrl->portnum;
   }
 else
   {
    server_host=Url->host;
    server_port=Url->portnum;
   }

 if(sproxy)
   SETSOCKSHOSTPORT(sproxy,server_host,server_port,socks_host,socks_port);

 /* Open the connection. */

 server=OpenClientSocket(server_host,server_port,socks_host,socks_port,socksremotedns,NULL);

 if(server==-1)
    msg=GetPrintMessage(Warning,"Cannot open the HTTPS connection to %s port %d; [%!s].",server_host,server_port);
 else
   {
    init_io(server);
    configure_io_timeout_rw(server,ConfigInteger(SocketTimeout));
   }

 if(proxy)
   {
    char *head,*hostport;
    int connect_status;
    Header *connect_request,*connect_reply;
    size_t headlen;

    if(!Url->port)
      {
       hostport=(char*)alloca(strlen(Url->host)+sizeof(":443"));
       stpcpy(stpcpy(hostport,Url->host),":443");
      }
    else
       hostport=Url->hostport;

    CreateHeader("CONNECT fakeurl HTTP/1.0\r\n",1,&connect_request);
    AddToHeader(connect_request,"Host",hostport);

    MakeRequestProxyAuthorised(proxyUrl,connect_request);

    ChangeURLInHeader(connect_request,Url->hostport);

    head=HeaderString(connect_request,&headlen);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to SSL proxy)\n%s",head);

    if(write_data(server,head,headlen)==-1)
       msg=GetPrintMessage(Warning,"Failed to write to remote SSL proxy; [%!s].");

    free(head);
    FreeHeader(connect_request);

    if(msg)
       return(msg);

    connect_status=ParseReply(server,&connect_reply);

    if(StderrLevel==ExtraDebug)
      {
       head=HeaderString(connect_reply,NULL);
       PrintMessage(ExtraDebug,"Incoming Reply Head (from SSL proxy)\n%s",head);
       free(head);    
      }

    if(connect_status!=200)
       msg=GetPrintMessage(Warning,"Received error message from SSL proxy; code=%d.",connect_status);

    FreeHeader(connect_reply);

    if(msg)
       return(msg);
   }

 if(configure_io_gnutls(server,Url->hostport,0))
    msg=GetPrintMessage(Warning,"Cannot secure the HTTPS connection to %s port %d; [%!s].",server_host,server_port);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL.

  char *HTTPS_Request Returns NULL on success, a useful message on error.

  URL *Url The URL to get.

  Header *request_head The head of the HTTPS request for the URL.

  Body *request_body The body of the HTTPS request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTPS_Request(URL *Url,Header *request_head,Body *request_body)
{
 char *msg=NULL,*head;
 size_t headlen;

 /* Make the request OK for a proxy or not. */

 if(proxyUrl)
    MakeRequestProxyAuthorised(proxyUrl,request_head);
 else
    MakeRequestNonProxy(Url,request_head);

 /* Send the request. */

 head=HeaderString(request_head,&headlen);

 if(proxyUrl)
    PrintMessage(ExtraDebug,"Outgoing Request Head (to proxy)\n%s",head);
 else
    PrintMessage(ExtraDebug,"Outgoing Request Head (to server)\n%s",head);

 if(write_data(server,head,headlen)==-1)
    msg=GetPrintMessage(Warning,"Failed to write head to remote HTTPS %s; [%!s].",proxyUrl?"proxy":"server");
 if(request_body)
    if(write_data(server,request_body->content,request_body->length)==-1)
       msg=GetPrintMessage(Warning,"Failed to write body to remote HTTPS %s; [%!s].",proxyUrl?"proxy":"server");

 free(head);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a line from the header of the reply for the URL.

  int HTTPS_ReadHead Returns the server socket file descriptor.

  Header **reply_head Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTPS_ReadHead(Header **reply_head)
{
 ParseReply(server,reply_head);

 return(server);
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  ssize_t HTTPS_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  size_t n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t HTTPS_ReadBody(char *s,size_t n)
{
 return(read_data(server,s,n));
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using HTTPS.

  int HTTPS_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTPS_Close(void)
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

#endif /* USE_GNUTLS */
