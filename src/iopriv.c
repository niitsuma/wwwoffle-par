/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/iopriv.c 1.4 2004/01/17 16:29:37 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Private functions for file input and output.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>

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

#include <errno.h>

#include <setjmp.h>
#include <signal.h>

#include "io.h"
#include "iopriv.h"
#include "errors.h"


/*+ A longjump context for write timeouts. +*/
static jmp_buf write_jmp_env;


/* Local functions */

static void sigalarm(int signum);



/*++++++++++++++++++++++++++++++++++++++
  Create an io_buffer structure.

  io_buffer *create_io_buffer The io_buffer to create.

  int size The size to allocate.
  ++++++++++++++++++++++++++++++++++++++*/

io_buffer *create_io_buffer(int size)
{
 io_buffer *new=(io_buffer*)calloc(1,sizeof(io_buffer));

 new->data=(char*)malloc(size);
 new->size=size;

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Destroy an io_buffer structure.

  io_buffer *buffer The io_buffer to destroy.
  ++++++++++++++++++++++++++++++++++++++*/

void destroy_io_buffer(io_buffer *buffer)
{
 free(buffer->data);

 free(buffer);
}


/*++++++++++++++++++++++++++++++++++++++
  Read some data from a file descriptor and buffer it with a timeout.

  int io_read_with_timeout Returns the number of bytes read.

  int fd The file descriptor to read from.

  io_buffer *out The IO buffer to output the data.

  int timeout The maximum time to wait for data (or 0 for no timeout).
  ++++++++++++++++++++++++++++++++++++++*/

int io_read_with_timeout(int fd,io_buffer *out,int timeout)
{
 int n;

 if(iobuf_isempty(out)) iobuf_reset(out);
 if(out->rear>=out->size)
   return 0;

 if(timeout)
   {
    fd_set readfd;
    struct timeval tv;

    do
      {
       FD_ZERO(&readfd);

       FD_SET(fd,&readfd);

       tv.tv_sec=timeout;
       tv.tv_usec=0;

       n=select(fd+1,&readfd,NULL,NULL,&tv);

       if(n==0) {
	 errno=ETIMEDOUT;
	 return(-1);
       }
       else if(n<0 && errno!=EINTR)
	 return(-1);
      }
    while(n<=0);
   }

 n=read(fd,out->data+out->rear,out->size-out->rear);

 if(n>0)
    out->rear += n;

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
  Write some data to a file descriptor from a buffer with a timeout.

  int io_write_with_timeout Returns the number of bytes written or negative on error.

  int fd The file descriptor to write to.

  io_buffer *in The IO buffer with the input data.

  int timeout The maximum time to wait for data (or 0 for no timeout).
  ++++++++++++++++++++++++++++++++++++++*/

int io_write_with_timeout(int fd,io_buffer *in,int timeout)
{
 int n;
 struct sigaction action;

 if(iobuf_isempty(in))
    return(0);

start:

 if(!timeout)
   {
    n=write_all(fd,iobuf_datastart(in),iobuf_numbytes(in));
    if(n>0) in->front += n;
    return(n);
   }

 action.sa_handler = sigalarm;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
   {
    PrintMessage(Warning, "Failed to set SIGALRM; cancelling timeout for writing.");
    timeout=0;
    goto start;
   }

 alarm(timeout);

 if(setjmp(write_jmp_env))
   {
    n=-1;
    errno=ETIMEDOUT;
   }
 else
    n=write_all(fd,iobuf_datastart(in),iobuf_numbytes(in));

 alarm(0);
 action.sa_handler = SIG_IGN;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGALRM, &action, NULL) != 0)
    PrintMessage(Warning, "Failed to clear SIGALRM.");

 if(n>0) in->front += n;
 return(n);
}
