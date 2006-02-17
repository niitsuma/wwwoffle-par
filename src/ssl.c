/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/ssl.c 1.21 2004/02/24 19:26:03 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8c.
  SSL (Secure Socket Layer) Tunneling functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>

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
static /*@null@*/ /*@observer@*/ char *proxy=NULL;
static /*@null@*/ /*@observer@*/ char *sproxy=NULL;
static int socksremotedns=0;

/*+ The file descriptor of the server. +*/
static int server=-1;


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using SSL tunneling.

  char *SSL_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open (used for host only).
  ++++++++++++++++++++++++++++++++++++++*/

char *SSL_Open(URL *Url)
{
 char *msg=NULL;
 char *server_host=NULL;
 int server_port=0;
 char *socks_host=NULL;
 int socks_port=0;

 /* Sort out the host. */

 if(IsLocalNetHost(Url->host)) {
   proxy=NULL;
   sproxy=NULL;
   socksremotedns=0;
 }
 else {
   proxy=ConfigStringURL(SSLProxy,Url);
   sproxy=ConfigStringURL(SocksProxy,Url);
   socksremotedns=ConfigBooleanURL(SocksRemoteDNS,Url);
 }     

 if(proxy) {
   char *hoststr, *portstr; int hostlen;

   SplitHostPort(proxy,&hoststr,&hostlen,&portstr);
   server_host=strndupa(hoststr,hostlen);
   if(portstr)
     server_port=atoi(portstr);
 }
 else {
   server_host=Url->host;
   if(Url->port) server_port=Url->portnum;
 }


 /* Open the connection. */

 server=-1;

 if(server_port)
   {
    if(sproxy)
      SETSOCKSHOSTPORT(sproxy,server_host,server_port,socks_host,socks_port);

    server=OpenClientSocket(server_host,server_port,socks_host,socks_port,socksremotedns,NULL);

    if(server==-1)
       msg=GetPrintMessage(Warning,"Cannot open the SSL connection to %s port %d; [%!s].",server_host,server_port);
    else
      {
       init_io(server);
       configure_io_read(server,ConfigInteger(SocketTimeout),0,0);
       configure_io_write(server,ConfigInteger(SocketTimeout),0,0);
      }
   }
 else
    msg=GetPrintMessage(Warning,"No port given for the SSL connection to %s.",server_host);

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

 if(proxy)
   {
     char *head; int headlen;

    ModifyRequest(Url,request_head);

    MakeRequestProxyAuthorised(proxy,request_head);

    ChangeURLInHeader(request_head,Url->hostport);

    head=HeaderString(request_head,&headlen);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to SSL proxy)\n%s",head);

    if(write_data(server,head,headlen)==-1)
       msg=GetPrintMessage(Warning,"Failed to write to remote SSL proxy; [%!s].");

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
 int n,nc,ns;
 char buffer[READ_BUFFER_SIZE];

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
       nc=read_data(client,buffer,READ_BUFFER_SIZE);
       if(nc>0)
          nc=write_data(server,buffer,nc);
      }
    if(FD_ISSET(server,&readfd))
      {
       ns=read_data(server,buffer,READ_BUFFER_SIZE);
       if(ns>0)
          ns=write_data(client,buffer,ns);
      }
   }
 while(nc>0 || ns>0);
 return;
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using SSL.

  int SSL_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int SSL_Close(void)
{
 finish_io(server);
 return(CloseSocket(server));
}
