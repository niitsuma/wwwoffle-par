/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/info.c 1.16 2004/08/25 18:22:21 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8d.
  Generate information about the contents of the web pages that are cached in the system.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 2002,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

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

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "config.h"
#include "document.h"


static void InfoCachedPage(int fd,URL *Url,int which);

static void InfoCached(int fd,int spool,URL *Url,/*@null@*/ Header *spooled_head);
static void InfoContents(int fd,int spool,URL *Url);
static void output_content(int fd,char *type,char **url);
static int sort_alpha(char **a,char **b);
static void InfoSource(int fd,int spool,URL *Url,/*@null@*/ Header *spooled_head);

static void InfoRequestPage(int fd,URL *Url);
static void InfoRequestedPage(int fd,URL *Url,Header *request_head,/*@null@*/ Body *request_body);


/*++++++++++++++++++++++++++++++++++++++
  Display some information about the spooled copy of the URL.

  int fd The file descriptor to write the output to.

  URL *Url The URL that was requested for the info.

  Header *request_head The header of the original request.

  Body *request_body The body of the original request.
  ++++++++++++++++++++++++++++++++++++++*/

void InfoPage(int fd,URL *Url,Header *request_head,Body *request_body)
{
 if(!Url->local)
    InfoRequestedPage(fd,Url,request_head,request_body);
 else if(!strcmp(Url->path,"/info/request") && Url->args)
    InfoRequestPage(fd,Url);
 else if(!strcmp(Url->path,"/info/url") && Url->args)
    InfoCachedPage(fd,Url,0);
 else if(!strcmp(Url->path,"/info/content") && Url->args)
    InfoCachedPage(fd,Url,1);
 else if(!strcmp(Url->path,"/info/source") && Url->args)
    InfoCachedPage(fd,Url,2);
 else
    HTMLMessage(fd,404,"WWWOFFLE Illegal Info Page",NULL,"InfoIllegal",
                "url",Url->pathp,
                NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Display some information about the spooled copy of the URL.

  int fd The file descriptor to write the output to.

  URL *Url The URL that was requested for the info.

  int which Selects which of the types of information that is requested.
  ++++++++++++++++++++++++++++++++++++++*/

static void InfoCachedPage(int fd,URL *Url,int which)
{
 int spool;
 URL *refUrl;
 char *refurl=NULL;
 Header *spooled_head=NULL;

 refurl=URLDecodeFormArgs(Url->args);

 refUrl=SplitURL(refurl);

 spool=OpenWebpageSpoolFile(1,refUrl);

 if(spool!=-1)
   {
    init_io(spool);

    ParseReply(spool,&spooled_head);

#if USE_ZLIB
    if(GetHeader2(spooled_head,"Pragma","wwwoffle-compressed"))
      {
       char *content_encoding;

       if((content_encoding=GetHeader(spooled_head,"Content-Encoding")))
         {
          RemoveFromHeader(spooled_head,"Content-Encoding");
          RemoveFromHeader2(spooled_head,"Pragma","wwwoffle-compressed");
          configure_io_read(spool,-1,2,0);
         }
      }
#endif
   }

 if(which==0)
    InfoCached(fd,spool,refUrl,spooled_head);
 else if(which==1)
    InfoContents(fd,spool,refUrl);
 else if(which==2)
    InfoSource(fd,spool,refUrl,spooled_head);

 if(spool!=-1)
   {
    FreeHeader(spooled_head);

    finish_io(spool);
    close(spool);
   }

 free(refurl);
 FreeURL(refUrl);
}


/*++++++++++++++++++++++++++++++++++++++
  Display the cache information about this page.

  int fd The file descriptor to write to.

  int spool The spooled page file descriptor.

  URL *Url The URL of the spooled page.

  Header *spooled_head The header of the cached page.
  ++++++++++++++++++++++++++++++++++++++*/

static void InfoCached(int fd,int spool,URL *Url,Header *spooled_head)
{
 char *head1=NULL,*head2=NULL;

 if(spooled_head)
   {
    head1=HeaderString(spooled_head);
    head1[strlen(head1)-4]=0;

    ModifyReply(Url,spooled_head);
    RemoveFromHeader(spooled_head,"Content-Length");

    head2=HeaderString(spooled_head);
    head2[strlen(head2)-4]=0;
   }

 HTMLMessage(fd,200,"WWWOFFLE Info Cached",NULL,"InfoCached",
             "url",Url->name,
             "cached",spool==-1?"":"yes",
             "head1",head1,
             "head2",head2,
             NULL);

 if(spooled_head)
   {
    free(head1);
    free(head2);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Display the information about the page contents.

  int fd The file descriptor to write to.

  int spool The spooled page file descriptor.

  URL *Url The URL of the spooled page.
  ++++++++++++++++++++++++++++++++++++++*/

static void InfoContents(int fd,int spool,URL *Url)
{
 HTMLMessage(fd,200,"WWWOFFLE Info Cached",NULL,"InfoContents-Head",
             "url",Url->name,
             "cached",spool==-1?"":"yes",
             NULL);

 if(spool!=-1)
   {
    char **list,*refresh;

    lseek(spool,0,SEEK_SET);
    reinit_io(spool);

    ParseDocument(spool,Url,1);

    if((refresh=MetaRefresh()))
      {
       char *list[2];

       list[0]=refresh;
       list[1]=NULL;

       output_content(fd,"Refresh",list);
      }

    if((list=GetReferences(RefStyleSheet)))
       output_content(fd,"StyleSheet",list);

    if((list=GetReferences(RefImage)))
       output_content(fd,"Image",list);

    if((list=GetReferences(RefFrame)))
       output_content(fd,"Frame",list);

    if((list=GetReferences(RefScript)))
       output_content(fd,"Script",list);

    if((list=GetReferences(RefObject)))
       output_content(fd,"Object",list);

    if((list=GetReferences(RefInlineObject)))
       output_content(fd,"Object",list);

    if((list=GetReferences(RefLink)))
       output_content(fd,"Link",list);

    ResetReferences();
   }

 HTMLMessageBody(fd,"InfoContents-Tail",
                 NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Output the part of the web page for one item of content.

  int fd The file descriptor to write to.

  char *type The type of the content.

  char **url The list of URLs for the type of content.
  ++++++++++++++++++++++++++++++++++++++*/

static void output_content(int fd,char *type,char **url)
{
 int i,count;

 for(count=0;url[count];count++)
    ;

 qsort(url,count,sizeof(char*),(int (*)(const void*,const void*))sort_alpha);

 for(i=0;i<count;i++)
   {
    URL *Url=SplitURL(url[i]);
    int dontget,cached;

    dontget=ConfigBooleanMatchURL(DontGet,Url);
    cached=ExistsWebpageSpoolFile(Url);

    HTMLMessageBody(fd,"InfoContents-Body",
                    "type",type,
                    "refurl",url[i],
                    "dontget",dontget?"yes":"",
                    "cached",cached?"yes":"",
                    NULL);

    FreeURL(Url);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into alphabetical order.

  int sort_alpha Returns the comparison of the pointers to strings.

  char **a A pointer to the first URL string.

  char **b A pointer to the second URL string.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_alpha(char **a,char **b)
{
 return(strcmp(*a,*b));
}


/*++++++++++++++++++++++++++++++++++++++
  Display the source HTML of the page.

  int fd The file descriptor to write to.

  int spool The spooled page file descriptor.

  URL *Url The URL of the spooled page.

  Header *spooled_head The header of the cached page.
  ++++++++++++++++++++++++++++++++++++++*/

static void InfoSource(int fd,int spool,URL *Url,Header *spooled_head)
{
 int text=0;

 if(spooled_head)
   {
    char *type=GetHeader(spooled_head,"Content-Type");

    if(!type)
       text=0;
    else if(!strncmp(type,"text/",5))
       text=1;
    else if(!strncmp(type,"application/x-javascript",24))
       text=1;
    else if(!strncmp(type,"application/xhtml",17))
       text=1;
    else if(!strncmp(type,"application/xml",15))
       text=1;
   }

 HTMLMessage(fd,200,"WWWOFFLE Info Cached",NULL,"InfoSource-Head",
             "url",Url->name,
             "cached",spool==-1?"":"yes",
             "text",text?"yes":"no",
             NULL);

 if(text && spool!=-1)
   {
    int n;
    char buffer[READ_BUFFER_SIZE+1];

    while((n=read_data(spool,buffer,READ_BUFFER_SIZE))>0)
      {
       char *html;

       buffer[n]=0;
       html=HTMLString(buffer,0);

       write_string(fd,html);

       free(html);
      }
   }

 HTMLMessageBody(fd,"InfoSource-Tail",
                 NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Display some information about the request that was sent for the URL.

  int fd The file descriptor to write the output to.

  URL *Url The URL that was requested.
  ++++++++++++++++++++++++++++++++++++++*/

static void InfoRequestPage(int fd,URL *Url)
{
 char *url=NULL;

 url=URLDecodeFormArgs(Url->args);

 HTMLMessage(fd,200,"WWWOFFLE Info Request",
             NULL,
             "InfoRequest",
             "url",url,
             NULL);

 free(url);
}


/*++++++++++++++++++++++++++++++++++++++
  Display some information about the request that was sent for the URL.

  int fd The file descriptor to write the output to.

  URL *Url The URL that was requested.

  Header *request_head The header of the original request.

  Body *request_body The body of the original request.
  ++++++++++++++++++++++++++++++++++++++*/

static void InfoRequestedPage(int fd,URL *Url,Header *request_head,Body *request_body)
{
 char *head1,*head2;

 head1=HeaderString(request_head);
 head1[strlen(head1)-4]=0;

 ModifyRequest(Url,request_head);

 head2=HeaderString(request_head);
 head2[strlen(head2)-4]=0;

 HTMLMessageHead(fd,200,"WWWOFFLE Info Requested",
                 "Cache-Control","no-cache",
                 "Pragma","no-cache",
                 NULL);

 HTMLMessageBody(fd,"InfoRequested",
                 "url",Url->name,
                 "head1",head1,
                 "head2",head2,
                 "anybody",request_body?"yes":"",
                 "body",request_body?request_body->content:NULL,
                 NULL);

 free(head1);
 free(head2);
}
