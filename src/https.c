/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9f.
  Functions for getting URLs using HTTPS.
  ******************/ /******************
  Originally written by Andrew M. Bishop.
  Extensively modified by Paul A. Rombouts.

  This file Copyright 1997-2010 Andrew M. Bishop
  Parts of this file Copyright (C) 2007,2008 Paul A. Rombouts
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

/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using HTTPS.

  char *HTTPS_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTPS_Open(Connection *connection,URL *Url)
{
 char *msg=NULL;
 char *proxy=NULL,*sproxy=NULL;
 int socksremotedns=0,server=-1;
 URL *connectUrl=Url,*proxyUrl=NULL;
 socksdata_t *socksdata=NULL,socksbuf;

 /* Sort out the host. */

 if(!IsLocalNetHost(Url->host)) {
   proxy=ConfigStringURL(SSLProxy,Url);
   sproxy=ConfigStringURL(SocksProxy,Url);
   socksremotedns=ConfigBooleanURL(SocksRemoteDNS,Url);
 }

 if(proxy)
   connectUrl=proxyUrl=CreateURL("http",proxy,"/",NULL,NULL,NULL);

 /* Open the connection. */

 if((sproxy && !(socksdata= MakeSocksData(sproxy,socksremotedns,&socksbuf))) ||
    (server=OpenUrlSocket(connectUrl,socksdata))==-1)
   {
    msg=GetPrintMessage(Warning,"Cannot open the https (SSL) connection to %s port %d; [%!s].",connectUrl->host,connectUrl->portnum);
    goto return_msg;
   }
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
    ssize_t err;

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

    PrintMessage(ExtraDebug,"Outgoing Request Head (to https (SSL) proxy)\n%s",head);

    err=write_data(server,head,headlen);

    free(head);
    FreeHeader(connect_request);

    if(err<0) {
       msg=GetPrintMessage(Warning,"Failed to write to remote https (SSL) proxy; [%!s].");
       goto close_return_msg;
    }

    connect_status=ParseReply(server,&connect_reply);

    if(StderrLevel==ExtraDebug) {
      if(connect_reply)
	{
	  head=HeaderString(connect_reply,NULL);
	  PrintMessage(ExtraDebug,"Incoming Reply Head (from SSL proxy)\n%s",head);
	  free(head);    
	}
      else
	PrintMessage(ExtraDebug,"Incoming Reply Head (from https (SSL) proxy) is empty.");
    }

    if(connect_reply) FreeHeader(connect_reply);

    if(connect_status!=200) {
       msg=GetPrintMessage(Warning,"Received error message from https (SSL) proxy; code=%d.",connect_status);
       goto close_return_msg;
    }
   }

 if(configure_io_gnutls(server,Url->hostport,0)) {
    msg=GetPrintMessage(Warning,"Cannot secure the https (SSL) connection to %s port %d; [%!s].",connectUrl->host,connectUrl->portnum);
    goto close_return_msg;
 }

 connection->fd=server;
 connection->proxyUrl=proxyUrl;
 return(NULL);

close_return_msg:
 finish_io(server);
 CloseSocket(server);
return_msg:
 if(proxyUrl) FreeURL(proxyUrl);
 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL.

  char *HTTPS_Request Returns NULL on success, a useful message on error.

  URL *Url The URL to get.

  Header *request_head The head of the HTTPS request for the URL.

  Body *request_body The body of the HTTPS request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTTPS_Request(Connection *connection,URL *Url,Header *request_head,Body *request_body)
{
 char *msg=NULL,*head;
 size_t headlen;
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
    msg=GetPrintMessage(Warning,"Failed to write head to remote https (SSL) %s; [%!s].",proxyUrl?"proxy":"server");
 else if(request_body && write_data(connection->fd,request_body->content,request_body->length)<0)
    msg=GetPrintMessage(Warning,"Failed to write body to remote https (SSL) %s; [%!s].",proxyUrl?"proxy":"server");

 free(head);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a line from the header of the reply for the URL.

  Header *HTTPS_ReadHead Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

Header *HTTPS_ReadHead(Connection *connection)
{
 Header *reply_head=NULL;
 ParseReply(connection->fd,&reply_head);

 return reply_head;
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  ssize_t HTTPS_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  size_t n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t HTTPS_ReadBody(Connection *connection,char *s,size_t n)
{
 return(read_data(connection->fd,s,n));
}


int HTTPS_FinishBody(Connection *connection)
{
 return(finish_io_content(connection->fd));
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using HTTPS.

  int HTTPS_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int HTTPS_Close(Connection *connection)
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

#endif /* USE_GNUTLS */
