/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/ssl.c 1.17 2002/10/12 19:49:22 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7g.
  SSL (Secure Socket Layer) Tunneling functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02 Andrew M. Bishop
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

#include <fcntl.h>
#include <errno.h>

#include "wwwoffle.h"
#include "errors.h"
#include "misc.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


/*+ Set to the name of the proxy if there is one. +*/
static /*@null@*/ /*@observer@*/ char *proxy=NULL;

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
 char *hoststr,*portstr;
 char *server_host=NULL;
 int server_port=0;

 /* Sort out the host. */

 proxy=ConfigStringURL(SSLProxy,Url);
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

 server=-1;

 if(server_port)
   {
    server=OpenClientSocket(hoststr,server_port);
    init_buffer(server);

    if(server==-1)
       msg=PrintMessage(Warning,"Cannot open the SSL connection to %s port %d; [%!s].",hoststr,server_port);
   }
 else
    msg=PrintMessage(Warning,"No port given for the SSL connection to %s.",server_host);

 RejoinHostPort(server_host,hoststr,portstr);

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
    char *head;

    ModifyRequest(Url,request_head);

    MakeRequestProxyAuthorised(proxy,request_head);

    ChangeURLInHeader(request_head,Url->host);

    head=HeaderString(request_head);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to SSL proxy)\n%s",head);

    if(write_string(server,head)==-1)
       msg=PrintMessage(Warning,"Failed to write to remote SSL proxy; [%!s].");

    free(head);
   }
 else
    HTMLMessageHead(client,200,"WWWOFFLE SSL OK",
                    "Content-Type",NULL,
                    NULL);

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

 while(1)
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
          write_data(server,buffer,nc);
      }
    if(FD_ISSET(server,&readfd))
      {
       ns=read_data(server,buffer,READ_BUFFER_SIZE);
       if(ns>0)
          write_data(client,buffer,ns);
      }

    if(nc==0 && ns==0)
       return;
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using SSL.

  int SSL_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int SSL_Close(void)
{
 return(CloseSocket(server));
}
