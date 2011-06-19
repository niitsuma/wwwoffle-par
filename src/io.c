/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/io.c 2.60 2007/03/25 11:05:46 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Functions for file input and output.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2004,2006,2007,2008 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

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

#include "io.h"
#include "iopriv.h"
#include "errors.h"

#ifdef CLIENT_ONLY
#undef USE_GNUTLS
#define USE_GNUTLS 0
#endif

/*+ The buffer size for write accumulation (use same size as MIN_CHUNK_SIZE). +*/
#define WRITE_BUFFER_SIZE (IO_BUFFER_SIZE/4)


/*+ The number of IO contexts allocated. +*/
static int nio=0;

/*+ The allocated IO contexts. +*/
static /*@only@*/ io_context **io_contexts;


/*+ The IO functions error number. +*/
int io_errno=0;

/*+ The IO functions error message string. +*/
char /*@null@*/ *io_strerror=NULL;


static ssize_t io_write_data(int fd,io_context *context,io_buffer *iobuffer);


/*++++++++++++++++++++++++++++++++++++++
  Initialise the IO context used for this file descriptor.

  int fd The file descriptor to initialise.
  ++++++++++++++++++++++++++++++++++++++*/

void init_io(int fd)
{
 if(fd<0)
    PrintMessage(Fatal,"IO: Function init_io(%d) was called with an invalid argument.",fd);

 /* Allocate some space for new IO contexts */

 if(fd>=nio)
   {
    int new_nio=fd+9;
    io_contexts=(io_context**)realloc((void*)io_contexts,new_nio*sizeof(io_context*));

    for(;nio<new_nio;nio++)
       io_contexts[nio]=NULL;
   }

 /* Allocate the new context */

 if(io_contexts[fd])
    PrintMessage(Fatal,"IO: Function init_io(%d) was called twice without calling finish_io(%d).",fd,fd);

 io_contexts[fd]=(io_context*)calloc(1,sizeof(io_context));
 io_contexts[fd]->content_remaining=CUNDEF;
 /* io_contexts[fd]->content_overflow=0; */
}


/*++++++++++++++++++++++++++++++++++++++
  Re-initialise a file descriptor (e.g. after seeking on a file).

  int fd The file descriptor to re-initialise.
  ++++++++++++++++++++++++++++++++++++++*/

