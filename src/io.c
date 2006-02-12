/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/io.c 2.41 2004/01/17 16:29:37 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Functions for file input and output.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/

/* Note: some of the code here has been modified by Paul Rombouts
   to suit personal taste.
*/

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


/*+ The number of IO contexts allocated. +*/
static int nio=0;

/*+ The allocated IO contexts. +*/
static /*@only@*/ io_context **io_contexts;


/*+ The chunked encoding/compression error number. +*/
int io_errno=0;

/*+ The chunked encoding/compression error message string. +*/
char /*@null@*/ *io_strerror=NULL;



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

 /* FIXME
    Re-initialise when already using compression/chunked encoding
    currently not used anywhere, only re-initialise when seeking.
    Difficult to handle because cannot start compression part-way
    through a data stream.
    Getting here should be a fatal error?
    FIXME */

 if(context->r_line_data)
   {
    free(context->r_line_data);
    context->r_line_data=NULL;
    linebuf_reset(context);
   }

 context->r_raw_bytes=0;
 context->w_raw_bytes=0;
 context->error=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Configure the IO context used for this file descriptor when reading.

  int fd The file descriptor.

  int timeout The read timeout or 0 for none or -1 to keep the same.

  int zlib The flag to indicate the new zlib compression method.

  int chunked The flag to indicate the new chunked encoding method.
  ++++++++++++++++++++++++++++++++++++++*/

