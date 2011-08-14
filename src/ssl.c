/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9f.
  SSL (Secure Socket Layer) Tunneling functions.
  ******************/ /******************
  Written by Andrew M. Bishop.
  Modified by Paul A. Rombouts.

  This file Copyright 1998-2010 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2004,2006,2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <unistd.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <errno.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "headbody.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


/*+ Set to the name of the proxy if there is one. +*/
static URL /*@null@*/ /*@only@*/ *proxyUrl=NULL;

/*+ The file descriptor of the server. +*/
static int server=-1;


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using SSL tunnelling.

  char *SSL_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open (used for host only).
  ++++++++++++++++++++++++++++++++++++++*/

char *SSL_Open(URL *Url)
{
 char *msg=NULL;
 char *proxy=NULL,*sproxy=NULL;
 int socksremotedns=0;
 socksdata_t *socksdata=NULL,socksbuf;

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
   Url=proxyUrl=CreateURL("http",proxy,"/",NULL,NULL,NULL);

 /* Open the connection. */

 server=-1;

 if(Url->portnum)
   {
    if((sproxy && !(socksdata= MakeSocksData(sproxy,socksremotedns,&socksbuf))) ||
       (server=OpenUrlSocket(Url,socksdata))==-1)
      {
       msg=GetPrintMessage(Warning,"Cannot open the https (SSL) connection to %s port %d; [%!s].",Url->host,Url->portnum);
      }
    else
      {
       init_io(server);
       configure_io_timeout_rw(server,ConfigInteger(SocketTimeout));
      }
   }
 else
    msg=GetPrintMessage(Warning,"No port given for the https (SSL) connection to %s.",Url->host);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL and reply to the client.

  char *SSL_Request Returns NULL on success, a useful message on error.

  int client The client socket.

  URL *Url The URL to get (used for host only).

  Header *request_head The head of the HTTP request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *SSL_Request(int client,URL *Url,Header *request_head)
{
 char *msg=NULL;

 if(proxyUrl)
   {
    char *head; size_t headlen;

    ModifyRequest(Url,request_head);

    MakeRequestProxyAuthorised(proxyUrl,request_head);

    ChangeURLInHeader(request_head,Url->hostport);

    head=HeaderString(request_head,&headlen);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to https (SSL) proxy)\n%s",head);

    if(write_data(server,head,headlen)<0)
       msg=GetPrintMessage(Warning,"Failed to write to remote https (SSL) proxy; [%!s].");

    free(head);
   }
 else
    out_err=write_string(client,"HTTP/1.0 200 WWWOFFLE SSL OK\r\n\r\n");

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Perform the transfer between client and proxy/server.

  int client The client socket.
  ++++++++++++++++++++++++++++++++++++++*/

void SSL_Transfer(int client)
{
 int nfd=client>server?client+1:server+1;
 fd_set readfd;
 struct timeval tv;
 int n;
 ssize_t nc,ns;
 char buffer[IO_BUFFER_SIZE];

 do
   {
    nc=ns=0;

    FD_ZERO(&readfd);

    FD_SET(server,&readfd);
    FD_SET(client,&readfd);

    tv.tv_sec=ConfigInteger(SocketTimeout);
    tv.tv_usec=0;

    n=select(nfd,&readfd,NULL,NULL,&tv);

    if(n<0 && errno==EINTR)
       continue;
    else if(n<=0)
       return;

    if(FD_ISSET(client,&readfd))
      {
       nc=read_data(client,buffer,IO_BUFFER_SIZE);
       if(nc>0)
          nc=write_data(server,buffer,nc);
      }
    if(FD_ISSET(server,&readfd))
      {
       ns=read_data(server,buffer,IO_BUFFER_SIZE);
       if(ns>0)
          ns=write_data(client,buffer,ns);
      }
   }
 while(nc>0 || ns>0);
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using SSL.

  int SSL_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int SSL_Close(void)
{
 unsigned long r,w;

 if(finish_tell_io(server,&r,&w)<0)
   PrintMessage(Inform,"Error finishing IO on socket with remote host [%!s].");

 PrintMessage(Inform,"Server bytes; %lu Read, %lu Written.",r,w); /* Used in audit-usage.pl */

 if(proxyUrl)
    FreeURL(proxyUrl);
 proxyUrl=NULL;

 return(CloseSocket(server));
}
