/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/iognutls.c 1.13 2006/07/21 17:46:52 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Functions for file input and output using gnutls.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 2005,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

#include <errno.h>

#include <setjmp.h>
#include <signal.h>

#if USE_GNUTLS
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#endif

#include "misc.h"
#include "io.h"
#include "iopriv.h"
#include "errors.h"


#if USE_GNUTLS

#include "certificates.h"


/*+ A longjump context for write timeouts. +*/
static jmp_buf write_jmp_env;


/* Local functions */

static void sigalarm(int signum);

static ssize_t gnutls_write_all(gnutls_session_t session,const char *data,size_t n);

static void set_gnutls_error(int err,gnutls_session_t session);


/*++++++++++++++++++++++++++++++++++++++
  Initialise the gnutls context information.

  io_gnutls *io_init_gnutls Returns a new gnutls io context.

  int fd The file descriptor for the session.

  const char *host The name of the server to serve as or NULL for a client.

  int type A flag set to 0 for client connection, 1 for built-in server or 2 for a fake server.
  ++++++++++++++++++++++++++++++++++++++*/

io_gnutls *io_init_gnutls(int fd,const char *host,int type)
{
 io_gnutls *context=(io_gnutls*)calloc(1,sizeof(io_gnutls));
 int err;

 /* Initialise the gnutls session. */

 err=gnutls_init(&context->session,type?GNUTLS_SERVER:GNUTLS_CLIENT);

 if(err!=0)
   {
    set_gnutls_error(err,context->session);

    PrintMessage(Warning,"GNUTLS Failed to initialise session [%s].",io_strerror);

    free(context);
    return(NULL);
   }

 err=gnutls_set_default_priority(context->session);

 if(err!=0)
   {
    set_gnutls_error(err,context->session);
    gnutls_deinit(context->session);

    PrintMessage(Warning,"GNUTLS Failed to set session priority [%s].",io_strerror);

    free(context);
    return(NULL);
   }

 /* Set the server credentials */

 if(type==2)
    context->cred=GetFakeCredentials(host);
 else if(type==1)
    context->cred=GetServerCredentials(host);
 else /* if(type==0) */
    context->cred=GetClientCredentials();

 if(!context->cred)
   {
    if(io_strerror)
       free(io_strerror);
    io_strerror=strdup("IO(gnutls): Failed to get credentials");

    gnutls_deinit(context->session);

    PrintMessage(Warning,"GNUTLS Failed to get server credentials [%s].",io_strerror);

    free(context);
    return(NULL);
   }

 err=gnutls_credentials_set(context->session,GNUTLS_CRD_CERTIFICATE,context->cred);

 if(err!=0)
   {
    set_gnutls_error(err,context->session);
    gnutls_deinit(context->session);

    PrintMessage(Warning,"GNUTLS Failed to set session credentials [%s].",io_strerror);

    free(context);
    return(NULL);
   }

 /* Set the file descriptor */

 context->fd=fd;

 gnutls_transport_set_ptr(context->session,(gnutls_transport_ptr_t)fd);

 /* Handshake the session on the socket */

 do
   {
    err=gnutls_handshake(context->session);
   }
 while(err<0 && !gnutls_error_is_fatal(err));

 if(err!=0)
   {
    set_gnutls_error(err,context->session);
    gnutls_bye(context->session,GNUTLS_SHUT_WR);
    gnutls_deinit(context->session);

    PrintMessage(Warning,"GNUTLS handshake has failed [%s].",io_strerror);

    free(context);
    return(NULL);
   }

 /* Save the server credentials */

 if(type==0)
    PutRealCertificate(context->session,host);

 return(context);
}


/*++++++++++++++++++++++++++++++++++++++
  Finalise the gnutls data stream.

  int io_finish_gnutls Returns 0 on completion, negative if error.

  io_gnutls *context The gnutls context information.
  ++++++++++++++++++++++++++++++++++++++*/

