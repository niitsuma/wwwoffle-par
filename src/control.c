/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/control.c 2.61 2004/09/28 16:25:30 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8c.
  The HTML interactive control pages.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"


/*+ The action to perform. +*/
typedef enum _Action
{
 None,                          /*+ Undecided. +*/

 Online,                        /*+ Tell the server that we are online. +*/
 Autodial,                      /*+ Tell the server that we are in autodial mode. +*/
 Offline,                       /*+ Tell the server that we are offline. +*/
 Fetch,                         /*+ Tell the server to fetch the requested pages. +*/
 Config,                        /*+ Tell the server to re-read the configuration file. +*/
 Purge,                         /*+ Tell the server to purge pages. +*/
 Status,                        /*+ Get the server status. +*/

 Delete,                        /*+ Delete a page from the cache or a request from the outgoing directory. +*/
 DeleteMultiple,                /*+ Delete mulitple pages from the cache or requests from the outgoing directory. +*/

 Edit                           /*+ Edit the config file. +*/
}
Action;


static void ActionControlPage(int fd,Action action,char *command);
static void DeleteControlPage(int fd,URL *Url,/*@null@*/ Body *request_body);
static void DeleteMultipleControlPages(int fd,URL *Url,/*@null@*/ Body *request_body);
static void ControlAuthFail(int fd,char *url);

static void delete_req(int fd,char *req,int all,/*@null@*/ char *username,/*@null@*/ char *password,int count);
static void delete_mon(int fd,char *mon,int all,/*@null@*/ char *username,/*@null@*/ char *password,int count);
static void delete_url(int fd,char *url,int all,/*@null@*/ char *username,/*@null@*/ char *password,int count);


/*++++++++++++++++++++++++++++++++++++++
  Send to the client one of the pages to control WWWOFFLE using HTML.

  int fd The file descriptor of the client.

  URL *Url The Url that was requested.

  Body *request_body The body of the HTTP request.
  ++++++++++++++++++++++++++++++++++++++*/

void ControlPage(int fd,URL *Url,Body *request_body)
{
 Action action=None;
 char *command="";
 char *newpath=Url->path+strlitlen("/control/");  /* remove the '/control/' */

 /* remove trailing '/' */
 {char *p=strchrnul(newpath,0)-1; if(p>=newpath && *p=='/') newpath=STRDUPA2(newpath,p); }

 /* Determine the action. */

 if(!strcmp(newpath,"online"))
   {action=Online;command="-online";}
 else if(!strcmp(newpath,"autodial"))
   {action=Autodial;command="-autodial";}
 else if(!strcmp(newpath,"offline"))
   {action=Offline;command="-offline";}
 else if(!strcmp(newpath,"fetch"))
   {action=Fetch;command="-fetch";}
 else if(!strcmp(newpath,"config"))
   {action=Config;command="-config";}
 else if(!strcmp(newpath,"purge"))
   {action=Purge;command="-purge";}
 else if(!strcmp(newpath,"status"))
   {action=Status;command="-status";}
 else if(!strcmp_litbeg(newpath,"delete-multiple"))
    action=DeleteMultiple;
 else if(!strcmp_litbeg(newpath,"delete"))
    action=Delete;
 else if(!strcmp(newpath,"edit"))
    action=Edit;

 /* Check the authorisation. */

 if(ConfigString(PassWord))
   {
    if(!Url->pass && action==Delete)
      {
       char **args=NULL,**argsp;
       int hashash=0;

       if(Url->args && *Url->args!='!')
          args=SplitFormArgs(Url->args);
       else if(request_body)
         {
          char *form=URLRecodeFormArgs(request_body->content);
          args=SplitFormArgs(form);
          free(form);
         }

       if(args)
         {
          for(argsp=args;*argsp;argsp++)
            {
             if(!strcmp_litbeg(*argsp,"hash=") && (*argsp)[strlitlen("hash=")])
                hashash=1;
             else
                PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",*argsp,Url->name);
            }

          free(*args);
          free(args);
         }

       if(!hashash)
         {
          ControlAuthFail(fd,Url->pathp);
          return;
         }
      }
    else if(!Url->pass || strcmp(Url->pass,ConfigString(PassWord)))
      {
       ControlAuthFail(fd,Url->pathp);
       return;
      }
   }

 /* Perform the action. */

 if(action==None) {
   if(*newpath)
     HTMLMessage(fd,404,"WWWOFFLE Illegal Control Page",NULL,"ControlIllegal",
		 "url",Url->pathp,
		 NULL);
   else
     HTMLMessage(fd,200,"WWWOFFLE Control Page",NULL,"ControlPage",
		 NULL);
 }
 else if(action==Delete)
    DeleteControlPage(fd,Url,request_body);
 else if(action==DeleteMultiple)
    DeleteMultipleControlPages(fd,Url,request_body);
 else if(action==Edit)
    ControlEditPage(fd,Url->args,request_body);
 else
    ActionControlPage(fd,action,command);

}


