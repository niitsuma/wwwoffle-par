/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/ftp.c 1.84 2006/01/08 10:27:21 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Functions for getting URLs using FTP.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "headbody.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"


/*+ Set this to 1 to see the full dialog with the FTP server. +*/
#define DEBUG_FTP 0

/*+ Set to the name of the proxy if there is one. +*/
static URL /*@null@*/ /*@only@*/ *proxyUrl=NULL;
/* Information used to connect via a SOCKS proxy. */
static char /*@null@*/ /*@observer@*/ *sproxy=NULL;
static int socksremotedns=0;
static char rhost_ipstr[ipaddr_strlen];

/*+ The file descriptor of the socket +*/
static int server_ctrl=-1,      /*+ for the control connection to the server. +*/
           server_data=-1;      /*+ for the data connection to the server. +*/

/*+ A header to contain the reply. +*/
static /*@only@*/ /*@null@*/ Header *bufferhead=NULL;

/*+ A buffer to contain the reply body. +*/
static char /*@only@*/ /*@null@*/ *buffer=NULL;
/*+ A buffer to contain the reply tail. +*/
static char /*@only@*/ /*@null@*/ *buffertail=NULL;

/*+ The number of characters in the buffer +*/
static int nbuffer=0,           /*+ in total for the body part. +*/
           nread=0,             /*+ that have been read for the body. +*/
           nbuffertail=0,       /*+ in total for the tail . +*/
           nreadtail=0;         /*+ that have been read from the tail. +*/