int io_finish_gnutls(io_gnutls *context)
{
 gnutls_bye(context->session,GNUTLS_SHUT_WR);

 gnutls_deinit(context->session);

 FreeCredentials(context->cred);

 free(context);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Read data from a gnutls session and buffer it with a timeout.

  ssize_t io_gnutls_read_with_timeout Returns the number of bytes read.

  io_gnutls *context The gnutls context information.

  io_buffer *out The IO buffer to output the data.

  unsigned timeout The maximum time to wait for data to be read (or 0 for no timeout).
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t io_gnutls_read_with_timeout(io_gnutls *context,io_buffer *out,unsigned timeout)
{
 ssize_t n;

 if(iobuf_isempty(out)) iobuf_reset(out);
 if(out->rear>=out->size)
   return 0;

 if(timeout)
   {
    fd_set readfd;
    struct timeval tv;

    tv.tv_sec=timeout;
    tv.tv_usec=0;

    do
      {
       FD_ZERO(&readfd);

       FD_SET(context->fd,&readfd);

       n=select(context->fd+1,&readfd,NULL,NULL,&tv);

       if(n==0) {
	 errno=ETIMEDOUT;
	 return(-1);
       }
       else if(n<0 && errno!=EINTR)
	 return(-1);
      }
    while(n<=0);
   }

 do
   {
    n=gnutls_record_recv(context->session,out->data+out->rear,out->size-out->rear);
   }
 while(n==GNUTLS_E_INTERRUPTED || n==GNUTLS_E_AGAIN);

 if(n==GNUTLS_E_REHANDSHAKE)
    gnutls_alert_send(context->session,GNUTLS_AL_WARNING,GNUTLS_A_NO_RENEGOTIATION);

 if(n==GNUTLS_E_UNEXPECTED_PACKET_LENGTH) /* Seems to happen for some servers but data is OK. */
    n=0;

 if(n>0)
    out->rear+=n;

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  The signal handler for the alarm signal to timeout the socket write.

  int signum The signal number.
  ++++++++++++++++++++++++++++++++++++++*/

static void sigalarm(/*@unused@*/ int signum)
{
 longjmp(write_jmp_env,1);
}


/*++++++++++++++++++++++++++++++++++++++
  Write some data to a gnutls session from a buffer with a timeout.

  ssize_t io_gnutls_write_with_timeout Returns the number of bytes written or negative on error.

  io_gnutls *context The gnutls context information.

  io_buffer *in The IO buffer with the input data.

  unsigned timeout The maximum time to wait for data to be written (or 0 for no timeout).
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t io_gnutls_write_with_timeout(io_gnutls *context,io_buffer *in,unsigned timeout)
{
 size_t numbytes,n,limit;
 ssize_t m;
 struct sigaction action;

 if(iobuf_isempty(in))
    return(0);

start:

 numbytes=iobuf_numbytes(in);
 n=0;

 if(!timeout)
   {
    m=gnutls_write_all(context->session,iobuf_datastart(in),numbytes);
    if(m<0) return m;
    in->front += m;
    n += m;
    return(n);
   }

 limit = (numbytes>4*IO_BUFFER_SIZE)?IO_BUFFER_SIZE:4*IO_BUFFER_SIZE;
 
 do {
   size_t nb=numbytes-n;

   if(nb>limit) nb=limit;

   action.sa_handler = sigalarm;
   sigemptyset(&action.sa_mask);
   action.sa_flags = 0;
   if(sigaction(SIGALRM, &action, NULL) != 0)
     {
       PrintMessage(Warning, "Failed to set SIGALRM; cancelling timeout for writing.");
       timeout=0;
       goto start;
     }

   if(setjmp(write_jmp_env)) {
     m=-1;
     errno=ETIMEDOUT;
   }
   else {
     alarm(timeout);
     m=gnutls_write_all(context->session,iobuf_datastart(in),nb);
   }

   alarm(0);
   action.sa_handler = SIG_IGN;
   sigemptyset(&action.sa_mask);
   action.sa_flags = 0;
   if(sigaction(SIGALRM, &action, NULL) != 0)
     PrintMessage(Warning, "Failed to clear SIGALRM.");

   if(m<0) return m;
   in->front += m;
   n += m;
 } while(n<numbytes);

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  A function to write all of a buffer of data to a gnutls session.

  ssize_t gnutls_write_all Returns the number of bytes written.

  gnutls_session_t session The gnutls session.

  const char *data The data buffer to write.

  size_t n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

static ssize_t gnutls_write_all(gnutls_session_t session,const char *data,size_t n)
{
 size_t nw=0;

 while(nw<n)
   {
    ssize_t m;

    do
      {
       m=gnutls_record_send(session,data+nw,n-nw);
      }
    while(m==GNUTLS_E_INTERRUPTED || m==GNUTLS_E_AGAIN);

    if(m<0)
      return m;

    nw+=m;
   }

 return(nw);
}


/*++++++++++++++++++++++++++++++++++++++
  Set the error status when there is a gnutls error.

  int err The error number.

  gnutls_session_t session The session information if one is active.
  ++++++++++++++++++++++++++++++++++++++*/

static void set_gnutls_error(int err,gnutls_session_t session)
{
 const char *type,*msg;

 if(err==GNUTLS_E_WARNING_ALERT_RECEIVED)
    type="Warning Alert: ";
 else if(err==GNUTLS_E_FATAL_ALERT_RECEIVED)
    type="Fatal Alert: ";
 else
    type="";

 if(session && *type)
    msg=gnutls_alert_get_name(gnutls_alert_get(session));
 else if(*type)
    msg="!Unknown Alert!";
 else
    msg=gnutls_strerror(err);

 io_errno=err;
 errno=ERRNO_USE_IO_ERRNO;

 if(io_strerror)
    free(io_strerror);

 io_strerror=x_asprintf("IO(gnutls): %s%s",type,msg);
}

#endif /* USE_GNUTLS */
