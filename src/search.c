/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/search.c 1.39 2005/09/04 15:56:20 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Handle the interface to the ht://Dig, mnoGoSearch (UdmSearch), Namazu and Hyper Estraier search engines.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2005,2006 Paul A. Rombouts
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
#include <errno.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "proto.h"


/* Local functions */

static void SearchIndex(int fd,URL *Url);
static void SearchIndexRoot(int fd);
static void SearchIndexProtocol(int fd,char *proto);
static void SearchIndexHost(int fd,char *proto,char *host);
static void SearchIndexLastTime(int fd);

static int HTSearch(int fd,char *args);
static int mnoGoSearch(int fd,char *args);
static int Namazu(int fd,char *args);
static int EstSeek(int fd,char *args);

static int SearchScript(int fd,char *args,char *name,char *script,char *path);


/*++++++++++++++++++++++++++++++++++++++
  Create a page for one of the search pages on the local server.

  SearchPage writes the content directly to the client and returns -1,
            or it uses a temporary file and returns its file descriptor.

  int fd The file descriptor (of the client) to write the output to.

  URL *Url The URL that specifies the path for the page.

  Header *request_head The head of the request.

  Body *request_body The body of the request that was made for this page.
  ++++++++++++++++++++++++++++++++++++++*/

int SearchPage(int fd,URL *Url,Header *request_head,Body *request_body)
{
 char *newpath=Url->path+strlitlen("/search/");
 int tmpfd=-1;

 if(!strcmp_litbeg(newpath,"index/"))
    SearchIndex(fd,Url);
 else if(!strcmp(newpath,"htdig/htsearch")) {
   if((tmpfd=CreateTempSpoolFile())==-1) goto tmpfderror;
   if(!HTSearch(tmpfd,Url->args)) goto scriptfailed;
 }
 else if(!strcmp(newpath,"mnogosearch/mnogosearch") ||
	 !strcmp(newpath,"udmsearch/udmsearch"))
 {
   if((tmpfd=CreateTempSpoolFile())==-1) goto tmpfderror;
   if(!mnoGoSearch(tmpfd,Url->args)) goto scriptfailed;
 }
 else if(!strcmp(newpath,"namazu/namazu")) {
   if((tmpfd=CreateTempSpoolFile())==-1) goto tmpfderror;
   if(!Namazu(tmpfd,Url->args)) goto scriptfailed;
 }
 else if(!strcmp(newpath,"hyperestraier/estseek")) {
   if((tmpfd=CreateTempSpoolFile())==-1) goto tmpfderror;
   if(!EstSeek(fd,Url->args)) goto scriptfailed;
 }
 else if((!strcmp_litbeg(newpath,"htdig/") && !strchr(newpath+strlitlen("htdig/"),'/')) ||
	 (!strcmp_litbeg(newpath,"mnogosearch/") && !strchr(newpath+strlitlen("mnogosearch/"),'/')) ||
	 (!strcmp_litbeg(newpath,"udmsearch/") && !strchr(newpath+strlitlen("udmsearch/"),'/')) ||
	 (!strcmp_litbeg(newpath,"namazu/") && !strchr(newpath+strlitlen("namazu/"),'/')) ||
	 (!strcmp_litbeg(newpath,"hyperestraier/") && !strchr(newpath+strlitlen("hyperestraier/"),'/')) ||
	 (!strchr(newpath,'/')))
    tmpfd=LocalPage(fd,Url,request_head,request_body);
 else
    HTMLMessage(fd,404,"WWWOFFLE Illegal Search Page",NULL,"SearchIllegal",
                "url",Url->path,
                NULL);

 return tmpfd;

tmpfderror:
 PrintMessage(Warning,"Cannot open temporary spool file; [%!s].");
 HTMLMessage(fd,500,"WWWOFFLE Server Error",NULL,"ServerError",
	     "error","Cannot open temporary file.",
	     NULL);
 return -1;

scriptfailed:
 HTMLMessage(fd,500,"WWWOFFLE Server Error",NULL,"ServerError",
	     "error","Problem running search program.",
	     NULL);
 CloseTempSpoolFile(tmpfd);
 return -1;
}


