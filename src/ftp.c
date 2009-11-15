/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/ftp.c 1.84 2006/01/08 10:27:21 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Functions for getting URLs using FTP.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007,2008 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

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

static /*@null@*/ char *htmlise_dir_entry(int server_data,char *line,hint_t *hint);


/*++++++++++++++++++++++++++++++++++++++
  Open a connection to get a URL using FTP.

  char *FTP_Open Returns NULL on success, a useful message on error.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

char *FTP_Open(Connection *connection,URL *Url)
{
 char *msg=NULL;
 char *proxy=NULL,*sproxy=NULL;
 int socksremotedns=0,server_ctrl=-1;
 URL *proxyUrl=NULL;
 socksdata_t *socksdata=NULL;

 /* Sort out the host. */

 if(!IsLocalNetHost(Url->host)) {
   proxy=ConfigStringURL(Proxies,Url);
   sproxy=ConfigStringURL(SocksProxy,Url);
   socksremotedns=ConfigBooleanURL(SocksRemoteDNS,Url);
 }

 if(proxy)
   Url=proxyUrl=CreateURL("http",proxy,"/",NULL,NULL,NULL);

 /* Open the connection. */

 if((sproxy && !(socksdata= MakeSocksData(sproxy,socksremotedns,NULL))) ||
    (server_ctrl=OpenUrlSocket(Url,socksdata))==-1)
   {
    msg=GetPrintMessage(Warning,"Cannot open the FTP control connection to %s port %d; [%!s].",Url->host,Url->portnum);
    if(proxyUrl) FreeURL(proxyUrl);
    if(socksdata) free(socksdata);
   }
 else
   {
    init_io(server_ctrl);
    configure_io_timeout_rw(server_ctrl,ConfigInteger(SocketTimeout));
    connection->ctrlfd=server_ctrl;
    connection->fd= (proxyUrl? server_ctrl: -1);
    connection->proxyUrl=proxyUrl;
    connection->socksproxy=socksdata;
    connection->loggedin=0;
    connection->tryepsv=ConfigBooleanURL(FTPTryEPSV,Url);
    connection->expectmore=0;
#if 0
    /* We can also rely on ConnectionOpen to initialise this. */
    connection->bufferhead=NULL;
    connection->buffer=NULL;
    connection->buffertail=NULL;
#endif
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

char *FTP_Request(Connection *connection,URL *Url,Header *request_head,Body *request_body)
{
 char *msg=NULL,*str=NULL;
 char *path,*file=NULL;
 char *host,*mimetype="text/html";
 char *msg_reply=NULL; size_t msg_reply_len=0;
 char sizebuf[MAX_INT_STR+1];
 int rcode,l,port;
 time_t modtime=0;
 char *user,*pass;
 URL *proxyUrl=connection->proxyUrl;
 int server_ctrl=connection->ctrlfd,server_data=-1;
 Header *bufferhead=NULL;
 char *buffer=NULL;
 char *buffertail=NULL;
 hint_t *hint=NULL;

 /* Initial setting up. */

 sizebuf[0]=0;

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
    else if(request_body && write_data(server_ctrl,request_body->content,request_body->length)<0)
       msg=GetPrintMessage(Warning,"Failed to write body to remote FTP proxy; [%!s].");

    free(head);

    return(msg);
   }

 /* Else Sort out the path. */

 path=URLDecodeGeneric(Url->path);
 if(*path) {
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

 connection->fd=-1;

 if(connection->loggedin) goto logged_in;

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

 rcode=atoi(str);
 if(rcode!=220)
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

 rcode=atoi(str);
 if(rcode!=230 && rcode!=331)
   {
    chomp_str(str);
    msg=GetPrintMessage(Warning,"Got '%s' message after sending 'USER' command to FTP server.",str);
    goto free_return;
   }

 if(rcode==331)
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

    rcode=atoi(str);
    if(rcode==530)
      {
       CreateHeader("HTTP/1.0 401 FTP invalid password",0,&bufferhead);
       AddToHeader(bufferhead,"Content-Type",mimetype);
       AddToHeader(bufferhead,"Content-Length","0");

       goto near_end;
      }

    if(rcode!=202 && rcode!=230)
      {
       chomp_str(str);
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'PASS' command to FTP server.",str);
       goto free_return;
      }
   }

 connection->loggedin=1;
 logged_in:

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

 rcode=atoi(str);
 if(rcode!=250)
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

    rcode=atoi(str);
    if(rcode!=250 && rcode!=501 && rcode!=530 && rcode!=550)
      {
       chomp_str(str);
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'CWD' command to FTP server.",str);
       goto free_return;
      }

    if(rcode==250)
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

 rcode=atoi(str);
 if(rcode!=200)
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

       rcode=atoi(str);
       if(rcode==213)
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

       rcode=atoi(str);
       if(rcode==213)
         {
          int year,mon,mday,hour,min,sec;

          if(sscanf(str,"%*d %4d%2d%2d%2d%2d%2d",&year,&mon,&mday,&hour,&min,&sec)==6)
            {
             struct tm modtm;
	     char *ims;

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

	     if((ims=GetHeader(request_head,"If-Modified-Since")))
	       {
		time_t lastmodtime=DateToTimeT(ims);

		if(modtime<=lastmodtime)
		  {
		   CreateHeader("HTTP/1.0 304 FTP Not Modified",0,&bufferhead);
		   AddToHeader(bufferhead,"Content-Type",mimetype);
		   AddToHeader(bufferhead,"Content-Length","0");

		   goto near_end;
		  }
	       }
            }
         }
      }
   }

 /* Create the data connection. */

 if(!connection->tryepsv) goto trypasv;

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

 rcode=atoi(str);
 if(rcode==229)
   {
    host=strchr(str,'(');
    if(!host || sscanf(host+1,"%*c%*c%*c%d%*c",&port)!=1)
      {
       chomp_str(str);
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'EPSV' command, cannot parse.",str);
       goto free_return;
      }
    host=NULL;
   }
 else if(str[0]!='5' || str[1]!='0' || !isdigit(str[2]))
   {
    chomp_str(str);
    msg=GetPrintMessage(Warning,"Got '%s' message after sending 'EPSV' command",str);
    goto free_return;
   }
 else {
   connection->tryepsv=0;
 trypasv:
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

    rcode=atoi(str);
    if(rcode!=227)
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
 }

 /* Open the FTP data connection. */

 {
   IPADDR *a=NULL,*sa=NULL,addr;
   char *shost=NULL;
   int sport=0;

   if(!(connection->socksproxy && connection->socksproxy->remotedns)) {
     if(host) {
       if(!resolve_name(host,&addr)) {
	 msg=GetPrintMessage(Warning,"Cannot open the FTP data connection: unknown host '%s' [%!s].",host);
	 goto free_return;
       }
       a= &addr;
     }
     else {
       if(!(connection->Url->addrvalid)) {
	 /* Shouldn't happen */
	 msg=GetPrintMessage(Warning,"Cannot open the FTP data connection: unresolved host '%s'.",connection->Url->host);
	 goto free_return;
       }
       a= &connection->Url->addr;
     }
   }

   if(connection->socksproxy) {
     sa=a;
     if(connection->socksproxy->remotedns)
       shost= (host? host: connection->Url->host);
     sport=port;
     a= &connection->socksproxy->addr;
     port= connection->socksproxy->port;
   }

   server_data=OpenClientSocketAddr(a,port,sa,shost,sport);
 }

 if(server_data==-1)
   {
    msg=GetPrintMessage(Warning,"Cannot open the FTP data connection [%!s].");
    goto free_return;
   }
 else
   {
    init_io(server_data);
    configure_io_timeout_rw(server_data,ConfigInteger(SocketTimeout));
    connection->fd=server_data;
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

       for(i=0; ;i++)
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

	  if(i || !str)
	    break;

	  rcode=atoi(str);
          if(rcode==150 || rcode==125)
	    break;

	  chomp_str(str);
	  PrintMessage(Warning,"Got '%s' message after sending '%s' command to FTP server.",str,command);
         }
      }

    if(!str)
      {
       msg=GetPrintMessage(Warning,"No reply from FTP server to '%s' command; timed out?",command);
       goto free_return;
      }

    rcode=atoi(str);
    if(rcode==150 || rcode==125)
      {
       connection->expectmore=1;
      }
    else
      {
       chomp_str(str);
       msg=GetPrintMessage(Warning,"Got '%s' message after sending '%s' command to FTP server.",str,command);
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

    if(!str)
      {
       msg=GetPrintMessage(Warning,"No reply from FTP server to 'STOR' command; timed out?");
       goto free_return;
      }

    rcode=atoi(str);
    if(rcode==150 || rcode==125)
      {
       connection->expectmore=1;
      }
    else
      {
       chomp_str(str);
       msg=GetPrintMessage(Warning,"Got '%s' message after sending 'STOR' command to FTP server.",str);
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
       AddToHeader(bufferhead,"Last-Modified",RFC822Date(modtime,1));

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

       hint=malloc(sizeof(hint_t));
       hint->line=0;
       hint->field=0;
      }
   }
 else
    buffer=HTMLMessageString("FTPPut",
                             "url",Url->name,
                             NULL);

