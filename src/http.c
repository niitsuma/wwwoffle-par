/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/http.c 1.35 2003/12/14 10:53:53 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Functions for getting URLs using HTTP.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


/*+ Set to the name of the proxy if there is one. +*/
static /*@null@*/ /*@observer@*/ char *proxy=NULL;

/*+ The file descriptor of the server. +*/
static int server=-1;


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using HTTP.

  char *HTTP_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTP_Open(URL *Url)
{
 char *msg=NULL;
 char *hoststr,*portstr;
 char *server_host=NULL;
 int server_port=Protocols[Protocol_HTTP].defport;

 /* Sort out the host. */

 proxy=ConfigStringURL(Proxies,Url);
 if(IsLocalNetHost(Url->host))
    proxy=NULL;

 if(proxy)
    server_host=proxy;
 else
    server_host=Url->host;

 SplitHostPort(server_host,&hoststr,&portstr);

 if(portstr)
    server_port=atoi(portstr);

 /* Open the connection. */

 server=OpenClientSocket(hoststr,server_port);

 if(server==-1)
    msg=GetPrintMessage(Warning,"Cannot open the HTTP connection to %s port %d; [%!s].",hoststr,server_port);
 else
   {
    init_io(server);
    configure_io_read(server,ConfigInteger(SocketTimeout),0,0);
    configure_io_write(server,ConfigInteger(SocketTimeout),0,0);
   }

 RejoinHostPort(server_host,hoststr,portstr);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL.

  char *HTTP_Request Returns NULL on success, a useful message on error.

  URL *Url The URL to get.

  Header *request_head The head of the HTTP request for the URL.

  Body *request_body The body of the HTTP request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTP_Request(URL *Url,Header *request_head,Body *request_body)
{
 char *msg=NULL,*head;

 /* Make the request OK for a proxy or not. */

 if(proxy)
    MakeRequestProxyAuthorised(proxy,request_head);
 else
    MakeRequestNonProxy(Url,request_head);

 /* Send the request. */

 head=HeaderString(request_head);

 if(proxy)
    PrintMessage(ExtraDebug,"Outgoing Request Head (to proxy)\n%s",head);
 else
    PrintMessage(ExtraDebug,"Outgoing Request Head (to server)\n%s",head);

 if(write_string(server,head)==-1)
    msg=GetPrintMessage(Warning,"Failed to write head to remote HTTP %s; [%!s].",proxy?"proxy":"server");
 if(request_body)
    if(write_data(server,request_body->content,request_body->length)==-1)
       msg=GetPrintMessage(Warning,"Failed to write body to remote HTTP %s; [%!s].",proxy?"proxy":"server");

 free(head);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a line from the header of the reply for the URL.

  int HTTP_ReadHead Returns the server socket file descriptor.

  Header **reply_head Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTP_ReadHead(Header **reply_head)
{
 ParseReply(server,reply_head);

 return(server);
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  int HTTP_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  int n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTP_ReadBody(char *s,int n)
{
 return(read_data(server,s,n));
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using HTTP.

  int HTTP_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTP_Close(void)
{
 unsigned long r,w;

 finish_tell_io(server,&r,&w);

 PrintMessage(Inform,"Server bytes; %d Read, %d Written.",r,w); /* Used in audit-usage.pl */

 return(CloseSocket(server));
}
