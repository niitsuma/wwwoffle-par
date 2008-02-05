/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/finger.c 1.30 2007/04/01 10:38:28 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  Functions for getting URLs using Finger.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1998,99,2000,01,02,03,04,05,06,07 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007,2008 Paul A. Rombouts
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


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using Finger.

  char *Finger_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *Finger_Open(Connection *connection,URL *Url)
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
    msg=GetPrintMessage(Warning,"Cannot open the Finger connection to %s port %d; [%!s].",Url->host,Url->portnum);
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

  char *Finger_Request Returns NULL on success, a useful message on error.

  URL *Url The URL to get.

  Header *request_head The head of the Finger request for the URL.

  Body *request_body The body of the Finger request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *Finger_Request(Connection *connection,URL *Url,Header *request_head,/*@unused@*/ Body *request_body)
{
 char *msg=NULL;
 char *user;
 URL *proxyUrl=connection->proxyUrl;
 int server=connection->fd;

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

  Header *Finger_ReadHead Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

Header *Finger_ReadHead(Connection *connection)
{
 Header *reply_head=NULL;

 /* Take a simple route if it is proxied. */

 if(connection->proxyUrl)
    ParseReply(connection->fd,&reply_head);
 else /* Else send the header. */
   { 
     CreateHeader("HTTP/1.0 200 Finger OK",0,&reply_head);

     AddToHeader(reply_head,"Content-Type","text/plain");
   }

 return reply_head;
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  ssize_t Finger_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  size_t n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t Finger_ReadBody(Connection *connection,char *s,size_t n)
{
 return(read_data(connection->fd,s,n));
}


int Finger_FinishBody(Connection *connection)
{
 return(finish_io_content(connection->fd));
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using Finger.

  int Finger_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int Finger_Close(Connection *connection)
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