near_end:
 if(bufferhead->status<400 && GetHeader2(request_head,"Connection","Keep-Alive"))
   AddToHeader(bufferhead,"Connection","Keep-Alive");

 connection->bufferhead=bufferhead;
 connection->buffer=buffer;
 connection->buffertail=buffertail;
 connection->nbuffer=buffer?strlen(buffer):0; connection->nread=0;
 connection->nbuffertail=buffertail?strlen(buffertail):0; connection->nreadtail=0;
 connection->hint=hint;

free_return:
 free(path);
 if(str) free(str);
 if(msg_reply) free(msg_reply);

 return(msg);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a line from the header of the reply for the URL.

  Header *FTP_ReadHead Returns the header of the reply.
  ++++++++++++++++++++++++++++++++++++++*/

Header *FTP_ReadHead(Connection *connection)
{
 Header *reply_head=NULL;

 /* Take a simple route if it is proxied. */

 if(connection->proxyUrl)
    ParseReply(connection->ctrlfd,&reply_head);
 else /* Else send the header. */
   {
     reply_head=connection->bufferhead;
     connection->bufferhead=NULL;
   }

 return reply_head;
}


/*++++++++++++++++++++++++++++++++++++++
  Read bytes from the body of the reply for the URL.

  ssize_t FTP_ReadBody Returns the number of bytes read on success, -1 on error.

  char *s A string to fill in with the information.

  size_t n The number of bytes to read.
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t FTP_ReadBody(Connection *connection,char *s,size_t n)
{
 ssize_t m;
 int server_data;
 char *buffer,*buffertail;
 int nbuffer,nread,nbuffertail,nreadtail;

 /* Take a simple route if it is proxied. */

 if(connection->proxyUrl)
    return(read_data(connection->ctrlfd,s,n));

 /* Else send the data then the tail. */

 m=0;
 buffer=connection->buffer;
 buffertail=connection->buffertail;
 nbuffer=connection->nbuffer;
 nread=connection->nread;
 nbuffertail=connection->nbuffertail;
 nreadtail=connection->nreadtail;
 server_data=connection->fd;

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
	     buffer=htmlise_dir_entry(server_data,buffer,connection->hint);
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
 else if(buffer) /* Done a PUT */
   {
     for(;nread<nbuffer && m<n;nread++,m++)
       s[m]=buffer[nread];
   }
 else /* File not dir entry. */
   return read_data(server_data,s,n);

 connection->buffer=buffer;
 connection->nbuffer=nbuffer;
 connection->nread=nread;
 /* connection->nbuffertail=nbuffertail; */
 connection->nreadtail=nreadtail;

 return m;
}


