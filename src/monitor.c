/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9e.
  The functions for monitoring URLs.
  ******************/ /******************
  Written by Andrew M. Bishop.
  Modified by Paul A. Rombouts.

  This file Copyright 1998,99,2000,01,02,03,04,05,06,08 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2004,2005,2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
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

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "headbody.h"
#include "errors.h"


#ifndef O_BINARY
/*+ A work-around for needing O_BINARY with Win32 to use binary mode. +*/
#define O_BINARY 0
#endif


/* Local functions */

static void MonitorFormShow(int fd,char *request_args);
static void MonitorFormParse(int fd,URL *Url,char *request_args,/*@null@*/ Body *request_body);


/*++++++++++++++++++++++++++++++++++++++
  Send to the client a page to allow monitoring using HTML.

  int fd The file descriptor of the client.

  URL *Url The URL that was used to request this page.

  Body *request_body The body of the HTTP request.
  ++++++++++++++++++++++++++++++++++++++*/

void MonitorPage(int fd,URL *Url,Body *request_body)
{
 if(!strcmp("/monitor-options/",Url->path))
    MonitorFormShow(fd,Url->args);
 else if(!strcmp("/monitor-request/",Url->path))
    MonitorFormParse(fd,Url,Url->args,request_body);
 else
    HTMLMessage(fd,404,"WWWOFFLE Illegal Monitor Page",NULL,"MonitorIllegal",
                "url",Url->pathp,
                NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Create the the form.

  int fd The file descriptor of the client.

  char *request_args The arguments to the requesting URL.
  ++++++++++++++++++++++++++++++++++++++*/

static void MonitorFormShow(int fd,char *request_args)
{
 char MofY[13],DofM[32],DofW[8],HofD[25];
 char dofmstr[128],hofdstr[64],*p;
 char *url=NULL;
 int i,exists=0;

 if(request_args)
    url=URLDecodeFormArgs(request_args);

 if(url)
   {
    URL *monUrl=SplitURL(url);
    exists=ReadMonitorTimesSpoolFile(monUrl,MofY,DofM,DofW,HofD);
    FreeURL(monUrl);
   }
 else
   {
    strcpy(MofY,"111111111111");
    strcpy(DofM,"1111111111111111111111111111111");
    strcpy(DofW,"1111111");
    strcpy(HofD,"100000000000000000000000");
   }

 if(!strcmp(DofM,"1111111111111111111111111111111"))
    strcpy(dofmstr,"*");
 else
   {
    *dofmstr=0;
    p=dofmstr;
    for(i=0;i<31;i++)
       if(DofM[i]=='1')
         {
          sprintf(p,"%d ",i+1);
          p+=strlen(p);
         }
   }

 if(!strcmp(HofD,"100000000000000000000000"))
    strcpy(hofdstr,"0");
 else
   {
    *hofdstr=0;
    p=hofdstr;
    for(i=0;i<24;i++)
       if(HofD[i]=='1')
         {
          sprintf(p,"%d ",i);
          p+=strlen(p);
         }
   }

 HTMLMessage(fd,200,"WWWOFFLE Monitor Page",NULL,"MonitorPage",
             "url",url,
             "exists",exists==0?NULL:"Yes",
             "mofy1",MofY[0]=='1'?"Yes":NULL,
             "mofy2",MofY[1]=='1'?"Yes":NULL,
             "mofy3",MofY[2]=='1'?"Yes":NULL,
             "mofy4",MofY[3]=='1'?"Yes":NULL,
             "mofy5",MofY[4]=='1'?"Yes":NULL,
             "mofy6",MofY[5]=='1'?"Yes":NULL,
             "mofy7",MofY[6]=='1'?"Yes":NULL,
             "mofy8",MofY[7]=='1'?"Yes":NULL,
             "mofy9",MofY[8]=='1'?"Yes":NULL,
             "mofy10",MofY[9]=='1'?"Yes":NULL,
             "mofy11",MofY[10]=='1'?"Yes":NULL,
             "mofy12",MofY[11]=='1'?"Yes":NULL,
             "dofm",dofmstr,
             "dofw0",DofW[0]=='1'?"Yes":NULL,
             "dofw1",DofW[1]=='1'?"Yes":NULL,
             "dofw2",DofW[2]=='1'?"Yes":NULL,
             "dofw3",DofW[3]=='1'?"Yes":NULL,
             "dofw4",DofW[4]=='1'?"Yes":NULL,
             "dofw5",DofW[5]=='1'?"Yes":NULL,
             "dofw6",DofW[6]=='1'?"Yes":NULL,
             "hofd",hofdstr,
             NULL);

 if(url)
    free(url);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the reply from the form.

  int fd The file descriptor of the client.

  URL *Url The URL of the form that is being processed.

  char *request_args The arguments to the requesting URL.

  Body *request_body The body of the HTTP request sent by the client.
  ++++++++++++++++++++++++++++++++++++++*/

static void MonitorFormParse(int fd,URL *Url,char *request_args,Body *request_body)
{
 int i,mfd=-1;
 char **args,*url=NULL;
 URL *monUrl;
 char mofy[13]="NNNNNNNNNNNN",*dofm=NULL,dofw[8]="NNNNNNN",*hofd=NULL;
 char MofY[13],DofM[32],DofW[8],HofD[25];

 if((request_body && !request_body->length) || (!request_body && !request_args))
   {
    HTMLMessage(fd,404,"WWWOFFLE Monitor Form Error",NULL,"MonitorFormError",
                "body",NULL,
                NULL);
    return;
   }

 if(request_body)
   {
    char *form=URLRecodeFormArgs(request_body->content);
    args=SplitFormArgs(form);
    free(form);
   }
 else
    args=SplitFormArgs(request_args);

 for(i=0;args[i];i++)
   {
    if(!strcmp_litbeg(args[i],"url="))
       url=TrimArgs(URLDecodeFormArgs(args[i]+strlitlen("url=")));
    else if(!strcmp_litbeg(args[i],"mofy1="))
       mofy[0]=args[i][strlitlen("mofy1=")];
    else if(!strcmp_litbeg(args[i],"mofy2="))
       mofy[1]=args[i][strlitlen("mofy2=")];
    else if(!strcmp_litbeg(args[i],"mofy3="))
       mofy[2]=args[i][strlitlen("mofy3=")];
    else if(!strcmp_litbeg(args[i],"mofy4="))
       mofy[3]=args[i][strlitlen("mofy4=")];
    else if(!strcmp_litbeg(args[i],"mofy5="))
       mofy[4]=args[i][strlitlen("mofy5=")];
    else if(!strcmp_litbeg(args[i],"mofy6="))
       mofy[5]=args[i][strlitlen("mofy6=")];
    else if(!strcmp_litbeg(args[i],"mofy7="))
       mofy[6]=args[i][strlitlen("mofy7=")];
    else if(!strcmp_litbeg(args[i],"mofy8="))
       mofy[7]=args[i][strlitlen("mofy8=")];
    else if(!strcmp_litbeg(args[i],"mofy9="))
       mofy[8]=args[i][strlitlen("mofy9=")];
    else if(!strcmp_litbeg(args[i],"mofy10="))
       mofy[9]=args[i][strlitlen("mofy10=")];
    else if(!strcmp_litbeg(args[i],"mofy11="))
       mofy[10]=args[i][strlitlen("mofy11=")];
    else if(!strcmp_litbeg(args[i],"mofy12="))
       mofy[11]=args[i][strlitlen("mofy12=")];
    else if(!strcmp_litbeg(args[i],"dofm="))
       dofm=args[i]+strlitlen("dofm=");
    else if(!strcmp_litbeg(args[i],"dofw0="))
       dofw[0]=args[i][strlitlen("dofw0=")];
    else if(!strcmp_litbeg(args[i],"dofw1="))
       dofw[1]=args[i][strlitlen("dofw1=")];
    else if(!strcmp_litbeg(args[i],"dofw2="))
       dofw[2]=args[i][strlitlen("dofw2=")];
    else if(!strcmp_litbeg(args[i],"dofw3="))
       dofw[3]=args[i][strlitlen("dofw3=")];
    else if(!strcmp_litbeg(args[i],"dofw4="))
       dofw[4]=args[i][strlitlen("dofw4=")];
    else if(!strcmp_litbeg(args[i],"dofw5="))
       dofw[5]=args[i][strlitlen("dofw5=")];
    else if(!strcmp_litbeg(args[i],"dofw6="))
       dofw[6]=args[i][strlitlen("dofw6=")];
    else if(!strcmp_litbeg(args[i],"hofd="))
       hofd=args[i]+strlitlen("hofd=");
    else
       PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",args[i],Url->name);
   }

 if(url==NULL || *url==0)
   {
    HTMLMessage(fd,404,"WWWOFFLE Monitor Form Error",NULL,"MonitorFormError",
                "body",request_body?request_body->content:request_args,
                NULL);
    if(url) free(url);
    free(args[0]);
    free(args);
    return;
   }

 monUrl=SplitURL(url);
 free(url);

 /* Parse the requested time */

 strcpy(MofY,"000000000000");
 for(i=0;i<12;i++)
    if(mofy[i]=='Y')
       MofY[i]='1';

 if(!dofm || !*dofm)
    strcpy(DofM,"1111111111111111111111111111111");
 else
   {
    int d,any=0,range=0,lastd=0;
    char *p=dofm;
    strcpy(DofM,"0000000000000000000000000000000");

    while(*p)
      {
       while(*p && !isdigit(*p))
         {
          if(*p=='*')
            {
             strcpy(DofM,"1111111111111111111111111111111");
             any=1;
            }
          if(*p=='-')
             range=1;
          p++;
         }
       if(!*p)
          break;
       d=atoi(p)-1;
       if(range)
         {
          if(d>30)
             d=30;
          for(;lastd<=d;lastd++)
             DofM[lastd]='1';
          range=0;
          any++;
         }
       else if(d>=0 && d<31)
         {
          DofM[d]='1';
          any++;
          lastd=d;
         }
       while(isdigit(*p))
          p++;
      }

    if(range)
       for(;lastd<=30;lastd++)
          DofM[lastd]='1';
    else
       if(!any)
          strcpy(DofM,"1111111111111111111111111111111");
   }

 strcpy(DofW,"0000000");
 for(i=0;i<7;i++)
    if(dofw[i]=='Y')
       DofW[i]='1';

 if(!hofd || !*hofd)
    strcpy(HofD,"100000000000000000000000");
 else
   {
    int h,any=0,range=0,lasth=0;
    char *p=hofd;
    strcpy(HofD,"000000000000000000000000");

    while(*p)
      {
       while(*p && !isdigit(*p))
         {
          if(*p=='*')
            {
             strcpy(HofD,"111111111111111111111111");
             any=1;
            }
          if(*p=='-')
             range=1;
          p++;
         }
       if(!*p)
          break;
       h=atoi(p);
       if(range)
         {
          if(h>23)
             h=23;
          for(;lasth<=h;lasth++)
             HofD[lasth]='1';
          range=0;
          any++;
         }
       else if(h>=0 && h<24)
         {
          HofD[h]='1';
          any++;
          lasth=h;
         }
       while(isdigit(*p))
          p++;
      }

    if(range)
       for(;lasth<=23;lasth++)
          HofD[lasth]='1';
    else
       if(!any)
          strcpy(HofD,"100000000000000000000000");
   }

 mfd=CreateMonitorSpoolFile(monUrl,MofY,DofM,DofW,HofD);

 if(mfd==-1)
    HTMLMessage(fd,500,"WWWOFFLE Server Error",NULL,"ServerError",
                "error","Cannot open file to store monitor request",
                NULL);
 else
   {
    Header *new_request_head=RequestURL(monUrl,NULL);
    size_t headlen;
    char *head=HeaderString(new_request_head,&headlen);

    init_io(mfd);

    write_data(mfd,head,headlen);

    finish_io(mfd);
    close(mfd);

    HTMLMessage(fd,200,"WWWOFFLE Monitor Will Get",NULL,"MonitorWillGet",
                "url",monUrl->name,
                NULL);

    free(head);
    FreeHeader(new_request_head);
   }

 free(args[0]);
 free(args);

 FreeURL(monUrl);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert monitor requests into outgoing requests.
  ++++++++++++++++++++++++++++++++++++++*/

void RequestMonitoredPages(void)
{
 DIR *dir;
 struct dirent* ent;

 /* Open the monitor subdirectory. */

 if(chdir("monitor"))
   {PrintMessage(Warning,"Cannot change to directory 'monitor'; [%!s] no files monitored.");return;}

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory 'monitor'; [%!s] no files monitored.");goto changeback_return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read directory 'monitor'; [%!s] no files monitored.");goto cleanup_return;}

 /* Scan through all of the files. */

 do
   {
    struct stat buf; URL *Url;

    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(stat(ent->d_name,&buf))
      {PrintMessage(Inform,"Cannot stat file 'monitor/%s'; [%!s] race condition?",ent->d_name);return;}
    else if(S_ISREG(buf.st_mode) && *ent->d_name=='O' && (Url=FileNameToURL(ent->d_name)))
      {
       int last,next;

       ChangeBackToSpoolDir();

       MonitorTimes(Url,&last,&next);

       PrintMessage(Debug,"Monitoring '%s' last=%dh next=%dh => %s",Url->name,last,next,next?"No":"Yes");

       if(chdir("monitor"))
	 {PrintMessage(Warning,"Cannot change to directory 'monitor'; [%!s] monitoring aborted.");FreeURL(Url);goto cleanup_return;}

       if(next==0)
         {
          int ifd=open(ent->d_name,O_RDONLY|O_BINARY);

          if(ifd==-1)
             PrintMessage(Warning,"Cannot open monitored file 'monitor/%s' to read; [%!s].",ent->d_name);
          else
            {
             int ofd;

             init_io(ifd);

             ChangeBackToSpoolDir();

             ofd=OpenNewOutgoingSpoolFile();

             if(ofd==-1)
                PrintMessage(Warning,"Cannot open outgoing spool file for monitored URL '%s'; [%!s].",Url->name);
             else
               {
		ssize_t n;
                char buffer[IO_BUFFER_SIZE];

                init_io(ofd);

                while((n=read_data(ifd,buffer,IO_BUFFER_SIZE))>0)
		  if(write_data(ofd,buffer,n)<0) {
		    PrintMessage(Warning,"Cannot write to outgoing file; disk full?");
		    break;
		  }

                finish_io(ofd);
                CloseNewOutgoingSpoolFile(ofd,Url);

               }

             finish_io(ifd);
             close(ifd);

	     if(chdir("monitor"))
	       {PrintMessage(Warning,"Cannot change to directory 'monitor'; [%!s] monitoring aborted.");FreeURL(Url);goto cleanup_return;}

	     {
	       local_URLToFileName(Url,'M',0,file)
	       if(utime(file,NULL))
		 PrintMessage(Warning,"Cannot change timestamp of monitored file 'monitor/%s'; [%!s].",file);
	     }
            }
         }

       FreeURL(Url);
      }
   }
 while((ent=readdir(dir)));

cleanup_return:
 closedir(dir);
changeback_return:
 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Calculate the monitoring interval and times of previous and next monitoring.

  URL *Url The URL of the file to check for.

  int *last The time in hours ago that the file was last monitored.

  int *next The time in hours from now that it should next be monitored.
  ++++++++++++++++++++++++++++++++++++++*/

void MonitorTimes(URL *Url,int *last,int *next)
{
 time_t now=time(NULL),then,when;
 char MofY[13],DofM[32],DofW[8],HofD[25];
 int mofy,dofm,dofw,hofd;
 struct tm *tim;

 then=ReadMonitorTimesSpoolFile(Url,MofY,DofM,DofW,HofD);

 now =3600*(now/3600);
 then=3600*(then/3600);

 if(then>now)
    then=now;

 *next=0;

 mofy=dofm=dofw=hofd=0;
 for(when=then+3600;when<=now;when+=3600)
   {
    if(hofd==0)
      {
       tim=localtime(&when);
       mofy=tim->tm_mon;
       dofm=tim->tm_mday-1;
       dofw=tim->tm_wday;
       hofd=tim->tm_hour;
      }

    if(MofY[mofy]=='1' && DofM[dofm]=='1' && DofW[dofw]=='1' && HofD[hofd]=='1')
      {*next=0;break;}

    hofd=(hofd+1)%24;
   }

 if(when>=now)
   {
    hofd=0;
    for(when=now;when<(now+31*24*3600);when+=3600)
      {
       if(hofd==0)
         {
          tim=localtime(&when);
          mofy=tim->tm_mon;
          dofm=tim->tm_mday-1;
          dofw=tim->tm_wday;
          hofd=tim->tm_hour;
         }

       if(MofY[mofy]=='1' && DofM[dofm]=='1' && DofW[dofw]=='1' && HofD[hofd]=='1')
          break;

       hofd=(hofd+1)%24;
      }

    *next=(when-now)/3600;
   }

 *last=(now-then)/3600;
}
