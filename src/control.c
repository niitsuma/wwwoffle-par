/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/control.c 2.55 2002/10/12 20:26:22 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7g.
  The HTML interactive control pages.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02 Andrew M. Bishop
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
#include "misc.h"
#include "config.h"
#include "sockets.h"
#include "errors.h"


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
 char *newpath=(char*)malloc(strlen(Url->path)-8);
 char *command="";

 strcpy(newpath,Url->path+9);        /* remove the '/control/' */

 if(*newpath && newpath[strlen(newpath)-1]=='/')
    newpath[strlen(newpath)-1]=0;

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
 else if(!strncmp(newpath,"delete-multiple",15))
    action=DeleteMultiple;
 else if(!strncmp(newpath,"delete",6))
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
             if(!strncmp(*argsp,"hash=",5) && (*argsp)[5])
                hashash=1;
            }

          free(*args);
          free(args);
         }

       if(!hashash)
         {
          ControlAuthFail(fd,Url->pathp);
          free(newpath);
          return;
         }
      }
    else if(!Url->pass)
      {
       ControlAuthFail(fd,Url->pathp);
       free(newpath);
       return;
      }
    else if(strcmp(Url->pass,ConfigString(PassWord)))
      {
       ControlAuthFail(fd,Url->pathp);
       free(newpath);
       return;
      }
   }

 /* Perform the action. */

 if(action==None && Url->path[9])
    HTMLMessage(fd,404,"WWWOFFLE Illegal Control Page",NULL,"ControlIllegal",
                "url",Url->pathp,
                NULL);
 else if(action==None)
    HTMLMessage(fd,200,"WWWOFFLE Control Page",NULL,"ControlPage",
                NULL);
 else if(action==Delete)
    DeleteControlPage(fd,Url,request_body);
 else if(action==DeleteMultiple)
    DeleteMultipleControlPages(fd,Url,request_body);
 else if(action==Edit)
    ControlEditPage(fd,Url->args,request_body);
 else
    ActionControlPage(fd,action,command);

 free(newpath);
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
 int socket=OpenClientSocket(localhost,ConfigInteger(WWWOFFLE_Port));
 init_buffer(socket);

 if(socket==-1)
   {
    PrintMessage(Warning,"Cannot open connection to wwwoffle server %s port %d.",localhost,ConfigInteger(WWWOFFLE_Port));
    HTMLMessage(fd,500,"WWWOFFLE Server Error",NULL,"ServerError",
                "error","Cannot open connection to wwwoffle server on localhost",
                NULL);
   }
 else
   {
    char *buffer=NULL;
    int error=0;

    HTMLMessage(fd,200,"WWWOFFLE Control Page",NULL,"ControlWWWOFFLE-Head",
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

    if(error==-1)
       write_string(fd,"Error writing the command to the server.");
    else
       while((buffer=read_line(socket,buffer)))
          write_string(fd,buffer);

    HTMLMessageBody(fd,"ControlWWWOFFLE-Tail",
                    NULL);
   }

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

 if(strlen(Url->path+9)>=10 && !strncmp(Url->path+9+10,"-all",4))
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
       if(!strncmp(*argsp,"hash=",5))
          hash=URLDecodeFormArgs(&(*argsp)[5]);
       if(!strncmp(*argsp,"username=",9))
          username=&(*argsp)[9];
       if(!strncmp(*argsp,"password=",9))
          password=&(*argsp)[9];
       if(!strncmp(*argsp,"url=",4))
          page=URLDecodeFormArgs(&(*argsp)[4]);
      }
   }

 if(!strncmp(Url->path+9,"delete-url",10))
    url=page;
 else if(!strncmp(Url->path+9,"delete-mon",10))
   {
    mon=page;

    if(all)
       mon="all";
   }
 else if(!strncmp(Url->path+9,"delete-req",10))
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

    if(hash && (!req || !realhash || strcmp(hash,realhash)))
      {
       HTMLMessageBody(fd,"ControlDelete-Body",
                       "count","",
                       "url",all?"":req,
                       "all",all?"yes":"",
                       "error","Hash does not match",
                       NULL);
      }
    else if(req)
       delete_req(fd,req,all,username,password,0);
    else if(mon)
       delete_mon(fd,mon,all,username,password,0);
    else if(url)
       delete_url(fd,url,all,username,password,0);

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

 if(!strncmp(Url->path+9,"delete-multiple-url",19))
    url="yes";
 else if(!strncmp(Url->path+9,"delete-multiple-mon",19))
    mon="yes";
 else if(!strncmp(Url->path+9,"delete-multiple-req",19))
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
       if(!strncmp(*argsp,"username=",9))
          username=&(*argsp)[9];
       if(!strncmp(*argsp,"password=",9))
          password=&(*argsp)[9];
      }

    for(argsp=args;*argsp;argsp++)
      {
       char *equal=strchr(*argsp,'=');

       if(!strncmp(*argsp,"url",3) && equal)
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
 char count_str[8];

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
       err="Illegal Protocol";

    FreeURL(reqUrl);
   }

 HTMLMessageBody(fd,"ControlDelete-Body",
                 "count",count>0?count_str:"",
                 "url",all?"":req,
                 "all",all?"yes":"",
                 "error",err,
                 NULL);
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
 char count_str[8];

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
       err="Illegal Protocol";

    FreeURL(monUrl);
   }

 HTMLMessageBody(fd,"ControlDelete-Body",
                 "count",count>0?count_str:"",
                 "url",all?"":mon,
                 "all",all?"yes":"",
                 "error",err,
                 NULL);
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
 char count_str[8];

 sprintf(count_str,"%d",count);

 if(username)
    AddURLPassword(urlUrl,username,password);

 if(urlUrl->Protocol)
    err=DeleteWebpageSpoolFile(urlUrl,!!all);
 else
    err="Illegal Protocol";

 HTMLMessageBody(fd,"ControlDelete-Body",
                 "count",count>0?count_str:"",
                 "url",url,
                 "all",all?"yes":"",
                 "error",err,
                 NULL);

 FreeURL(urlUrl);
}