int FTP_FinishBody(Connection *connection)
{
 int err=0;
 int server_ctrl=connection->ctrlfd,server_data=connection->fd;

 /* Take a simple route if it is proxied. */

 if(connection->proxyUrl)
   return(finish_io_content(server_ctrl));

 /* Else close the data socket, finish reading the control socket and clean up. */

 if(server_data!=-1)
   {
    unsigned long r=0,w=0;
    int errc;
    err=finish_tell_io(server_data,&r,&w);
    if((errc=CloseSocket(server_data))<0)
      err=errc;

    connection->fd=-1;
    connection->rbytes += r;
    connection->wbytes += w;
   }

 if(connection->expectmore)
   {
    char *str=NULL;
    do
      {
       str=read_line(server_ctrl,str);
       if(str)
	 PrintMessage(ExtraDebug,"FTP: got: %s",str);
      }
    while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

    if(!str) {
      PrintMessage(Warning,"No further reply from FTP server; timed out?");
      errno=ENODATA;
      err=-1;
    }
    else {
      int rcode=atoi(str);
      if(rcode!=226 && rcode!=250) {
	chomp_str(str);
	PrintMessage(Warning,"Got '%s' message after reading data from FTP server.",str);
	errno=ENOMSG;
	err=-1;
      }
      free(str);
    }

    connection->expectmore=0;
   }

 if(connection->bufferhead) {FreeHeader(connection->bufferhead); connection->bufferhead=NULL;}
 if(connection->buffer) {free(connection->buffer); connection->buffer=NULL;}
 if(connection->buffertail) {free(connection->buffertail); connection->buffertail=NULL;}
 if(connection->hint) {free(connection->hint); connection->hint=NULL;}

 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Close a connection opened using FTP.

  int FTP_Close Return 0 on success, -1 on error.
  ++++++++++++++++++++++++++++++++++++++*/

int FTP_Close(Connection *connection)
{
 unsigned long r1=0,w1=0,r2=0,w2=0;
 int server_ctrl=connection->ctrlfd,server_data=connection->fd;

 /* Take a simple route if it is proxied. */

 if(connection->proxyUrl)
   {
    if(finish_tell_io(server_ctrl,&r1,&w1)<0)
      PrintMessage(Inform,"Error finishing IO on socket with remote host [%!s].");

    PrintMessage(Inform,"Server bytes; %lu Read, %lu Written.",r1,w1); /* Used in audit-usage.pl */
   }
 else
   {
    /* Else say goodbye and close all of the sockets, */

    if(server_data!=-1)
      {
	if(finish_tell_io(server_data,&r2,&w2)<0)
	  PrintMessage(Inform,"Error finishing IO on data socket with remote host [%!s].");
	CloseSocket(server_data);
      }

    write_string(server_ctrl,"QUIT\r\n");

    {
      char *str=NULL;
      do
	{
	  str=read_line(server_ctrl,str);
	  if(str)
	    PrintMessage(ExtraDebug,"FTP: sent 'QUIT'; got: %s",str);
	}
      while(str && (!isdigit(str[0]) || !isdigit(str[1]) || !isdigit(str[2]) || str[3]!=' '));

      if(str)
	free(str);
    }

    if(finish_tell_io(server_ctrl,&r1,&w1)<0)
      PrintMessage(Inform,"Error finishing IO on control socket with remote host [%!s].");

    PrintMessage(Inform,"Server bytes; %lu Read, %lu Written.",
		 r1+connection->rbytes+r2,
		 w1+connection->wbytes+w2); /* Used in audit-usage.pl */
   }

#if 0
 /* We can clean up the connection context here, or rely on ConnectionClose to do it. */
 if(connection->bufferhead) {FreeHeader(connection->bufferhead); connection->bufferhead=NULL;}
 if(connection->buffer) {free(connection->buffer); connection->buffer=NULL;}
 if(connection->buffertail) {free(connection->buffertail); connection->buffertail=NULL;}
 if(connection->hint) {free(connection->hint); connection->hint=NULL;}

 if(connection->proxyUrl) {FreeURL(connection->proxyUrl); connection->proxyUrl=NULL;}
 if(connection->socksproxy) {free(connection->socksproxy); connection->socksproxy=NULL;}
#endif

 return(CloseSocket(server_ctrl));
}


struct pointer_pair {char *beg,*end;};
#define MaxFields 24

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

static char *htmlise_dir_entry(int server_data,char *line,hint_t *hint)
{
 int i,isdir=0,islink=0;
 struct pointer_pair p[MaxFields];
 int file=0,fileend=0,link=0,linkend=0;

 if((line=read_line(server_data,line))) {

   i=split_line(line,p);

   if(i>=8 && (*p[0].beg=='-' || *p[0].beg=='d' || *p[0].beg=='l')) /* A UNIX 'ls -l' listing. */
     {
       if(hint) ++ hint->line;

       if(*p[0].beg=='d')
	 isdir=1;
       else if(*p[0].beg=='l')
	 islink=1;

       if(hint && hint->field && hint->field<i) {
	 if(i<MaxFields) {
	   if(islink) {
	     int ia,iamax=i-2;
	     for(ia=hint->field+1;ia<=iamax;++ia) {
	       char *pa=p[ia].beg;
	       if(pa+2==p[ia].end && *pa=='-' && *(pa+1)=='>') {
		 file=hint->field; fileend=ia-1;
		 link=ia+1; linkend=i-1;
		 break;
	       }
	     }
	   }
	   else {
	     link=file=hint->field; linkend=fileend=i-1;;
	   }
	 }
       }
       else if(i==8 || i==9) {
	 linkend=link=fileend=file=i-1;
	 if(hint) {
	   if(!hint->field) {
	     if(hint->line <= 2 && isdir) {
	       char *pb=p[file].beg, *pe=p[file].end;
	       if(*pb=='.' && (pb+1==pe || (*(pb+1)=='.' && pb+2==pe)))
		 hint->field=file;
	     }
	   }
	   else if(hint->field!=file)
	     hint->field=0;
	 }
       }
       else if(islink && (i==10 || i==11)) {
	 int ia=i-2; char *pa=p[ia].beg;
	 if(pa+2==p[ia].end && *pa=='-' && *(pa+1)=='>')
	   {fileend=file=i-3; linkend=link=i-1;}
       }
     }

   if(file)
     {
       char *pfile=p[file].beg,*endf=p[fileend].end, *plink=p[link].beg,*endl=p[linkend].end;
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
	 q=mempcpy(q,p[file].beg,p[fileend].end-p[file].beg);
	 q=stpcpy(q,"</a>");

	 if(islink && link!=file) {
	   q=mempcpy(q,p[fileend].end,p[link].beg-p[fileend].end);
	   q=stpcpy(q,"<a href=\"");
	   if(!(*linkurlenc=='/' || (*linkurlenc=='.' && (*(linkurlenc+1)=='/' || (*(linkurlenc+1)=='.' && *(linkurlenc+2)=='/')))))
	     q=stpcpy(q,"./");
	   q=stpcpy(q,linkurlenc);
	   q=stpcpy(q,"\">");
	   q=mempcpy(q,p[link].beg,p[linkend].end-p[link].beg);
	   q=stpcpy(q,"</a>");
	   q=stpcpy(q,p[linkend].end);
	 }
	 else
	   q=stpcpy(q,p[fileend].end);

       }

       free(hline);
       free(fileurlenc);
       if(linkurlenc) free(linkurlenc);
     }
 }

 return(line);
}
