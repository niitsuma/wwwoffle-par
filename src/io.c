/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/io.c 2.29 2002/09/12 18:16:24 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7f.
  Functions for file input and output.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02 Andrew M. Bishop
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

#include <fcntl.h>
#include <errno.h>

#if USE_ZLIB
#include <zlib.h>
#endif

#include "wwwoffle.h"
#include "errors.h"
#include "misc.h"


/*+ The buffer size for reading lines. +*/
#define BUFSIZE 64

/*+ The buffer of data from each of the files. +*/
static char **fdbuf=NULL;

/*+ The number of bytes of data buffered for each file. +*/
static int *fdbytes=NULL;

/*+ The number of file buffers allocated. +*/
static int nfdbuf=0;

/*+ The timeout in seconds for reading from a socket. +*/
static int read_timeout=0;


#if USE_ZLIB
/*+ A data structure to hold the deflate stream and the gzip info. +*/
typedef struct _zdata
{
 int direction;                 /*+ The direction, compress or uncompress +*/
 z_stream stream;               /*+ The deflate / inflate stream. +*/

 unsigned long crc;             /*+ The gzip crc. +*/

 int doing_head;                /*+ A flag to indicate that we are doing a gzip head. +*/
 int head_extra_len;            /*+ A gzip header extra field length. +*/
 int head_flag;                 /*+ A flag to store the gzip header flag. +*/

 int doing_body;                /*+ A flag to indicate that we are doing a gzip body. +*/

 int doing_tail;                /*+ A flag to indicate that we are doing a gzip tail. +*/
 unsigned long tail_crc;        /*+ The crc value stored in the gzip tail. +*/
 unsigned long tail_len;        /*+ The length value stored in the gzip tail. +*/
}
zdata;

/*+ The buffer to hold the compression information. +*/
static zdata **fdzlib=NULL;

/*+ The size of the temporary buffer for compressing/uncompressing, smaller than the read/write buffer. +*/
#define ZBUFFER_SIZE (READ_BUFFER_SIZE/4)

/*+ A temporary buffer for decompressing from or compressing into +*/
static char zbuffer[ZBUFFER_SIZE];

/*+ The compression error number. +*/
int z_errno=0;

/*+ The compression error message string. +*/
char *z_strerror=NULL;
#endif


static int read_into_buffer(int fd);
static int read_into_buffer_or_timeout(int fd);
static int read_from_buffer(int fd,char *buffer,int n);

static int write_all(int fd,const char *data,int n);

#if USE_ZLIB
static int read_uncompressing_from_buffer(int fd,char *buffer,int n);

static int read_uncompressing(int fd,char *buffer,int n);

static int write_compressing(int fd,const char *buffer,int n);

static int parse_gzip_head(int fd,char *buffer,int n);
static int parse_gzip_tail(int fd,char *buffer,int n);

static void set_zerror(char *msg);
#endif

/*++++++++++++++++++++++++++++++++++++++
  Set the timeout for reading from a socket.

  int timeout The timeout in seconds.
  ++++++++++++++++++++++++++++++++++++++*/

void set_read_timeout(int timeout)
{
 read_timeout=timeout;
}


/*++++++++++++++++++++++++++++++++++++++
  Call fgets and realloc the buffer as needed to get a whole line.

  char *fgets_realloc Returns the modified buffer (NULL at the end of the file).

  char *buffer The current buffer.

  FILE *file The file to read from.
  ++++++++++++++++++++++++++++++++++++++*/

char *fgets_realloc(char *buffer,FILE *file)
{
 int n=0;
 char *buf;

 if(!buffer)
    buffer=(char*)malloc((BUFSIZE+1));

 while((buf=fgets(&buffer[n],BUFSIZE,file)))
   {
    int s=strlen(buf);
    n+=s;

    if(buffer[n-1]=='\n')
       break;
    else
       buffer=(char*)realloc(buffer,n+(BUFSIZE+1));
   }

 if(!buf)
   {free(buffer);buffer=NULL;}

 return(buffer);
}


