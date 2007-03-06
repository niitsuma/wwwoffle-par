/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/io.h 1.14 2006/01/20 19:01:29 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Functions for file input and output (public interfaces).
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2004,2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef IO_H
#define IO_H    /*+ To stop multiple inclusions. +*/


#include <unistd.h>


/*+ The size of the buffer to use for the IO internals (also used elsewhere as general buffer size). +*/
#define IO_BUFFER_SIZE 4096

/*+ The buffer size for reading lines. +*/
#define LINE_BUFFER_SIZE 256

/*+ To reduce the risk of stack overflow, dynamically sized buffer arrays should not be made larger than this limit. +*/
#define MAXDYNBUFSIZE (16*1024)

/*+ Don't bother compressing content smaller than this +*/
#define MINCOMPRSIZE 256

/* Used as a value for the content length in case it is undefined */
#define CUNDEF (~0L)

/* In io.c */

void init_io(int fd);
void reinit_io(int fd);

void configure_io_timeout(int fd,int timeout_r,int timeout_w);
inline static void configure_io_timeout_rw(int fd,int timeout) {configure_io_timeout(fd,timeout,timeout);}
void configure_io_content_length(int fd,unsigned long content_length);

#if USE_ZLIB
int configure_io_zlib(int fd,int zlib_r,int zlib_w);
#endif

int configure_io_chunked(int fd,int chunked_r,int chunked_w);

#if USE_GNUTLS
int configure_io_gnutls(int fd,const char *host,int type);
#endif

ssize_t read_data(int fd,/*@out@*/ char *buffer,size_t n);

char /*@null@*/ *read_line(int fd,/*@out@*/ /*@returned@*/ /*@null@*/ char *line);

ssize_t /*@alt void@*/ write_data(int fd,const char *data,size_t n);
ssize_t /*@alt void@*/ write_buffer_data(int fd,const char *data,size_t n);

ssize_t /*@alt void@*/ write_string(int fd,const char *str);

#ifdef __GNUC__
ssize_t /*@alt void@*/ write_formatted(int fd,const char *fmt,...) /*@printflike@*/ __attribute__ ((format (printf,2,3)));
#else
ssize_t /*@alt void@*/ write_formatted(int fd,const char *fmt,...) /*@printflike@*/;
#endif

int tell_io(int fd,/*@out@*/ /*@null@*/ unsigned long* r,/*@out@*/ /*@null@*/ unsigned long *w);

#define finish_io(fd) finish_tell_io(fd,NULL,NULL)

int finish_tell_io(int fd,/*@out@*/ /*@null@*/ unsigned long* r,/*@out@*/ /*@null@*/ unsigned long *w);

ssize_t read_all_or_timeout(int fd,char *buf,size_t n,unsigned timeout);
#define read_all(fd,buf,n) read_all_or_timeout(fd,buf,n,0)
ssize_t write_all(int fd,const char *data,size_t n);

#endif /* IO_H */
