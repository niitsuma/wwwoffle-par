/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/local.c 1.11 2004/01/11 10:28:20 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Serve the local web-pages and handle the language selection.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02,03,04 Andrew M. Bishop
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

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "headbody.h"
#include "errors.h"
#include "config.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif


static char *FindLanguageFile(char* search, struct stat *buf);
static char /*@null@*/ /*@observer@*/ **get_languages(int *ndirs);


/*+ The language header that the client sent. +*/
static HeaderList /*@null@*/ /*@only@*/ *accept_languages=NULL;
static int num_lang_dirs=0;
static char **lang_dirs=NULL;

#ifndef TEST_ONLY
/*++++++++++++++++++++++++++++++++++++++
  Output a local page.
  LocalPage writes the content directly to the client and returns -1,
            or it uses a temporary file and returns its file descriptor.

  int fd The file descriptor to write to.

  URL *Url The URL for the local page.

  Header *request_head The request that was made for this page.

  Body *request_body The body of the request that was made for this page.
  ++++++++++++++++++++++++++++++++++++++*/

int LocalPage(int fd,URL *Url,Header *request_head,Body *request_body)
{
 int tmpfd=-1,found=0;
 char *file,*path;
 struct stat buf;

 /* Don't allow paths backwards */

 if(strstr(Url->path,"/../"))
   {
    PrintMessage(Warning,"Illegal path containing '/../' for the local page '%s'.",Url->path);
    HTMLMessage(fd,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
                "url",Url->path,
                NULL);
    return 0;
   }

 /* Get the filename */

 path=URLDecodeGeneric(Url->path+1);

 if((file=FindLanguageFile(path,&buf)))
   {
    if(S_ISREG(buf.st_mode) && buf.st_mode&S_IROTH)
      {
       if(buf.st_mode&S_IXOTH && IsCGIAllowed(Url->path))
         {
	  tmpfd=CreateTempSpoolFile();
	  if(tmpfd==-1) {
	    PrintMessage(Warning,"Cannot open temporary spool file; [%!s].");
	    HTMLMessage(fd,500,"WWWOFFLE Server Error",NULL,"ServerError",
			"error","Cannot open temporary file.",
			NULL);
	  }
	  else if(!LocalCGI(tmpfd,file,Url,request_head,request_body)) {
	    HTMLMessage(fd,500,"WWWOFFLE Local Program Execution Error",NULL,"ExecCGIFailed",
			"url",Url->path,
			NULL);
	    CloseTempSpoolFile(tmpfd);
	    tmpfd=-1;

	  }
          found=1;
         }
       else
         {
          int htmlfd=open(file,O_RDONLY|O_BINARY);

          if(htmlfd==-1)
             PrintMessage(Warning,"Cannot open the local page '%s' [%!s].",file);
          else
            {
             time_t since=0;

	     init_io(htmlfd);

             PrintMessage(Debug,"Using the local page '%s'.",file);

             {
	       char *ims=GetHeader(request_head,"If-Modified-Since");
	       if(ims) since=DateToTimeT(ims);
	     }

             if(since>=buf.st_mtime) {
                HTMLMessageHead(fd,304,"WWWOFFLE Not Modified",
                                "Content-Length","0",
				NULL);
		if(out_err==-1)
		  PrintMessage(Warning,"Cannot write local page '%s' to client [%!s].",file);
	     }
             else
               {
		 {
		   char date[MAXDATESIZE]; char length[24];

		   RFC822Date_r(buf.st_mtime,1,date);
		   sprintf(length,"%lu",(unsigned long)buf.st_size);

		   HTMLMessageHead(fd,200,"WWWOFFLE Local OK",
				   "Last-Modified",date,
				   "Content-Type",WhatMIMEType(file),
				   "Content-Length",length,
				   NULL);
		 }

		 if(out_err==-1)
		   PrintMessage(Warning,"Cannot write local page '%s' to client [%!s].",file);
		 else if(!head_only) {
		   char buffer[READ_BUFFER_SIZE];
		   int n;
		   for(;;) {
		     n=read_data(htmlfd,buffer,READ_BUFFER_SIZE);
		     if(n<0)
		       PrintMessage(Warning,"Cannot read local page '%s' [%!s].",file);
		     if(n<=0)
		       break;
		     if(write_data(fd,buffer,n)<0) {
		       PrintMessage(Warning,"Cannot write local page '%s' to client [%!s].",file);
		       break;
		     }
		   }
		 }
               }

             finish_io(htmlfd);
             close(htmlfd);

             found=1;
            }
         }
      }
    else if(S_ISDIR(buf.st_mode))
      {
       char *localhost=GetLocalHost(1);
       char *dir=(char*)malloc(strlen(Url->path)+strlen(localhost)+sizeof("http:///index.html"));
       char *p;

       PrintMessage(Debug,"Using the local directory '%s'.",file);

       p=stpcpy(stpcpy(stpcpy(dir,"http://"),localhost),Url->path);
       if(*(p-1)!='/') *p++='/';
       stpcpy(p,"index.html");
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

 free(path);

 if(!found)
   {
    PrintMessage(Warning,"Cannot find a local URL '%s'.",Url->path);
    HTMLMessage(fd,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
                "url",Url->path,
                NULL);
   }

 return tmpfd;
}
#endif


/*++++++++++++++++++++++++++++++++++++++
  Set the language that will be accepted for the messages.

  Header *head The header.
  ++++++++++++++++++++++++++++++++++++++*/

void SetLanguage(Header *head)
{
  HeaderList *accept_list=GetHeaderList(head,"Accept-Language");
  if(accept_list)
    {
      if(accept_languages) FreeHeaderList(accept_languages);
      if(lang_dirs) {
	int i;
	for(i=0;i<num_lang_dirs;++i)
	  free(lang_dirs[i]);
	free(lang_dirs);
      }

      accept_languages=accept_list;
      num_lang_dirs=-1;   /* negative value signals accept_language has changed */
      lang_dirs=NULL;
    }
}


/*++++++++++++++++++++++++++++++++++++++
  Find the language specific message file.

  char *FindLanguageFile Returns the file name or NULL.

  char* search The name of the file to search for (e.g. 'messages/foo.html' or 'local/bar.html').
  struct stat *buf  Information about the attributes of the file.
  ++++++++++++++++++++++++++++++++++++++*/

static char *FindLanguageFile(char* search, struct stat *buf)
{
 char *file=NULL;
 int idir=0;
 char **dirs;
 int ndirs=0;

 /* Get the list of directories for languages. */

 dirs=get_languages(&ndirs);

 /* Find the file */

 if(!strcmp_litbeg(search,"local/"))
    idir=-1;

 for(; idir<(ndirs+2); ++idir)
   {
    char *dir="",*tryfile;

    if(idir<0)
       ;                        /* must include "local" at the start. */
    else if(idir<ndirs)
       dir=dirs[idir];          /* is formatted like "html/$LANG/" */
    else if(idir==ndirs)
       dir="html/default/";
    else if(idir==(ndirs+1))
       dir="html/en/";

    tryfile=(char*)malloc(strlen(dir)+strlen(search)+1);
    stpcpy(stpcpy(tryfile,dir),search);

    if(!stat(tryfile,buf))
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
 struct stat buf;

 /* Find the file. */

 file=FindLanguageFile(search,&buf);

 if(file)
   {
     fd=open(file,O_RDONLY|O_BINARY);

     free(file);
   }

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the language list and construct a list of directories.

  char **get_languages Returns the list of directories to try.

  int *ndirs Returns the number of directories in the list.
  ++++++++++++++++++++++++++++++++++++++*/

static char **get_languages(int *ndirs)
{
  if(num_lang_dirs<0)  /* negative value signals that accept_languages has changed */
   {
    num_lang_dirs=0;

    if(accept_languages)
      {
       int i;

       lang_dirs=(char**)malloc(sizeof(char*)*(accept_languages->n));

       for(i=0;i<accept_languages->n;++i)
	 {
	   HeaderListItem *item=&(accept_languages->item[i]);
	   if(item->qval>0 && isalpha(*(item->val)) && !strchr(item->val,'/'))
	     {
	       char *p,*q;

	       lang_dirs[num_lang_dirs++]= p= (char*)malloc(strlen(item->val)+sizeof("html//"));
	       p= stpcpy(p,"html/");
	       q= item->val;
	       while(*q && isalpha(*q))
		 *p++=tolower(*q++);
	       *p++='/';
	       *p=0;
	     }
	 }
      }
   }

 *ndirs=num_lang_dirs;

 return(lang_dirs);
}