/*++++++++++++++++++++++++++++++++++++++
  Produce one of the indexes for htdig or mnoGoSearch (UdmSearch) to search.

  int fd The file descriptor to write to.

  URL *Url The URL that was requested.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndex(int fd,URL *Url)
{
 char *proto,*host="";
 int lasttime;

 proto=strdupa(Url->path+strlitlen("/search/index/"));
 {char *p=strchrnul(proto,0)-1; if(p>=proto && *p=='/') *p=0;}

 lasttime=!strcmp(proto,"lasttime");

 {char *p=strchr(proto,'/'); if(p) {*p=0; host=p+1;}}

 if(*proto)
   {
    int i;
    for(i=0;i<NProtocols;++i)
       if(!strcmp(Protocols[i].name,proto))
          goto found_protocol;

    *proto=0;
   found_protocol: ;
   }

 if(!lasttime &&
    ((*host && (!*proto || strchr(host,'/') || !strcmp(host,"..") || !strcmp(host,"."))) ||
     (Url->path[strlitlen("/search/index/")] && !*proto)))
    HTMLMessage(fd,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
                "url",Url->name,
                NULL);
 else
   {
    HTMLMessageHead(fd,200,"WWWOFFLE Search Index",
                    NULL);
    if(out_err==-1 || head_only) return;
    out_err=write_string(fd,"<html>\n"
			 "<head>\n"
			 "<meta name=\"robots\" content=\"noindex\">\n"
			 "<title></title>\n"
			 "</head>\n"
			 "<body>\n");
    if(out_err==-1) return;

    if(lasttime)
       SearchIndexLastTime(fd);
    else if(!*host && !*proto)
       SearchIndexRoot(fd);
    else if(!*host)
       SearchIndexProtocol(fd,proto);
    else
       SearchIndexHost(fd,proto,host);

    if(out_err==-1) return;

    out_err=write_string(fd,"</body>\n"
			 "</html>\n");
   }

}


/*++++++++++++++++++++++++++++++++++++++
  Index the root of the cache.

  int fd The file descriptor to write to.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexRoot(int fd)
{
 int i;

 for(i=0;i<NProtocols;i++)
    out_err=write_formatted(fd,"<a href=\"%s/\"> </a>\n",Protocols[i].name);
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

 /* Open the spool directory. */

 if(chdir(proto))
    return;

 dir=opendir(".");
 if(!dir)
   {ChangeBackToSpoolDir();return;}

 ent=readdir(dir);
 if(!ent)
   {closedir(dir);ChangeBackToSpoolDir();return;}

 /* Output all of the host sub-directories. */

 do
   {
    struct stat buf;

    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(!stat(ent->d_name,&buf) && S_ISDIR(buf.st_mode))
      {
       out_err=write_formatted(fd,"<a href=\"%s/\"> </a>\n",ent->d_name);
       if(out_err==-1) break;
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Create an index of the pages on a host.

  int fd The file descriptor to write to.

  char *proto The protocol to index.

  char *host The name of the subdirectory.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexHost(int fd,char *proto,char *host)
{
 DIR *dir;
 struct dirent* ent;

 /* Open the spool subdirectory. */

 if(chdir(proto))
    return;

 /* Open the spool subdirectory. */

 if(chdir(host))
   {ChangeBackToSpoolDir();return;}

 dir=opendir(".");
 if(!dir)
   {ChangeBackToSpoolDir();return;}

 ent=readdir(dir);
 if(!ent)
   {closedir(dir);ChangeBackToSpoolDir();return;}

 /* Output all of the file names. */

 do
   {
    URL *Url;

    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(*ent->d_name=='D' && (Url=FileNameToURL(ent->d_name)))
      {
       if(!strcmp(Url->proto,"http"))
          out_err=write_formatted(fd,"<a href=\"%s\"> </a>\n",Url->name);
       else
          out_err=write_formatted(fd,"<a href=\"/%s/%s\"> </a>\n",Url->proto,Url->hostp);

       FreeURL(Url);
       if(out_err==-1) break;
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Create an index of the lasttime accessed pages.

  int fd The file descriptor to write to.
  ++++++++++++++++++++++++++++++++++++++*/

static void SearchIndexLastTime(int fd)
{
 DIR *dir;
 struct dirent* ent;

 /* Open the spool subdirectory. */

 if(chdir("lasttime"))
    return;

 dir=opendir(".");
 if(!dir)
   {ChangeBackToSpoolDir();return;}

 ent=readdir(dir);
 if(!ent)
   {closedir(dir);ChangeBackToSpoolDir();return;}

 /* Output all of the file names. */

 do
   {
    URL *Url;

    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(*ent->d_name=='D' && (Url=FileNameToURL(ent->d_name)))
      {
       if(!strcmp(Url->proto,"http"))
          out_err=write_formatted(fd,"<a href=\"%s\"> </a>\n",Url->name);
       else
          out_err=write_formatted(fd,"<a href=\"/%s/%s\"> </a>\n",Url->proto,Url->hostp);

       FreeURL(Url);
       if(out_err==-1) break;
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Perform an htdig search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static int HTSearch(int fd,char *args)
{
 return SearchScript(fd,args,"htdig","htsearch","search/htdig/scripts/wwwoffle-htsearch");
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a mnogosearch (udmsearch) search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static int mnoGoSearch(int fd,char *args)
{
 return SearchScript(fd,args,"mnogosearch","mnogosearch","search/mnogosearch/scripts/wwwoffle-mnogosearch");
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a Namazu search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static int Namazu(int fd,char *args)
{
 return SearchScript(fd,args,"namazu","namazu","search/namazu/scripts/wwwoffle-namazu");
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a Hyper Estraier search using the data from the posted form.

  int fd The file descriptor to write to.

  char *args The arguments of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static int EstSeek(int fd,char *args)
{
 return SearchScript(fd,args,"hyperestraier","estseek","search/hyperestraier/scripts/wwwoffle-estseek");
}


/*++++++++++++++++++++++++++++++++++++++
  A macro definition that makes environment variable setting a little easier.

  varname The environment variable that is to be set.

  value The value of the environment variable.
  ++++++++++++++++++++++++++++++++++++++*/

#define putenv_var_val(varname,value) \
{ \
  char *envstr = (char *)malloc(sizeof(varname "=")+strlen(value)); \
  stpcpy(stpcpy(envstr,varname "="),value); \
 if(putenv(envstr)==-1) \
     env_err=1; \
}


/*++++++++++++++++++++++++++++++++++++++
  Perform a search using one of the three search methods.

  int fd The file descriptor to write to.

  char *args The arguments of the request.

  char *name The name of the search method.

  char* script The name of the script.

  char* path The path to the script to execute.
  ++++++++++++++++++++++++++++++++++++++*/

static int SearchScript(int fd,char *args,char *name,char *script,char *path)
{
 pid_t pid;

 if((pid=fork())==-1)
   {
    PrintMessage(Warning,"Cannot fork to call %s search script; [%!s].",name);
   }
 else if(pid) /* parent */
   {
    int status;

    if(waitpid(pid, &status, 0)!=-1 && WIFEXITED(status) && WEXITSTATUS(status)==0)
       return 1;
   }
 else /* child */
   {
    int cgi_fd;
    int env_err=0;

    /* Set up stdout (fd is the only file descriptor we *must* keep). */

    if(fd!=STDOUT_FILENO)
      {
       if(dup2(fd,STDOUT_FILENO)==-1)
          PrintMessage(Fatal,"Cannnot create standard output for %s search script [%!s].",name);
       /* finish_io(fd); */
       close(fd);
       /* init_io(STDOUT_FILENO); */
      }

    /* Set up stdin and stderr. */

    cgi_fd=open("/dev/null",O_RDONLY);
    if(cgi_fd!=STDIN_FILENO)
      {
       if(dup2(cgi_fd,STDIN_FILENO)==-1)
          PrintMessage(Fatal,"Cannnot create standard input for %s search script [%!s].",name);
       close(cgi_fd);
      }

    if(fcntl(STDERR_FILENO,F_GETFL,0)==-1 && errno==EBADF) /* stderr is not open */
      {
       cgi_fd=open("/dev/null",O_WRONLY);

       if(cgi_fd!=STDERR_FILENO)
         {
          if(dup2(cgi_fd,STDERR_FILENO)==-1)
             PrintMessage(Fatal,"Cannnot create standard error for %s search script [%!s].",name);
          close(cgi_fd);
         }
      }

    write_formatted(STDOUT_FILENO,"HTTP/1.0 200 %s search output\r\n",name);

    /* Set up the environment. */

    /*@-mustfreefresh@*/

    putenv_var_val("REQUEST_METHOD","GET");

    if(args)
       putenv_var_val("QUERY_STRING",args)
    else
       putenv("QUERY_STRING=");

    putenv_var_val("SCRIPT_NAME",script);

    if(env_err)
       PrintMessage(Fatal,"Failed to create environment for %s search script [%!s].",name);

    /*@=mustfreefresh@*/

    execl(path,path,NULL);
    PrintMessage(Warning,"Cannot exec %s search script %s [%!s]",name,path);
    exit(1);
   }

 return 0;
}