/*++++++++++++++++++++++++++++++++++++++
  The control page that performs an action.

  int fd The file descriptor to write to.

  Action action The action to perform.

  char *command The command line argument that would be used with wwwoffle.
  ++++++++++++++++++++++++++++++++++++++*/

static void ActionControlPage(int fd,Action action,char *command)
{
 char *localhost=GetLocalHost(0);
 int error=0;
 int socket=OpenClientSocket(localhost,ConfigInteger(WWWOFFLE_Port),NULL,0,NULL);

 if(socket==-1)
   {
    PrintMessage(Warning,"Cannot open connection to wwwoffle server %s port %d.",localhost,ConfigInteger(WWWOFFLE_Port));
    HTMLMessage(fd,500,"WWWOFFLE Server Error",NULL,"ServerError",
                "error","Cannot open connection to wwwoffle server on localhost",
                NULL);
    goto free_return;
   }

 init_io(socket);
 configure_io_read(socket,ConfigInteger(SocketTimeout),0,0);
 configure_io_write(socket,ConfigInteger(SocketTimeout),0,0);

 HTMLMessageHead(fd,200,"WWWOFFLE Control Page",
                 "Cache-Control","no-cache",
                 "Expires","0",
                 NULL);

 if(out_err!=-1 && !head_only)
   HTMLMessageBody(fd,"ControlWWWOFFLE-Head",
		   "command",command,
		   NULL);

 /* Send the message. */

 if(ConfigString(PassWord))
   write_formatted(socket,"WWWOFFLE PASSWORD %s\r\n",ConfigString(PassWord));

 if(action==Online)
   error=write_string(socket,"WWWOFFLE ONLINE\r\n");
 else if(action==Autodial)
   error=write_string(socket,"WWWOFFLE AUTODIAL\r\n");
 else if(action==Offline)
   error=write_string(socket,"WWWOFFLE OFFLINE\r\n");
 else if(action==Fetch)
   error=write_string(socket,"WWWOFFLE FETCH\r\n");
 else if(action==Config)
   error=write_string(socket,"WWWOFFLE CONFIG\r\n");
 else if(action==Purge)
   error=write_string(socket,"WWWOFFLE PURGE\r\n");
 else if(action==Status)
   error=write_string(socket,"WWWOFFLE STATUS\r\n");

 if(error==-1) {
   if(out_err!=-1 && !head_only)
     write_string(fd,"Error writing the command to the server.");
 }
 else
   {
     char buffer[READ_BUFFER_SIZE]; int n;
     while((n=read_data(socket,buffer,READ_BUFFER_SIZE))>0)
       if(out_err!=-1 && !head_only && write_data(fd,buffer,n)<0) {
	 PrintMessage(Warning,"Cannot write to client [%!s]");
	 goto finishio_free_return;
       }
   }

 if(out_err!=-1 && !head_only)
   HTMLMessageBody(fd,"ControlWWWOFFLE-Tail",
		   NULL);

finishio_free_return:
 finish_io(socket);
 CloseSocket(socket);
free_return:
 free(localhost);
}


/*++++++++++++++++++++++++++++++++++++++
  The control page that deletes a cached page or a request.

  int fd The file descriptor to write to.

  URL *Url the URL that was asked for.

  Body *request_body The body of the delete command.
  ++++++++++++++++++++++++++++++++++++++*/