static /*@null@*/ char *htmlise_dir_entry(char *line);


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using FTP.

  char *FTP_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *FTP_Open(URL *Url)
{
 char *msg=NULL;
 char *proxy=NULL;
 char *server_host=NULL;
 int server_port=0;
 char *socks_host=NULL;
 int socks_port=0;

 /* Sort out the host. */

 sproxy=NULL;
 socksremotedns=0;
 if(!IsLocalNetHost(Url->host)) {
   proxy=ConfigStringURL(Proxies,Url);
   sproxy=ConfigStringURL(SocksProxy,Url);
   socksremotedns=ConfigBooleanURL(SocksRemoteDNS,Url);
 }

 if(proxyUrl) {
   FreeURL(proxyUrl);
   proxyUrl=NULL;
 }
 if(proxy) {
   proxyUrl=CreateURL("http",proxy,"/",NULL,NULL,NULL);
   server_host=proxyUrl->host;
   server_port=proxyUrl->portnum;
 }
 else {
   server_host=Url->host;
   server_port=Url->portnum;
 }

 if(sproxy)
   SETSOCKSHOSTPORT(sproxy,server_host,server_port,socks_host,socks_port);

 /* Open the connection. */

 server_ctrl=OpenClientSocket(server_host,server_port,socks_host,socks_port,socksremotedns,rhost_ipstr);

 if(server_ctrl==-1)
    msg=GetPrintMessage(Warning,"Cannot open the FTP control connection to %s port %d; [%!s].",server_host,server_port);
 else
   {
    init_io(server_ctrl);
    configure_io_timeout_rw(server_ctrl,ConfigInteger(SocketTimeout));
   }

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the server to request the URL.

  char *FTP_Request Returns NULL on success, a useful message on error.

  URL *Url The URL to get.

  Header *request_head The head of the HTTP request for the URL.

  Body *request_body The body of the HTTP request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

char *FTP_Request(URL *Url,Header *request_head,Body *request_body)
{
 char *msg=NULL,*str=NULL;
 char *path,*file=NULL;
 char *host,*shost=NULL,*mimetype="text/html";
 char *msg_reply=NULL; size_t msg_reply_len=0;
 char sizebuf[MAX_INT_STR+1];
 int l,port,sport=0;
 time_t modtime=0;
 char *user,*pass;

 /* Initial setting up. */

 sizebuf[0]=0;

 buffer=NULL;
 bufferhead=NULL;
 buffertail=NULL;

 /* Take a simple route if it is proxied. */

 if(proxyUrl)
   {
    char *head; size_t headlen;

    /* Make the request OK for a proxy. */

    MakeRequestProxyAuthorised(proxyUrl,request_head);

    /* Send the request. */

    head=HeaderString(request_head,&headlen);

    PrintMessage(ExtraDebug,"Outgoing Request Head (to proxy)\n%s",head);

    if(write_data(server_ctrl,head,headlen)<0)
       msg=GetPrintMessage(Warning,"Failed to write head to remote FTP proxy; [%!s].");
    if(request_body)
       if(write_data(server_ctrl,request_body->content,request_body->length)<0)
          msg=GetPrintMessage(Warning,"Failed to write body to remote FTP proxy; [%!s].");

    free(head);

    return(msg);
   }

 /* Else Sort out the path. */

 path=URLDecodeGeneric(Url->path);
 {
   char *p=strchrnul(path,0)-1;
   if(*p=='/')
     {
       *p=0;
     }
   else
     while(--p>=path)
       if(*p=='/')
         {
          *p=0;
          file=p+1;
          break;
         }
 }

 if(Url->user)
   {
    user=Url->user;
    pass=Url->pass;
   }
 else
   {
    user=ConfigStringURL(FTPAuthUser,Url);
    if(!user)
       user=ConfigString(FTPUserName);

    pass=ConfigStringURL(FTPAuthPass,Url);
    if(!pass)
       pass=ConfigString(FTPPassWord);
   }

 /* send all the RFC959 commands. */

 server_data=-1;

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: connected; got: %s",str);

    if(!file && !*path && str && isdigit(str[0]) && isdigit(str[1]) && isdigit(str[2]) && str[3]=='-')
      {
       strn_append(&msg_reply,&msg_reply_len,str+4);
      }
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(!str)
   {
    msg=GetPrintMessage(Warning,"No reply from FTP server when connected; timed out?");
    goto free_return;
   }

 if(atoi(str)!=220)
   {
    chomp_str(str);
    msg=GetPrintMessage(Warning,"Got '%s' message when connected to FTP server.",str);
    goto free_return;
   }

 /* Login */

 if(write_formatted(server_ctrl,"USER %s\r\n",user)<0)
   {
    msg=GetPrintMessage(Warning,"Failed to write 'USER' command to remote FTP host; [%!s].");
    goto free_return;
   }

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: sent 'USER %s'; got: %s",user,str);
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(!str)
   {
    msg=GetPrintMessage(Warning,"No reply from FTP server to 'USER' command; timed out?");
    goto free_return;
   }

 if(atoi(str)!=230 && atoi(str)!=331)
   {
    chomp_str(str);
    msg=GetPrintMessage(Warning,"Got '%s' message after sending 'USER' command to FTP server.",str);
    goto free_return;
   }

 if(atoi(str)==331)
   {
    if(write_formatted(server_ctrl,"PASS %s\r\n",pass)<0)
      {
       msg=GetPrintMessage(Warning,"Failed to write 'PASS' command to remote FTP host; [%!s].");
       goto free_return;
      }

    do
      {
       str=read_line(server_ctrl,str);
       if(str)
          PrintMessage(ExtraDebug,"FTP: sent 'PASS %s'; got: %s",pass,str);

       if(!file && !*path && str && isdigit(str[0]) && isdigit(str[1]) && isdigit(str[2]) && str[3]=='-')
         {
          strn_append(&msg_reply,&msg_reply_len,str+4);
         }
      }
    while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

    if(!str)
      {
       msg=GetPrintMessage(Warning,"No reply from FTP server to 'PASS' command; timed out?");
       goto free_return;
      }

    if(atoi(str)==530)
      {
       CreateHeader("HTTP/1.0 401 FTP invalid password",0,&bufferhead);

       goto near_end;
      }

    if(atoi(str)!=202 && atoi(str)!=230)
      {
       chomp_str(str);
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'PASS' command to FTP server.",str);
       goto free_return;
      }
   }

 /* Change directory */

 if(write_formatted(server_ctrl,"CWD %s\r\n",*path?path:"/")<0)
   {
    msg=GetPrintMessage(Warning,"Failed to write 'CWD' command to remote FTP host; [%!s].");
    goto free_return;
   }

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: sent 'CWD %s' got: %s",*path?path:"/",str);

    if(!file && str && isdigit(str[0]) && isdigit(str[1]) && isdigit(str[2]) && str[3]=='-')
      {
       strn_append(&msg_reply,&msg_reply_len,str+4);
      }
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(!str)
   {
    msg=GetPrintMessage(Warning,"No reply from FTP server to 'CWD' command; timed out?");
    goto free_return;
   }

 if(atoi(str)!=250)
   {
    chomp_str(str);
    msg=GetPrintMessage(Warning,"Got '%s' message after sending 'CWD' command to FTP server.",str);
    goto free_return;
   }

 /* Change directory again to see if file is a dir. */

 if(file)
   {
    if(write_formatted(server_ctrl,"CWD %s\r\n",file)<0)
      {
       msg=GetPrintMessage(Warning,"Failed to write 'CWD' command to remote FTP host; [%!s].");
       goto free_return;
      }

    do
      {
       str=read_line(server_ctrl,str);
       if(str)
          PrintMessage(ExtraDebug,"FTP: sent 'CWD %s' got: %s",file,str);
      }
    while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

    if(!str)
      {
       msg=GetPrintMessage(Warning,"No reply from FTP server to 'CWD' command; timed out?");
       goto free_return;
      }

    if(atoi(str)!=250 && atoi(str)!=501 && atoi(str)!=530 && atoi(str)!=550)
      {
       chomp_str(str);
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'CWD' command to FTP server.",str);
       goto free_return;
      }

    if(atoi(str)==250)
      {
       char loc[strlen(Url->file)+2];

       {char *p=stpcpy(loc,Url->file); *p++='/'; *p=0;}

       CreateHeader("HTTP/1.0 302 FTP is a directory",0,&bufferhead);
       AddToHeader(bufferhead,"Location",loc);
       AddToHeader(bufferhead,"Content-Type",mimetype);

       buffer=HTMLMessageString("Redirect",
                                "location",loc,
                                NULL);

       goto near_end;
      }
   }

 /* Set mode to binary or ASCII */

 if(write_formatted(server_ctrl,"TYPE %c\r\n",file?'I':'A')<0)
   {
    msg=GetPrintMessage(Warning,"Failed to write 'TYPE' command to remote FTP host; [%!s].");
    goto free_return;
   }

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: sent 'TYPE %c'; got: %s",file?'I':'A',str);
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(!str)
   {
    msg=GetPrintMessage(Warning,"No reply from FTP server to 'TYPE' command; timed out?");
    goto free_return;
   }

 if(atoi(str)!=200)
   {
    chomp_str(str);
    msg=GetPrintMessage(Warning,"Got '%s' message after sending 'TYPE' command to FTP server.",str);
    goto free_return;
   }

 if(!strcmp(request_head->method,"GET"))
   {
    /* Try and get the size and modification time. */

    if(file)
      {
       if(write_formatted(server_ctrl,"SIZE %s\r\n",file)<0)
         {
          msg=GetPrintMessage(Warning,"Failed to write 'SIZE' command to remote FTP host; [%!s].");
	  goto free_return;
         }

       do
         {
          str=read_line(server_ctrl,str);
          if(str)
             PrintMessage(ExtraDebug,"FTP: sent 'SIZE %s'; got: %s",file,str);
         }
       while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

       if(!str)
         {
          msg=GetPrintMessage(Warning,"No reply from FTP server to 'SIZE' command; timed out?");
	  goto free_return;
         }

       if(atoi(str)==213)
          if(str[4])
             sprintf(sizebuf,"%ld",atol(str+4));

       if(write_formatted(server_ctrl,"MDTM %s\r\n",file)<0)
         {
          msg=GetPrintMessage(Warning,"Failed to write 'MDTM' command to remote FTP host; [%!s].");
	  goto free_return;
         }

       do
         {
          str=read_line(server_ctrl,str);
          if(str)
             PrintMessage(ExtraDebug,"FTP: sent 'MDTM %s'; got: %s",file,str);
         }
       while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

       if(!str)
         {
          msg=GetPrintMessage(Warning,"No reply from FTP server to 'MDTM' command; timed out?");
	  goto free_return;
         }

       if(atoi(str)==213)
         {
          int year,mon,mday,hour,min,sec;

          if(sscanf(str,"%*d %4d%2d%2d%2d%2d%2d",&year,&mon,&mday,&hour,&min,&sec)==6)
            {
             struct tm modtm;

             memset(&modtm,0,sizeof(modtm));
             modtm.tm_year=year-1900;
             modtm.tm_mon=mon-1;
             modtm.tm_mday=mday;
             modtm.tm_hour=hour;
             modtm.tm_min=min;
             modtm.tm_sec=sec;

	     /* To use mktime() properly, we have to set the TZ environment variable first.
		Using timegm is less portable, but much more convenient. */
             modtime=timegm(&modtm);
            }
         }
      }
   }

 /* Create the data connection. */

 if(write_string(server_ctrl,"EPSV\r\n")<0)
   {
    msg=GetPrintMessage(Warning,"Failed to write 'EPSV' command to remote FTP host; [%!s].");
    goto free_return;
   }

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: sent 'EPSV'; got: %s",str);
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(!str)
   {
    msg=GetPrintMessage(Warning,"No reply from FTP server to 'EPSV' command; timed out?");
    goto free_return;
   }

 if(atoi(str)==229)
   {
    host=strchr(str,'(');
    if(!host || sscanf(host+1,"%*c%*c%*c%d%*c",&port)!=1)
      {
       chomp_str(str);
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'EPSV' command, cannot parse.",str);
       goto free_return;
      }
    host= ((*rhost_ipstr)?rhost_ipstr:Url->host);
   }
 else if(str[0]!='5' || str[1]!='0' || !isdigit(str[2]))
   {
    chomp_str(str);
    msg=GetPrintMessage(Warning,"Got '%s' message after sending 'EPSV' command",str);
    goto free_return;
   }
 else
   {
    int port_l,port_h;

    /* Let's try PASV instead then */

    if(write_string(server_ctrl,"PASV\r\n")<0)
      {
       msg=GetPrintMessage(Warning,"Failed to write 'PASV' command to remote FTP host; [%!s].");
       goto free_return;
      }

    do
      {
       str=read_line(server_ctrl,str);
       if(str)
          PrintMessage(ExtraDebug,"FTP: sent 'PASV'; got: %s",str);
      }
    while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

    if(!str)
      {
       msg=GetPrintMessage(Warning,"No reply from FTP server to 'PASV' command; timed out?");
       goto free_return;
      }

    if(atoi(str)!=227)
      {
       chomp_str(str);
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'PASV' command",str);
       goto free_return;
      }

    if((host=strchr(str,',')))
      {
       while(--host>=str && isdigit(*host));
       ++host;
      }

    if(!host || sscanf(host,"%*d,%*d,%*d,%*d%n,%d,%d",&l,&port_h,&port_l)!=2)
      {
       chomp_str(str);
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'PASV' command, cannot parse.",str);
       goto free_return;
      }

    port=port_l+256*port_h;

    host[l]=0;
    while(--l>=0)
       if(host[l]==',')
          host[l]='.';
   }

 if(sproxy)
   SETSOCKSHOSTPORT(sproxy,host,port,shost,sport);

 server_data=OpenClientSocket(host,port,shost,sport,socksremotedns,NULL);

 if(server_data==-1)
   {
    msg=GetPrintMessage(Warning,"Cannot open the FTP data connection [%!s].");
    goto free_return;
   }
 else
   {
    init_io(server_data);
    configure_io_timeout_rw(server_data,ConfigInteger(SocketTimeout));
   }

 /* Make the request */

 if(!strcmp(request_head->method,"GET"))
   {
    char *command;

    if(file)
      {
       command="RETR";

       if(write_formatted(server_ctrl,"RETR %s\r\n",file)<0)
         {
          msg=GetPrintMessage(Warning,"Failed to write 'RETR' command to remote FTP host; [%!s].");
	  goto free_return;
         }

       do
         {
          str=read_line(server_ctrl,str);
          if(str)
             PrintMessage(ExtraDebug,"FTP: sent 'RETR %s'; got: %s",file,str);
         }
       while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

       mimetype=WhatMIMEType(file);
      }
    else
      {
       int i;

       for(i=0;i<2;i++)
         {
          if(i==0)
             command="LIST -a";
          else
             command="LIST";

          if(write_formatted(server_ctrl,"%s\r\n",command)<0)
            {
             msg=GetPrintMessage(Warning,"Failed to write '%s' command to remote FTP host; [%!s].",command);
             goto free_return;
            }

          do
            {
             str=read_line(server_ctrl,str);
             if(str)
                PrintMessage(ExtraDebug,"FTP: sent '%s'; got: %s",command,str);
            }
          while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

          if(i==0 && str && (atoi(str)!=150 && atoi(str)!=125))
            {
             chomp_str(str);
             PrintMessage(Warning,"Got '%s' message after sending '%s' command to FTP server.",str,command);
            }
          else
             break;
         }
      }

    if(str && (atoi(str)==150 || atoi(str)==125))
      {
       char *p=str;
       while(*p!='\r' && *p!='\n')
          p++;
       *p=0;
      }
    else
      {
       if(str)
         {
          chomp_str(str);
          msg=GetPrintMessage(Warning,"Got '%s' message after sending '%s' command to FTP server.",str,command);
         }
       else
          msg=GetPrintMessage(Warning,"No reply from FTP server to '%s' command; timed out?",command);
       goto free_return;
      }
   }
 else if(file) /* PUT */
   {
    if(file)
      {
       if(write_formatted(server_ctrl,"STOR %s\r\n",file)<0)
         {
          msg=GetPrintMessage(Warning,"Failed to write 'STOR' command to remote FTP host; [%!s].");
          goto free_return;
         }

       do
         {
          str=read_line(server_ctrl,str);
          if(str)
             PrintMessage(ExtraDebug,"FTP: sent 'STOR %s'; got: %s",file,str);
         }
       while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));
      }
    else
      {
       msg=GetPrintMessage(Warning,"Cannot use the PUT method on a directory name");
       goto free_return;
      }

    if(str && (atoi(str)==150 || atoi(str)==125))
      {
       char *p=str;
       while(*p!='\r' && *p!='\n')
          p++;
       *p=0;
      }
    else
      {
       if(str)
         {
          chomp_str(str);
          msg=GetPrintMessage(Warning,"Got '%s' message after sending 'STOR' command to FTP server.",str);
         }
       else
          msg=GetPrintMessage(Warning,"No reply from FTP server to 'STOR' command; timed out?");
       goto free_return;
      }

    write_data(server_data,request_body->content,request_body->length);
   }

 /* Prepare the HTTP header. */

 CreateHeader("HTTP/1.0 200 FTP Proxy OK",0,&bufferhead);
 AddToHeader(bufferhead,"Content-Type",mimetype);

 if(!strcmp(request_head->method,"GET"))
   {
    if(*sizebuf)
       AddToHeader(bufferhead,"Content-Length",sizebuf);

    if(modtime!=0)
      {
       char *ims;

       AddToHeader(bufferhead,"Last-Modified",RFC822Date(modtime,1));

       if((ims=GetHeader(request_head,"If-Modified-Since")))
         {
          time_t lastmodtime=DateToTimeT(ims);

          if(modtime<=lastmodtime)
            {
             bufferhead->status=304;
             ReplaceOrAddInHeader(bufferhead,"Content-Length","0");

             goto near_end;
            }
         }
      }

    if(!file)
      {
       if(msg_reply)
         {
          char *old_msg_reply=msg_reply;
          msg_reply=HTMLString(msg_reply,0);
          free(old_msg_reply);
         }

       buffer=HTMLMessageString("FTPDir-Head",
                                "url",Url->name,
                                "base",Url->file,
                                "message",msg_reply,
                                NULL);

       buffertail=HTMLMessageString("FTPDir-Tail",
                                    NULL);
      }
   }
 else
    buffer=HTMLMessageString("FTPPut",
                             "url",Url->name,
                             NULL);