void reinit_io(int fd)
{
 io_context *context;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function reinit_io(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function reinit_io(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

#if USE_ZLIB
 if(context->r_zlib_context || context->w_zlib_context)
    PrintMessage(Fatal,"IO: Function reinit_io(%d) was called while zlib compression enabled.",fd);
#endif

 if(context->r_chunk_context || context->w_chunk_context)
    PrintMessage(Fatal,"IO: Function reinit_io(%d) was called while chunked encoding enabled.",fd);

#if USE_GNUTLS
 if(context->gnutls_context)
    PrintMessage(Fatal,"IO: Function reinit_io(%d) was called while gnutls encryption enabled.",fd);
#endif

 if(context->r_file_data)
   {
    destroy_io_buffer(context->r_file_data);
    context->r_file_data=NULL;
   }

 context->r_raw_bytes=0;
 context->w_raw_bytes=0;
 context->content_remaining=CUNDEF;
 context->content_overflow=0;
 context->read_eof=0;
 context->saved_errno=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Configure the timeout in the IO context used for this file descriptor.

  int fd The file descriptor.

  int timeout_r The read timeout or 0 for none or -1 to leave unchanged.

  int timeout_w The write timeout or 0 for none or -1 to leave unchanged.
  ++++++++++++++++++++++++++++++++++++++*/

void configure_io_timeout(int fd,int timeout_r,int timeout_w)
{
 io_context *context;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function configure_io_timeout(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_timeout(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Set the timeouts */

 if(timeout_r>=0)
    context->r_timeout=timeout_r;

 if(timeout_w>=0)
    context->w_timeout=timeout_w;
}


/*++++++++++++++++++++++++++++++++++++++
  Configure the expected content_length in the IO context used for this file descriptor.

  int fd The file descriptor.

  unsigned long content_length The expected content length of the data to read.
  ++++++++++++++++++++++++++++++++++++++*/

void configure_io_content_length(int fd,unsigned long content_length)
{
 io_context *context;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function configure_io_content_length(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_content_length(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 if(content_length!=CUNDEF && context->r_file_data && !iobuf_isempty(context->r_file_data)) {
   size_t nb=iobuf_numbytes(context->r_file_data);

   if(nb>content_length) {
     /* If the data in the read data buffer exceeds content length,
	leave the excess in the read data buffer, but remember the size. */
     context->content_overflow = nb-content_length;
     nb=content_length;
   }

   /* Subtract size of the data already in read data buffer from remaining
      content size. */
   content_length -= nb;
 }

 /* Set the remaining content size */

 context->content_remaining=content_length;
}

/* io_content_remaining returns the remaining raw-content length for a file descriptor. */
unsigned long io_content_remaining(int fd)
{
 io_context *context;
 unsigned long content_rem;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function io_content_remaining(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function io_content_remaining(%d) was called without calling init_io(%d) first.",fd,fd);

 context= io_contexts[fd];
 content_rem= context->content_remaining;

 /* If the read buffer is non-empty, add the remaining bytes in the buffer. */
 if(content_rem!=CUNDEF && context->r_file_data && !iobuf_isempty(context->r_file_data))
   content_rem += iobuf_numbytes(context->r_file_data) - context->content_overflow;

 return content_rem;
}


#if USE_ZLIB

/*++++++++++++++++++++++++++++++++++++++
  Configure the compression option for the IO context used for this file descriptor.

  int fd The file descriptor.

  int zlib_r The flag to indicate the new zlib compression method for reading or -1 to leave unchanged.

  int zlib_w The flag to indicate the new zlib compression method for writing or -1 to leave unchanged.
  ++++++++++++++++++++++++++++++++++++++*/

int configure_io_zlib(int fd,int zlib_r,int zlib_w)
{
 io_context *context;
 int retval=0;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function configure_io_zlib(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_zlib(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Create the zlib decompression context for reading */

 if(zlib_r>=0) {
   if(zlib_r) {
     if(!context->r_zlib_context)
       {
	 context->r_zlib_context=io_init_zlib_uncompress(zlib_r);
	 if(!context->r_zlib_context)
	   PrintMessage(Fatal,"IO: Could not initialise zlib uncompression; [%!s].");

	 if(context->r_chunk_context && !context->r_zlch_data)
	   context->r_zlch_data=create_io_buffer(IO_BUFFER_SIZE);

	 context->r_file_data=resize_io_buffer(context->r_file_data,IO_BUFFER_SIZE);
       }
   }
   else { /* !zlib_r */
     if(context->r_zlib_context)
       {
	 /* Check for a previous error */

	 if(context->saved_errno) {
	   errno=context->saved_errno;
	   retval=-1;
	   goto cleanup_r_zlib_context;
	 }

	 {
	   int err=io_finish_zlib_uncompress(context->r_zlib_context,NULL);
	   if(err<0) retval=err;
	 }

       cleanup_r_zlib_context:
	 destroy_io_zlib(context->r_zlib_context);
	 context->r_zlib_context=NULL;

	 if(context->r_zlch_data) {
	   destroy_io_buffer(context->r_zlch_data);
	   context->r_zlch_data=NULL;
	 }
       }
   }
 }

 /* Create the zlib compression context for writing */

 if(zlib_w>=0) {
   if(zlib_w) {
     if(!context->w_zlib_context)
       {
	 context->w_zlib_context=io_init_zlib_compress(zlib_w);
	 if(!context->w_zlib_context)
	   PrintMessage(Fatal,"IO: Could not initialise zlib compression; [%!s].");

	 if(context->w_chunk_context && !context->w_zlch_data)
	   context->w_zlch_data=create_io_buffer(IO_BUFFER_SIZE);

	 if(!context->w_file_data)
	   context->w_file_data=create_io_buffer(IO_BUFFER_SIZE);
       }
   }
   else { /* !zlib_w */
     if(context->w_zlib_context)
       {
	 /* Check for a previous error */

	 if(context->saved_errno) {
	   errno=context->saved_errno;
	   retval=-1;
	   goto cleanup_w_zlib_context;
	 }

	 /* Write out any remaining data */
	 {
	   io_buffer *buffer_data=context->w_buffer_data;
	   if(buffer_data && !iobuf_isempty(buffer_data)) {
	     ssize_t err=io_write_data(fd,context,buffer_data);
	     if(err<0) {retval=err; goto cleanup_w_zlib_context;}
	   }
	 }

	 if(!context->w_chunk_context)
	   {
	     int more; ssize_t err;
	     do
	       {
		 more=io_finish_zlib_compress(context->w_zlib_context,context->w_file_data);
		 if(more<0) {retval=more; goto cleanup_w_zlib_context;}
#if USE_GNUTLS
		 if(context->gnutls_context)
		   err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
		 else
#endif
		   err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
		 if(err<0) {retval=err; goto cleanup_w_zlib_context;}
		 context->w_raw_bytes+=err;
	       }
	     while(more>0);
	   }
	 else /* context->w_chunk_context */
	   {
	     int more,more2; ssize_t err;
	     do
	       {
		 more=io_finish_zlib_compress(context->w_zlib_context,context->w_zlch_data);
		 if(more<0) {retval=more; goto cleanup_w_zlib_context;}
		 more2=io_chunk_encode(context->w_zlch_data,context->w_chunk_context,context->w_file_data);
		 if(more2<0) {retval=more2; goto cleanup_w_zlib_context;}
#if USE_GNUTLS
		 if(context->gnutls_context)
		   err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
		 else
#endif
		   err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
		 if(err<0) {retval=err; goto cleanup_w_zlib_context;}
		 context->w_raw_bytes+=err;
	       }
	     while(more>0);
	   }

       cleanup_w_zlib_context:
	 destroy_io_zlib(context->w_zlib_context);
	 context->w_zlib_context=NULL;

	 if(context->w_zlch_data) {
	   destroy_io_buffer(context->w_zlch_data);
	   context->w_zlch_data=NULL;
	 }
       }
   }
 }

 if(retval<0)
   context->saved_errno= errno?errno:EIO;

 return retval;
}

#endif /* USE_ZLIB */


/*++++++++++++++++++++++++++++++++++++++
  Configure the chunked encoding/decoding for the IO context used for this file descriptor.

  int fd The file descriptor.

  int chunked_r The flag to indicate the new chunked encoding method for reading or -1 to leave unchanged.

  int chunked_w The flag to indicate the new chunked encoding method for writing or -1 to leave unchanged.
  ++++++++++++++++++++++++++++++++++++++*/

int configure_io_chunked(int fd,int chunked_r,int chunked_w)
{
 io_context *context;
 int retval=0;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function configure_io_chunked(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_chunked(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Create the chunked decoding context for reading */

 if(chunked_r>=0) {
   if(chunked_r) {
     if(!context->r_chunk_context)
       {
	 context->r_chunk_context=io_init_chunk_decode();
	 if(!context->r_chunk_context)
	   PrintMessage(Fatal,"IO: Could not initialise chunked decoding; [%!s].");

#if USE_ZLIB
	 if(context->r_zlib_context && !context->r_zlch_data)
	   context->r_zlch_data=create_io_buffer(IO_BUFFER_SIZE);
#endif

	 context->r_file_data=resize_io_buffer(context->r_file_data,IO_BUFFER_SIZE);
       }
   }
   else { /* !chunked_r */
     if(context->r_chunk_context)
       {
	 /* Check for a previous error */

	 if(context->saved_errno) {
	   errno=context->saved_errno;
	   retval=-1;
	   goto cleanup_r_chunk_context;
	 }

	 {
	   int err=io_finish_chunk_decode(context->r_chunk_context,NULL);
	   if(err<0) retval=err;
	 }

       cleanup_r_chunk_context:
	 destroy_io_chunk(context->r_chunk_context);
	 context->r_chunk_context=NULL;

#if USE_ZLIB
	 if(context->r_zlch_data) {
	   destroy_io_buffer(context->r_zlch_data);
	   context->r_zlch_data=NULL;
	 }
#endif
       }
   }
 }

 /* Create the chunked encoding context for writing */

 if(chunked_w>=0) {
   if(chunked_w) {
     if(!context->w_chunk_context)
       {
	 context->w_chunk_context=io_init_chunk_encode();
	 if(!context->w_chunk_context)
	   PrintMessage(Fatal,"IO: Could not initialise chunked encoding; [%!s].");

#if USE_ZLIB
	 if(context->w_zlib_context && !context->w_zlch_data)
	   context->w_zlch_data=create_io_buffer(IO_BUFFER_SIZE);
#endif

	 context->w_file_data=resize_io_buffer(context->w_file_data,IO_BUFFER_SIZE+MAXCHUNKPADDING);
       }
   }
   else { /* !chunked_w */
     if(context->w_chunk_context)
       {
	 int more; ssize_t err;

	 /* Check for a previous error */

	 if(context->saved_errno) {
	   errno=context->saved_errno;
	   retval=-1;
	   goto cleanup_w_chunk_context;
	 }

	 /* Write out any remaining data */
	 {
	   io_buffer *buffer_data=context->w_buffer_data;
	   if(buffer_data && !iobuf_isempty(buffer_data)) {
	     err=io_write_data(fd,context,buffer_data);
	     if(err<0) {retval=err; goto cleanup_w_chunk_context;}
	   }
	 }

	 do
	   {
	     more=io_finish_chunk_encode(context->w_chunk_context,context->w_file_data);
	     if(more<0) {retval=more; goto cleanup_w_chunk_context;}
#if USE_GNUTLS
	     if(context->gnutls_context)
	       err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
	     else
#endif
	       err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
	     if(err<0) {retval=err; goto cleanup_w_chunk_context;}
	     context->w_raw_bytes+=err;
	   }
	 while(more>0);

       cleanup_w_chunk_context:
	 destroy_io_chunk(context->w_chunk_context);
	 context->w_chunk_context=NULL;

#if USE_ZLIB
	 if(context->w_zlch_data) {
	   destroy_io_buffer(context->w_zlch_data);
	   context->w_zlch_data=NULL;
	 }
#endif
       }
   }
 }

 if(retval<0)
   context->saved_errno= errno?errno:EIO;

 return retval;
}


#if USE_GNUTLS

/*++++++++++++++++++++++++++++++++++++++
  Configure the encryption in the IO context used for this file descriptor.

  int configure_io_gnutls returns 0 if OK or an error code.

  int fd The file descriptor to configure.

  const char *host The name of the server to serve as or NULL for a client.

  int type A flag set to 0 for client connection, 1 for built-in server or 2 for a fake server.
  ++++++++++++++++++++++++++++++++++++++*/

int configure_io_gnutls(int fd,const char *host,int type)
{
 io_context *context;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function configure_io_gnutls(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_gnutls(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 if(context->gnutls_context) {
    PrintMessage(Warning,"IO: Function configure_io_gnutls(%d) was called with GNUTLS context already initialised.",fd);
    return -1;
 }

 /* Initialise the GNUTLS context information. */

 context->gnutls_context=io_init_gnutls(fd,host,type);

 return(!context->gnutls_context);
}

#endif


/*++++++++++++++++++++++++++++++++++++++
  Read data from a file descriptor instead of read().

  ssize_t read_data Returns the number of bytes read or 0 for end of file.

  int fd The file descriptor.

  char *buffer The buffer to put the data into.

  size_t n The maximum number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t read_data(int fd,char *buffer,size_t n)
{
 io_context *context;
 ssize_t err=0,nr;
 io_buffer iobuffer;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function read_data(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function read_data(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Check for a previous error */

 if(context->saved_errno) {
   errno=context->saved_errno;
   return -1;
 }

 if(n==0) return 0;

 /* Create the output buffer */

 iobuffer.data=buffer;
 iobuffer.size=n;
 iobuffer.front=0;
 iobuffer.rear=0;

 /* Read in new data */

#if USE_ZLIB
 if(!context->r_zlib_context)
   {
#endif
    if(!context->r_chunk_context)
      {
	if(context->r_file_data && !iobuf_isempty(context->r_file_data)) {
	  size_t nb= iobuf_numbytes(context->r_file_data) - context->content_overflow;

	  if(nb>n)
	    nb=n;

	   memcpy(buffer,iobuf_datastart(context->r_file_data),nb);
	   context->r_file_data->front+=nb;
	   nr=nb;
	}
	else if(context->content_remaining && !context->read_eof) {
#if USE_GNUTLS
	  if(context->gnutls_context)
	    err=io_gnutls_read_with_timeout(context->gnutls_context,&iobuffer,context->r_timeout);
	  else
#endif
	    err=io_read_with_timeout(fd,&iobuffer,context->r_timeout);

	  if(err<0) goto return_err;
	  if(err==0) context->read_eof=1;
	  context->r_raw_bytes+=err;
	  if(context->content_remaining!=CUNDEF) {
	    if(err>context->content_remaining) {
	      /* store excess data in r_file_data */
	      size_t rem= err-context->content_remaining;
	      context->r_file_data=resize_io_buffer(context->r_file_data,rem);
	      memcpy(context->r_file_data->data,buffer+context->content_remaining,rem);
	      context->r_file_data->front=0;
	      context->r_file_data->rear=rem;
	      context->content_overflow=rem;
	      err=context->content_remaining;
	    }
	    context->content_remaining-=err;
	  }
	  nr=err;
	}
	else
	  nr=0;
      }
    else /* if(context->r_chunk_context) */
      {
       do
         {
	  io_buffer *rdata=context->r_file_data;
	  if(context->content_remaining && iobuf_isempty(rdata) && !context->read_eof) {
#if USE_GNUTLS
	    if(context->gnutls_context)
	      err=io_gnutls_read_with_timeout(context->gnutls_context,rdata,context->r_timeout);
	    else
#endif
	      err=io_read_with_timeout(fd,rdata,context->r_timeout);
	    if(err<0) goto return_err;
	    if(err==0) context->read_eof=1;
	    context->r_raw_bytes+=err;
	    if(context->content_remaining!=CUNDEF) {
	      /* With chunked encoding a defined content-length header should
		 never happen in practice. */
	      if(err>context->content_remaining) {
		/* leave excess data in context->r_file_data, but remember its size. */
		context->content_overflow = err-context->content_remaining;
		err=context->content_remaining;
	      }
	      context->content_remaining-=err;
	    }
	  }
	  rdata->rear -= context->content_overflow;
	  err=io_chunk_decode(rdata,context->r_chunk_context,&iobuffer);
	  rdata->rear += context->content_overflow;
	  if(err<0) goto return_err;
	  if(err) {
	    if(context->content_remaining==CUNDEF) {
	      context->content_remaining=0;
	      context->content_overflow = iobuf_numbytes(rdata);
	    }
	    break;
	  }
         }
       while(iobuffer.rear==0);
       nr=iobuffer.rear;
      }
#if USE_ZLIB
   }
 else /* if(context->r_zlib_context) */
   {
    if(!context->r_chunk_context)
      {
       do
         {
	  io_buffer *rdata=context->r_file_data;
	  if(context->content_remaining && iobuf_hasroom(rdata) && !context->read_eof) {
#if USE_GNUTLS
	    if(context->gnutls_context)
	      err=io_gnutls_read_with_timeout(context->gnutls_context,rdata,context->r_timeout);
	    else
#endif
	      err=io_read_with_timeout(fd,rdata,context->r_timeout);
	    if(err<0) goto return_err;
	    if(err==0) context->read_eof=1;
	    context->r_raw_bytes+=err;
	    if(context->content_remaining!=CUNDEF) {
	      if(err>context->content_remaining) {
		/* leave excess data in context->r_file_data, but remember its size. */
		context->content_overflow = err-context->content_remaining;
		err=context->content_remaining;
	      }
	      context->content_remaining-=err;
	    }
	  }
	  rdata->rear -= context->content_overflow;
	  err=io_zlib_uncompress(rdata,context->r_zlib_context,&iobuffer);
	  rdata->rear += context->content_overflow;
	  if(err<0) goto return_err;
         }
       while(!err && iobuffer.rear==0);
       nr=iobuffer.rear;
      }
    else /* if(context->r_chunk_context) */
      {
       do
         {
	  do
	    {
	     io_buffer *rdata=context->r_file_data;
	     if(context->content_remaining && iobuf_isempty(rdata) && !context->read_eof) {
#if USE_GNUTLS
	       if(context->gnutls_context)
		 err=io_gnutls_read_with_timeout(context->gnutls_context,rdata,context->r_timeout);
	       else
#endif
		 err=io_read_with_timeout(fd,rdata,context->r_timeout);
	       if(err<0) goto return_err;
	       if(err==0) context->read_eof=1;
	       context->r_raw_bytes+=err;
	       if(context->content_remaining!=CUNDEF) {
		 /* With chunked encoding a defined content-length header should
		    never happen in practice. */
		 if(err>context->content_remaining) {
		   /* leave excess data in context->r_file_data, but remember its size. */
		   context->content_overflow = err-context->content_remaining;
		   err=context->content_remaining;
		 }
		 context->content_remaining-=err;
	       }
	     }
	     rdata->rear -= context->content_overflow;
	     err=io_chunk_decode(rdata,context->r_chunk_context,context->r_zlch_data);
	     rdata->rear += context->content_overflow;
	     if(err<0) goto return_err;
	     if(err) {
	       if(context->content_remaining==CUNDEF) {
		 context->content_remaining=0;
		 context->content_overflow = iobuf_numbytes(rdata);
	       }
	       break;
	     }
	    }
	  while(iobuf_isempty(context->r_zlch_data));

	  err=io_zlib_uncompress(context->r_zlch_data,context->r_zlib_context,&iobuffer);

          if(err<0) goto return_err;
         }
       while(!err && iobuffer.rear==0);
       nr=iobuffer.rear;
      }
   }
#endif /* USE_ZLIB */

 return(nr);

return_err:
 context->saved_errno= errno?errno:EIO;
 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a single line of data from a file descriptor like fgets does from a FILE*.

  char *read_line Returns the modified string or NULL for the end of file.

  int fd The file descriptor.

  char *line The previously allocated line of data.
  ++++++++++++++++++++++++++++++++++++++*/

char *read_line(int fd,char *line)
{
 io_context *context;
 io_buffer *line_data;
 size_t n,alloclen;
 int found;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function read_line(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function read_line(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Check for a previous error */

 if(context->saved_errno) {
   errno=context->saved_errno;
   goto free_line_return_null;
 }

 /* Create the temporary line buffer if there is not one */

 if(!context->r_file_data)
    context->r_file_data=create_io_buffer(LINE_BUFFER_SIZE);

 /* Use the existing data or read in some more */

 line_data=context->r_file_data;
 n=0; alloclen=0; found=0;
 do
   {
    ssize_t nb; size_t i,limit;

    if(n>=alloclen) {
      alloclen=n+LINE_BUFFER_SIZE;
      line=(char*)realloc((void*)line,alloclen+1);
    }

    if(iobuf_isempty(line_data)) {
      /* FIXME
	 Cannot call read_line() on compressed files.
	 Probably not important, wasn't possible with old io.c either.
	 FIXME */

      if(context->read_eof)
	goto free_line_return_null;

#if USE_GNUTLS
      if(context->gnutls_context)
	nb=io_gnutls_read_with_timeout(context->gnutls_context,line_data,context->r_timeout);
      else
#endif
      nb=io_read_with_timeout(fd,line_data,context->r_timeout);

      if(nb<=0) {
	if(nb<0)
	  context->saved_errno= errno?errno:EIO;
	else
	  context->read_eof=1;

	goto free_line_return_null;
      }

      context->r_raw_bytes+=nb;
    }

    limit=line_data->front+(alloclen-n);
    if(limit>line_data->rear)
      limit=line_data->rear;

    for(i=line_data->front; i<limit; ++i)
      if(line_data->data[i]=='\n')
	{
	  found=1;
	  ++i;
	  break;
	}

    nb=i-line_data->front;
    memcpy(&line[n],iobuf_datastart(line_data),nb);
    line_data->front += nb;
    n += nb;
   }
 while(!found);

 line[n]=0;

 return(line);

free_line_return_null:
 if(line) free(line);
 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Write data to a file descriptor instead of write().

  ssize_t write_data Returns the number of bytes written.

  int fd The file descriptor.

  const char *data The data buffer to write.

  size_t n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t write_data(int fd,const char *data,size_t n)
{
 io_context *context;
 io_buffer iobuffer, *buffer_data;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function write_data(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function write_data(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Check for a previous error */

 if(context->saved_errno) {
   errno=context->saved_errno;
   return -1;
 }

 buffer_data=context->w_buffer_data;
 if(context->w_buffer_data && !iobuf_isempty(buffer_data))
   {
    ssize_t err=io_write_data(fd,context,buffer_data);
    if(err<0) return(err);
   }

 /* Create the temporary input buffer */

 iobuffer.data=(char*)data;
 iobuffer.size=n;
 iobuffer.front=0;
 iobuffer.rear=n;

 return(io_write_data(fd,context,&iobuffer));
}


/*++++++++++++++++++++++++++++++++++++++
  Write data to a file descriptor instead of write() but with buffering.

  ssize_t write_buffer_data Returns the number of bytes written.

  int fd The file descriptor.

  const char *data The data buffer to write.

  size_t n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t write_buffer_data(int fd,const char *data,size_t n)
{
 io_context *context;
 io_buffer *buffer_data;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function write_buffer_data(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function write_buffer_data(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Check for a previous error */

 if(context->saved_errno) {
   errno=context->saved_errno;
   return -1;
 }

 /* Create the temporary write buffer if there is not one */

 if(!context->w_buffer_data)
    context->w_buffer_data=create_io_buffer(2*WRITE_BUFFER_SIZE);

 buffer_data=context->w_buffer_data;
 if(iobuf_isempty(buffer_data)) iobuf_reset(buffer_data);
 if((n+buffer_data->rear)<= buffer_data->size && (!iobuf_isempty(buffer_data) || n<WRITE_BUFFER_SIZE))
   {
    memcpy(buffer_data->data+buffer_data->rear,data,n);
    buffer_data->rear+=n;

    if(iobuf_numbytes(buffer_data)>=WRITE_BUFFER_SIZE) {
       ssize_t err=io_write_data(fd,context,buffer_data);
       if(err<0) return(err);
    }
   }
 else
   {
    io_buffer iobuffer;
    size_t m=0;
    ssize_t err;

    if(!iobuf_isempty(buffer_data))
      {
       if(iobuf_numbytes(buffer_data)<WRITE_BUFFER_SIZE) {
	 m= buffer_data->front+WRITE_BUFFER_SIZE;
	 if(m>buffer_data->size) m=buffer_data->size;
	 m -= buffer_data->rear;
	 if(m>n) m=n;
	 memcpy(buffer_data->data+buffer_data->rear,data,m);
	 buffer_data->rear+=m;
       }

       err=io_write_data(fd,context,buffer_data);
       if(err<0) return(err);
      }

    /* Create the temporary input buffer */

    iobuffer.data=(char*)data;
    iobuffer.size=n;
    iobuffer.front=m;
    iobuffer.rear=n;

    err=io_write_data(fd,context,&iobuffer);
    if(err<0) return(err);
   }

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  An internal function to actually write the data bypassing the write buffer.

  ssize_t io_write_data Returns the number of bytes written.

  int fd The file descriptor to write to.

  io_context *context The IO context to write to.

  io_buffer *iobuffer The buffer of data to write.
  ++++++++++++++++++++++++++++++++++++++*/

static ssize_t io_write_data(int fd,io_context *context,io_buffer *iobuffer)
{
 ssize_t err=0,nw;

 if(iobuf_isempty(iobuffer))
    return(0);

 nw=iobuf_numbytes(iobuffer);

 /* Write the output data */

#if USE_ZLIB
 if(!context->w_zlib_context)
   {
#endif
    if(!context->w_chunk_context)
      {
#if USE_GNUTLS
       if(context->gnutls_context)
          err=io_gnutls_write_with_timeout(context->gnutls_context,iobuffer,context->w_timeout);
       else
#endif
          err=io_write_with_timeout(fd,iobuffer,context->w_timeout);
       if(err<0) goto return_err;
       context->w_raw_bytes+=err;
      }
    else /* if(context->w_chunk_context) */
      {
       do
         {
          err=io_chunk_encode(iobuffer,context->w_chunk_context,context->w_file_data);
          if(err<0) goto return_err;
          if(!iobuf_isempty(context->w_file_data))
            {
#if USE_GNUTLS
             if(context->gnutls_context)
                err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
             else
#endif
                err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
             if(err<0) goto return_err;
             context->w_raw_bytes+=err;
            }
         }
       while(!iobuf_isempty(iobuffer));
      }
#if USE_ZLIB
   }
 else /* if(context->w_zlib_context) */
   {
    if(!context->w_chunk_context)
      {
       do
         {
          err=io_zlib_compress(iobuffer,context->w_zlib_context,context->w_file_data);
          if(err<0) goto return_err;
          if(!iobuf_isempty(context->w_file_data))
            {
#if USE_GNUTLS
             if(context->gnutls_context)
                err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
             else
#endif
                err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
             if(err<0) goto return_err;
             context->w_raw_bytes+=err;
            }
         }
       while(!iobuf_isempty(iobuffer));
      }
    else /* if(context->w_chunk_context) */
      {
       do
         {
          err=io_zlib_compress(iobuffer,context->w_zlib_context,context->w_zlch_data);
          if(err<0) goto return_err;
          if(!iobuf_isempty(context->w_zlch_data))
            {
             err=io_chunk_encode(context->w_zlch_data,context->w_chunk_context,context->w_file_data);
             if(err<0) goto return_err;
             if(!iobuf_isempty(context->w_file_data))
               {
#if USE_GNUTLS
                if(context->gnutls_context)
                   err=io_gnutls_write_with_timeout(context->gnutls_context,context->w_file_data,context->w_timeout);
                else
#endif
                   err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
                if(err<0) goto return_err;
                context->w_raw_bytes+=err;
               }
            }
         }
       while(!iobuf_isempty(iobuffer));
      }
   }
#endif /* USE_ZLIB */

 return(nw);

return_err:
 context->saved_errno= errno?errno:EIO;
 return(err);
 
}


/*++++++++++++++++++++++++++++++++++++++
  Write a simple string to a file descriptor like fputs does to a FILE*.

  ssize_t write_string Returns the number of bytes written.

  int fd The file descriptor.

  const char *str The string.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t write_string(int fd,const char *str)
{
 return(write_data(fd,str,strlen(str)));
}


/*++++++++++++++++++++++++++++++++++++++
  Write a formatted string to a file descriptor like fprintf does to a FILE*.

  ssize_t write_formatted Returns the number of bytes written.

  int fd The file descriptor.

  const char *fmt The format string.

  ... The other arguments.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t write_formatted(int fd,const char *fmt,...)
{
 ssize_t n;
 char *str;
 va_list ap;

#ifdef __STDC__
 va_start(ap,fmt);
#else
 va_start(ap);
#endif

 n=vasprintf(&str,fmt,ap);

 va_end(ap);

 /* Write the string. */
 if(n>=0) {
   n=write_data(fd,str,n);
   free(str);
 }

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the number of raw bytes read and written to the file descriptor.

  int tell_io Normally returns 0 on success, negative if there was a previous IO error.

  int fd The file descriptor.

  unsigned long* r Returns the number of bytes read.

  unsigned long *w Returns the raw number of bytes written.
  ++++++++++++++++++++++++++++++++++++++*/

int tell_io(int fd,unsigned long* r,unsigned long *w)
{
 io_context *context;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function tell_io(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function tell_io(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Return the data */

 if(r) {
   *r=context->r_raw_bytes;
   if(context->r_file_data)
     *r-=iobuf_numbytes(context->r_file_data); /* Pretend we have not read the contents of the line data buffer */
 }

 if(w)
    *w=context->w_raw_bytes;

 if(context->saved_errno) {
   errno=context->saved_errno;
   return -1;
 }

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Finish with the IO context used for this file descriptor and report the number of bytes.

  int finish_tell_io returns 0 on success, negative if some type of IO error occurred.

  int fd The file descriptor to finish.

  unsigned long* r Returns the number of bytes read.

  unsigned long *w Returns the raw number of bytes written.
  ++++++++++++++++++++++++++++++++++++++*/

int finish_tell_io_full(int fd, int partialreset, unsigned long* r,unsigned long *w)
{
 io_context *context;
 int retval=0;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function finish_io(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function finish_io(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Check for a previous error */

 if(context->saved_errno) {
   errno=context->saved_errno;
   retval=-1;
   goto cleanup_return;
 }

 /* Finish the reading side */

 {
   int err=configure_io_chunked(fd,0,-1);
   if(err<0) {retval=err; goto cleanup_return;}
 }

#if USE_ZLIB
 {
   int err=configure_io_zlib(fd,0,-1);
   if(err<0) {retval=err; goto cleanup_return;}
 }
#endif /* USE_ZLIB */

 /* Write out any remaining data */
 {
   io_buffer *buffer_data=context->w_buffer_data;
   if(buffer_data && !iobuf_isempty(buffer_data)) {
     ssize_t err=io_write_data(fd,context,buffer_data);
     if(err<0) {retval=err; goto cleanup_return;}
   }
 }

#if USE_ZLIB
 {
   int err=configure_io_zlib(fd,-1,0);
   if(err<0) {retval=err; goto cleanup_return;}
 }
#endif /* USE_ZLIB */

 {
   int err=configure_io_chunked(fd,-1,0);
   if(err<0) {retval=err;}
 }

cleanup_return:
 /* Destroy the encryption information */

#if USE_GNUTLS
 if(!partialreset && context->gnutls_context)
    io_finish_gnutls(context->gnutls_context);
#endif

 /* Return the data */

 if(r)
    *r=context->r_raw_bytes;

 if(w)
    *w=context->w_raw_bytes;

 /* Free all data structures */

#if USE_ZLIB
 if(context->r_zlib_context)
    destroy_io_zlib(context->r_zlib_context);

 if(context->r_zlch_data)
    destroy_io_buffer(context->r_zlch_data);
#endif

 if(context->r_chunk_context)
    destroy_io_chunk(context->r_chunk_context);

 if(!partialreset && context->r_file_data)
    destroy_io_buffer(context->r_file_data);

 if(context->w_buffer_data)
   destroy_io_buffer(context->w_buffer_data);

#if USE_ZLIB
 if(context->w_zlib_context)
    destroy_io_zlib(context->w_zlib_context);

 if(context->w_zlch_data)
    destroy_io_buffer(context->w_zlch_data);
#endif

 if(context->w_chunk_context)
    destroy_io_chunk(context->w_chunk_context);

 if(context->w_file_data)
    destroy_io_buffer(context->w_file_data);

 if(partialreset) {
#if USE_ZLIB
   context->r_zlib_context=NULL;
   context->r_zlch_data=NULL;
#endif
   context->r_chunk_context=NULL;
   /* keep context->r_file_data */
   context->content_remaining=CUNDEF;
   context->content_overflow=0;
   context->w_buffer_data=NULL;
#if USE_ZLIB
   context->w_zlib_context=NULL;
   context->w_zlch_data=NULL;
#endif
   context->w_chunk_context=NULL;
   context->w_file_data=NULL;
   /* keep context->gnutls_context */
 }
 else {
   /* context->gnutls_context freed by io_finish_gnutls */

   free(context);

   io_contexts[fd]=NULL;
 }

 return retval;
}


/*++++++++++++++++++++++++++++++++++++++
  Try to read all requested bytes from a file descriptor and buffer it with a timeout.

  ssize_t read_all_or_timeout Returns the number of bytes read.

  int fd The file descriptor to read from.

  char *buf The buffer to store the data.

  size_t n  The size of the output buf.
            read_all_or_timeout will try to fill the entire buffer,
	    thus if the return value is less than count, either
	    EOF or an error has occurred.

  unsigned timeout The maximum time to wait for data (or 0 for no timeout).
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t read_all_or_timeout(int fd,char *buf,size_t n,unsigned timeout)
{
 size_t total=0;

 while (total<n) {
   int nsel; ssize_t m;
   if(timeout) {
     fd_set readfd;
     struct timeval tv;

     tv.tv_sec=timeout;
     tv.tv_usec=0;

     do {
       FD_ZERO(&readfd);

       FD_SET(fd,&readfd);

       nsel=select(fd+1,&readfd,NULL,NULL,&tv);

       if(nsel==0) {
	 errno=ETIMEDOUT;
	 return(-1);
       }
       else if(nsel<0 && errno!=EINTR)
	 return(-1);
     }
     while(nsel<=0);
   }
   m=read(fd,buf+total,n-total);
   if(m<0)
     return m;
   if(m==0)
     break;
   total += m;
 }

 return(total);
}


/*++++++++++++++++++++++++++++++++++++++
  A function to write all of a buffer of data to a file descriptor.

  ssize_t write_all Returns the number of bytes written.

  int fd The file descriptor.

  const char *data The data buffer to write.

  size_t n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t write_all(int fd,const char *data,size_t n)
{
 size_t written=0;

 while(written<n)
   {
    ssize_t m=write(fd,data+written,n-written);

    if(m<0)
      return m;

    written+=m;
   }

 return written;
}

#ifndef CLIENT_ONLY
/* Check if there is more data to read from a file descriptor.

   check_more_to_read polls a file descriptor fd, and in case of a read event,
   tries to read some data. The second file descriptor fd2 is also polled
   (unless it is equal to -1), but no attempt is made to read actual data from it.
   sigmask is a pointer to a signal mask which is passed on to the pselect call.

   check_more_to_read returns 2  if there was a read event on the second file descriptor fd2,
			      1  if there is more data to read on fd,
			      0  if an end-of-file condition was detected,
			     -1  if there was an error,
			     -2  if there was no read event within the timeout period.
			     -3  if a signal was received during the timeout period.
*/

int check_more_to_read(int fd, int fd2, unsigned timeout, const sigset_t *sigmask)
{
 io_context *context;
 int maxfd,n;
 fd_set readfds;
 struct timespec tv;
 ssize_t nb;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function check_more_to_read(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function check_more_to_read(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Check for a previous error */

 if(context->saved_errno) {
   errno=context->saved_errno;
   return -1;
 }

 if(context->r_file_data) {
   if(!iobuf_isempty(context->r_file_data))
     return 1;
 }
 else {
   /* Create a read buffer if there is not one. */
    context->r_file_data=create_io_buffer(LINE_BUFFER_SIZE);
 }

 if(context->read_eof)
   return 0;

 tv.tv_sec=timeout;
 tv.tv_nsec=0;

 do {
   FD_ZERO(&readfds);

   FD_SET(fd,&readfds);
   maxfd=fd;

   if(fd2!=-1) {
     FD_SET(fd2,&readfds);
     if(fd2>maxfd) maxfd=fd2;
   }

   n=pselect(maxfd+1,&readfds,NULL,NULL,&tv,sigmask);

   if(n==0) {
     errno=ETIMEDOUT;
     return(-2);
   }
   else if(n<0) {
     if(errno==EINTR) {
       if(timeout)
	 return(-3);
     }
     else
       return(-1);
   }
 }
 while(n<=0);

 if(fd2!=-1 && FD_ISSET(fd2,&readfds))
    return 2;

#if USE_GNUTLS
 if(context->gnutls_context)
   nb=io_gnutls_read_with_timeout(context->gnutls_context,context->r_file_data,0);
 else
#endif
   nb=io_read_with_timeout(fd,context->r_file_data,0);

 if(nb<0) {
   if(errno)
     context->saved_errno= errno;
   return -1;
 }
 else if(nb==0) {
   context->read_eof=1;
   return 0;
 }
 else
   return 1;
}


/* io_read_eof returns end-of-file flag. */
int io_read_eof(int fd)
{
 if(fd<0)
    PrintMessage(Fatal,"IO: Function io_read_eof(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function io_read_eof(%d) was called without calling init_io(%d) first.",fd,fd);

 return io_contexts[fd]->read_eof;
}
#endif /* CLIENT_ONLY */
