/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Serve the local web-pages and handle the language selection.
  ******************/ /******************
  Originally written by Andrew M. Bishop.
  Extensively modified by Paul A. Rombouts.

  This file Copyright 1998,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007,2008 Paul A. Rombouts
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


#ifndef O_BINARY
/*+ A work-around for needing O_BINARY with Win32 to use binary mode. +*/
#define O_BINARY 0
#endif


/* Local functions */

static char /*@null@*/ /*@observer@*/ **get_languages(int *ndirs);
static char /*@null@*/ *find_language_file(char* search, struct stat *buf);


/* Local variables */

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

 if((file=find_language_file(path,&buf)))
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
		/* if(out_err==-1)
		     PrintMessage(Inform,"Cannot write local page '%s' to client [%!s].",file); */
	     }
             else
               {
		 int chunked;
		 {
		   unsigned long content_length;
		   char date[MAXDATESIZE]; char length[MAX_INT_STR+1];

		   RFC822Date_r(buf.st_mtime,1,date);
		   content_length= buf.st_size;
		   sprintf(length,"%lu",content_length);

		   chunked=HTMLMessageHead(fd,200,"WWWOFFLE Local OK",
				   "Last-Modified",date,
				   "Content-Type",WhatMIMEType(file),
				   "Content-Length",length,
				   NULL);
		   if(!chunked)
		     configure_io_content_length(htmlfd,content_length);
		 }

		 if(out_err==-1)
		   PrintMessage(Inform,"Cannot write local page '%s' to client [%!s].",file);
		 else if(!head_only) {
		   char buffer[IO_BUFFER_SIZE];
		   ssize_t n;
		   for(;;) {
		     n=read_data(htmlfd,buffer,IO_BUFFER_SIZE);
		     if(n<0) {
		       PrintMessage(Warning,"Cannot read local page '%s' [%!s].",file);
		       if(!chunked)
			 client_keep_connection=0;
		       break;
		     }
		     else if(n==0) {
		       if(client_keep_connection && !chunked) {
			 unsigned long content_remaining= io_content_remaining(htmlfd);
			 if(content_remaining) {
			   /* This is very unlikely, but it might happen
			      if the file is modified while we are reading it. */
			   PrintMessage(Inform, (content_remaining!=CUNDEF)?
					"Could not read remaining raw content (%lu bytes) from local file, "
					"dropping persistent connection to client.":
					"Length of remaining raw content of local file is undefined, "
					"dropping persistent connection to client.",
					content_remaining);
			   client_keep_connection=0;
			 }
		       }
		       break;
		     }

		     if(write_data(fd,buffer,n)<0) {
		       PrintMessage(Inform,"Cannot write local page '%s' to client [%!s].",file);
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
       char *localurl=GetLocalURL();
       char *dir=(char*)malloc(strlen(localurl)+strlen(Url->path)+sizeof("/index.html"));
       char *p;

       PrintMessage(Debug,"Using the local directory '%s'.",file);

       p=stpcpy(stpcpy(dir,localurl),Url->path);
       if(p==dir || *(p-1)!='/') *p++='/';
       stpcpy(p,"index.html");
       HTMLMessage(fd,302,"WWWOFFLE Local Dir Redirect",dir,"Redirect",
                   "location",dir,
                   NULL);

       free(dir);
       free(localurl);

       found=1;
      }
    else
       PrintMessage(Warning,"Not a regular file or wrong permissions for the local page '%s'.",file);

    free(file);
   }

 free(path);

 if(!found)
   {
    PrintMessage(Inform,"Cannot find a local URL '%s'.",Url->path);
    HTMLMessage(fd,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
                "url",Url->path,
                NULL);
   }

 return tmpfd;
}
#endif


/* OpenLocalFile opens a local file and returns the file descriptor.
   char *path: the path to the file (rooted in the WWWOFFLE spool directory).
*/
int OpenLocalFile(char *path)
{
 int fd=-1;
 char *file,*decpath;
 struct stat buf;

 if(path[0]!='/') {
    PrintMessage(Warning,"Path '%s' to local file should begin with '/'.",path);
    return -1;
 }    

 /* Don't allow paths backwards */

 if(strstr(path,"/../"))
   {
    PrintMessage(Warning,"Illegal path containing '/../' for the local file '%s'.",path);
    return -1;
   }

 /* Get the filename */

 decpath=URLDecodeGeneric(path+1);

 if((file=find_language_file(decpath,&buf))) {
   if(S_ISREG(buf.st_mode) && buf.st_mode&S_IROTH) {
     fd=open(file,O_RDONLY|O_BINARY);

     if(fd==-1)
       PrintMessage(Warning,"Cannot open the local file '%s' [%!s].",file);
   }
   else
     PrintMessage(Warning,"Not a regular file or wrong permissions for the local file '%s'.",file);

   free(file);
 }
 else
    PrintMessage(Warning,"Cannot find a local file with the path '%s'.",path);

 free(decpath);

 return fd;
}


/*++++++++++++++++++++++++++++++++++++++
  Set (or reset) the languages that will be accepted for the messages.

  Header *head The header (may be NULL).
  ++++++++++++++++++++++++++++++++++++++*/

void SetLanguage(Header *head)
{
  if(accept_languages) FreeHeaderList(accept_languages);
  accept_languages= (head?GetHeaderList(head,"Accept-Language"):NULL);

  if(lang_dirs) {
    int i;
    for(i=0;i<num_lang_dirs;++i)
      free(lang_dirs[i]);
    free(lang_dirs);
  }

  num_lang_dirs=0;
  lang_dirs=NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Find the language specific message file.

  char *find_language_file Returns the file name or NULL.

  char* search The name of the file to search for (e.g. 'messages/foo.html' or 'local/bar.html').
  struct stat *buf  Information about the attributes of the file.
  ++++++++++++++++++++++++++++++++++++++*/

static char *find_language_file(char* search, struct stat *buf)
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

 file=find_language_file(search,&buf);

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
 if(accept_languages)
   {
    int i;

    if(lang_dirs) {for(i=0;i<num_lang_dirs;++i) free(lang_dirs[i]);}
    num_lang_dirs=0;
    lang_dirs=(char**)realloc(lang_dirs,sizeof(char*)*(accept_languages->n));

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

    FreeHeaderList(accept_languages);
    accept_languages=NULL;
   }

 *ndirs=num_lang_dirs;

 return(lang_dirs);
}