/*++++++++++++++++++++++++++++++++++++++
  Initialise the buffer used for this file descriptor.

  int fd The file descriptor to initialise.
  ++++++++++++++++++++++++++++++++++++++*/

void init_buffer(int fd)
{
 if(fd==-1)
    return;

 if(fd>=nfdbuf)
   {
    fdbuf=(char**)realloc((void*)fdbuf,(fd+9)*sizeof(char**));
    fdbytes=(int*)realloc((void*)fdbytes,(fd+9)*sizeof(int));
#if USE_ZLIB
    fdzlib=(zdata**)realloc((void*)fdzlib,(fd+9)*sizeof(zdata*));
#endif

    for(;nfdbuf<=(fd+8);nfdbuf++)
      {
       fdbuf[nfdbuf]=NULL;
       fdbytes[nfdbuf]=0;
#if USE_ZLIB
       fdzlib[nfdbuf]=NULL;
#endif
      }
   }

 if(!fdbuf[fd])
    fdbuf[fd]=(char*)malloc(BUFSIZE);

 fdbytes[fd]=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Read all of the data from the file descriptor and dump it.

  int empty_buffer Returns the amount that was read.

  int fd The file descriptor to read from.
  ++++++++++++++++++++++++++++++++++++++*/

int empty_buffer(int fd)
{
 int nr=fdbytes[fd];

 while(1)
   {
    int n;
    fd_set readfd;
    struct timeval tv;

    while(1)
      {
       FD_ZERO(&readfd);

       FD_SET(fd,&readfd);

       tv.tv_sec=tv.tv_usec=0;

       n=select(fd+1,&readfd,NULL,NULL,&tv);

       if(n>0)
          break;
       else if(n==0 || errno!=EINTR)
          return(nr);
      }

    n=read(fd,fdbuf[fd],BUFSIZE);

    if(n>0)
       nr+=n;
    else
       return(nr);
   }

 /*@notreached@*/

 return(nr);
}


/*++++++++++++++++++++++++++++++++++++++
  Read data from a file descriptor.

  int read_data Returns the number of bytes read or 0 for end of file.

  int fd The file descriptor.

  char *buffer The buffer to put the data into.

  int n The maximum number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

int read_data(int fd,char *buffer,int n)
{
 int nr=0;

#if USE_ZLIB
repeat:
#endif

 if(fdbytes[fd])
   {
#if USE_ZLIB
    if(fdzlib[fd])
       nr=read_uncompressing_from_buffer(fd,buffer,n);
    else
#endif
       nr=read_from_buffer(fd,buffer,n);

    if(nr)
       return(nr);
   }

#if USE_ZLIB
 if(fdzlib[fd])
   {
    nr=read_uncompressing(fd,buffer,n);
    if(nr==0 && (fdzlib[fd]->doing_head || fdzlib[fd]->doing_body || fdzlib[fd]->doing_tail))
       goto repeat;
   }
 else
#endif
    nr=read(fd,buffer,n);

 return(nr);
}


/*++++++++++++++++++++++++++++++++++++++
  Read data from a file descriptor, with a timeout.

  int read_or_timeout Returns the number of bytes read or -1 for a timeout.

  int fd The file descriptor.

  char *buffer The buffer to put the data into.

  int n The maximum number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

int read_data_or_timeout(int fd,char *buffer,int n)
{
 int nr=0;

#if USE_ZLIB
repeat:
#endif

 if(fdbytes[fd])
   {
#if USE_ZLIB
    if(fdzlib[fd])
       nr=read_uncompressing_from_buffer(fd,buffer,n);
    else
#endif
       nr=read_from_buffer(fd,buffer,n);

    if(nr)
       return(nr);
   }

 if(!read_timeout)
   {
#if USE_ZLIB
    if(fdzlib[fd])
      {
       nr=read_uncompressing(fd,buffer,n);
       if(nr==0 && (fdzlib[fd]->doing_head || fdzlib[fd]->doing_body || fdzlib[fd]->doing_tail))
          goto repeat;
      }
    else
#endif
       nr=read(fd,buffer,n);
   }
 else
   {
    fd_set readfd;
    struct timeval tv;

    while(1)
      {
       FD_ZERO(&readfd);

       FD_SET(fd,&readfd);

       tv.tv_sec=read_timeout;
       tv.tv_usec=0;

       nr=select(fd+1,&readfd,NULL,NULL,&tv);

       if(nr>0)
          break;
       else if(nr==0 || errno!=EINTR)
         {
          if(nr==0)
             errno=ETIMEDOUT;
          return(-1);
         }
      }

#if USE_ZLIB
    if(fdzlib[fd])
      {
       nr=read_uncompressing(fd,buffer,n);
       if(nr==0 && (fdzlib[fd]->doing_head || fdzlib[fd]->doing_body || fdzlib[fd]->doing_tail))
          goto repeat;
      }
    else
#endif
       nr=read(fd,buffer,n);
   }

 return(nr);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a single line of data from a file descriptor.

  char *read_line Returns the modified string or NULL for the end of file.

  int fd The file descriptor.

  char *line The previously allocated line of data.

  This is a replacement for the previous fgets_realloc() function with file descriptors instead of stdio.
  ++++++++++++++++++++++++++++++++++++++*/

char *read_line(int fd,char *line)
{
 int found=0,eof=0;
 int n=0;

 if(!line)
    line=(char*)malloc((BUFSIZE+1));

 do
   {
    int i;

    if(!fdbytes[fd])
       if(read_into_buffer(fd)<=0)
         {eof=1;break;}

    for(i=0;i<fdbytes[fd];i++)
       if(fdbuf[fd][i]=='\n')
         {
          found=1;
          n+=read_from_buffer(fd,&line[n],i+1);
          line[n]=0;
          break;
         }

    if(!found)
      {
       n+=read_from_buffer(fd,&line[n],BUFSIZE);
       line=(char*)realloc((void*)line,n+(BUFSIZE+1));
      }
   }
 while(!found && !eof);

 if(eof)
   {free(line);line=NULL;}

 return(line);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a single line of data from a file descriptor with a timeout.

  char *read_line Returns the modified string or NULL for the end of file or timeout.

  int fd The file descriptor.

  char *line The previously allocated line of data.

  This is a replacement for the previous fgets_realloc() function with file descriptors instead of stdio.
  ++++++++++++++++++++++++++++++++++++++*/

char *read_line_or_timeout(int fd,char *line)
{
 int found=0,eof=0;
 int n=0;

 if(!read_timeout)
    return(read_line(fd,line));

 if(!line)
    line=(char*)malloc((BUFSIZE+1));

 do
   {
    int i;

    if(!fdbytes[fd])
       if(read_into_buffer_or_timeout(fd)<=0)
         {eof=1;break;}

    for(i=0;i<fdbytes[fd];i++)
       if(fdbuf[fd][i]=='\n')
         {
          found=1;
          n+=read_from_buffer(fd,&line[n],i+1);
          line[n]=0;
          break;
         }

    if(!found)
      {
       n+=read_from_buffer(fd,&line[n],BUFSIZE);
       line=(char*)realloc((void*)line,n+(BUFSIZE+1));
      }
   }
 while(!found && !eof);

 if(eof)
   {free(line);line=NULL;}

 return(line);
}


/*++++++++++++++++++++++++++++++++++++++
  Read some data from a file descriptor and buffer it.

  int read_into_buffer Returns the number of bytes read.

  int fd The file descriptor to read from.
  ++++++++++++++++++++++++++++++++++++++*/

static int read_into_buffer(int fd)
{
 int n;

 n=read(fd,fdbuf[fd]+fdbytes[fd],BUFSIZE-fdbytes[fd]);
 fdbytes[fd]+=n;

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  Read some data from a file descriptor and buffer it with a timeout.

  int read_into_buffer_or_timeout Returns the number of bytes read.

  int fd The file descriptor to read from.
  ++++++++++++++++++++++++++++++++++++++*/

static int read_into_buffer_or_timeout(int fd)
{
 int n;
 fd_set readfd;
 struct timeval tv;

 while(1)
   {
    FD_ZERO(&readfd);

    FD_SET(fd,&readfd);

    tv.tv_sec=read_timeout;
    tv.tv_usec=0;

    n=select(fd+1,&readfd,NULL,NULL,&tv);

    if(n>0)
       break;
    else if(n==0 || errno!=EINTR)
      {
       if(n==0)
          errno=ETIMEDOUT;
       return(-1);
      }
   }

 n=read(fd,fdbuf[fd]+fdbytes[fd],BUFSIZE-fdbytes[fd]);
 fdbytes[fd]+=n;

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  Read some data from the buffer.

  int read_from_buffer Returns the number of bytes read.

  int fd The file descriptor buffer to read from.

  char *buffer The buffer to copy the data into.

  int n The maximum number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

static int read_from_buffer(int fd,char *buffer,int n)
{
 if(n>=fdbytes[fd])
   {
    memcpy(buffer,fdbuf[fd],fdbytes[fd]);
    n=fdbytes[fd];
    fdbytes[fd]=0;
   }
 else
   {
    memcpy(buffer,fdbuf[fd],n);
    fdbytes[fd]-=n;
    memmove(fdbuf[fd],fdbuf[fd]+n,fdbytes[fd]);
   }

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  A function to write binary data to a file descriptor instead of write().

  int write_data Returns the number of bytes written.

  int fd The file descriptor.

  const char *buffer The data buffer to write.

  int n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

int write_data(int fd,const char *data,int n)
{
#if USE_ZLIB
 if(fdzlib[fd])
    n=write_compressing(fd,data,n);
 else
#endif
    n=write_all(fd,data,n);

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  A function to write a simple string to a file descriptor like fputs does to a FILE*.

  int write_string Returns the number of bytes written.

  int fd The file descriptor.

  const char *str The string.
  ++++++++++++++++++++++++++++++++++++++*/

int write_string(int fd,const char *str)
{
 int n=strlen(str);

#if USE_ZLIB
 if(fdzlib[fd])
    n=write_compressing(fd,str,n);
 else
#endif
    n=write_all(fd,str,n);

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  A function to write a formatted string to a file descriptor like fprintf does to a FILE*.

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

#if USE_ZLIB
 if(fdzlib[fd])
    n=write_compressing(fd,str,n);
 else
#endif
    n=write_all(fd,str,n);

 free(str);

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  A function to write all of a buffer of data to a file descriptor.

  int write_all Returns the number of bytes written.

  int fd The file descriptor.

  const char *buffer The data buffer to write.

  int n The number of bytes to write.
  ++++++++++++++++++++++++++++++++++++++*/

static int write_all(int fd,const char *data,int n)
{
 int nn=0;

 /* Unroll the first loop to optimise the obvious case. */

 nn=write(fd,data,n);

 if(nn<0 || nn==n)
    return(nn);

 /* Loop around until the data is finished. */

 do
   {
    int m=write(fd,data+nn,n-nn);

    if(m<0)
      {n=m;break;}
    else
       nn+=m;
   }
 while(nn<n);

 return(n);
}


#if USE_ZLIB

/*++++++++++++++++++++++++++++++++++++++
  Initialise the compression buffer used for this file descriptor.

  int init_zlib_buffer Returns zero on success, else return error information.

  int fd The file descriptor to initialise.

  int direction Set to +ve to uncompress and -ve to compress, use +/-1 for zlib & +/-2 for gzip.
  ++++++++++++++++++++++++++++++++++++++*/

int init_zlib_buffer(int fd,int direction)
{
 if(fd==-1)
    return(1);

 if(fdzlib[fd])
    return(2);

 fdzlib[fd]=(zdata*)calloc(1,sizeof(zdata));

 fdzlib[fd]->direction=direction;

 if(direction>0)
   {
    if(direction==1)
       z_errno=inflateInit(&fdzlib[fd]->stream);
    else
       z_errno=inflateInit2(&fdzlib[fd]->stream,-MAX_WBITS);

    if(z_errno!=Z_OK)
      {
       set_zerror(fdzlib[fd]->stream.msg);
       return(3);
      }

    if(fdzlib[fd]->direction==2)
      {
       fdzlib[fd]->crc=crc32(0L,Z_NULL,0);

       fdzlib[fd]->doing_head=1;
       fdzlib[fd]->doing_body=0;
      }
    else
      {
       fdzlib[fd]->doing_head=0;
       fdzlib[fd]->doing_body=1;
      }

    fdzlib[fd]->doing_tail=0;
   }
 else if(direction<0)
   {
    if(direction==-1)
       z_errno=deflateInit(&fdzlib[fd]->stream,Z_DEFAULT_COMPRESSION);
    else
       z_errno=deflateInit2(&fdzlib[fd]->stream,Z_DEFAULT_COMPRESSION,Z_DEFLATED,-MAX_WBITS,MAX_MEM_LEVEL,Z_DEFAULT_STRATEGY);

    if(z_errno!=Z_OK)
      {
       set_zerror(fdzlib[fd]->stream.msg);
       return(3);
      }

    if(direction==-2)
      {
       static char gz_head[10]={0x1f,0x8b,8,0,0,0,0,0,0,0xff};

       write_all(fd,gz_head,10);
       fdzlib[fd]->crc=crc32(0L,Z_NULL,0);
      }
   }
 else
    return(4);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Finalise the compression buffer used for this file descriptor.

  int finish_zlib_buffer Returns zero on success, else return error information.

  int fd The file descriptor to finalise.
  ++++++++++++++++++++++++++++++++++++++*/

int finish_zlib_buffer(int fd)
{
 if(fd==-1)
    return(1);

 if(!fdzlib[fd])
    return(2);

 if(fdzlib[fd]->direction>0)
   {
    z_errno=inflateEnd(&fdzlib[fd]->stream);
    if(z_errno!=Z_OK)
      {
       set_zerror(fdzlib[fd]->stream.msg);
       return(3);
      }
   }
 else if(fdzlib[fd]->direction<0)
   {
    /* Finish deflating the buffer and writing it. */

    do
      {
       fdzlib[fd]->stream.avail_in=0;
       fdzlib[fd]->stream.next_in="";

       fdzlib[fd]->stream.next_out=zbuffer;
       fdzlib[fd]->stream.avail_out=ZBUFFER_SIZE;

       if(fdzlib[fd]->direction==-1)
          z_errno=deflate(&fdzlib[fd]->stream,Z_FINISH);
       else
          z_errno=deflate(&fdzlib[fd]->stream,Z_FINISH);

       if(z_errno==Z_STREAM_END || z_errno==Z_OK)
          if(write_all(fd,zbuffer,ZBUFFER_SIZE-fdzlib[fd]->stream.avail_out)<0)
             break;
      }
    while(fdzlib[fd]->stream.avail_out==0);

    if(fdzlib[fd]->direction==-2)
      {
       int i;
       char gz_tail[8];

       for(i=0;i<4;i++)
         {
          gz_tail[i]=fdzlib[fd]->crc&0xff;
          fdzlib[fd]->crc>>=8;
          gz_tail[i+4]=fdzlib[fd]->stream.total_in&0xff;
          fdzlib[fd]->stream.total_in>>=8;
         }

       write_all(fd,gz_tail,8);
      }

    z_errno=deflateEnd(&fdzlib[fd]->stream);

    if(z_errno!=Z_OK)
      {
       set_zerror(fdzlib[fd]->stream.msg);
       return(3);
      }
   }
 else
    return(4);

 free(fdzlib[fd]);
 fdzlib[fd]=NULL;

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Read some data from the buffer.

  int read_uncompressing_from_buffer Returns the number of bytes read.

  int fd The file descriptor buffer to read from.

  char *buffer The buffer to copy the data into.

  int n The maximum number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

static int read_uncompressing_from_buffer(int fd,char *buffer,int n)
{
 if(fdzlib[fd]->doing_head)
   {
    int nb=parse_gzip_head(fd,fdbuf[fd],fdbytes[fd]);

    if(nb<0)
       return(-1);
    else if(nb==fdbytes[fd])
      {
       fdbytes[fd]=0;
       return(0);
      }
    else if(nb>0)
      {
       fdbytes[fd]-=nb;
       memmove(fdbuf[fd],fdbuf[fd]+nb,fdbytes[fd]);
      }
   }
 else if(fdzlib[fd]->doing_tail)
   {
    int nb=parse_gzip_tail(fd,fdbuf[fd],fdbytes[fd]);

    if(nb<0)
       return(-1);
    else
      {
       fdbytes[fd]=0;
       return(0);
      }
   }

 fdzlib[fd]->stream.next_out=buffer;
 fdzlib[fd]->stream.avail_out=n;

 fdzlib[fd]->stream.next_in=fdbuf[fd];
 fdzlib[fd]->stream.avail_in=fdbytes[fd];

 z_errno=inflate(&fdzlib[fd]->stream,Z_NO_FLUSH);

 if(z_errno!=Z_STREAM_END && z_errno!=Z_OK)
   {
    set_zerror(fdzlib[fd]->stream.msg);
    return(-1);
   }

 if(fdzlib[fd]->direction==2)
   {
    fdzlib[fd]->crc=crc32(fdzlib[fd]->crc,buffer,n-fdzlib[fd]->stream.avail_out);

    if(z_errno==Z_STREAM_END)
      {
       fdzlib[fd]->doing_body=0;
       fdzlib[fd]->doing_tail=1;
      }
   }

 n-=fdzlib[fd]->stream.avail_out;

 memmove(fdbuf[fd],fdbuf[fd]+(fdbytes[fd]-fdzlib[fd]->stream.avail_in),fdzlib[fd]->stream.avail_in);
 fdbytes[fd]=fdzlib[fd]->stream.avail_in;

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  Read data from a file descriptor and uncompress it.

  int read_uncompressing Returns the number of bytes read or 0 for end of file.

  int fd The file descriptor.

  char *buffer The buffer to put the data into.

  int n The maximum number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

static int read_uncompressing(int fd,char *buffer,int n)
{
 int nr;

 nr=read(fd,zbuffer,ZBUFFER_SIZE);

 if(fdzlib[fd]->doing_head && nr==0)
    fdzlib[fd]->doing_head=0;

 if(fdzlib[fd]->doing_body && nr==0)
    fdzlib[fd]->doing_body=0;

 if(nr<=0)
    return(nr);

 if(fdzlib[fd]->doing_head)
   {
    int nb=parse_gzip_head(fd,zbuffer,nr);

    if(nb<0)
       return(-1);
    else if(nb==nr)
       return(0);
    else if(nb>0)
      {
       memmove(zbuffer,zbuffer+nb,nr-nb);
       nr-=nb;
      }

    fdzlib[fd]->doing_head=0;
    fdzlib[fd]->doing_body=1;
   }
 else if(fdzlib[fd]->doing_tail)
   {
    int nb=parse_gzip_tail(fd,zbuffer,nr);

    if(nb<0)
       return(-1);
    else
       return(0);
   }

 fdzlib[fd]->stream.next_out=buffer;
 fdzlib[fd]->stream.avail_out=n;

 fdzlib[fd]->stream.next_in=zbuffer;
 fdzlib[fd]->stream.avail_in=nr;

 z_errno=inflate(&fdzlib[fd]->stream,Z_NO_FLUSH);

 if(z_errno!=Z_STREAM_END && z_errno!=Z_OK)
   {
    set_zerror(fdzlib[fd]->stream.msg);
    return(-1);
   }

 if(fdzlib[fd]->direction==2)
   {
    fdzlib[fd]->crc=crc32(fdzlib[fd]->crc,buffer,n-fdzlib[fd]->stream.avail_out);

    if(z_errno==Z_STREAM_END)
      {
       fdzlib[fd]->doing_body=0;
       fdzlib[fd]->doing_tail=1;
      }
   }

 /* Put the remaining bytes into fdbuf for later. */

 if(fdzlib[fd]->stream.avail_in)
   {
    if(fdzlib[fd]->stream.avail_in>BUFSIZE)
       fdbuf[fd]=(char*)realloc((void*)fdbuf[fd],fdzlib[fd]->stream.avail_in);
    memcpy(fdbuf[fd],fdzlib[fd]->stream.next_in,fdzlib[fd]->stream.avail_in);
    fdbytes[fd]=fdzlib[fd]->stream.avail_in;
   }

 return(n-fdzlib[fd]->stream.avail_out);
}


/*++++++++++++++++++++++++++++++++++++++
  Write data to a file descriptor and compress it.

  int write_compressing Returns the number of bytes written.

  int fd The file descriptor.

  const char *data The data.

  int n The number of bytes of data.
  ++++++++++++++++++++++++++++++++++++++*/

static int write_compressing(int fd,const char *buffer,int n)
{
 fdzlib[fd]->stream.avail_in=n;
 fdzlib[fd]->stream.next_in=(char*)buffer;

 do
   {
    fdzlib[fd]->stream.next_out=zbuffer;
    fdzlib[fd]->stream.avail_out=ZBUFFER_SIZE;

    z_errno=deflate(&fdzlib[fd]->stream,Z_NO_FLUSH);

    if(z_errno==Z_STREAM_END || z_errno==Z_OK)
      {
       if(write_all(fd,zbuffer,ZBUFFER_SIZE-fdzlib[fd]->stream.avail_out)<0)
          return(-1);
      }
    else
      {
       set_zerror(fdzlib[fd]->stream.msg);
       return(-1);
      }
   }
 while(z_errno!=Z_STREAM_END && fdzlib[fd]->stream.avail_in>0);

 if(fdzlib[fd]->direction==-2)
    fdzlib[fd]->crc=crc32(fdzlib[fd]->crc,buffer,n);

 return(n);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse a gzip header.

  int parse_gzip_head Returns the amount of the buffer consumed (not all if head finished).

  int fd The file descriptor we are reading from.

  char *buffer The buffer of new data.

  int n The amount of new data.
  ++++++++++++++++++++++++++++++++++++++*/

static int parse_gzip_head(int fd,char *buffer,int n)
{
 unsigned char *p0=buffer,*p=buffer;

 do
   {
    switch(fdzlib[fd]->doing_head)
      {
      case 1:                   /* magic byte 1 */
       if(*p!=0x1f)
         {set_zerror("not gzip format");return(-1);}
       fdzlib[fd]->doing_head++;n--;p++;
       break;
      case 2:                   /* magic byte 2 */
       if(*p!=0x8b)
         {set_zerror("not gzip format");return(-1);}
       fdzlib[fd]->doing_head++;n--;p++;
       break;
      case 3:                   /* method byte */
       if(*p!=Z_DEFLATED)
         {set_zerror("not gzip format");return(-1);}
       fdzlib[fd]->doing_head++;n--;p++;
       break;
      case 4:                   /* flag byte */
       fdzlib[fd]->head_flag=*p;
       fdzlib[fd]->doing_head++;n--;p++;
       break;
      case 5:                   /* time byte 0 */
      case 6:                   /* time byte 1 */
      case 7:                   /* time byte 2 */
      case 8:                   /* time byte 3 */
      case 9:                   /* xflags */
      case 10:                  /* os flag */
       fdzlib[fd]->doing_head++;n--;p++;
       break;
      case 11:                  /* extra field length byte 1 */
       if(!(fdzlib[fd]->head_flag&0x04))
         {fdzlib[fd]->doing_head=14;break;}
       fdzlib[fd]->head_extra_len=*p;
       fdzlib[fd]->doing_head++;n--;p++;
       break;
      case 12:                  /* extra field length byte 2 */
       fdzlib[fd]->head_extra_len+=*p<<8;
       fdzlib[fd]->doing_head++;n--;p++;
       break;
      case 13:                  /* extra field bytes */
       if(fdzlib[fd]->head_extra_len==0)
          fdzlib[fd]->doing_head++;
       fdzlib[fd]->head_extra_len--;
       n--;p++;
       break;
      case 14:                  /* orig name bytes */
       if(!(fdzlib[fd]->head_flag&0x08))
         {fdzlib[fd]->doing_head++;break;}
       if(*p==0)
          fdzlib[fd]->doing_head++;
       n--;p++;
       break;
      case 15:                  /* comment bytes */
       if(!(fdzlib[fd]->head_flag&0x10))
         {fdzlib[fd]->doing_head++;break;}
       if(*p==0)
          fdzlib[fd]->doing_head++;
       n--;p++;
       break;
      case 16:                  /* head crc byte 1 */
       if(!(fdzlib[fd]->head_flag&0x02))
         {fdzlib[fd]->doing_head=18;break;}
       fdzlib[fd]->doing_head++;n--;p++;
       break;
      case 17:                  /* head crc byte 2 */
       fdzlib[fd]->doing_head++;n--;p++;
       break;
      case 18:                  /* finished */
      case 0:                   /* never happens */
       n=0;
       fdzlib[fd]->doing_head=0;
       fdzlib[fd]->doing_body=1;
       break;
      }
   }
 while(n>0);

 return(p-p0);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse a gzip tail and check the checksum.

  int parse_gzip_tail Returns the amount of the buffer consumed (not all if tail finished).

  int fd The file descriptor we are reading from.

  char *buffer The buffer of new data.

  int n The amount of new data.
  ++++++++++++++++++++++++++++++++++++++*/

static int parse_gzip_tail(int fd,char *buffer,int n)
{
 unsigned char *p0=buffer,*p=buffer;

 do
   {
    switch(fdzlib[fd]->doing_tail)
      {
      case 1:                   /* crc byte 1 */
       fdzlib[fd]->tail_crc=(unsigned long)*p;
       fdzlib[fd]->doing_tail++;n--;p++;
       break;
      case 2:                   /* crc byte 2 */
       fdzlib[fd]->tail_crc+=(unsigned long)*p<<8;
       fdzlib[fd]->doing_tail++;n--;p++;
       break;
      case 3:                   /* crc byte 3 */
       fdzlib[fd]->tail_crc+=(unsigned long)*p<<16;
       fdzlib[fd]->doing_tail++;n--;p++;
       break;
      case 4:                   /* crc byte 4 */
       fdzlib[fd]->tail_crc+=(unsigned long)*p<<24;
       fdzlib[fd]->doing_tail++;n--;p++;
       if(fdzlib[fd]->tail_crc!=fdzlib[fd]->crc)
         {set_zerror("gzip crc error");return(-1);}
       break;
      case 5:                   /* length byte 1 */
       fdzlib[fd]->tail_len=(unsigned long)*p;
       fdzlib[fd]->doing_tail++;n--;p++;
       break;
      case 6:                   /* length byte 2 */
       fdzlib[fd]->tail_len+=(unsigned long)*p<<8;
       fdzlib[fd]->doing_tail++;n--;p++;
       break;
      case 7:                   /* length byte 3 */
       fdzlib[fd]->tail_len+=(unsigned long)*p<<16;
       fdzlib[fd]->doing_tail++;n--;p++;
       break;
      case 8:                   /* length byte 4 */
       fdzlib[fd]->tail_len+=(unsigned long)*p<<24;
       fdzlib[fd]->doing_tail++;n--;p++;
       if(fdzlib[fd]->tail_len!=fdzlib[fd]->stream.total_out)
         {set_zerror("gzip length error");return(-1);}
      /*@fallthrough@*/
      case 0:                   /* never happens */
       n=0;
       fdzlib[fd]->doing_tail=0;
       break;
      }
   }
 while(n>0);

 return(p-p0);
}


/*++++++++++++++++++++++++++++++++++++++
  Set the error status when there is a compression error.

  char *msg The error message.
  ++++++++++++++++++++++++++++++++++++++*/

void set_zerror(char *msg)
{
 errno=ERRNO_USE_Z_ERRNO;

 if(z_strerror)
    free(z_strerror);
 z_strerror=(char*)malloc(8+strlen(msg)+1);

 sprintf(z_strerror,"zlib: %s",msg);
}

#endif /* USE_ZLIB */
