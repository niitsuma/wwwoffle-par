/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/io.h 1.5 2004/01/17 16:22:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Functions for file input and output (public interfaces).
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef IO_H
#define IO_H    /*+ To stop multiple inclusions. +*/

/*+ The size of the buffer to use when reading from the cache or a socket. +*/
#define READ_BUFFER_SIZE 4096


/* In io.c */

void init_io(int fd);
void reinit_io(int fd);

void configure_io_read(int fd,int timeout,int zlib,int chunked);
void configure_io_write(int fd,int timeout,int zlib,int chunked);

int read_data(int fd,/*@out@*/ char *buffer,int n);

char /*@null@*/ *read_line(int fd,/*@out@*/ /*@returned@*/ /*@null@*/ char *line);

int /*@alt void@*/ write_data(int fd,const char *data,int n);

int /*@alt void@*/ write_string(int fd,const char *str);

#ifdef __GNUC__
int /*@alt void@*/ write_formatted(int fd,const char *fmt,...) /*@printflike@*/ __attribute__ ((format (printf,2,3)));
#else
int /*@alt void@*/ write_formatted(int fd,const char *fmt,...) /*@printflike@*/;
#endif

void tell_io(int fd,/*@out@*/ /*@null@*/ unsigned long* r,/*@out@*/ /*@null@*/ unsigned long *w);

#define finish_io(fd) finish_tell_io(fd,NULL,NULL);

void finish_tell_io(int fd,/*@out@*/ /*@null@*/ unsigned long* r,/*@out@*/ /*@null@*/ unsigned long *w);

#endif /* IO_H */
