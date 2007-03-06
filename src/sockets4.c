/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/sockets4.c 2.27 2005/10/15 18:00:57 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  IPv4 Socket manipulation routines.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2004,2005,2006,2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
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

#include <netdb.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <setjmp.h>
#include <signal.h>
#include <pwd.h>

#include "errors.h"
#include "misc.h"
#include "io.h"
#include "configpriv.h"
#include "config.h"
#include "sockets.h"


/* struct for sending SOCKS v4 CONNECT/BIND requests. */
typedef struct {
  uint8_t version;
  uint8_t command;
  uint16_t port;
  uint32_t addr;
  char userid[0];
}
socks_hdr_t;

/* Local functions */

static void sigalarm(int signum);
static struct hostent /*@null@*/ *gethostbyname_or_timeout(char *name);
static struct hostent /*@null@*/ *gethostbyaddr_or_timeout(char *addr,size_t  len,int type);
static int connect_or_timeout(int sockfd,struct sockaddr *serv_addr,size_t addrlen);
static int IsLocalAddrPort(struct in_addr a, int port);

/* Local variables */

/*+ The timeout on DNS lookups. +*/
static int timeout_dns=0;

/*+ The timeout for socket connections. +*/
static int timeout_connect=0;

/*+ A longjump variable for aborting DNS lookups. +*/
static jmp_buf dns_jmp_env;