near_end:

 nbuffer=buffer?strlen(buffer):0; nread=0;
 nbuffertail=buffertail?strlen(buffertail):0; nreadtail=0;

free_return:
 free(path);
 if(str) free(str);
 if(msg_reply) free(msg_reply);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a line from the header of the reply for the URL.

  int FTP_ReadHead Returns the server socket file descriptor.

  Header **reply_head Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

int FTP_ReadHead(Header **reply_head)
{
 /* Take a simple route if it is proxied. */

 if(proxyUrl)
   {
    ParseReply(server_ctrl,reply_head);

    return(server_ctrl);
   }

 /* Else send the header. */

 *reply_head=bufferhead;

 return(server_data);
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  ssize_t FTP_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  size_t n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t FTP_ReadBody(char *s,size_t n)
{
 ssize_t m=0;

 /* Take a simple route if it is proxied. */

 if(proxyUrl)
    return(read_data(server_ctrl,s,n));

 /* Else send the data then the tail. */

 if(server_data==-1)            /* Redirection */
   {
    for(;nread<nbuffer && m<n;nread++,m++)
       s[m]=buffer[nread];

    for(;nreadtail<nbuffertail && m<n;nreadtail++,m++)
       s[m]=buffertail[nreadtail];
   }
 else if(buffertail)
   {
     if(buffer)  /* Middle of dir entry */
       {
	 for(;nread<nbuffer && m<n;nread++,m++)
	   s[m]=buffer[nread];

	 if(nread==nbuffer)
	   {
	     buffer=htmlise_dir_entry(buffer);
	     if(buffer)
	       {nbuffer=strlen(buffer); nread=0;}
	   }
       }

     if(!buffer) /* End of dir entry. */
       {
	 for(;nreadtail<nbuffertail && m<n;nreadtail++,m++)
	   s[m]=buffertail[nreadtail];
       }
   }
 else if(!buffer) /* File not dir entry. */
   m=read_data(server_data,s,n);
 else /* if(buffer && !buffertail) */ /* Done a PUT */
   {
     for(;nread<nbuffer && m<n;nread++,m++)
       s[m]=buffer[nread];
   }

 return(m);
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using FTP.

  int FTP_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int FTP_Close(void)
{
 int err=0;
 char *str=NULL;
 unsigned long r1=0,w1=0,r2=0,w2=0;

 /* Take a simple route if it is proxied. */

 if(proxyUrl)
   {
    if(finish_tell_io(server_ctrl,&r1,&w1)<0)
      PrintMessage(Inform,"Error finishing IO on socket with remote host [%!s].");

    PrintMessage(Inform,"Server bytes; %lu Read, %lu Written.",r1,w1); /* Used in audit-usage.pl */

    return(CloseSocket(server_ctrl));
   }

 /* Else say goodbye and close all of the sockets, */

 if(server_data!=-1)
   {
    if(finish_tell_io(server_data,&r2,&w2)<0)
      PrintMessage(Inform,"Error finishing IO on socket with remote host [%!s].");
    CloseSocket(server_data);
   }

 write_string(server_ctrl,"QUIT\r\n");

 do
   {
    str=read_line(server_ctrl,str);
    if(str)
       PrintMessage(ExtraDebug,"FTP: sent 'QUIT'; got: %s",str);
   }
 while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

 if(finish_tell_io(server_ctrl,&r1,&w1)<0)
   PrintMessage(Inform,"Error finishing IO on socket with remote host [%!s].");
 err=CloseSocket(server_ctrl);

 PrintMessage(Inform,"Server bytes; %lu Read, %lu Written.",r1+r2,w1+w2); /* Used in audit-usage.pl */

 if(str)
    free(str);

 if(buffer)
    free(buffer);
 if(buffertail)
    free(buffertail);

 if(proxyUrl) {
   FreeURL(proxyUrl);
   proxyUrl=NULL;
 }
 sproxy=NULL;

 return(err);
}


struct pointer_pair {char *beg,*end;};
#define MaxFields 12

inline static int split_line(char *line,struct pointer_pair *p)
{
  int i;
  char *q;

  for(q=line,i=0;*q && i<MaxFields;++i)
    {
      while(isspace(*q)) if(!*++q) goto done;
      p[i].beg=q;
      while(*++q && !isspace(*q));
      p[i].end=q;
    }

 done:
  return i;
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a line from the ftp server dir listing into a pretty listing.

  char *htmlise_dir_entry Returns the next line.

  char *line The previous line.
  ++++++++++++++++++++++++++++++++++++++*/

static char *htmlise_dir_entry(char *line)
{
 int i,isdir=0,islink=0;
 struct pointer_pair p[MaxFields];
 int file=0,link=0;

 if((line=read_line(server_data,line))) {

   i=split_line(line,p);

   if(i>=8 && (*p[0].beg=='-' || *p[0].beg=='d' || *p[0].beg=='l')) /* A UNIX 'ls -l' listing. */
     {
       if(i==8 || i==9)
	 {link=file=i-1;}
       else if(i==10 || i==11) {
	 int ia=i-2; char *pa=p[ia].beg;
	 if(*pa=='-' && *(pa+1)=='>' && pa+2==p[ia].end)
	   {file=i-3; link=i-1;}
       }

       if(*p[0].beg=='d')
	 isdir=1;
       else if(*p[0].beg=='l')
	 islink=1;
     }

   if(file)
     {
       char *pfile=p[file].beg,*endf=p[file].end, *plink=p[link].beg,*endl=p[link].end;
       char *hline,*fileurlenc,*linkurlenc=NULL;
       size_t linelen;

       hline=HTMLString(line,0);
       i=split_line(hline,p);

       /* Get the filename and link URLs. */

       fileurlenc=STRDUP3(pfile,endf,URLEncodePath);

       linelen=strlen(hline)+strlen(fileurlenc)+strlitlen("<a href=\".//\"></a>");

       if(islink && link!=file) {
	 linkurlenc=STRDUP3(plink,endl,URLEncodePath);
	 linelen+=strlen(linkurlenc)+strlitlen("<a href=\"./\"></a>");
       }

       /* The buffer allocated by read_line() to hold line is at least LINE_BUFFER_SIZE+1 bytes large. */
       if(linelen>LINE_BUFFER_SIZE) {
	 free(line);
	 line=(char*)malloc(linelen+1);
       }

       /* Create the line. */

       {
	 char *q;
	 q=mempcpy(line,hline,p[file].beg-hline);
	 q=stpcpy(q,"<a href=\"");
	 if(!(isdir && *fileurlenc=='.' && (!*(fileurlenc+1) || *(fileurlenc+1)=='/' || (*(fileurlenc+1)=='.' && (!*(fileurlenc+2) || *(fileurlenc+2)=='/')))))
	   q=stpcpy(q,"./");
	 q=stpcpy(q,fileurlenc);
	 if(isdir && *(q-1)!='/')
	   *q++='/';
	 q=stpcpy(q,"\">");
	 q=mempcpy(q,p[file].beg,p[file].end-p[file].beg);
	 q=stpcpy(q,"</a>");

	 if(islink && link!=file) {
	   q=mempcpy(q,p[file].end,p[link].beg-p[file].end);
	   q=stpcpy(q,"<a href=\"");
	   if(!(*linkurlenc=='/' || (*linkurlenc=='.' && (*(linkurlenc+1)=='/' || (*(linkurlenc+1)=='.' && *(linkurlenc+2)=='/')))))
	     q=stpcpy(q,"./");
	   q=stpcpy(q,linkurlenc);
	   q=stpcpy(q,"\">");
	   q=mempcpy(q,p[link].beg,p[link].end-p[link].beg);
	   q=stpcpy(q,"</a>");
	   q=stpcpy(q,p[link].end);
	 }
	 else
	   q=stpcpy(q,p[file].end);

       }

       free(hline);
       free(fileurlenc);
       if(linkurlenc) free(linkurlenc);
     }
 }

 return(line);
}
