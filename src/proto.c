/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/proto.c 1.18 2005/12/14 19:27:22 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Information about the protocols that wwwoffle supports.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,2001,03,05 Andrew M. Bishop
  Parts of this file Copyright (C) 2003,2004,2006,2008 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "proto.h"
#include "errors.h"
#include "misc.h"
#include "headbody.h"
#include "sockets.h"


/*+ The list of protocols. +*/
const Protocol Protocols[]={
 {
  "http",                       /* name */
  80,                           /* defport */
  1,                            /* proxyable */
  1,                            /* postable */
  1,                            /* putable */
  1,                            /* allowkeepalive */
#ifndef CLIENT_ONLY
  HTTP_Open,                    /* open */
  HTTP_Request,                 /* request */
  HTTP_ReadHead,                /* readhead */
  HTTP_ReadBody,                /* readbody */
  HTTP_FinishBody,              /* finishbody */
  HTTP_Close                    /* close */
#else
  NULL,NULL,NULL,NULL,NULL,NULL
#endif
 },
#if USE_GNUTLS
 {
  "https",                      /* name */
  443,                          /* defport */
  1,                            /* proxyable */
  1,                            /* postable */
  1,                            /* putable */
  1,                            /* allowkeepalive */
#ifndef CLIENT_ONLY
  HTTPS_Open,                   /* open */
  HTTPS_Request,                /* request */
  HTTPS_ReadHead,               /* readhead */
  HTTPS_ReadBody,               /* readbody */
  HTTPS_FinishBody,             /* finishbody */
  HTTPS_Close                   /* close */
#else
  NULL,NULL,NULL,NULL,NULL,NULL
#endif
 },
#endif
 {
  "ftp",                        /* name */
  21,                           /* defport */
  1,                            /* proxyable */
  0,                            /* postable */
  1,                            /* putable */
  1,                            /* allowkeepalive */
#ifndef CLIENT_ONLY
  FTP_Open,                     /* open */
  FTP_Request,                  /* request */
  FTP_ReadHead,                 /* readhead */
  FTP_ReadBody,                 /* readbody */
  FTP_FinishBody,               /* finishbody */
  FTP_Close                     /* close */
#else
  NULL,NULL,NULL,NULL,NULL,NULL
#endif
 },
 {
  "finger",                     /* name */
  79,                           /* defport */
  0,                            /* proxyable */
  0,                            /* postable */
  0,                            /* putable */
  0,                            /* allowkeepalive */
#ifndef CLIENT_ONLY
  Finger_Open,                  /* open */
  Finger_Request,               /* request */
  Finger_ReadHead,              /* readhead */
  Finger_ReadBody,              /* readbody */
  Finger_FinishBody,            /* finishbody */
  Finger_Close                  /* close */
#else
  NULL,NULL,NULL,NULL,NULL,NULL
#endif
 }
};

/*+ The number of protocols. +*/
int NProtocols=sizeof(Protocols)/sizeof(Protocol);


/*++++++++++++++++++++++++++++++++++++++
  Determing the functions to handle a protocol.

  const Protocol *GetProtocol Returns a pointer to the Protocol structure for this protocol.

  const URL *Url The URL to check.
  ++++++++++++++++++++++++++++++++++++++*/