/*++++++++++++++++++++++++++++++++++++++
  Opens a socket for a client.

  int OpenClientSocket Returns the socket file descriptor.

  char* host The name of the remote host.

  int port The port number.

  For SOCKS v4 connections:

    char* shost The name of the destination host the SOCKS server should connect to
		(should be null if host is the destination).

    int sport   The destination port number.

    int socks_remote_dns  If nonzero, use SOCKS v4A to resolve destination host name remotely.

    char* shost_ipbuf  Buffer used to return the string representation of the IP address
		       of the destination host (i.e. shost if not null, otherwise host).
		       Only makes sense if socks_remote_dns==0.
		       Pointer to buffer may be null.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenClientSocket(char* host,int port, char *shost,int sport,int socks_remote_dns,char *shost_ipbuf)
{
 int s;
 struct sockaddr_in server;
 struct hostent* hp;

 server.sin_family=AF_INET;
 server.sin_port=htons(port);

 hp=gethostbyname_or_timeout(host);
 if(!hp)
   {
    uint32_t addr=inet_addr(host);
    if(addr!=-1)
       hp=gethostbyaddr_or_timeout((char*)&addr,sizeof(addr),AF_INET);

    if(!hp)
      {
       if(errno!=ETIMEDOUT)
          errno=ERRNO_USE_H_ERRNO;
       PrintMessage(Warning,"Unknown host '%s' for %s [%!s].",shost?"SOCKS proxy":"server",host);
       return(-1);
      }
   }

 if(hp->h_addrtype!=AF_INET || hp->h_length!=sizeof(server.sin_addr)) {
   PrintMessage(Warning,"Cannot connect to '%s': unexpected address type.",host);
   errno=EAFNOSUPPORT;
   return -1;
 }
 memcpy(&server.sin_addr,hp->h_addr,sizeof(server.sin_addr));
 if(shost_ipbuf && !shost)
   strcpy(shost_ipbuf,inet_ntoa(server.sin_addr));

 /* Try not to connect to our own HTTP port. */
 if(IsLocalAddrPort(server.sin_addr,port)) {
   PrintMessage(Warning,"Not connecting to '%s:%d': IP address %s matches that of localhost.",host,port,inet_ntoa(server.sin_addr));
   errno=EPERM;
   return -1;
 }

 s=socket(PF_INET,SOCK_STREAM,0);
 if(s==-1)
   {
    PrintMessage(Warning,"Failed to create client socket [%!s].");
    return(-1);
   }

 if(connect_or_timeout(s,(struct sockaddr*)&server,sizeof(server))==-1)
   {
    PrintMessage(Warning,"Failed to connect socket to '%s' port '%d' [%!s].",host,port);
    close(s);
    return -1;
   }

 if(shost) {
   socks_hdr_t *shdr;
   unsigned hdrsize,dstsize=0;
   int rv;
   struct in_addr a;

   if(!socks_remote_dns) {
     /* Resolve name of destination host locally.*/
     hp=gethostbyname_or_timeout(shost);
     if(!hp)
       {
	 uint32_t addr=inet_addr(shost);
	 if(addr!=-1)
	   hp=gethostbyaddr_or_timeout((char*)&addr,sizeof(addr),AF_INET);

	 if(!hp)
	   {
	     if(errno!=ETIMEDOUT)
	       errno=ERRNO_USE_H_ERRNO;
	     PrintMessage(Warning,"Unknown host '%s' for server [%!s].",shost);
	     close(s);
	     return(-1);
	   }
       }

     if(hp->h_addrtype!=AF_INET || hp->h_length!=sizeof(a)) {
       PrintMessage(Warning,"Cannot connect to '%s': unexpected address type.",shost);
       close(s);
       errno=EAFNOSUPPORT;
       return -1;
     }
     memcpy(&a,hp->h_addr,sizeof(a));
     if(shost_ipbuf)
       strcpy(shost_ipbuf,inet_ntoa(a));

     /* Some sanity checks */
     {
       const char *p=(char*)&a;
       if(p[0]==0 && p[1]==0 && p[2]==0) {
	 PrintMessage(Warning,"Not connecting to '%s:%d': host has invalid IP address %s ",shost,sport,inet_ntoa(a));
	 close(s);
	 errno=EPERM;
	 return -1;
       }
     }

     /* Try not to connect to our own HTTP port. */
     if(IsLocalAddrPort(a,sport)) {
       PrintMessage(Warning,"Not connecting to '%s:%d': IP address %s matches that of localhost.",shost,sport,inet_ntoa(a));
       close(s);
       errno=EPERM;
       return -1;
     }
   }
   else {
     /* Resolve name of destination host remotely using SOCKS v4A protocol.*/
     static const char dummy_addr[sizeof(a)] = {0,0,0,1};

     memcpy(&a,dummy_addr,sizeof(a));
     if(shost_ipbuf)
       shost_ipbuf[0]=0;

     dstsize=strlen(shost)+1;

     /* Some sanity checks */
     if(dstsize>256) {
       PrintMessage(Warning,"Not connecting to '%s:%d': host name too long.",shost,sport);
       close(s);
       errno=EMSGSIZE;
       return -1;
     }
#if 0
     if(IsLocalHostPort2(shost,sport)) {
       PrintMessage(Warning,"Not connecting to '%s:%d': name and port match that of localhost.",shost,sport);
       close(s);
       errno=EPERM;
       return -1;
     }
#endif
   }     

   {
     uid_t uid;
     struct passwd *pwd;
     unsigned nmsize;
     char *p;
     char uidbuf[MAX_INT_STR+1];

     uid=getuid();
     pwd=getpwuid(uid);
     if(pwd)
       nmsize= strlen(pwd->pw_name);
     else
       nmsize= sprintf(uidbuf,"%d",uid);
     ++nmsize; /* include the terminating null byte */
     hdrsize=sizeof(socks_hdr_t)+nmsize+dstsize;
     shdr=alloca(hdrsize);

     shdr->version=4; /* SOCKS protocol version number */
     shdr->command=1; /* CONNECT request */
     shdr->port=htons(sport);
     shdr->addr=a.s_addr;
     p=mempcpy(shdr->userid,pwd?pwd->pw_name:uidbuf,nmsize);
     if(socks_remote_dns)
       memcpy(p,shost,dstsize); /* Add name of destination for SOCKS v4A */
   }

   /* Should we use "write_all_or_timeout"?
      Immediately after connect_or_timeout succeeded, writing should not block and
      we are only sending a small data packet. */
   if(write_all(s,(char *)shdr,hdrsize)!=hdrsize) {
     PrintMessage(Warning,"Failed to write CONNECT command to SOCKS server '%s' port '%d' [%!s].",host,port);
     close(s);
     return -1;
   }

   if((rv=read_all_or_timeout(s,(char *)shdr,sizeof(socks_hdr_t),timeout_connect))!=sizeof(socks_hdr_t)) {
     if(rv>=0) {
       PrintMessage(Warning,"Failed to read SOCKS response from '%s' port '%d'; expected %d bytes, got %d bytes.",host,port,sizeof(socks_hdr_t),rv);
       errno=ENODATA;
     }
     else
       PrintMessage(Warning,"Failed to read SOCKS response from '%s' port '%d' [%!s].",host,port);
     close(s);
     return -1;
   }
   if(shdr->version) {
     PrintMessage(Warning,"Bad SOCKS version number (%d) in response from '%s' port '%d'.",shdr->version,host,port);
     close(s);
     errno=EPROTO;
     return -1;
   }
   if(shdr->command!=90) {
     PrintMessage(Warning,"Request to SOCKS server '%s' port '%d' failed: error code %d.",host,port,shdr->command);
     close(s);
     errno=ECONNREFUSED;
     return -1;
   }
 }

 return(s);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified IP address is the localhost.

  int IsLocalPort Return true if the address is the local host, and the port is our http port.

  struct in_addr a The address of the host to be checked.
  int port         The port number to be checked.

  ++++++++++++++++++++++++++++++++++++++*/

