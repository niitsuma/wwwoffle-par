/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/proto.c 1.18 2005/12/14 19:27:22 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Information about the protocols that wwwoffle supports.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,2001,03,05 Andrew M. Bishop
  Parts of this file Copyright (C) 2003,2004,2006 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "proto.h"
#include "config.h"
#include "errors.h"
#include "misc.h"


/*+ The list of protocols. +*/
const Protocol Protocols[]={
 {
  "http",                       /* name */
  80,                           /* defport */
  1,                            /* proxyable */
  1,                            /* postable */
  1,                            /* putable */
#ifndef CLIENT_ONLY
  HTTP_Open,                    /* open */
  HTTP_Request,                 /* request */
  HTTP_ReadHead,                /* readhead */
  HTTP_ReadBody,                /* readbody */
  HTTP_Close                    /* close */
#else
  NULL,NULL,NULL,NULL,NULL
#endif
 },
#if USE_GNUTLS
 {
  "https",                      /* name */
  443,                          /* defport */
  1,                            /* proxyable */
  1,                            /* postable */
  1,                            /* putable */
#ifndef CLIENT_ONLY
  HTTPS_Open,                   /* open */
  HTTPS_Request,                /* request */
  HTTPS_ReadHead,               /* readhead */
  HTTPS_ReadBody,               /* readbody */
  HTTPS_Close                   /* close */
#else
  NULL,NULL,NULL,NULL,NULL
#endif
 },
#endif
 {
  "ftp",                        /* name */
  21,                           /* defport */
  1,                            /* proxyable */
  0,                            /* postable */
  1,                            /* putable */
#ifndef CLIENT_ONLY
  FTP_Open,                     /* open */
  FTP_Request,                  /* request */
  FTP_ReadHead,                 /* readhead */
  FTP_ReadBody,                 /* readbody */
  FTP_Close                     /* close */
#else
  NULL,NULL,NULL,NULL,NULL
#endif
 },
 {
  "finger",                     /* name */
  79,                           /* defport */
  0,                            /* proxyable */
  0,                            /* postable */
  0,                            /* putable */
#ifndef CLIENT_ONLY
  Finger_Open,                  /* open */
  Finger_Request,               /* request */
  Finger_ReadHead,              /* readhead */
  Finger_ReadBody,              /* readbody */
  Finger_Close                  /* close */
#else
  NULL,NULL,NULL,NULL,NULL
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

 PrintMessage(Fatal,"No protocol matches '%s'",Url->proto);

 /* Shouldn't get here */
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
