/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/local.c 1.4 2002/10/26 11:04:04 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7g.
  Serve the local web-pages and handle the language selection.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

#include <sys/stat.h>
#include <fcntl.h>

#include "version.h"
#include "wwwoffle.h"
#include "errors.h"
#include "config.h"
#include "misc.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif


static char /*@null@*/ /*@observer@*/ **get_languages(int *ndirs);


/*+ The language header that the browser sent. +*/
static char /*@null@*/ /*@only@*/ *accept_language=NULL;


/*++++++++++++++++++++++++++++++++++++++
  Output a local page.

  int fd The file descriptor to write to.

  URL *Url The URL for the local page.

  Header *request_head The request that was made for this page.

  Body *request_body The body of the request that was made for this page.
  ++++++++++++++++++++++++++++++++++++++*/

void LocalPage(int fd,URL *Url,Header *request_head,Body *request_body)
{
 int found=0;
 char *file;

 /* Don't allow paths backwards */

 if(strstr(Url->path,"/../"))
   {
    PrintMessage(Warning,"Illegal path containing '/../' for the local page '%s'.",Url->path);
    HTMLMessage(fd,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
                "url",Url->path,
                NULL);
    return;
   }

 /* Get the filename */

 if((file=FindLanguageFile(Url->path+1)))
   {
    struct stat buf;

    if(stat(file,&buf))
       ;
    else if(S_ISREG(buf.st_mode) && buf.st_mode&S_IROTH)
      {
       if(buf.st_mode&S_IXOTH && IsCGIAllowed(Url->path))
         {
          LocalCGI(fd,Url,file,request_head,request_body);
          found=1;
         }
       else
         {
          int htmlfd=open(file,O_RDONLY|O_BINARY);
          init_buffer(htmlfd);

          if(htmlfd==-1)
             PrintMessage(Warning,"Cannot open the local page '%s' [%!s].",file);
          else
            {
             char *ims=NULL;
             time_t since=0;

             PrintMessage(Debug,"Using the local page '%s'.",file);

             if((ims=GetHeader(request_head,"If-Modified-Since")))
                since=DateToTimeT(ims);

             if(since>=buf.st_mtime)
                HTMLMessageHead(fd,304,"WWWOFFLE Not Modified",
                                NULL);
             else
               {
                char buffer[READ_BUFFER_SIZE];
                int n;

                HTMLMessageHead(fd,200,"WWWOFFLE Local OK",
                                "Last-Modified",RFC822Date(buf.st_mtime,1),
                                "Content-Type",WhatMIMEType(file),
                                NULL);

                while((n=read_data(htmlfd,buffer,READ_BUFFER_SIZE))>0)
                   write_data(fd,buffer,n);
               }

             close(htmlfd);

             found=1;
            }
         }
      }
    else if(S_ISDIR(buf.st_mode))
      {
       char *localhost=GetLocalHost(1);
       char *dir=(char*)malloc(strlen(Url->path)+strlen(localhost)+24);

       PrintMessage(Debug,"Using the local directory '%s'.",file);

       strcpy(dir,"http://");
       strcat(dir,localhost);
       strcat(dir,Url->path);
       if(dir[strlen(dir)-1]!='/')
          strcat(dir,"/");
       strcat(dir,"index.html");
       HTMLMessage(fd,302,"WWWOFFLE Local Dir Redirect",dir,"Redirect",
                   "location",dir,
                   NULL);

       free(dir);
       free(localhost);

       found=1;
      }
    else
       PrintMessage(Warning,"Not a regular file or wrong permissions for the local page '%s'.",file);

    free(file);
   }

 if(!found)
   {
    PrintMessage(Warning,"Cannot find a local URL '%s'.",Url->path);
    HTMLMessage(fd,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
                "url",Url->path,
                NULL);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Set the language that will be accepted for the mesages.

  char *accept The contents of the Accept-Language header.
  ++++++++++++++++++++++++++++++++++++++*/

void SetLanguage(char *accept)
{
 if(accept)
   {
    accept_language=(char*)malloc(strlen(accept)+1);
    strcpy(accept_language,accept);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Find the language specific message file.

  char *FindLanguageFile Returns the file name or NULL.

  char* search The name of the file to search for (e.g. 'messages/foo.html' or 'local/bar.html').
  ++++++++++++++++++++++++++++++++++++++*/

char *FindLanguageFile(char* search)
{
 char *file=NULL;
 int dirn=0;
 char **dirs;
 int ndirs=0;

 /* Get the list of directories for languages. */

 dirs=get_languages(&ndirs);

 /* Find the file */

 if(!strncmp(search,"local/",sizeof("local")))
    dirn=-1;

 while(dirn<(ndirs+2))
   {
    struct stat buf;
    char *dir="",*tryfile;

    if(dirn==-1)
       dir="./";                /* must include "local" at the start. */
    else if(dirn<ndirs)
       dir=dirs[dirn];          /* is formatted like "html/$LANG/" */
    else if(dirn==ndirs)
       dir="html/default/";
    else if(dirn==(ndirs+1))
       dir="html/en/";

    dirn++;

    tryfile=(char*)malloc(strlen(dir)+strlen(search)+1);
    strcpy(tryfile,dir);
    strcat(tryfile,search);

    if(!stat(tryfile,&buf))
      {
       file=tryfile;
       break;
      }

    free(tryfile);
   }

 return(file);
}


/*++++++++++++++++++++++++++++++++++++++
  Open the language specific message file.

  int OpenLanguageFile Returns the file descriptor or -1.

  char* search The name of the file to search for (e.g. 'messages/foo.html' or 'local/bar.html').
  ++++++++++++++++++++++++++++++++++++++*/

int OpenLanguageFile(char* search)
{
 int fd=-1;
 char *file;

 /* Find the file. */

 file=FindLanguageFile(search);

 if(file)
   {
    fd=open(file,O_RDONLY|O_BINARY);

    free(file);
   }

 if(fd!=-1)
    init_buffer(fd);

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the language string and add the directories to the list.

  char ***get_languages Returns the list of directories to try.

  int *ndirs Returns the number of directories in the list.
  ++++++++++++++++++++++++++++++++++++++*/

static char **get_languages(int *ndirs)
{
 static char **dirs=NULL;
 static int n_dirs=0;
 static int first=1;

 if(first)
   {
    first=0;

    if(accept_language)
      {
       int i;
       HeaderList *list=SplitHeaderList(accept_language);

       for(i=0;i<list->n;i++)
          if(list->item[i].qval>0)
            {
             if((n_dirs%8)==0)
                dirs=(char**)realloc((void*)dirs,(8+n_dirs)*sizeof(char*));

             dirs[n_dirs]=(char*)malloc(8+strlen(list->item[i].val)+1);
             sprintf(dirs[n_dirs],"html/%s/",list->item[i].val);

             n_dirs++;
            }

       FreeHeaderList(list);
      }
   }

 *ndirs=n_dirs;

 return(dirs);
}