static int IsLocalAddrPort(struct in_addr a, int port)
{
 if(LocalHost && port==ConfigInteger(HTTP_Port)) {
   int i;
   for(i=0;i<LocalHost->nentries;++i) {
     struct in_addr b;
     if(inet_aton(LocalHost->key[i].string,&b) && b.s_addr==a.s_addr)
       return 1;
   }
 }

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Opens a socket for a server.

  int OpenServerSocket Returns a socket to be a server.

  char *host The local IP address to use.

  int port The port number to use.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenServerSocket(char *host,int port)
{
 int s;
 struct sockaddr_in server;
 int reuse_addr=1;
 struct hostent* hp;

 server.sin_family=AF_INET;
 server.sin_port=htons(port);

 if(!strcmp(host,"0.0.0.0"))
    server.sin_addr.s_addr=INADDR_ANY;
 else
   {
    hp=gethostbyname_or_timeout(host);
    if(!hp)
      {
       uint32_t addr=inet_addr(host);
       if(addr!=-1)
          hp=gethostbyaddr_or_timeout((char*)&addr,sizeof(addr),AF_INET);

       if(!hp)
         {
          if(errno!=ETIMEDOUT)
             errno=ERRNO_USE_H_ERRNO;
          PrintMessage(Warning,"Unknown host '%s' for server [%!s].",host);
          return(-1);
         }
      }

    if(hp->h_addrtype!=AF_INET || hp->h_length!=sizeof(server.sin_addr)) {
      PrintMessage(Warning,"Cannot create server socket for '%s': unexpected address type.",host);
      errno=EAFNOSUPPORT;
      return -1;
    }
    memcpy((char*)&server.sin_addr,(char*)hp->h_addr,sizeof(server.sin_addr));
   }

 s=socket(PF_INET,SOCK_STREAM,0);
 if(s==-1)
   {
    PrintMessage(Warning,"Failed to create server socket [%!s].");
    return(-1);
   }

 setsockopt(s,SOL_SOCKET,SO_REUSEADDR,&reuse_addr,sizeof(reuse_addr));

 if(bind(s,(struct sockaddr*)&server,sizeof(server))==-1)
   {
    PrintMessage(Warning,"Failed to bind server socket to '%s' port '%d' [%!s].",host,port);
    return(-1);
   }

 listen(s,4);

 return(s);
}


/*++++++++++++++++++++++++++++++++++++++
  Waits for a connection on the socket.

  int AcceptConnect returns a socket with a remote connection on the end.

  int socket Specifies the socket to check for.
  ++++++++++++++++++++++++++++++++++++++*/

int AcceptConnect(int socket)
{
 int s;

 s=accept(socket,(struct sockaddr*)0,(socklen_t*)0);

 if(s==-1)
    PrintMessage(Warning,"Failed to accept on server socket [%!s].");

 return(s);
}


/*++++++++++++++++++++++++++++++++++++++
  Determines the hostname and port number used for a socket on the other end.

  int SocketRemoteName Returns 0 on success and -1 on failure.

  int socket Specifies the socket to check.

  char **name Returns the hostname or NULL if failed to get hostname.

  char **ipname Returns the hostname as an IP address.

  int *port Returns the port number.
  ++++++++++++++++++++++++++++++++++++++*/

int SocketRemoteName(int socket,char **name,char **ipname,int *port)
{
 struct sockaddr_in server;
 socklen_t length=sizeof(server);
 int retval;
 static char host[MAXHOSTNAMELEN],ip[16];

 retval=getpeername(socket,(struct sockaddr*)&server,&length);
 if(retval==-1)
    PrintMessage(Warning,"Failed to get socket peer name [%!s].");
 else
   {
    strcpy(ip,inet_ntoa(server.sin_addr));

    if(name)
      {
#ifdef __CYGWIN__
      if(!strcmp(ip,"127.0.0.1"))
	*name="localhost";
      else
#endif
	{
	  struct hostent* hp=gethostbyaddr_or_timeout((char*)&server.sin_addr,sizeof(server.sin_addr),AF_INET);
	  if(hp)
	    {
	      strcpy(host,hp->h_name);
	      *name=host;
	    }
	  else
	    *name=NULL;
	}
      }

    if(ipname)
       *ipname=ip;

    if(port)
       *port=ntohs(server.sin_port);
   }

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Determines the hostname and port number used for a socket on this end.

  int SocketLocalName Returns 0 on success and -1 on failure.

  int socket Specifies the socket to check.

  char **name Returns the hostname.

  char **ipname Returns the hostname as an IP address.

  int *port Returns the port number.
  ++++++++++++++++++++++++++++++++++++++*/

int SocketLocalName(int socket,char **name,char **ipname,int *port)
{
 struct sockaddr_in server;
 socklen_t length=sizeof(server);
 int retval;
 static char host[MAXHOSTNAMELEN],ip[16];

 retval=getsockname(socket,(struct sockaddr*)&server,&length);
 if(retval==-1)
    PrintMessage(Warning,"Failed to get socket local name [%!s].");
 else
   {
    strcpy(ip,inet_ntoa(server.sin_addr));

    if(name)
      {
#ifdef __CYGWIN__
      if(!strcmp(ip,"127.0.0.1"))
	*name="localhost";
      else
#endif
	{
	  struct hostent* hp=gethostbyaddr_or_timeout((char*)&server.sin_addr,sizeof(server.sin_addr),AF_INET);
	  if(hp)
	    {
	      strcpy(host,hp->h_name);
	      *name=host;
	    }
	  else
	    *name=NULL;
	}
      }

    if(ipname)
       *ipname=ip;

    if(port)
       *port=ntohs(server.sin_port);
   }

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Closes a previously opened socket.

  int CloseSocket Returns 0 on success, -1 on error.

  int socket The socket to close
  ++++++++++++++++++++++++++++++++++++++*/

int CloseSocket(int socket)
{
 int retval=close(socket);

 if(retval==-1)
    PrintMessage(Warning,"Failed to close socket [%!s].");

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Shuts down a previously opened socket cleanly.

  int ShutdownSocket Returns 0 on success, -1 on error.

  int socket The socket to close
  ++++++++++++++++++++++++++++++++++++++*/

int ShutdownSocket(int socket)
{
 struct linger lingeropt;

 shutdown(socket, 1);

 /* Check socket and read until end or error. */

 while(1)
   {
    char buffer[1024];
    int n;
    fd_set readfd;
    struct timeval tv;

    FD_ZERO(&readfd);

    FD_SET(socket,&readfd);

    tv.tv_sec=tv.tv_usec=0;

    n=select(socket+1,&readfd,NULL,NULL,&tv);

    if(n>0)
      {
       n=read(socket,buffer,1024);

       if(n<=0)
          break;
      }
    else if(n==0 || errno!=EINTR)
       break;
   }

 /* Enable lingering */

 lingeropt.l_onoff=1;
 lingeropt.l_linger=15;

 if(setsockopt(socket,SOL_SOCKET,SO_LINGER,&lingeropt,sizeof(lingeropt))<0)
    PrintMessage(Important,"Failed to set socket SO_LINGER option [%!s].");

 return(CloseSocket(socket));
}


/*++++++++++++++++++++++++++++++++++++++
  Get the Fully Qualified Domain Name for the local host.

  char *GetFQDN Return the FQDN.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetFQDN(void)
{
 static char fqdn[256],**h;
 struct hostent* hp;
 int type;
 int length;
 char addr[sizeof(struct in_addr)];

 /* Try gethostname(). */

 if(gethostname(fqdn,255)==-1)
   {
    PrintMessage(Warning,"Failed to get hostname [%!s].");
    return(NULL);
   }

 if(strchr(fqdn,'.'))
    return(fqdn);

 /* Try gethostbyname(). */

 hp=gethostbyname_or_timeout(fqdn);
 if(!hp)
   {
    if(errno!=ETIMEDOUT)
       errno=ERRNO_USE_H_ERRNO;
    PrintMessage(Warning,"Failed to get name/IP address for host '%s' [%!s].",fqdn);
    return(NULL);
   }

 if(strchr(hp->h_name,'.'))
   {
    strcpy(fqdn,hp->h_name);
    return(fqdn);
   }

 for(h=hp->h_aliases;*h;h++)
    if(strchr(*h,'.'))
      {
       strcpy(fqdn,*h);
       return(fqdn);
      }

 /* Try gethostbyaddr(). */

 type=hp->h_addrtype;
 length=hp->h_length;
 if(length>sizeof(addr)) {
   PrintMessage(Warning,"Cannot get name/IP address for host '%s': unexpected address type.",fqdn);
   errno=EAFNOSUPPORT;
   return NULL;
 }
 memcpy(addr,(char*)hp->h_addr,length);

 hp=gethostbyaddr_or_timeout(addr,length,type);
 if(!hp)
   {
    if(errno!=ETIMEDOUT)
       errno=ERRNO_USE_H_ERRNO;
    PrintMessage(Warning,"Failed to get hostname for IP address [%!s].");
    return(NULL);
   }

 if(strchr(hp->h_name,'.'))
   {
    strcpy(fqdn,hp->h_name);
    return(fqdn);
   }

 for(h=hp->h_aliases;*h;h++)
    if(strchr(*h,'.'))
      {
       strcpy(fqdn,*h);
       return(fqdn);
      }

 strcpy(fqdn,hp->h_name);

 return(fqdn);
}


/*++++++++++++++++++++++++++++++++++++++
  Set the timeout for the DNS system calls.

  int timeout The timeout in seconds.
  ++++++++++++++++++++++++++++++++++++++*/

void SetDNSTimeout(int timeout)
{
 timeout_dns=timeout;
}


/*++++++++++++++++++++++++++++++++++++++
  Set the timeout for the connect system call.

  int timeout The timeout in seconds.
  ++++++++++++++++++++++++++++++++++++++*/

void SetConnectTimeout(int timeout)
{
 timeout_connect=timeout;
}


/*++++++++++++++++++++++++++++++++++++++
  The signal handler for the alarm signal to timeout the DNS lookup.

  int signum The signal number.
  ++++++++++++++++++++++++++++++++++++++*/

static void sigalarm(int signum)
{
 longjmp(dns_jmp_env,1);
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a name lookup with a timeout.

  struct hostent *gethostbyname_or_timeout Returns the host information.

  char *name The name of the host.
  ++++++++++++++++++++++++++++++++++++++*/

static struct hostent *gethostbyname_or_timeout(char *name)
{
 struct hostent *hp;
 struct sigaction action;

start:

 if(!timeout_dns)
   {
    hp=gethostbyname(name);
    return(hp);
   }

 /* DNS with timeout */

 action.sa_handler = sigalarm;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
   {
    PrintMessage(Warning, "Failed to set SIGALRM; cancelling timeout for DNS.");
    timeout_dns=0;
    goto start;
   }

 alarm(timeout_dns);

 if(setjmp(dns_jmp_env))
   {
    hp=NULL;
    errno=ETIMEDOUT;
   }
 else
    hp=gethostbyname(name);

 alarm(0);
 action.sa_handler = SIG_IGN;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
    PrintMessage(Warning, "Failed to clear SIGALRM.");

 return(hp);
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a name lookup with a timeout.

  struct hostent *gethostbyaddr_or_timeout Returns the host information.

  char *addr The address of the host.

  size_t len The length of the address.

  int type The type of the address.
  ++++++++++++++++++++++++++++++++++++++*/

static struct hostent *gethostbyaddr_or_timeout(char *addr,size_t len,int type)
{
 struct hostent *hp;
 struct sigaction action;

start:

 if(!timeout_dns)
   {
    hp=gethostbyaddr(addr,len,type);
    return(hp);
   }

 /* DNS with timeout */

 action.sa_handler = sigalarm;
 sigemptyset (&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
   {
    PrintMessage(Warning, "Failed to set SIGALRM; cancelling timeout for DNS.");
    timeout_dns=0;
    goto start;
   }

 alarm(timeout_dns);

 if(setjmp(dns_jmp_env))
   {
    hp=NULL;
    errno=ETIMEDOUT;
   }
 else
    hp=gethostbyaddr(addr,len,type);

 alarm(0);
 action.sa_handler = SIG_IGN;
 sigemptyset (&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
    PrintMessage(Warning, "Failed to clear SIGALRM.");

 return(hp);
}


/*++++++++++++++++++++++++++++++++++++++
  Call the connect function with a timeout.

  int connect_or_timeout Returns an error status.

  int sockfd The socket to be connected.

  struct sockaddr *serv_addr The address information.

  size_t addrlen The length of the address information.
  ++++++++++++++++++++++++++++++++++++++*/

static int connect_or_timeout(int sockfd,struct sockaddr *serv_addr,size_t addrlen)
{
 int noblock,flags;
 int retval;

 if(!timeout_connect)
    return(connect(sockfd,serv_addr,addrlen));

 /* Connect with timeout */

#ifdef O_NONBLOCK
 flags=fcntl(sockfd,F_GETFL,0);
 if(flags!=-1)
    noblock=fcntl(sockfd,F_SETFL,flags|O_NONBLOCK);
 else
    noblock=-1;
#else
 flags=1;
 noblock=ioctl(sockfd,FIONBIO,&flags);
#endif

 retval=connect(sockfd,serv_addr,addrlen);

 if(retval==-1 && noblock!=-1 && errno==EINPROGRESS)
   {
    fd_set writefd;
    struct timeval tv;

    FD_ZERO(&writefd);
    FD_SET(sockfd,&writefd);

    tv.tv_sec=timeout_connect;
    tv.tv_usec=0;

    retval=select(sockfd+1,NULL,&writefd,NULL,&tv);

    if(retval>0)
      {
       socklen_t arglen=sizeof(int);

       if(getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&retval,&arglen)<0)
          retval=errno;

       if(retval!=0)
          errno=retval,retval=-1;
       if(errno==EINPROGRESS)
          errno=ETIMEDOUT;
      }
    else if(retval==0)
       errno=ETIMEDOUT,retval=-1;
   }

 if(retval>=0)
   {
#ifdef O_NONBLOCK
    flags=fcntl(sockfd,F_GETFL,0);
    if(flags!=-1)
       fcntl(sockfd,F_SETFL,flags&~O_NONBLOCK);
#else
    flags=0;
    ioctl(sockfd,FIONBIO,&flags);
#endif
   }

 return(retval);
}
