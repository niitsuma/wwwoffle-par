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
 char *hoststr,*portstr;
 char *server_host=NULL;
 int server_port=Protocols[Protocol_Finger].defport;

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
 init_buffer(server);

 if(server==-1)
    msg=PrintMessage(Warning,"Cannot open the Finger connection to %s port %d; [%!s].",hoststr,server_port);

 RejoinHostPort(server_host,hoststr,portstr);

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
 char *user,*slash;

 /* Take a simple route if it is proxied. */

 if(proxy)
   {
    char *head;

    MakeRequestProxyAuthorised(proxy,request_head);

    head=HeaderString(request_head);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to proxy)\n%s",head);

    if(write_string(server,head)==-1)
       msg=PrintMessage(Warning,"Failed to write to remote Finger proxy; [%!s].");

    free(head);

    return(msg);
   }

 /* Else Sort out the path. */

 user=(char*)malloc(strlen(Url->path));
 strcpy(user,Url->path+1);

 if((slash=strchr(user,'/')))
    *slash=0;

 if(*user)
   {
    if(write_formatted(server,"/W %s\r\n",user)==-1)
       msg=PrintMessage(Warning,"Failed to write to remote Finger server; [%!s].");
   }
 else
   {
    if(write_string(server,"/W\r\n")==-1)
       msg=PrintMessage(Warning,"Failed to write to remote Finger server; [%!s].");
   }

 free(user);

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
    ParseReply(server,reply_head);

    return(server);
   }

 /* Else send the header. */

 *reply_head=CreateHeader("HTTP/1.0 200 Finger OK",0);

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
