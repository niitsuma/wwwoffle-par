/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/proto.c 1.11 2001/07/20 19:06:36 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7.
  Information about the protocols that wwwoffle supports.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,2001 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>

#include "misc.h"
#include "proto.h"


/*+ The list of protocols. +*/
Protocol Protocols[]={
 {
  Protocol_HTTP,                /* number */
  "http",                       /* name */
  80,                           /* defport */
  1,                            /* proxyable */
  1,                            /* postable */
  1,                            /* putable */
  HTTP_Open,                    /* open */
  HTTP_Request,                 /* request */
  HTTP_ReadHead,                /* readhead */
  HTTP_ReadBody,                /* readbody */
  HTTP_Close                    /* close */
 },
 {
  Protocol_FTP,                 /* number */
  "ftp",                        /* name */
  21,                           /* defport */
  1,                            /* proxyable */
  0,                            /* postable */
  1,                            /* putable */
  FTP_Open,                     /* open */
  FTP_Request,                  /* request */
  FTP_ReadHead,                 /* readhead */
  FTP_ReadBody,                 /* readbody */
  FTP_Close                     /* close */
 },
 {
  Protocol_Finger,              /* number */
  "finger",                     /* name */
  79,                           /* defport */
  0,                            /* proxyable */
  0,                            /* postable */
  0,                            /* putable */
  Finger_Open,                  /* open */
  Finger_Request,               /* request */
  Finger_ReadHead,              /* readhead */
  Finger_ReadBody,              /* readbody */
  Finger_Close                  /* close */
 }
};

/*+ The number of protocols. +*/
int NProtocols=sizeof(Protocols)/sizeof(Protocol);
