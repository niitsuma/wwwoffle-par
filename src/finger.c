/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/finger.c 1.16 2002/06/23 15:05:22 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7c.
  Functions for getting URLs using Finger.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "wwwoffle.h"
#include "errors.h"
#include "misc.h"
#include "headbody.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


/*+ Set to the name of the proxy if there is one. +*/
static /*@observer@*/ /*@null@*/ char *proxy=NULL;

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
 char *server_host=NULL;
 int server_port=Protocols[Protocol_Finger].defport;

 /* Sort out the host. */

 if(IsLocalNetHost(Url->host))
   proxy=NULL;
 else
   proxy=ConfigStringURL(Proxies,Url);

 if(proxy) {
   char *hoststr, *portstr; int hostlen;

   SplitHostPort(proxy,&hoststr,&hostlen,&portstr);
   server_host=strndupa(hoststr,hostlen);
   if(portstr)
     server_port=atoi(portstr);
 }
 else {
   server_host=Url->host;
   server_port=Url->portnum;
 }


 /* Open the connection. */

 server=OpenClientSocket(server_host,server_port);

 if(server!=-1)
   init_buffer(server);
 else
   msg=PrintMessage(Warning,"Cannot open the Finger connection to %s port %d; [%!s].",server_host,server_port);

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

 if(proxy)
   {
    char *head; int head_len;

    MakeRequestProxyAuthorised(proxy,request_head);

    head=HeaderString(request_head,&head_len);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to proxy)\n%s",head);

    if(write_all(server,head,head_len)<0)
       msg=PrintMessage(Warning,"Failed to write to remote Finger proxy; [%!s].");

    free(head);

    return(msg);
   }

 /* Else Sort out the path. */

 user=Url->path+1;
 {char *slash=strchr(user,'/'); if(slash) user=STRDUPA2(user,slash);}

 if(*user)
   {
    if(write_formatted(server,"/W %s\r\n",user)<0)
       msg=PrintMessage(Warning,"Failed to write to remote Finger server; [%!s].");
   }
 else
   {
    if(write_string(server,"/W\r\n")<0)
       msg=PrintMessage(Warning,"Failed to write to remote Finger server; [%!s].");
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

 if(proxy)
   {
    ParseReply_or_timeout(server,reply_head,NULL);

    return(server);
   }

 /* Else send the header. */

 CreateHeader("HTTP/1.0 200 Finger OK",0,reply_head);

 AddToHeader(*reply_head,"Content-Type","text/plain");

 return(server);
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  int Finger_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  int n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

int Finger_ReadBody(char *s,int n)
{
 return(read_data_or_timeout(server,s,n));
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using Finger.

  int Finger_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int Finger_Close(void)
{
 return(CloseSocket(server));
}
