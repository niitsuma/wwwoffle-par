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


/*+ The buffer size for reading lines. +*/
#define LINE_BUFFER_SIZE 128


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
 if(fd==-1)
    PrintMessage(Fatal,"IO: Function init_io(%d) was called with an invalid argument.",fd);

 /* Allocate some space for new IO contexts */

 if(fd>=nio)
   {
    io_contexts=(io_context**)realloc((void*)io_contexts,(fd+9)*sizeof(io_context**));

    for(;nio<=(fd+8);nio++)
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

 if(fd==-1)
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
    context->r_line_data_len=0;
   }

 context->r_raw_bytes=0;
 context->w_raw_bytes=0;
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

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function configure_io_read(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_read(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Set the timeout */

 if(timeout>=0)
    context->r_timeout=timeout;

 /* Create the zlib decompression context */

 if(zlib<0)
    ;
 else if(zlib && !context->r_zlib_context)
   {
    context->r_zlib_context=io_init_zlib_uncompress(zlib);
    if(!context->r_zlib_context)
       PrintMessage(Fatal,"IO: Could not initialise zlib uncompression; [%!s].");
    change_buffers=1;
   }
 else if(!zlib && context->r_zlib_context)
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

 /* Create the chunked decoding context */

 if(chunked<0)
    ;
 else if(chunked && !context->r_chunk_context)
   {
    context->r_chunk_context=io_init_chunk_decode();
    if(!context->r_chunk_context)
       PrintMessage(Fatal,"IO: Could not initialise chunked decoding; [%!s].");
    change_buffers=1;
   }
 else if(!chunked && context->r_chunk_context)
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

 /* Change the buffers */

 if(change_buffers)
   {
    if(context->r_zlch_data)
       destroy_io_buffer(context->r_zlch_data);

    if(context->r_file_data)
       destroy_io_buffer(context->r_file_data);

    if(!context->r_chunk_context && !context->r_zlib_context)
      {
       context->r_zlch_data=NULL;
       context->r_file_data=NULL;
      }
    else if(context->r_chunk_context && !context->r_zlib_context)
      {
       context->r_zlch_data=NULL;
       context->r_file_data=create_io_buffer(READ_BUFFER_SIZE);
      }
    else if(!context->r_chunk_context && context->r_zlib_context)
      {
       context->r_zlch_data=NULL;
       context->r_file_data=create_io_buffer(READ_BUFFER_SIZE);
      }
    else /* if(context->r_chunk_context && context->r_zlib_context) */
      {
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

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function configure_io_write(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function configure_io_write(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Set the timeout */

 if(timeout>=0)
    context->w_timeout=timeout;

 /* Create the zlib compression context */

 if(zlib<0)
    ;
 else if(zlib && !context->w_zlib_context)
   {
    context->w_zlib_context=io_init_zlib_compress(zlib);
    if(!context->w_zlib_context)
       PrintMessage(Fatal,"IO: Could not initialise zlib compression; [%!s].");
    change_buffers=1;
   }
 else if(!zlib && context->w_zlib_context)
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

 /* Create the chunked encoding context */

 if(chunked<0)
    ;
 else if(chunked && !context->w_chunk_context)
   {
    context->w_chunk_context=io_init_chunk_encode();
    if(!context->w_chunk_context)
       PrintMessage(Fatal,"IO: Could not initialise chunked encoding; [%!s].");
    change_buffers=1;
   }
 else if(!chunked && context->w_chunk_context)
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

 /* Create the buffers */

 if(change_buffers)
   {
    if(context->w_zlch_data)
       destroy_io_buffer(context->w_zlch_data);

    if(context->w_file_data)
       destroy_io_buffer(context->w_file_data);

    if(!context->w_chunk_context && !context->w_zlib_context)
      {
       context->w_zlch_data=NULL;
       context->w_file_data=NULL;
      }
    else if(context->w_chunk_context && !context->w_zlib_context)
      {
       context->w_zlch_data=NULL;
       context->w_file_data=create_io_buffer(READ_BUFFER_SIZE);
      }
    else if(!context->w_chunk_context && context->w_zlib_context)
      {
       context->w_zlch_data=NULL;
       context->w_file_data=create_io_buffer(READ_BUFFER_SIZE+16);
      }
    else /* if(context->w_chunk_context && context->w_zlib_context) */
      {
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

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function read_data(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function read_data(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Create the output buffer */

 iobuffer.data=buffer;
 iobuffer.size=n;
 iobuffer.length=0;

 /* Finish the line data if there is any */

 if(context->r_line_data_len)
   {
    if(!context->r_chunk_context && !context->r_zlib_context)
      {
       if(iobuffer.size>context->r_line_data_len)
         {
          memcpy(iobuffer.data,context->r_line_data,context->r_line_data_len);
          iobuffer.length+=context->r_line_data_len;
          context->r_line_data_len=0;
         }
       else
         {
          memcpy(iobuffer.data,context->r_line_data,iobuffer.size);
          iobuffer.length+=iobuffer.size;
          memmove(context->r_line_data,context->r_line_data+iobuffer.size,context->r_line_data_len-iobuffer.size);
          context->r_line_data_len-=iobuffer.size;
         }

       return(iobuffer.length);
      }
    else
      {
       memcpy(context->r_file_data->data,context->r_line_data,context->r_line_data_len);
       context->r_file_data->length+=context->r_line_data_len;
       context->r_line_data_len=0;
      }
   }

 /* Read in new data */

 if(!context->r_chunk_context && !context->r_zlib_context)
   {
    nr=io_read_with_timeout(fd,&iobuffer,context->r_timeout);
    if(nr>0) context->r_raw_bytes+=nr;
   }
 else if(context->r_chunk_context && !context->r_zlib_context)
   {
    do
      {
       err=io_read_with_timeout(fd,context->r_file_data,context->r_timeout);
       if(err>0) context->r_raw_bytes+=err;
       if(err<0) break;
       err=io_chunk_decode(context->r_file_data,context->r_chunk_context,&iobuffer);
       if(err<0 || err==1) break;
      }
    while(iobuffer.length==0);
    nr=iobuffer.length;
   }
 else if(!context->r_chunk_context && context->r_zlib_context)
   {
    do
      {
       err=io_read_with_timeout(fd,context->r_file_data,context->r_timeout);
       if(err>0) context->r_raw_bytes+=err;
       if(err<0) break;
       err=io_zlib_uncompress(context->r_file_data,context->r_zlib_context,&iobuffer);
       if(err<0 || err==1) break;
      }
    while(iobuffer.length==0);
    nr=iobuffer.length;
   }
 else /* if(context->r_chunk_context && context->r_zlib_context) */
   {
    do
      {
       err=io_read_with_timeout(fd,context->r_file_data,context->r_timeout);
       if(err>0) context->r_raw_bytes+=err;
       if(err<0) break;
       err=io_chunk_decode(context->r_file_data,context->r_chunk_context,context->r_zlch_data);
       if(err<0) break;
       err=io_zlib_uncompress(context->r_zlch_data,context->r_zlib_context,&iobuffer);
       if(err<0 || err==1) break;
      }
    while(iobuffer.length==0);
    nr=iobuffer.length;
   }

 if(err<0)
    return(err);
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
 int found=0,eof=0;
 int n=0;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function read_line(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function read_line(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Create the temporary line buffer if there is not one */

 if(!context->r_line_data)
   {
    context->r_line_data=(char*)malloc(LINE_BUFFER_SIZE+1);
    context->r_line_data_len=0;
   }

 /* Use the existing data or read in some more */

 do
   {
    line=(char*)realloc((void*)line,n+(LINE_BUFFER_SIZE+1));

    if(context->r_line_data_len>0)
      {
       for(n=0;n<context->r_line_data_len;n++)
          if(context->r_line_data[n]=='\n')
            {
             found=1;
             n++;
             break;
            }

       memcpy(line,context->r_line_data,n);

       if(n==context->r_line_data_len)
          context->r_line_data_len=0;
       else
         {
          context->r_line_data_len-=n;
          memmove(context->r_line_data,context->r_line_data+n,context->r_line_data_len);
         }
      }
    else
      {
       int nn;
       io_buffer iobuffer;

       /* FIXME
          Cannot call read_line() on compressed files.
          Probably not important, wasn't possible with old io.c either.
          FIXME */

       iobuffer.data=line+n;
       iobuffer.size=LINE_BUFFER_SIZE;
       iobuffer.length=0;

       nn=io_read_with_timeout(fd,&iobuffer,context->r_timeout);
       if(nn>0) context->r_raw_bytes+=nn;

       if(nn<=0)
         {eof=1;break;}
       else
          nn+=n;

       for(;n<nn;n++)
          if(line[n]=='\n')
            {
             found=1;
             n++;
             break;
            }

       if(found)
         {
          context->r_line_data_len=nn-n;
          memcpy(context->r_line_data,line+n,context->r_line_data_len);
         }
      }
   }
 while(!found && !eof);

 if(found)
    line[n]=0;

 if(eof)
   {free(line);line=NULL;}

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

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function write_data(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function write_data(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Create the temporary input buffer */

 iobuffer.data=(char*)data;
 iobuffer.size=n;
 iobuffer.length=n;

 /* Write the output data */

 if(!context->w_chunk_context && !context->w_zlib_context)
   {
    err=io_write_with_timeout(fd,&iobuffer,context->w_timeout);
    if(err>0) context->w_raw_bytes+=err;
   }
 else if(context->w_chunk_context && !context->w_zlib_context)
   {
    do
      {
       err=io_chunk_encode(&iobuffer,context->w_chunk_context,context->w_file_data);
       if(err<0) break;
       err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
       if(err>0) context->w_raw_bytes+=err;
       if(err<0) break;
      }
    while(iobuffer.length>0);
   }
 else if(!context->w_chunk_context && context->w_zlib_context)
   {
    do
      {
       err=io_zlib_compress(&iobuffer,context->w_zlib_context,context->w_file_data);
       if(err<0) break;
       err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
       if(err>0) context->w_raw_bytes+=err;
       if(err<0) break;
      }
    while(iobuffer.length>0);
   }
 else /* if(context->w_chunk_context && context->w_zlib_context) */
   {
    do
      {
       err=io_zlib_compress(&iobuffer,context->w_zlib_context,context->w_zlch_data);
       if(err<0) break;
       err=io_chunk_encode(context->w_zlch_data,context->w_chunk_context,context->w_file_data);
       if(err<0) break;
       err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
       if(err>0) context->w_raw_bytes+=err;
       if(err<0) break;
      }
    while(iobuffer.length>0);
   }

 if(err<0)
    return(err);
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
 int i,n,width;
 char *str,*strp;
 va_list ap;

 /* Estimate the length of the string. */

#ifdef __STDC__
 va_start(ap,fmt);
#else
 va_start(ap);
#endif

 n=strlen(fmt);

 for(i=0;fmt[i];i++)
    if(fmt[i]=='%')
      {
       i++;

       if(fmt[i]=='%')
          continue;

       width=atoi(fmt+i);
       if(width<0)
          width=-width;

       while(!isalpha(fmt[i]))
          i++;

       switch(fmt[i])
         {
         case 's':
          strp=va_arg(ap,char*);
          if(width && width>strlen(strp))
             n+=width;
          else
             n+=strlen(strp);
          break;

         default:
          (void)va_arg(ap,void*);
          if(width && width>16)
             n+=width;
          else
             n+=16;
         }
      }

 va_end(ap);

 /* Allocate the string and vsprintf into it. */

 str=(char*)malloc(n+1);

#ifdef __STDC__
 va_start(ap,fmt);
#else
 va_start(ap);
#endif

 n=vsprintf(str,fmt,ap);

 va_end(ap);

 /* Write the string. */

 n=write_data(fd,str,n);

 free(str);

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the number of raw bytes read and written to the file descriptor.

  int fd The file descriptor.

  unsigned long* r Returns the number of bytes read.

  unsigned long *w Returns the raw number of bytes written.
  ++++++++++++++++++++++++++++++++++++++*/

void tell_io(int fd,unsigned long* r,unsigned long *w)
{
 io_context *context;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function tell_io(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function tell_io(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Return the data */

 if(r)
    *r=context->r_raw_bytes-context->r_line_data_len; /* Pretend we have not read the contents of the line data buffer */

 if(w)
    *w=context->w_raw_bytes;
}


/*++++++++++++++++++++++++++++++++++++++
  Finish with the IO context used for this file descriptor and report the number of bytes.

  int fd The file descriptor to finish.

  unsigned long* r Returns the number of bytes read.

  unsigned long *w Returns the raw number of bytes written.
  ++++++++++++++++++++++++++++++++++++++*/

void finish_tell_io(int fd,unsigned long* r,unsigned long *w)
{
 io_context *context;
 int more;

 if(fd==-1)
    PrintMessage(Fatal,"IO: Function finish_io(%d) was called with an invalid argument.",fd);

 if(nio<=fd || !io_contexts[fd])
    PrintMessage(Fatal,"IO: Function finish_io(%d) was called without calling init_io(%d) first.",fd,fd);

 context=io_contexts[fd];

 /* Finish the reading side */

 if(!context->r_chunk_context && !context->r_zlib_context)
    ;
 else if(context->r_chunk_context && !context->r_zlib_context)
   {
    io_finish_chunk_decode(context->r_chunk_context,NULL);
   }
 else if(!context->r_chunk_context && context->r_zlib_context)
   {
    io_finish_zlib_uncompress(context->r_zlib_context,NULL);
   }
 else /* if(context->r_chunk_context && context->r_zlib_context) */
   {
    io_finish_chunk_decode(context->r_chunk_context,NULL);
    io_finish_zlib_uncompress(context->r_zlib_context,NULL);
   }

 /* Write out any remaining data */

 if(!context->w_chunk_context && !context->w_zlib_context)
    ;
 else if(context->w_chunk_context && !context->w_zlib_context)
   {
    do
      {
       more=io_finish_chunk_encode(context->w_chunk_context,context->w_file_data);
       if(more>=0)
         {
          int err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
          if(err>0) context->w_raw_bytes+=err;
         }
      }
    while(more==1);
   }
 else if(!context->w_chunk_context && context->w_zlib_context)
   {
    do
      {
       more=io_finish_zlib_compress(context->w_zlib_context,context->w_file_data);
       if(more>=0)
         {
          int err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
          if(err>0) context->w_raw_bytes+=err;
         }
      }
    while(more==1);
   }
 else /* if(context->w_chunk_context && context->w_zlib_context) */
   {
    do
      {
       more=io_finish_zlib_compress(context->w_zlib_context,context->w_zlch_data);
       if(more>=0)
         {
          int more2=io_chunk_encode(context->w_zlch_data,context->w_chunk_context,context->w_file_data);
          if(more2>=0)
            {
             int err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
             if(err>0) context->w_raw_bytes+=err;
            }
         }
      }
    while(more==1);
    do
      {
       more=io_finish_chunk_encode(context->w_chunk_context,context->w_file_data);
       if(more>=0)
         {
          int err=io_write_with_timeout(fd,context->w_file_data,context->w_timeout);
          if(err>0) context->w_raw_bytes+=err;
         }
      }
    while(more==1);
   }

 /* Return the data */

 if(r)
    *r=context->r_raw_bytes;

 if(w)
    *w=context->w_raw_bytes;

 /* Free all data structures */

 if(context->r_line_data)
    free(context->r_line_data);

 if(context->r_zlch_data)
    destroy_io_buffer(context->r_zlch_data);

 if(context->r_file_data)
    destroy_io_buffer(context->r_file_data);

 if(context->w_zlch_data)
    destroy_io_buffer(context->w_zlch_data);

 if(context->w_file_data)
    destroy_io_buffer(context->w_file_data);

 free(context);

 io_contexts[fd]=NULL;
}
