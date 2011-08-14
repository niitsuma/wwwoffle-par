/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9b.
  Functions for file input and output with compression.
  ******************/ /******************
  Originally written by Andrew M. Bishop.
  Extensively modified by Paul A. Rombouts.

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2004,2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>

#include "misc.h"
#include "io.h"
#include "iopriv.h"
#include "errors.h"


/*+ The minimum chunk size to output (use same size as WRITE_BUFFER_SIZE).. +*/
#define MIN_CHUNK_SIZE (IO_BUFFER_SIZE/4)

/* Decoding states */
typedef enum _iochunk_state_t {
  iochunk_null,
  doing_head,
  doing_chunk,
  doing_trailer,
  iochunk_done
} iochunk_state_t;


/* Local functions */

static int parse_chunk_head(io_chunk *context,const char *buffer,size_t n);
static int parse_chunk_trailer(io_chunk *context,const char *buffer,size_t n);

static void set_error(const char *msg);


/*++++++++++++++++++++++++++++++++++++++
  Initialise the chunked-encoding context information.

  io_chunk *io_init_chunk_encode Returns a new chunked encoding context.

  io_chunk **new_context The new chunk context information to create.
  ++++++++++++++++++++++++++++++++++++++*/

io_chunk *io_init_chunk_encode(void)
{
 io_chunk *context=(io_chunk*)calloc(1,sizeof(io_chunk));

 context->chunk_buffer=create_io_buffer(MIN_CHUNK_SIZE);

 return(context);
}


/*++++++++++++++++++++++++++++++++++++++
  Initialise the chunked-decoding context information.

  io_chunk *io_init_chunk_decode Returns a new chunked decoding context.

  io_chunk **new_context The new chunk context information to create.
  ++++++++++++++++++++++++++++++++++++++*/

io_chunk *io_init_chunk_decode(void)
{
 io_chunk *context=(io_chunk*)calloc(1,sizeof(io_chunk));

 context->state=doing_head;
 context->ht_bytenr=3;         /* already skipped the first CRLF */

 return(context);
}


/*++++++++++++++++++++++++++++++++++++++
  Destroy an io_chunk structure.

  io_chunk *context The io_chunk to destroy.
  ++++++++++++++++++++++++++++++++++++++*/

void destroy_io_chunk(io_chunk *context)
{
  if(context->chunk_buffer) destroy_io_buffer(context->chunk_buffer);
  free(context);
}


/*++++++++++++++++++++++++++++++++++++++
  Read data from an input buffer and perform chunked encoding to an output buffer.

  int io_chunk_encode Returns 0 normally, 1 if finished or negative for error.

  io_buffer *in The input buffer, or NULL to flush the internal buffer.

  io_chunk *context The chunk context information.

  io_buffer *out The output buffer.
  ++++++++++++++++++++++++++++++++++++++*/