const Protocol *GetProtocol(const URL *Url)
{
 int i;

 for(i=0;i<NProtocols;i++)
    if(!strcmp(Protocols[i].name,Url->proto))
      return(&Protocols[i]);

 return NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if a specified URL protocol is handled by WWWOFFLE or not.

  int IsProtocolHandled Returns true if it is.

  const URL *Url The URL to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsProtocolHandled(const URL *Url)
{
 int i;

 for(i=0;i<NProtocols;i++)
    if(!strcmp(Protocols[i].name,Url->proto))
      return 1;

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Get the default port that would be used for this protocol.

  int DefaultPort Returns the port number.

  const URL *Url The URL to check.
  ++++++++++++++++++++++++++++++++++++++*/

int DefaultPort(const URL *Url)
{
 int i;

 for(i=0;i<NProtocols;i++)
    if(!strcmp(Protocols[i].name,Url->proto))
      return Protocols[i].defport;

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if a specified URL protocol can accept a POST request.

  int IsPOSTHandled Returns true if it is.

  const URL *Url The URL to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsPOSTHandled(const URL *Url)
{
 int i;

 for(i=0;i<NProtocols;i++)
    if(!strcmp(Protocols[i].name,Url->proto))
      return Protocols[i].postable;

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if a specified URL protocol can accept a PUT request.

  int IsPUTHandled Returns true if it is.

  const URL *Url The URL to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsPUTHandled(const URL *Url)
{
 int i;

 for(i=0;i<NProtocols;i++)
    if(!strcmp(Protocols[i].name,Url->proto))
      return Protocols[i].putable;

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if a specified URL protocol can be proxied.

  int IsProxyHandled Returns true if it is.

  const URL *Url The URL to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsProxyHandled(const URL *Url)
{
 int i;

 for(i=0;i<NProtocols;i++)
    if(!strcmp(Protocols[i].name,Url->proto))
      return Protocols[i].proxyable;

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Return a proxyable link to the URL (even for protocols that cannot be proxied).

  char *ProxyableLink Returns the link.

  URL *Url The URL to create a link for.
  ++++++++++++++++++++++++++++++++++++++*/

char *ProxyableLink(URL *Url)
{
 if(!Url->private_link)
   {
    if(!IsProxyHandled(Url))
      {
       char *localurl=GetLocalURL();
       Url->private_link=x_asprintf("%s/%s/%s",localurl,Url->proto,Url->hostp);
       free(localurl);
      }
    else
       Url->private_link=Url->name;
   }

 return(Url->private_link);
}


#ifndef CLIENT_ONLY

Connection *ConnectionOpen(URL *Url, char **errmsg)
{
  Connection *connection=NULL;
  const Protocol *protocol;
  char *err;

  protocol=GetProtocol(Url);
  if(protocol) {
    connection=malloc(sizeof(Connection));
    connection->protocol=protocol;
    connection->Url=NULL;
    connection->ctrlfd=-1;
    connection->fd=-1;
    connection->proxyUrl=NULL;
    connection->socksproxy=NULL;
    connection->bufferhead=NULL;
    connection->buffer=NULL;
    connection->buffertail=NULL;
    connection->nbuffer=0;
    connection->nread=0;
    connection->nbuffertail=0;
    connection->nreadtail=0;
    connection->hint=NULL;
    connection->rbytes=0;
    connection->wbytes=0;
    connection->loggedin=0;
    connection->expectmore=0;

    err= (protocol->open)(connection, Url);

    if(!err)
      connection->Url=CopyURL(Url);
    else {
      free(connection);
      connection=NULL;
    }
  }
  else
    err=GetPrintMessage(Warning,"ConnectionOpen: unsupported protocol '%s'",Url->proto);

  if(errmsg)
    *errmsg=err;
  else if(err)
    free(err);

  return connection;
}

#if 0
char *ConnectionRequest(Connection *connection,URL *Url,Header *request_head,Body *request_body)
{
  return (connection->protocol->request)(connection,request_head,request_body);
}

Header *ConnectionReadHead(Connection *connection)
{
  return (connection->protocol->readhead)(connection);
}

ssize_t ConnectionReadBody(Connection *connection,char *s,size_t n)
{
  return (connection->protocol->readbody)(connection,s,n);
}

int ConnectionFinishBody(Connection *connection)
{
  return (connection->protocol->finishbody)(connection);
}
#endif

int ConnectionClose(Connection *connection)
{
  int retval;

  if(connection->Url)
    PrintMessage(Debug,"Closing connection to %s",
		 connection->proxyUrl? connection->proxyUrl->hostport:
				       connection->Url->hostport);

  retval=(connection->protocol->close)(connection);
  if(connection->Url) FreeURL(connection->Url);
  if(connection->proxyUrl) FreeURL(connection->proxyUrl);
  if(connection->socksproxy) free(connection->socksproxy);
  if(connection->bufferhead) FreeHeader(connection->bufferhead);
  if(connection->buffer) free(connection->buffer);
  if(connection->buffertail) free(connection->buffertail);
  if(connection->hint) free(connection->hint);
  free(connection);
  return retval;
}


/* Check whether an existing connection can be re-used for Url2 */
int ConnectionIsReusable(Connection *connection, URL *Url2)
{
  URL *Url1=connection->Url;
  IPADDR *a1,*a2;
  char *proxy1,*proxy2,*sproxy1,*sproxy2;

  if(strcmp(Url1->proto,Url2->proto))
    return 0;

  if(IsLocalNetHost(Url1->host))
    return (Url1->portnum==Url2->portnum &&
	    (!strcmp(Url1->host,Url2->host) ||
	     (IsLocalNetHost(Url2->host) &&
	      (a1=get_url_ipaddr(Url1)) && (a2=get_url_ipaddr(Url2)) && IPADDR_EQUIV(a1,a2))));
  else if(IsLocalNetHost(Url2->host))
    return 0;

  sproxy1=ConfigStringURL(SocksProxy,Url1);
  sproxy2=ConfigStringURL(SocksProxy,Url2);

  if(!(sproxy1? sproxy2 && !strcmp(sproxy1,sproxy2) : !sproxy2))
    return 0;
  
  proxy1=ConfigStringURL(Proxies,Url1);
  proxy2=ConfigStringURL(Proxies,Url2);

  if(proxy1)
    return (proxy2 && !strcmp(proxy1,proxy2));
  else if(proxy2)
    return 0;    

  if(!strcmp(Url1->proto,"ftp")) { /* Check that the ftp passwords are the same. */
    char *user1,*pass1,*user2,*pass2;

    if(Url1->user)
      {
	user1=Url1->user;
	pass1=Url1->pass;
      }
    else
      {
	user1=ConfigStringURL(FTPAuthUser,Url1);
	if(!user1)
	  user1=ConfigString(FTPUserName);

	pass1=ConfigStringURL(FTPAuthPass,Url1);
	if(!pass1)
	  pass1=ConfigString(FTPPassWord);
      }

    if(Url2->user)
      {
	user2=Url2->user;
	pass2=Url2->pass;
      }
    else
      {
	user2=ConfigStringURL(FTPAuthUser,Url2);
	if(!user2)
	  user2=ConfigString(FTPUserName);

	pass2=ConfigStringURL(FTPAuthPass,Url2);
	if(!pass2)
	  pass2=ConfigString(FTPPassWord);
      }

    if(strcmp(user1,user2) || strcmp(pass1,pass2))
      return 0;
  }

  return (Url1->portnum==Url2->portnum &&
	  (!strcmp(Url1->host,Url2->host) ||
	   (!(sproxy1 && ConfigBooleanURL(SocksRemoteDNS,Url1)) &&
	    !(sproxy2 && ConfigBooleanURL(SocksRemoteDNS,Url2)) &&
	    (a1=get_url_ipaddr(Url1)) && (a2=get_url_ipaddr(Url2)) && IPADDR_EQUIV(a1,a2))));
}
#endif
