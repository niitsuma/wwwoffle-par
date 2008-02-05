/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/sockets.h 2.12 2004/01/17 16:22:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Socket function header file.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1996,97,98,99,2000,01,02,03,04 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2004,2005,2006,2008 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef SOCKETS_H
#define SOCKETS_H    /*+ To stop multiple inclusions. +*/

#include "misc.h"

/* in sockets.c */

int OpenClientSocketAddr(IPADDR *addr,int port, IPADDR *saddr,char *shost,int sport);
int OpenClientSocket(char *host,int port);
int OpenUrlSocket(URL *Url,socksdata_t *socksproxy);

int OpenServerSocket(char* host,int port);
int AcceptConnect(int socket);

int SocketRemoteName(int socket,/*@out@*/ /*@null@*/ char **name,/*@out@*/ /*@null@*/ char **ipname,/*@out@*/ /*@null@*/ int *port);
int SocketLocalName(int socket,/*@out@*/ /*@null@*/ char **name,/*@out@*/ /*@null@*/ char **ipname,/*@out@*/ /*@null@*/ int *port);

int /*@alt void@*/ CloseSocket(int socket);
int /*@alt void@*/ ShutdownSocket(int socket);

char /*@null@*/ *GetFQDN(void);

void SetDNSTimeout(int timeout);
void SetConnectTimeout(int timeout);
int resolve_name(char *name, IPADDR *a);
socksdata_t *MakeSocksData(char *hostport, unsigned short remotedns, socksdata_t *socksbuf);

/* Get the binary IP address of a URL host. */
inline static IPADDR *get_url_ipaddr(URL *Url)
{
  if(!(Url->addrvalid)) {
    if(!resolve_name(Url->host,&Url->addr))
      return NULL;
    Url->addrvalid=1;
  }
  return &Url->addr;
}

/* Macros to compare two IP addresses. The arguments a and b should have type IPADDR*. */
#if USE_IPV6
#define IPADDR_EQUIV(a,b) \
	((((uint32_t *) (a))[0] == ((uint32_t *) (b))[0]) && \
	 (((uint32_t *) (a))[1] == ((uint32_t *) (b))[1]) && \
	 (((uint32_t *) (a))[2] == ((uint32_t *) (b))[2]) && \
	 (((uint32_t *) (a))[3] == ((uint32_t *) (b))[3]))
#else /* use IPV4 */
#define IPADDR_EQUIV(a,b) ((a)->s_addr==(b)->s_addr)
#endif

#endif /* SOCKETS_H */