static void DeleteControlPage(int fd,URL *Url,Body *request_body)
{
 char *url=NULL,*req=NULL,*mon=NULL,**args=NULL,**argsp;
 char *hash=NULL,*realhash=NULL,*page=NULL,*username=NULL,*password=NULL;
 int all=0;

 /* Decide what sort of deletion is required. */

 if(strlen(Url->path+9)>=10 && !strcmp_litbeg(Url->path+9+10,"-all"))
    all=1;

 if(Url->args && *Url->args!='!')
    args=SplitFormArgs(Url->args);
 else if(request_body)
   {
    char *form=URLRecodeFormArgs(request_body->content);
    args=SplitFormArgs(form);
    free(form);
   }

 if(args)
   {
    for(argsp=args;*argsp;argsp++)
      {
       if(!strcmp_litbeg(*argsp,"hash="))
          hash=URLDecodeFormArgs(&(*argsp)[strlitlen("hash=")]);
       else if(!strcmp_litbeg(*argsp,"username="))
          username=&(*argsp)[strlitlen("username=")];
       else if(!strcmp_litbeg(*argsp,"password="))
          password=&(*argsp)[strlitlen("password=")];
       else if(!strcmp_litbeg(*argsp,"url="))
          page=TrimArgs(URLDecodeFormArgs(&(*argsp)[strlitlen("url=")]));
       else
          PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",*argsp,Url->name);
      }
   }

 if(!strcmp_litbeg(Url->path+9,"delete-url"))
    url=page;
 else if(!strcmp_litbeg(Url->path+9,"delete-mon"))
   {
    mon=page;

    if(all)
       mon="all";
   }
 else if(!strcmp_litbeg(Url->path+9,"delete-req"))
   {
    req=page;

    if(all)
       req="all";
   }

 /* Do the required deletion. */

 if(!url && !mon && !req)
   {
    PrintMessage(Important,"Invalid interactive delete page requested; path='%s' args='%s'.",Url->path,Url->args);
    HTMLMessage(fd,404,"WWWOFFLE Illegal Control Page",NULL,"ControlIllegal",
                "url",Url->pathp,
                NULL);
   }
 else
   {
    HTMLMessageHead(fd,200,"WWWOFFLE Delete Control Page",
                    NULL);
    if(out_err!=-1 && !head_only)
      HTMLMessageBody(fd,"ControlDelete-Head",
		      "req",req,
		      "mon",mon,
		      "url",url,
		      "hash",hash,
		      "path",Url->path,
		      NULL);

    if(hash && req)
      {
       URL *hashUrl=SplitURL(req);

       if(username)
          AddURLPassword(hashUrl,username,password);

       realhash=HashOutgoingSpoolFile(hashUrl);

       FreeURL(hashUrl);
      }

    if(hash && req && (!realhash || strcmp(hash,realhash)))
      {
	if(out_err!=-1 && !head_only)
	  HTMLMessageBody(fd,"ControlDelete-Body",
			  "count","",
			  "url",all?"":req,
			  "all",all?"yes":"",
			  "error",realhash?"Hash does not match":"Request already deleted",
			  NULL);
      }
    else if(req)
       delete_req(fd,req,all,username,password,0);
    else if(mon)
       delete_mon(fd,mon,all,username,password,0);
    else if(url)
       delete_url(fd,url,all,username,password,0);

    if(out_err!=-1 && !head_only)
      HTMLMessageBody(fd,"ControlDelete-Tail",
		      "req",req,
		      "mon",mon,
		      "url",url,
		      NULL);
   }

 if(args)
   {
    free(*args);
    free(args);
   }

 if(page)
    free(page);
 if(hash)
    free(hash);
 if(realhash)
    free(realhash);
}


/*++++++++++++++++++++++++++++++++++++++
  The control page that deletes multiple cached pages or requests.

  int fd The file descriptor to write to.

  URL *Url the URL that was asked for.

  Body *request_body The body of the delete command.
  ++++++++++++++++++++++++++++++++++++++*/