int io_chunk_encode(io_buffer *in,io_chunk *context,io_buffer *out)
{
 io_buffer *chunk_buffer=context->chunk_buffer;
 size_t nchunkbuf,chunk_size;

 if(iobuf_isempty(chunk_buffer)) iobuf_reset(chunk_buffer);
 chunk_size=nchunkbuf=iobuf_numbytes(chunk_buffer);

 if(in) {
   int numbytes=iobuf_numbytes(in);
   chunk_size += numbytes;

   /* If insufficient data buffer internally */

   if(chunk_size<chunk_buffer->size && chunk_buffer->rear+numbytes<=chunk_buffer->size)
     {
       memcpy(chunk_buffer->data+chunk_buffer->rear,iobuf_datastart(in),numbytes);
       in->front += numbytes;
       chunk_buffer->rear += numbytes;

       return(0);
     }
 }

 if(iobuf_isempty(out)) iobuf_reset(out);
 {
   size_t remout=out->size-out->rear;
   /* unsigned arithmetic, be careful. */
   if(remout<MAXCHUNKPADDING) return 0;
   remout -= MAXCHUNKPADDING;
   if(chunk_size>remout)
     chunk_size=remout;
 }

 /* Don't output a chunk smaller than the minimum. */
 if(chunk_size<(in?chunk_buffer->size:nchunkbuf))
   return(0);

 /* assertion: chunk_size>= nchunkbuf */

 out->rear += sprintf(out->data+out->rear,"%x\r\n",(unsigned)chunk_size);

 /* Output all of the internal buffer. */

 if(nchunkbuf>0)
   {
    memcpy(out->data+out->rear,iobuf_datastart(chunk_buffer),nchunkbuf);
    chunk_buffer->front += nchunkbuf;
    out->rear += nchunkbuf;
    chunk_size -= nchunkbuf;
   }

 /* If there is more to output then use the input buffer */

 if(chunk_size>0)
   {
    memcpy(out->data+out->rear,iobuf_datastart(in),chunk_size);
    in->front += chunk_size;
    out->rear += chunk_size;
   }

 out->data[out->rear++]='\r';
 out->data[out->rear++]='\n';

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Read some data from an input buffer and perform chunked decoding to an output buffer.

  int io_chunk_decode Returns 0 normally, 1 if finished or negative for error.

  io_buffer *in The input buffer.

  io_chunk *context The chunk context information.

  io_buffer *out The output buffer.
  ++++++++++++++++++++++++++++++++++++++*/

int io_chunk_decode(io_buffer *in,io_chunk *context,io_buffer *out)
{
 size_t numbytes;

 /* If there is no input data then we have finished early. */

 if(iobuf_isempty(in))
    return(1);

 if(iobuf_isempty(out)) iobuf_reset(out);

 do
   {
     numbytes=iobuf_numbytes(in);
    /* If parsing a header then do that */

    if(context->state==doing_head)
      {
       int nb=parse_chunk_head(context,iobuf_datastart(in),numbytes);

       if(nb<0) /* error */
          return(nb);

       in->front += nb;
       if(nb==numbytes) /* read all bytes, need more */
          return(0);
      }

    /* else if parsing a trailer then do that */

    else if(context->state==doing_trailer)
      {
       int nb=parse_chunk_trailer(context,iobuf_datastart(in),numbytes);

       if(nb<0) /* error */
          return(nb);

       in->front += nb;
       if(nb==numbytes) /* read all bytes, may need more */
          return(context->state==iochunk_done);
      }

    /* else if parsing a chunk do that */

    else if(context->state==doing_chunk)
      {
       int chunk_size=numbytes;

       if(!context->remain)
	 {set_error("chunk remainder is unexpectedly zero");return(-1);}

       if(chunk_size>context->remain)
          chunk_size=context->remain;

       {
	 size_t remout=out->size-out->rear;
	 if(chunk_size>remout)
	   chunk_size=remout;
       }

       if(chunk_size>0)
         {
          memcpy(out->data+out->rear,iobuf_datastart(in),chunk_size);
	  in->front += chunk_size;
          out->rear += chunk_size;

          context->remain -= chunk_size;

          if(context->remain==0)
            {
             context->state=doing_head;
             context->ht_bytenr=1;
            }
         }
      }

    /* else must be finished */

    else
       return(1);

   }
 while(!iobuf_isempty(in) && out->rear<out->size);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Finalise the chunked-encoding data stream and output all remaining bytes.

  int io_finish_chunk_encode Returns 0 on completion, 1 if there is more data, negative if error.

  io_chunk *context The chunk context information..

  io_buffer *out The final output buffer to fill with tail data.
  ++++++++++++++++++++++++++++++++++++++*/

int io_finish_chunk_encode(io_chunk *context,io_buffer *out)
{
 io_buffer *chunk_buffer=context->chunk_buffer;

 if(!iobuf_isempty(chunk_buffer)) {
   io_chunk_encode(NULL,context,out);

   if(!iobuf_isempty(chunk_buffer))
     return(1);
 }

 if(iobuf_isempty(out)) iobuf_reset(out);

 if((out->size-out->rear)<5)
    return(1);

 out->data[out->rear++]='0';
 out->data[out->rear++]='\r';
 out->data[out->rear++]='\n';
 out->data[out->rear++]='\r';
 out->data[out->rear++]='\n';

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Finalise the chunked-encoding data stream and output all remaining bytes.

  int io_finish_chunk_decode Returns 0 on completion, 1 if there is more data, negative if error.

  io_chunk *context The chunk context information..

  io_buffer *out The final output buffer to fill with tail data.
  ++++++++++++++++++++++++++++++++++++++*/

int io_finish_chunk_decode(io_chunk *context,/*@unused@*/ io_buffer *out)
{
 return(0);
}


/*+ For conversion from hex string to integer. +*/
static const unsigned char unhexstring[64]={ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
                                             0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
                                             0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
                                             0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0x60-0x6f "`abcdefghijklmno" */


/*++++++++++++++++++++++++++++++++++++++
  Parse a chunk header.

  int parse_chunk_head Returns the amount of the buffer consumed (not all if head finished).

  io_chunk *context The chunk context information.

  const char *buffer The buffer of new data.

  size_t n The amount of new data.
  ++++++++++++++++++++++++++++++++++++++*/

static int parse_chunk_head(io_chunk *context,const char *buffer,size_t n)
{
 const unsigned char *p0=(const unsigned char *)buffer,*p=(const unsigned char *)buffer;

 while(n>0)
   {
    switch(context->ht_bytenr)
      {
      case 1:                   /* CR from previous chunk */
       if(*p!='\r')
         {set_error("not chunked encoding; not CR");return(-1);}
       context->ht_bytenr++;n--;p++;
       break;
      case 2:                   /* LF from previous chunk */
       if(*p!='\n')
         {set_error("not chunked encoding; not LF");return(-1);}
       context->ht_bytenr++;n--;p++;
       break;
      case 3:                   /* length byte 1 */
       if(isxdigit(*p))
         {context->ht_bytenr++;context->remain=unhexstring[*p-48];}
       else if(*p==' ')
          ;
       else
         {set_error("not chunked encoding; not hex digit");return(-1);}
       n--;p++;
       break;
      case 4:                   /* length byte 2..16 or ';' or CR */
       if(isxdigit(*p))
          context->remain=(context->remain<<4)+unhexstring[*p-48];
       else if(*p==';')
          context->ht_bytenr=5;
       else if(*p=='\r')
          context->ht_bytenr=6;
       else if(*p==' ')
          ;
       else
         {set_error("not chunked encoding; not hex digit, ';' or CR");return(-1);}
       n--;p++;
       break;
      case 5:                   /* chunk extension */
       if(*p=='\r')
          context->ht_bytenr=6;
       n--;p++;
       break;
      case 6:                   /* LF */
       if(*p=='\n')
          context->ht_bytenr=0;
       else
         {set_error("not chunked encoding; not LF");return(-1);}
       n--;p++;
       break;
      case 0:                   /* finished */
       n=0;
      }
   }

 if(context->ht_bytenr==0)
   {
    if(context->remain)
       context->state=doing_chunk;
    else {
       context->state=doing_trailer;
       context->ht_bytenr=1;
    }
   }

 return(p-p0);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse a chunk trailer.

  int parse_chunk_trailer Returns the amount of the buffer consumed (not all if trailer finished).

  io_chunk *context The chunk context information.

  const char *buffer The buffer of new data.

  size_t n The amount of new data.
  ++++++++++++++++++++++++++++++++++++++*/

static int parse_chunk_trailer(io_chunk *context,const char *buffer,size_t n)
{
 const unsigned char *p0=(const unsigned char *)buffer,*p=(const unsigned char *)buffer;

 while(n>0)
   {
    switch(context->ht_bytenr)
      {
      case 1:                   /* trailer text first char */
       if(*p=='\r')
         {
          context->ht_bytenr=4;
          break;
         }
       context->ht_bytenr++;n--;p++;
       break;
      case 2:                   /* trailer text other chars or CR */
       if(*p=='\r')
          context->ht_bytenr++;
       n--;p++;
       break;
      case 3:                   /* trailer text LF */
       if(*p!='\n')
         {set_error("not chunked encoding; not LF");return(-1);}
       context->ht_bytenr=1;
       n--;p++;
       break;
      case 4:                   /* final CR */
       context->ht_bytenr++;n--;p++;
       break;
      case 5:                   /* final LF */
       if(*p!='\n')
         {set_error("not chunked encoding; not LF");return(-1);}
       context->ht_bytenr=0;n--;p++;
       break;
      case 0:                   /* finished */
       n=0;
      }
   }

 if(context->ht_bytenr==0)
   context->state=iochunk_done;

 return(p-p0);
}


/*++++++++++++++++++++++++++++++++++++++
  Set the error status when there is a chunk decoding error.

  const char *msg The error message.
  ++++++++++++++++++++++++++++++++++++++*/

static void set_error(const char *msg)
{
 errno=ERRNO_USE_IO_ERRNO;

 if(io_strerror)
    free(io_strerror);

 io_strerror=x_asprintf("IO(chunked): %s",msg);

 io_errno=-1;
}
