/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/search.c 1.24 2002/11/28 18:53:49 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7g.
  Handle the interface to the ht://Dig, mnoGoSearch (UdmSearch) and Namazu search engines.
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

#include <utime.h>
#include <sys/stat.h>

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <fcntl.h>
#include <sys/wait.h>

#include "wwwoffle.h"
#include "misc.h"
#include "proto.h"
#include "config.h"
#include "errors.h"


/*+ The file descriptor of the spool directory. +*/
extern int fSpoolDir;


static void SearchIndex(int fd,URL *Url);
static void SearchIndexRoot(int fd);
static void SearchIndexProtocol(int fd,char *proto);
static void SearchIndexHost(int fd,char *proto,char *host);
static void SearchIndexLastTime(int fd);

static void HTSearch(int fd,char *args);

static void mnoGoSearch(int fd,char *args);

static void Namazu(int fd,char *args);


/*++++++++++++++++++++++++++++++++++++++
  Create a page for one of the search pages on the local server.

  int fd The file descriptor to write the output to.

  URL *Url The URL that specifies the path for the page.

  Header *request_head The head of the request.

  Body *request_body The body of the request that was made for this page.
  ++++++++++++++++++++++++++++++++++++++*/

void SearchPage(int fd,URL *Url,Header *request_head,Body *request_body)
{
 if(!strncmp(Url->path+8,"index/",6))
    SearchIndex(fd,Url);
 else if(!strcmp(Url->path+8,"htdig/htsearch"))
    HTSearch(fd,Url->args);
 else if(!strcmp(Url->path+8,"mnogosearch/mnogosearch"))
    mnoGoSearch(fd,Url->args);
 else if(!strcmp(Url->path+8,"udmsearch/udmsearch"))
    mnoGoSearch(fd,Url->args);
 else if(!strcmp(Url->path+8,"namazu/namazu"))
    Namazu(fd,Url->args);
 else if(!strncmp(Url->path+8,"htdig/",6) && !strchr(Url->path+8+6,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else if(!strncmp(Url->path+8,"mnogosearch/",12) && !strchr(Url->path+8+12,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else if(!strncmp(Url->path+8,"udmsearch/",10) && !strchr(Url->path+8+10,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else if(!strncmp(Url->path+8,"namazu/",7) && !strchr(Url->path+8+7,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else if(!strchr(Url->path+8,'/'))
    LocalPage(fd,Url,request_head,request_body);
 else
    HTMLMessage(fd,404,"WWWOFFLE Illegal Search Page",NULL,"SearchIllegal",
                "url",Url->path,
                NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Produce one of the indexes for htdig or mnoGoSearch (UdmSearch) to search.

  int fd The file descriptor to write to.

  URL *Url The URL that was requested.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndex(int fd,URL *Url)
{
 char *proto=(char*)malloc(strlen(Url->path)-10),*host="";
 int lasttime;
 int i;

 strcpy(proto,Url->path+8+6);

 lasttime=!strcmp(proto,"lasttime");

 if(*proto && proto[strlen(proto)-1]=='/')
    proto[strlen(proto)-1]=0;

 for(i=0;proto[i];i++)
    if(proto[i]=='/')
      {
       proto[i]=0;
       host=proto+i+1;
       break;
      }

 if(*proto)
   {
    for(i=0;i<NProtocols;i++)
       if(!strcmp(Protocols[i].name,proto))
          break;
    if(i==NProtocols)
       *proto=0;
   }

 if(!lasttime &&
    ((*host && (strchr(host,'/') || !strcmp(host,"..") || !strcmp(host,"."))) ||
     (*proto && (!strcmp(host,"..") || !strcmp(host,".")))||
     (*host && !*proto) || (Url->path[8+6] && !*proto)))
    HTMLMessage(fd,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
                "url",Url->name,
                NULL);
 else
   {
    HTMLMessageHead(fd,200,"WWWOFFLE Search Index",
                    NULL);
    write_string(fd,"<html>\n"
                    "<head>\n"
                    "<meta name=\"robots\" content=\"noindex\">\n"
                    "<title></title>\n"
                    "</head>\n"
                    "<body>\n");

    if(lasttime)
       SearchIndexLastTime(fd);
    else if(!*host && !*proto)
       SearchIndexRoot(fd);
    else if(!*host)
       SearchIndexProtocol(fd,proto);
    else
       SearchIndexHost(fd,proto,host);

    write_string(fd,"</body>\n"
                    "</html>\n");
   }

 free(proto);
}


/*++++++++++++++++++++++++++++++++++++++
  Index the root of the cache.

  int fd The file descriptor to write to.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexRoot(int fd)
{
 int i;

 for(i=0;i<NProtocols;i++)
    write_formatted(fd,"<a href=\"%s/\"> </a>\n",Protocols[i].name);
}


/*++++++++++++++++++++++++++++++++++++++
  Index the hosts for one protocol in the cache.

  int fd The file descriptor to write to.

  char *proto The protocol to index.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexProtocol(int fd,char *proto)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;

 /* Open the spool directory. */

 if(chdir(proto))
    return;

 dir=opendir(".");
 if(!dir)
   {fchdir(fSpoolDir);return;}

 ent=readdir(dir);  /* skip .  */
 if(!ent)
   {closedir(dir);fchdir(fSpoolDir);return;}
 ent=readdir(dir);  /* skip .. */

 /* Output all of the host sub-directories. */

 while((ent=readdir(dir)))
    if(!stat(ent->d_name,&buf) && S_ISDIR(buf.st_mode))
      {
#if defined(__CYGWIN__)
       int i;

       for(i=0;ent->d_name[i];i++)
          if(ent->d_name[i]=='!')
             ent->d_name[i]=':';
#endif

       write_formatted(fd,"<a href=\"%s/\"> </a>\n",ent->d_name);
      }

 closedir(dir);

 fchdir(fSpoolDir);
}


/*++++++++++++++++++++++++++++++++++++++
  Create an index of the pages on a host.

  int fd The file descriptor to write to.

  char *proto The protocol to index.

  char *host The name of the subdirectory.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexHost(int fd,char *proto,char *host)
{
#if defined(__CYGWIN__)
 int i;
#endif
 DIR *dir;
 struct dirent* ent;
 char *url;

 /* Open the spool subdirectory. */

 if(chdir(proto))
    return;

 /* Open the spool subdirectory. */

#if defined(__CYGWIN__)
 for(i=0;host[i];i++)
    if(host[i]==':')
       host[i]='!';
#endif

 if(chdir(host))
   {fchdir(fSpoolDir);return;}

 dir=opendir(".");
 if(!dir)
   {fchdir(fSpoolDir);return;}

 ent=readdir(dir);  /* skip .  */
 if(!ent)
   {closedir(dir);fchdir(fSpoolDir);return;}
 ent=readdir(dir);  /* skip .. */

 /* Output all of the file names. */

 while((ent=readdir(dir)))
    if(*ent->d_name=='D' && (url=FileNameToURL(ent->d_name)))
      {
       URL *Url=SplitURL(url);

       if(Url->Protocol->number==Protocol_HTTP)
          write_formatted(fd,"<a href=\"%s\"> </a>\n",Url->name);
       else
          write_formatted(fd,"<a href=\"/%s/%s\"> </a>\n",Url->proto,Url->hostp);

       FreeURL(Url);
       free(url);
      }

 closedir(dir);

 fchdir(fSpoolDir);
}


/*++++++++++++++++++++++++++++++++++++++
  Create an index of the lasttime accessed pages.

  int fd The file descriptor to write to.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexLastTime(int fd)
{
 DIR *dir;
 struct dirent* ent;
 char *url;

 /* Open the spool subdirectory. */

 if(chdir("lasttime"))
    return;

 dir=opendir(".");
 if(!dir)
   {fchdir(fSpoolDir);return;}

 ent=readdir(dir);  /* skip .  */
 if(!ent)
   {closedir(dir);fchdir(fSpoolDir);return;}
 ent=readdir(dir);  /* skip .. */

 /* Output all of the file names. */

 while((ent=readdir(dir)))
    if(*ent->d_name=='D' && (url=FileNameToURL(ent->d_name)))
      {
       URL *Url=SplitURL(url);

       if(Url->Protocol->number==Protocol_HTTP)
          write_formatted(fd,"<a href=\"%s\"> </a>\n",Url->name);
       else
          write_formatted(fd,"<a href=\"/%s/%s\"> </a>\n",Url->proto,Url->hostp);

       FreeURL(Url);
       free(url);
      }

 closedir(dir);

 fchdir(fSpoolDir);
}


/*++++++++++++++++++++++++++++++++++++++
  Perform an htdig search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static void HTSearch(int fd,char *args)
{
 pid_t pid;

 if((pid=fork())==-1)
   PrintMessage(Warning,"Cannot fork to call htsearch; [%!s].");
 else if(pid)
   {
    int status;

    wait(&status);

    if(WIFEXITED(status) && WEXITSTATUS(status)==0)
       return;
   }
 else
   {
    if(fd!=1)
      {
       close(1);
       dup(fd);
       close(fd);
      }

    write_string(1,"HTTP/1.0 200 htsearch output\r\n");

    putenv("REQUEST_METHOD=GET");
    if(args)
      {
       char *query=(char*)malloc(strlen(args)+16);
       sprintf(query,"QUERY_STRING=%s",args);
       putenv(query);
      }
    else
       putenv("QUERY_STRING=");
    putenv("SCRIPT_NAME=htsearch");
    execl("search/htdig/scripts/wwwoffle-htsearch","search/htdig/scripts/wwwoffle-htsearch",NULL);
    PrintMessage(Warning,"Cannot exec search/htdig/scripts/wwwoffle-htsearch; [%!s]");
    exit(1);
   }

 lseek(fd,0,SEEK_SET);
 init_buffer(fd);
 ftruncate(fd,0);

 HTMLMessage(fd,500,"WWWOFFLE Server Error",NULL,"ServerError",
             "error","Problem running ht://Dig htsearch program.",
             NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a mnogosearch (udmsearch) search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static void mnoGoSearch(int fd,char *args)
{
 pid_t pid;

 if((pid=fork())==-1)
    PrintMessage(Warning,"Cannot fork to call mnogosearch; [%!s].");
 else if(pid)
   {
    int status;

    wait(&status);

    if(WIFEXITED(status) && WEXITSTATUS(status)==0)
       return;
   }
 else
   {
    if(fd!=1)
      {
       close(1);
       dup(fd);
       close(fd);
      }

    write_string(1,"HTTP/1.0 200 mnogosearch output\r\n");

    putenv("REQUEST_METHOD=GET");
    if(args)
      {
       char *query=(char*)malloc(strlen(args)+16);
       sprintf(query,"QUERY_STRING=%s",args);
       putenv(query);
      }
    else
       putenv("QUERY_STRING=");
    putenv("SCRIPT_NAME=mnogosearch");
    execl("search/mnogosearch/scripts/wwwoffle-mnogosearch","search/mnogosearch/scripts/wwwoffle-mnogosearch",NULL);
    PrintMessage(Warning,"Cannot exec search/mnogosearch/scripts/wwwoffle-mnogosearch; [%!s]");
    exit(1);
   }

 lseek(fd,0,SEEK_SET);
 init_buffer(fd);
 ftruncate(fd,0);

 HTMLMessage(fd,500,"WWWOFFLE Server Error",NULL,"ServerError",
             "error","Problem running mnoGoSearch search program.",
             NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a Namazu search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static void Namazu(int fd,char *args)
{
 pid_t pid;

 if((pid=fork())==-1)
    PrintMessage(Warning,"Cannot fork to call namazu; [%!s].");
 else if(pid)
   {
    int status;

    wait(&status);

    if(WIFEXITED(status) && WEXITSTATUS(status)==0)
       return;
   }
 else
   {
    if(fd!=1)
      {
       close(1);
       dup(fd);
       close(fd);
      }

    write_string(1,"HTTP/1.0 200 namazu output\r\n");

    putenv("REQUEST_METHOD=GET");
    if(args)
      {
       char *query=(char*)malloc(strlen(args)+16);
       sprintf(query,"QUERY_STRING=%s",args);
       putenv(query);
      }
    else
       putenv("QUERY_STRING=");
    putenv("SCRIPT_NAME=namazu");
    execl("search/namazu/scripts/wwwoffle-namazu","search/namazu/scripts/wwwoffle-namazu",NULL);
    PrintMessage(Warning,"Cannot exec search/namazu/scripts/wwwoffle-namazu; [%!s]");
    exit(1);
   }

 lseek(fd,0,SEEK_SET);
 init_buffer(fd);
 ftruncate(fd,0);

 HTMLMessage(fd,500,"WWWOFFLE Server Error",NULL,"ServerError",
             "error","Problem running Namazu search program.",
             NULL);
}
