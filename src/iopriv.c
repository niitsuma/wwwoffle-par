/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  Private functions for file input and output.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2004,2007,2008 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
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

  size_t size The size to allocate.
  ++++++++++++++++++++++++++++++++++++++*/

io_buffer *create_io_buffer(size_t size)
{
 io_buffer *new=(io_buffer*)calloc(1,sizeof(io_buffer));

 new->data=(char*)malloc(size);
 new->size=size;

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Resize an io_buffer structure.

  io_buffer *buffer The io_buffer to resize.

  size_t size The new size to allocate.
  ++++++++++++++++++++++++++++++++++++++*/

io_buffer *resize_io_buffer(io_buffer *buffer,size_t size)
{
 if(!buffer)
   buffer=(io_buffer*)calloc(1,sizeof(io_buffer));
 else if(buffer->size>=size)
   return buffer;

 buffer->data=(char*)realloc((void*)buffer->data,size);
 buffer->size=size;

 return buffer;
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

  ssize_t io_read_with_timeout Returns the number of bytes read.

  int fd The file descriptor to read from.

  io_buffer *out The IO buffer to output the data.

  unsigned timeout The maximum time to wait for data to be read (or 0 for no timeout).
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t io_read_with_timeout(int fd,io_buffer *out,unsigned timeout)
{
 ssize_t n;

 if(iobuf_isempty(out)) iobuf_reset(out);
 if(out->rear>=out->size)
   return 0;

 if(timeout)
   {
    int n;
    fd_set readfd;
    struct timeval tv;

    tv.tv_sec=timeout;
    tv.tv_usec=0;

    do
      {
       FD_ZERO(&readfd);

       FD_SET(fd,&readfd);

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

  ssize_t io_write_with_timeout Returns the number of bytes written or negative on error.

  int fd The file descriptor to write to.

  io_buffer *in The IO buffer with the input data.

  unsigned timeout The maximum time to wait for data to be written (or 0 for no timeout).
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t io_write_with_timeout(int fd,io_buffer *in,unsigned timeout)
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
    m=write_all(fd,iobuf_datastart(in),numbytes);
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
     m=write_all(fd,iobuf_datastart(in),nb);
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
