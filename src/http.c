/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/http.c 1.44 2006/01/08 10:27:21 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Functions for getting URLs using HTTP.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007,2008 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "headbody.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using HTTP.

  char *HTTP_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTP_Open(Connection *connection,URL *Url)
{
 char *msg=NULL;
 char *proxy=NULL,*sproxy=NULL;
 int socksremotedns=0,server=-1;
 URL *proxyUrl=NULL;
 socksdata_t *socksdata=NULL,socksbuf;

 /* Sort out the host. */

 if(!IsLocalNetHost(Url->host)) {
   proxy=ConfigStringURL(Proxies,Url);
   sproxy=ConfigStringURL(SocksProxy,Url);
   socksremotedns=ConfigBooleanURL(SocksRemoteDNS,Url);
 }

 if(proxy)
   Url=proxyUrl=CreateURL("http",proxy,"/",NULL,NULL,NULL);

 /* Open the connection. */

 if((sproxy && !(socksdata= MakeSocksData(sproxy,socksremotedns,&socksbuf))) ||
    (server=OpenUrlSocket(Url,socksdata))==-1)
   {
     msg=GetPrintMessage(Warning,"Cannot open the HTTP connection to %s port %d; [%!s].",Url->host,Url->portnum);
     if(proxyUrl) FreeURL(proxyUrl);
   }
 else
   {
     init_io(server);
     configure_io_timeout_rw(server,ConfigInteger(SocketTimeout));
     connection->fd=server;
     connection->proxyUrl=proxyUrl;
   }

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL.

  char *HTTP_Request Returns NULL on success, a useful message on error.

  URL *Url The URL to get.

  Header *request_head The head of the HTTP request for the URL.

  Body *request_body The body of the HTTP request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTP_Request(Connection *connection,URL *Url,Header *request_head,Body *request_body)
{
 char *msg=NULL,*head; size_t headlen;
 URL *proxyUrl=connection->proxyUrl;

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

 if(write_data(connection->fd,head,headlen)<0)
    msg=GetPrintMessage(Warning,"Failed to write head to remote HTTP %s; [%!s].",proxyUrl?"proxy":"server");
 else if(request_body && write_data(connection->fd,request_body->content,request_body->length)<0)
    msg=GetPrintMessage(Warning,"Failed to write body to remote HTTP %s; [%!s].",proxyUrl?"proxy":"server");

 free(head);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a line from the header of the reply for the URL.

  Header *HTTP_ReadHead Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

Header *HTTP_ReadHead(Connection *connection)
{
 Header *reply_head=NULL;
 ParseReply(connection->fd,&reply_head);

 return reply_head;
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  ssize_t HTTP_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  size_t n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t HTTP_ReadBody(Connection *connection,char *s,size_t n)
{
 return(read_data(connection->fd,s,n));
}


int HTTP_FinishBody(Connection *connection)
{
 return(finish_io_content(connection->fd));
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using HTTP.

  int HTTP_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTP_Close(Connection *connection)
{
 unsigned long r,w;
 int server=connection->fd;

 if(finish_tell_io(server,&r,&w)<0)
   PrintMessage(Inform,"Error finishing IO on socket with remote host [%!s].");

 PrintMessage(Inform,"Server bytes; %lu Read, %lu Written.",r,w); /* Used in audit-usage.pl */

#if 0
 /* We can clean up the connection context here, or rely on ConnectionClose to do it. */
 if(connection->proxyUrl) {FreeURL(connection->proxyUrl); connection->proxyUrl=NULL;}
#endif

 return(CloseSocket(server));
}