static void DeleteMultipleControlPages(int fd,URL *Url,Body *request_body)
{
 char *url=NULL,*req=NULL,*mon=NULL;

 /* Decide what sort of deletion is required. */

 if(!strcmp_litbeg(Url->path+9,"delete-multiple-url"))
    url="yes";
 else if(!strcmp_litbeg(Url->path+9,"delete-multiple-mon"))
    mon="yes";
 else if(!strcmp_litbeg(Url->path+9,"delete-multiple-req"))
    req="yes";

 /* Do the required deletion. */

 if(!request_body || (!url && !mon && !req))
   {
    PrintMessage(Important,"Invalid interactive delete page requested; path='%s'.",Url->path);
    HTMLMessage(fd,404,"WWWOFFLE Illegal Control Page",NULL,"ControlIllegal",
                "url",Url->pathp,
                NULL);
   }
 else
   {
    char *form,**args,**argsp;
    char *username=NULL,*password=NULL;

    HTMLMessageHead(fd,200,"WWWOFFLE Multiple Delete Control Page",
                    NULL);
    if(out_err!=-1 && !head_only)
      HTMLMessageBody(fd,"ControlDelete-Head",
		      "req",req,
		      "mon",mon,
		      "url",url,
		      "path",Url->path,
		      NULL);

    form=URLRecodeFormArgs(request_body->content);
    args=SplitFormArgs(form);
    free(form);

    for(argsp=args;*argsp;argsp++)
      {
       if(!strcmp_litbeg(*argsp,"username="))
          username=&(*argsp)[strlitlen("username=")];
       else if(!strcmp_litbeg(*argsp,"password="))
          password=&(*argsp)[strlitlen("password=")];
       else
          PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",*argsp,Url->name);
      }

    for(argsp=args;*argsp;argsp++)
      {
       char *equal=strchr(*argsp,'=');

       if(!strcmp_litbeg(*argsp,"url") && equal)
         {
          char *page=URLDecodeFormArgs(equal+1);

          if(req)
             delete_req(fd,page,0,username,password,1+(argsp-args));
          else if(mon)
             delete_mon(fd,page,0,username,password,1+(argsp-args));
          else if(url)
             delete_url(fd,page,0,username,password,1+(argsp-args));

          free(page);
         }
      }

    if(out_err!=-1 && !head_only)
      HTMLMessageBody(fd,"ControlDelete-Tail",
		      "req",req,
		      "mon",mon,
		      "url",url,
		      NULL);

    free(*args);
    free(args);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Inform the user that the authorisation failed.

  int fd The file descriptor to write to.

  char *url The specified path.
  ++++++++++++++++++++++++++++++++++++++*/

static void ControlAuthFail(int fd,char *url)
{
 HTMLMessageHead(fd,401,"WWWOFFLE Authorisation Failed",
                 "WWW-Authenticate","Basic realm=\"control\"",
                 NULL);
 if(out_err!=-1 && !head_only)
   HTMLMessageBody(fd,"ControlAuthFail",
		   "url",url,
		   NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a specified outgoing request URL.

  int fd The file descriptor to display the message on.

  char *req The URL to delete.

  int all The flag to indicate if all of them are to be deleted.

  char *username The username to add to the URL before trying to delete it.

  char *password The password to add to the URL before trying to delete it.

  int count Specifies which of a multiple delete that it is.
  ++++++++++++++++++++++++++++++++++++++*/

static void delete_req(int fd,char *req,int all,char *username,char *password,int count)
{
 char *err=NULL;
 char count_str[12];

 sprintf(count_str,"%d",count);

 if(all)
    err=DeleteOutgoingSpoolFile(NULL);
 else
   {
    URL *reqUrl=SplitURL(req);

    if(username)
       AddURLPassword(reqUrl,username,password);

    if(reqUrl->Protocol)
       err=DeleteOutgoingSpoolFile(reqUrl);
    else
       err=strdup("Illegal Protocol");

    FreeURL(reqUrl);
   }

 if(out_err!=-1 && !head_only)
   HTMLMessageBody(fd,"ControlDelete-Body",
		   "count",count>0?count_str:"",
		   "url",all?"":req,
		   "all",all?"yes":"",
		   "error",err,
		   NULL);

 if(err)
    free(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a specified monitored URL.

  int fd The file descriptor to display the message on.

  char *mon The URL to delete.

  int all The flag to indicate if all of them are to be deleted.

  char *username The username to add to the URL before trying to delete it.

  char *password The password to add to the URL before trying to delete it.

  int count Specifies which of a multiple delete that it is.
  ++++++++++++++++++++++++++++++++++++++*/

static void delete_mon(int fd,char *mon,int all,char *username,char *password,int count)
{
 char *err=NULL;
 char count_str[12];

 sprintf(count_str,"%d",count);

 if(all)
    err=DeleteMonitorSpoolFile(NULL);
 else
   {
    URL *monUrl=SplitURL(mon);

    if(username)
       AddURLPassword(monUrl,username,password);

    if(monUrl->Protocol)
       err=DeleteMonitorSpoolFile(monUrl);
    else
       err=strdup("Illegal Protocol");

    FreeURL(monUrl);
   }

 if(out_err!=-1 && !head_only)
   HTMLMessageBody(fd,"ControlDelete-Body",
		   "count",count>0?count_str:"",
		   "url",all?"":mon,
		   "all",all?"yes":"",
		   "error",err,
		   NULL);

 if(err)
    free(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a specified cached URL.

  int fd The file descriptor to display the message on.

  char *url The URL to delete.

  int all The flag to indicate if all of them are to be deleted.

  char *username The username to add to the URL before trying to delete it.

  char *password The password to add to the URL before trying to delete it.

  int count Specifies which of a multiple delete that it is.
  ++++++++++++++++++++++++++++++++++++++*/

static void delete_url(int fd,char *url,int all,char *username,char *password,int count)
{
 char *err=NULL;
 URL *urlUrl=SplitURL(url);
 char count_str[12];

 sprintf(count_str,"%d",count);

 if(username)
    AddURLPassword(urlUrl,username,password);

 if(urlUrl->Protocol)
    err=DeleteWebpageSpoolFile(urlUrl,all);
 else
    err=strdup("Illegal Protocol");

 if(out_err!=-1 && !head_only)
   HTMLMessageBody(fd,"ControlDelete-Body",
		   "count",count>0?count_str:"",
		   "url",url,
		   "all",all?"yes":"",
		   "error",err,
		   NULL);

 if(err)
    free(err);
 FreeURL(urlUrl);
}
