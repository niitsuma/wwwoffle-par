/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/sockets.h 2.12 2004/01/17 16:22:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Socket function header file.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef SOCKETS_H
#define SOCKETS_H    /*+ To stop multiple inclusions. +*/

/* in sockets.c */

int OpenClientSocket(char* host,int port, char *shost,int sport,int socks_remote_dns,char *shost_ipbuf);

int OpenServerSocket(char* host,int port);
int AcceptConnect(int socket);

int SocketRemoteName(int socket,/*@out@*/ /*@null@*/ char **name,/*@out@*/ /*@null@*/ char **ipname,/*@out@*/ /*@null@*/ int *port);
#if 0
int SocketLocalName(int socket,/*@out@*/ /*@null@*/ char **name,/*@out@*/ /*@null@*/ char **ipname,/*@out@*/ /*@null@*/ int *port);
#endif
int /*@alt void@*/ CloseSocket(int socket);
int /*@alt void@*/ ShutdownSocket(int socket);

char /*@null@*/ *GetFQDN(void);

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

/* A macro to prepare hostnames/ports for a call to OpenClientSocket()
   in case a SOCKS proxy is used.
   sproxy should be a string containing the hostname:port
   of the SOCKS server.
*/
#define SETSOCKSHOSTPORT(sproxy,host,port,shost,sport)		\
{								\
  char *tmp_hoststr, *tmp_portstr; int tmp_hostlen;		\
								\
  (shost)=(host);						\
  (sport)=(port);						\
  SplitHostPort(sproxy,&tmp_hoststr,&tmp_hostlen,&tmp_portstr);	\
  (host)=strndupa(tmp_hoststr,tmp_hostlen);			\
  (port)=(tmp_portstr?atoi(tmp_portstr):DEFSOCKSPORT);		\
}

#endif /* SOCKETS_H */
