/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/sockets.h 2.10 2002/03/24 16:22:13 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.6d.
  Socket function header file.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef SOCKETS_H
#define SOCKETS_H    /*+ To stop multiple inclusions. +*/

/* in sockets.c */

int OpenClientSocket(char* host, int port);

int OpenServerSocket(char* host,int port);
int AcceptConnect(int socket);

int SocketRemoteName(int socket,/*@out@*/ /*@null@*/ char **name,/*@out@*/ /*@null@*/ char **ipname,/*@out@*/ int *port);
int SocketLocalName(int socket,/*@out@*/ /*@null@*/ char **name,/*@out@*/ /*@null@*/ char **ipname,/*@out@*/ int *port);

int /*@alt void@*/ CloseSocket(int socket);
#ifdef __CYGWIN__
int /*@alt void@*/ CloseCygwinSocket(int socket);
#endif

char *GetFQDN(void);

void SetDNSTimeout(int timeout);
void SetConnectTimeout(int timeout);

/* following Added by Paul Rombouts */
#include <sys/param.h>

#if USE_IPV6
#define max_hostname_len NI_MAXHOST
#define ipaddr_strlen    INET6_ADDRSTRLEN

#else /* use IPV4 */
#define max_hostname_len MAXHOSTNAMELEN
#define ipaddr_strlen    16
#endif

#endif /* SOCKETS_H */