void configure_io_read(int fd,int timeout,int zlib,int chunked)
{
 io_context *context;
 int change_buffers=0;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function configure_io_read(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_read(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Set the timeout */

 if(timeout>=0)
    context->r_timeout=timeout;

 /* Create the zlib decompression context */

 if(zlib>=0) {
   if(zlib) {
     if(!context->r_zlib_context)
       {
	 context->r_zlib_context=io_init_zlib_uncompress(zlib);
	 if(!context->r_zlib_context)
	   PrintMessage(Fatal,"IO: Could not initialise zlib uncompression; [%!s].");
	 change_buffers=1;
       }
   }
   else { /* !zlib */
     if(context->r_zlib_context)
       {
	 /* FIXME
	    Stop decoding ready to read more non-compressed data
	    Difficult because two possible encoding types, zlib and chunked,
	    cannot finish one without finishing the other, what about wrong order?
	    Should call finish_io() and then init_io() in this case?
	    Getting here should be a fatal error?
	    Currently does nothing to allow finish_io() to work.
	    FIXME */
       }
   }
 }

 /* Create the chunked decoding context */

 if(chunked>=0) {
   if(chunked) {
     if(!context->r_chunk_context)
       {
	 context->r_chunk_context=io_init_chunk_decode();
	 if(!context->r_chunk_context)
	   PrintMessage(Fatal,"IO: Could not initialise chunked decoding; [%!s].");
	 change_buffers=1;
       }
   }
   else { /* !chunked */
     if(context->r_chunk_context)
       {
	 /* FIXME
	    Stop decoding ready to read more non-chunked data
	    Difficult because two possible encoding types, zlib and chunked,
	    cannot finish one without finishing the other, what about wrong order?
	    Should call finish_io() and then init_io() in this case?
	    Getting here should be a fatal error?
	    Currently does nothing to allow finish_io() to work.
	    FIXME */
       }
   }
 }

 /* Change the buffers */

 if(change_buffers)
   {
     if(context->r_zlch_data)
       destroy_io_buffer(context->r_zlch_data);

     if(context->r_file_data)
       destroy_io_buffer(context->r_file_data);

     if(!context->r_zlib_context) {
       context->r_zlch_data=NULL;

       if(!context->r_chunk_context)
	 context->r_file_data=NULL;
       else /* context->r_chunk_context && !context->r_zlib_context */
	 context->r_file_data=create_io_buffer(READ_BUFFER_SIZE);
     }
     else { /* context->r_zlib_context */
       if(!context->r_chunk_context)
	 context->r_zlch_data=NULL;
       else /* context->r_chunk_context && context->r_zlib_context */
	 context->r_zlch_data=create_io_buffer(READ_BUFFER_SIZE);

       context->r_file_data=create_io_buffer(READ_BUFFER_SIZE);
     }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Configure the IO context used for this file descriptor when writing.

  int fd The file descriptor.

  int timeout The write timeout or 0 for none or -1 to keep the same.

  int zlib The flag to indicate the new zlib compression method.

  int chunked The flag to indicate the new chunked encoding method.
  ++++++++++++++++++++++++++++++++++++++*/

void configure_io_write(int fd,int timeout,int zlib,int chunked)
{
 io_context *context;
 int change_buffers=0;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function configure_io_write(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_write(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Set the timeout */

 if(timeout>=0)
    context->w_timeout=timeout;

 /* Create the zlib compression context */

 if(zlib>=0) {
   if(zlib) {
     if(!context->w_zlib_context)
       {
	 context->w_zlib_context=io_init_zlib_compress(zlib);
	 if(!context->w_zlib_context)
	   PrintMessage(Fatal,"IO: Could not initialise zlib compression; [%!s].");
	 change_buffers=1;
       }
   }
   else { /* !zlib */
     if(context->w_zlib_context)
       {
	 /* FIXME
	    Stop compressing ready to write more non-compressed data
	    Difficult because two possible encoding types, zlib and chunked,
	    cannot finish one without finishing the other, what about wrong order?
	    Should call finish_io() and then init_io() in this case?
	    Getting here should be a fatal error?
	    Currently does nothing to allow finish_io() to work.
	    FIXME */
       }
   }
 }

 /* Create the chunked encoding context */

 if(chunked>=0) {
   if(chunked) {
     if(!context->w_chunk_context)
       {
	 context->w_chunk_context=io_init_chunk_encode();
	 if(!context->w_chunk_context)
	   PrintMessage(Fatal,"IO: Could not initialise chunked encoding; [%!s].");
	 change_buffers=1;
       }
   }
   else { /* !chunked */
     if(context->w_chunk_context)
       {
	 /* FIXME
	    Stop encoding ready to write more non-chunked data
	    Difficult because two possible encoding types, zlib and chunked,
	    cannot finish one without finishing the other, what about wrong order?
	    Should call finish_io() and then init_io() in this case?
	    Getting here should be a fatal error?
	    Currently does nothing to allow finish_io() to work.
	    FIXME */
       }
   }
 }

 /* Create the buffers */

 if(change_buffers)
   {
     if(context->w_zlch_data)
       destroy_io_buffer(context->w_zlch_data);

     if(context->w_file_data)
       destroy_io_buffer(context->w_file_data);

     if(!context->w_zlib_context) {
       context->w_zlch_data=NULL;

       if(!context->w_chunk_context)
	 context->w_file_data=NULL;
       else /* context->w_chunk_context && !context->w_zlib_context */
	 context->w_file_data=create_io_buffer(READ_BUFFER_SIZE);
     }
     else { /* context->w_zlib_context */
       if(!context->w_chunk_context)
	   context->w_zlch_data=NULL;
       else /* context->w_chunk_context && context->w_zlib_context */
	   context->w_zlch_data=create_io_buffer(READ_BUFFER_SIZE);

       context->w_file_data=create_io_buffer(READ_BUFFER_SIZE+16);
     }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Read data from a file descriptor instead of read().

  int read_data Returns the number of bytes read or 0 for end of file.

  int fd The file descriptor.

  char *buffer The buffer to put the data into.

  int n The maximum number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

int read_data(int fd,char *buffer,int n)
{
 io_context *context;
 int err=0,nr=0;
 io_buffer iobuffer;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function read_data(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function read_data(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Check for a previous error */

 if(context->error)
   return -1;

 /* Finish the line data if there is any */

 if(!linebuf_isempty(context))
   {
     int nb=linebuf_numbytes(context);
     if(!context->r_chunk_context && !context->r_zlib_context)
       {
	 if(n<nb)
	   nb=n;

	 memcpy(buffer,linebuf_datastart(context),nb);
	 context->r_line_data_front+=nb;
	 return nb;
       }
     else
       {
	 io_buffer *filebuf=context->r_file_data;
	 if(iobuf_isempty(filebuf) && filebuf->size>=nb)
	   {
	     memcpy(filebuf->data,linebuf_datastart(context),nb);
	     context->r_line_data_front+=nb;
	     filebuf->front=0;
	     filebuf->rear=nb;
	   }
	 else
	   PrintMessage(Fatal,"IO: Function read_data(%d) was called with a file buffer that was not correctly initialized or too small.",fd);
       }
   }

 /* Create the output buffer */

 iobuffer.data=buffer;
 iobuffer.size=n;
 iobuffer.front=0;
 iobuffer.rear=0;

 /* Read in new data */

 if(!context->r_zlib_context) {
   if(!context->r_chunk_context)
     {
       err=io_read_with_timeout(fd,&iobuffer,context->r_timeout);
       if(err>=0) {
	 nr=err;
	 context->r_raw_bytes+=nr;
       }
     }
   else /* context->r_chunk_context && !context->r_zlib_context */
     {
       do
	 {
	   err=io_read_with_timeout(fd,context->r_file_data,context->r_timeout);
	   if(err<0) break;
	   context->r_raw_bytes+=err;
	   err=io_chunk_decode(context->r_file_data,context->r_chunk_context,&iobuffer);
	   if(err) break;
	 }
       while(iobuffer.rear==0);
       nr=iobuffer.rear;
     }
 }
 else { /* context->r_zlib_context */
   if(!context->r_chunk_context)
     {
       do
	 {
	   err=io_read_with_timeout(fd,context->r_file_data,context->r_timeout);
	   if(err<0) break;
	   context->r_raw_bytes+=err;
	   err=io_zlib_uncompress(context->r_file_data,context->r_zlib_context,&iobuffer);
	   if(err) break;
	 }
       while(iobuffer.rear==0);
       nr=iobuffer.rear;
     }
   else /* context->r_chunk_context && context->r_zlib_context */
     {
       do
	 read_again:
	 {
	   int nread;
	   err=io_read_with_timeout(fd,context->r_file_data,context->r_timeout);
	   if(err<0) break;
	   nread=err;
	   context->r_raw_bytes+=err;
	   err=io_chunk_decode(context->r_file_data,context->r_chunk_context,context->r_zlch_data);
	   if(err<0) break;
	   if(nread>0 && iobuf_isempty(context->r_zlch_data)) goto read_again;
	   err=io_zlib_uncompress(context->r_zlch_data,context->r_zlib_context,&iobuffer);
	   if(err) break;
	 }
       while(iobuffer.rear==0);
       nr=iobuffer.rear;
     }
 }

 if(err<0) {
   context->error=err;
   return(err);
 }
 else
   return(nr);
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
 int found,n;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function read_line(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function read_line(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 if(context->error) {
   if(line) free(line);
   return NULL;
 }

 /* Create the temporary line buffer if there is not one */

 if(!context->r_line_data)
   {
    context->r_line_data=malloc(LINE_BUFFER_SIZE);
    linebuf_reset(context);
   }

 /* Use the existing data or read in some more */

 n=0; found=0;
 do
   {
     int nb,i;
     line=(char*)realloc((void*)line,n+(LINE_BUFFER_SIZE+1));

     if(linebuf_isempty(context)) {
       /* FIXME
          Cannot call read_line() on compressed files.
          Probably not important, wasn't possible with old io.c either.
          FIXME */

       io_buffer iobuffer={context->r_line_data,0,0,LINE_BUFFER_SIZE};
       nb=io_read_with_timeout(fd,&iobuffer,context->r_timeout);

       if(nb<=0) {
	 if(nb<0)
	   context->error=nb;
	 free(line);
	 return(NULL);
       }

       context->r_line_data_front=iobuffer.front;
       context->r_line_data_rear=iobuffer.rear;
       context->r_raw_bytes+=nb;
     }

     for(i=context->r_line_data_front; i<context->r_line_data_rear; ++i)
       if(context->r_line_data[i]=='\n')
         {
	   found=1;
	   ++i;
	   break;
         }

     nb=i-context->r_line_data_front;
     memcpy(&line[n],linebuf_datastart(context),nb);
     context->r_line_data_front += nb;
     n += nb;
   }
 while(!found);

 line[n]=0;
 return(line);
}


/*++++++++++++++++++++++++++++++++++++++
  Write data to a file descriptor instead of write().

  int write_data Returns the number of bytes written.

  int fd The file descriptor.

  const char *data The data buffer to write.

  int n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

int write_data(int fd,const char *data,int n)
{
 io_context *context;
 io_buffer iobuffer;
 int err=0;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function write_data(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function write_data(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Check for a previous error */

 if(context->error)
   return -1;

 /* Create the temporary input buffer */

 iobuffer.data=(char*)data;
 iobuffer.size=n;
 iobuffer.front=0;
 iobuffer.rear=n;

 /* Write the output data */

 if(!context->w_zlib_context) {
   if(!context->w_chunk_context)
     {
       err=io_write_with_timeout(fd,&iobuffer,context->w_timeout);
       if(err>0) context->w_raw_bytes+=err;
     }
   else /* context->w_chunk_context && !context->w_zlib_context */
     {
       do
	 {
	   err=io_chunk_encode(&iobuffer,context->w_chunk_context,context->w_file_data);
	   if(err<0) break;
	   err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
	   if(err<0) break;
	   context->w_raw_bytes+=err;
	 }
       while(iobuffer.front<iobuffer.rear);
     }
 }
 else { /* context->w_zlib_context */
   if(!context->w_chunk_context)
     {
       do
	 {
	   err=io_zlib_compress(&iobuffer,context->w_zlib_context,context->w_file_data);
	   if(err<0) break;
	   err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
	   if(err<0) break;
	   context->w_raw_bytes+=err;
	 }
       while(iobuffer.front<iobuffer.rear);
     }
   else /* context->w_chunk_context && context->w_zlib_context */
     {
       do
	 {
	   err=io_zlib_compress(&iobuffer,context->w_zlib_context,context->w_zlch_data);
	   if(err<0) break;
	   err=io_chunk_encode(context->w_zlch_data,context->w_chunk_context,context->w_file_data);
	   if(err<0) break;
	   err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
	   if(err<0) break;
	   context->w_raw_bytes+=err;
	 }
       while(iobuffer.front<iobuffer.rear);
     }
 }

 if(err<0) {
   context->error=err;
   return(err);
 }
 else
   return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  Write a simple string to a file descriptor like fputs does to a FILE*.

  int write_string Returns the number of bytes written.

  int fd The file descriptor.

  const char *str The string.
  ++++++++++++++++++++++++++++++++++++++*/

int write_string(int fd,const char *str)
{
 return(write_data(fd,str,strlen(str)));
}


/*++++++++++++++++++++++++++++++++++++++
  Write a formatted string to a file descriptor like fprintf does to a FILE*.

  int write_formatted Returns the number of bytes written.

  int fd The file descriptor.

  const char *fmt The format string.

  ... The other arguments.
  ++++++++++++++++++++++++++++++++++++++*/

int write_formatted(int fd,const char *fmt,...)
{
 int n;
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

 if(r)
    *r=context->r_raw_bytes-linebuf_numbytes(context); /* Pretend we have not read the contents of the line data buffer */

 if(w)
    *w=context->w_raw_bytes;

 return context->error;
}


/*++++++++++++++++++++++++++++++++++++++
  Finish with the IO context used for this file descriptor and report the number of bytes.

  int finish_tell_io returns 0 on success, negative if some type of IO error occurred.

  int fd The file descriptor to finish.

  unsigned long* r Returns the number of bytes read.

  unsigned long *w Returns the raw number of bytes written.
  ++++++++++++++++++++++++++++++++++++++*/

int finish_tell_io(int fd,unsigned long* r,unsigned long *w)
{
 io_context *context;
 int retval=0;

 if(fd<0)
    PrintMessage(Fatal,"IO: Function finish_io(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function finish_io(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Check for a previous error */

 if(context->error) {
   retval=context->error;
   goto cleanup_return;
 }

 /* Finish the reading side */

 if(context->r_chunk_context) {
   int err=io_finish_chunk_decode(context->r_chunk_context,NULL);
   if(err<0) {
     retval=err;
     goto cleanup_return;
   }
 }
 
 if(context->r_zlib_context) {
   int err=io_finish_zlib_uncompress(context->r_zlib_context,NULL);
   if(err<0) {
     retval=err;
     goto cleanup_return;
   }
 }

 /* Write out any remaining data */

 if(context->w_zlib_context) {
   if(!context->w_chunk_context)
     {
       int more,err;
       do
	 {
	   more=io_finish_zlib_compress(context->w_zlib_context,context->w_file_data);
	   if(more<0) {retval=more; goto cleanup_return;}
	   err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
	   if(err<0) {retval=err; goto cleanup_return;}
	   context->w_raw_bytes+=err;
	 }
       while(more>0);
     }
   else /* context->w_chunk_context && context->w_zlib_context */
     {
       int more,err;
       do
	 {
	   more=io_finish_zlib_compress(context->w_zlib_context,context->w_zlch_data);
	   if(more<0) {retval=more; goto cleanup_return;}
	   err=io_chunk_encode(context->w_zlch_data,context->w_chunk_context,context->w_file_data);
	   if(err<0) {retval=err; goto cleanup_return;}
	   err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
	   if(err<0) {retval=err; goto cleanup_return;}
	   context->w_raw_bytes+=err;
	 }
       while(more>0);
     }
 }

 if(context->w_chunk_context)
   {
     int more,err;
     do
       {
	 more=io_finish_chunk_encode(context->w_chunk_context,context->w_file_data);
	 if(more<0) {retval=more; goto cleanup_return;}
	 err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
	 if(err<0) {retval=err; goto cleanup_return;}
	 context->w_raw_bytes+=err;
       }
     while(more>0);
   }

 cleanup_return:
 /* Return the data */

 if(r)
    *r=context->r_raw_bytes;

 if(w)
    *w=context->w_raw_bytes;

 /* Free all data structures */

 if(context->r_line_data)
    free(context->r_line_data);

 if(context->r_zlib_context)
    free(context->r_zlib_context);

 if(context->r_zlch_data)
    destroy_io_buffer(context->r_zlch_data);

 if(context->r_chunk_context)
    destroy_io_chunk(context->r_chunk_context);

 if(context->r_file_data)
    destroy_io_buffer(context->r_file_data);

 if(context->w_zlib_context)
    free(context->w_zlib_context);

 if(context->w_zlch_data)
    destroy_io_buffer(context->w_zlch_data);

 if(context->w_chunk_context)
    destroy_io_chunk(context->w_chunk_context);

 if(context->w_file_data)
    destroy_io_buffer(context->w_file_data);

 free(context);

 io_contexts[fd]=NULL;

 return retval;
}


/*++++++++++++++++++++++++++++++++++++++
  Try to read all requested bytes from a file descriptor and buffer it with a timeout.

  int read_all_or_timeout Returns the number of bytes read.

  int fd The file descriptor to read from.

  char *buf The buffer to store the data.

  int n     The size of the output buf.
            read_all_or_timeout will try to fill the entire buffer,
	    thus if the return value is less than count, either
	    EOF or an error has occurred.

  int timeout The maximum time to wait for data (or 0 for no timeout).
  ++++++++++++++++++++++++++++++++++++++*/

int read_all_or_timeout(int fd,char *buf,int n,int timeout)
{
 int total=0;

 while (total<n) {
   int nsel,m;
   if(timeout) {
     do {
       fd_set readfd;
       struct timeval tv;

       FD_ZERO(&readfd);

       FD_SET(fd,&readfd);

       tv.tv_sec=timeout;
       tv.tv_usec=0;

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

  int write_all Returns the number of bytes written.

  int fd The file descriptor.

  const char *data The data buffer to write.

  int n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

int write_all(int fd,const char *data,int n)
{
 int written=0;

 while(written<n)
   {
    int m=write(fd,data+written,n-written);

    if(m<0)
      return m;

    written+=m;
   }

 return written;
}
