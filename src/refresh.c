/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/refresh.c 2.78 2004/01/17 16:31:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  The HTML interactive page to refresh a URL.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "document.h"


#define MAX_RECURSE_LOCATION 8

/* Local variables */

/*+ The options for recursive fetching; +*/
static char *recurse_url=NULL,  /*+ the starting URL. +*/
            *recurse_limit="";  /*+ the the URL prefix that must match. +*/

/*+ The options for the recursive fetching; +*/
static int recurse_depth=-1,    /*+ the depth to go from this URL. +*/
           recurse_force=0;     /*+ the option to force refreshing. +*/

/*+ The options for the recursive fetching of related items; +*/
static int recurse_stylesheets=0, /*+ stylesheets. +*/
           recurse_images=0,      /*+ images. +*/
           recurse_frames=0,      /*+ frames. +*/
           recurse_scripts=0,     /*+ scripts. +*/
           recurse_objects=0,     /*+ objects. +*/
           recurse_location=0;    /*+ Location headers. +*/

/* Local functions */

static void ParseRecurseOptions(char *options);

static /*@null@*/ char *RefreshFormParse(int fd,/*@null@*/ char *request_args,/*@null@*/ Body *request_body);

static int request_url(URL *Url,/*@null@*/ char *refresh,URL *refUrl);


/*++++++++++++++++++++++++++++++++++++++
  Send to the client a page to allow refreshes using HTML.

  char *RefreshPage Returns a modified URLs for a simple refresh.

  int fd The file descriptor of the client.

  URL *Url The URL that was used to request this page.

  Body *request_body The HTTP request body sent by the client.

  int *recurse Return value set to true if a recursive fetch was asked for.
  ++++++++++++++++++++++++++++++++++++++*/

char *RefreshPage(int fd,URL *Url,Body *request_body,int *recurse)
{
 char *newurl=NULL;
 char *url;

 if(Url->args)
    url=URLDecodeFormArgs(Url->args);
 else
    url=NULL;

 if(!strcmp("/refresh-options/",Url->path))
    HTMLMessage(fd,200,"WWWOFFLE Refresh Form",NULL,"RefreshPage",
                "url",url,
                "stylesheets",ConfigBooleanURL(FetchStyleSheets,NULL)?"yes":NULL,
                "images",ConfigBooleanURL(FetchImages,NULL)?"yes":NULL,
                "frames",ConfigBooleanURL(FetchFrames,NULL)?"yes":NULL,
                "scripts",ConfigBooleanURL(FetchScripts,NULL)?"yes":NULL,
                "objects",ConfigBooleanURL(FetchObjects,NULL)?"yes":NULL,
                NULL);
 else if(!strcmp("/refresh-request/",Url->path) && Url->args)
   {
    if((newurl=RefreshFormParse(fd,Url->args,request_body)))
       *recurse=-1;
   }
 else if(!strcmp("/refresh/",Url->path) && url)
   {
    newurl=(char*)malloc(strlen(url)+1);
    strcpy(newurl,url);
   }
 else if(!strcmp("/refresh-recurse/",Url->path) && Url->args)
   {
    *recurse=1;
    ParseRecurseOptions(Url->args);
    newurl=(char*)malloc(strlen(recurse_url)+1);
    strcpy(newurl,recurse_url);
   }
 else
    HTMLMessage(fd,404,"WWWOFFLE Illegal Refresh Page",NULL,"RefreshIllegal",
                "url",Url->pathp,
                NULL);

 if(url)
    free(url);

 return(newurl);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the reply from the form.

  char *RefreshFormParse Returns the first URL to get.

  int fd The file descriptor of the client.

  char *request_args The arguments of the requesting URL.

  Body *request_body The body of the HTTP request sent by the client.
  ++++++++++++++++++++++++++++++++++++++*/

static char *RefreshFormParse(int fd,char *request_args,Body *request_body)
{
 int i;
 char **args,*url=NULL,*limit="",*method=NULL;
 int depth=0,force=0;
 int stylesheets=0,images=0,frames=0,scripts=0,objects=0;
 URL *Url;
 char *new_url;

 if(!request_args && !request_body)
   {
    HTMLMessage(fd,404,"WWWOFFLE Refresh Form Error",NULL,"RefreshFormError",
                "body",NULL,
                NULL);
    return(NULL);
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
    if(!strncmp("url=",args[i],4))
       url=TrimArgs(URLDecodeFormArgs(args[i]+4));
    if(!strncmp("depth=",args[i],6))
       depth=atoi(args[i]+6);
    if(!strncmp("method=",args[i],7))
       method=args[i]+7;
    if(!strncmp("force=",args[i],6))
       force=!!(args[i][6]=='Y');
    if(!strncmp("stylesheets=",args[i],12))
       stylesheets=!!(args[i][12]=='Y');
    if(!strncmp("images=",args[i],7))
       images=!!(args[i][7]=='Y');
    if(!strncmp("frames=",args[i],7))
       frames=!!(args[i][7]=='Y');
    if(!strncmp("scripts=",args[i],8))
       scripts=!!(args[i][8]=='Y');
    if(!strncmp("objects=",args[i],8))
       objects=!!(args[i][8]=='Y');
   }

 if(url==NULL || *url==0 || method==NULL || *method==0)
   {
    HTMLMessage(fd,404,"WWWOFFLE Refresh Form Error",NULL,"RefreshFormError",
                "body",request_body?request_body->content:request_args,
                NULL);
    if(url) free(url);
    free(args[0]);
    free(args);
    return(NULL);
   }

 Url=SplitURL(url);
 free(url);

 if(!strcmp(method,"any"))
    limit="";
 else if(!strcmp(method,"proto"))
   {
    limit=(char*)malloc(strlen(Url->proto)+4);
    sprintf(limit,"%s://",Url->proto);
   }
 else if(!strcmp(method,"host"))
   {
    limit=(char*)malloc(strlen(Url->proto)+strlen(Url->host)+5);
    sprintf(limit,"%s://%s/",Url->proto,Url->host);
   }
 else if(!strcmp(method,"dir"))
   {
    char *p;
    limit=(char*)malloc(strlen(Url->proto)+strlen(Url->host)+strlen(Url->path)+5);
    sprintf(limit,"%s://%s%s",Url->proto,Url->host,Url->path);
    p=limit+strlen(limit)-1;
    while(p>limit && *p!='/')
       *p--=0;
   }
 else
    depth=0;

 free(args[0]);
 free(args);

 new_url=CreateRefreshPath(Url,limit,depth,force,stylesheets,images,frames,scripts,objects);

 if(*limit)
    free(limit);

 FreeURL(Url);

 return(new_url);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the recursive url options to decide what needs fetching recursively.

  char *options The options to use, encoding the depth and other things.
  ++++++++++++++++++++++++++++++++++++++*/

static void ParseRecurseOptions(char *options)
{
 recurse_url=NULL;
 recurse_depth=-1;
 recurse_limit="";

 if(options)
   {
    char **args;
    int i;

    recurse_stylesheets=0;
    recurse_images=0;
    recurse_frames=0;
    recurse_scripts=0;
    recurse_objects=0;

    args=SplitFormArgs(options);

    for(i=0;args[i];i++)
      {
       if(!strncmp("url=",args[i],4))
          recurse_url=TrimArgs(URLDecodeFormArgs(args[i]+4));
       if(!strncmp("depth=",args[i],6))
         {recurse_depth=atoi(args[i]+6); if(recurse_depth<0) recurse_depth=0;}
       if(!strncmp("limit=",args[i],6))
          recurse_limit=TrimArgs(URLDecodeFormArgs(args[i]+6));
       if(!strncmp("force=",args[i],6))
          recurse_force=!!(args[i][6]=='Y');

       if(!strncmp("stylesheets=",args[i],12))
          recurse_stylesheets=!!(args[i][12]=='Y')*2;
       if(!strncmp("images=",args[i],7))
          recurse_images=!!(args[i][7]=='Y')*2;
       if(!strncmp("frames=",args[i],7))
          recurse_frames=!!(args[i][7]=='Y')*2;
       if(!strncmp("scripts=",args[i],8))
          recurse_scripts=!!(args[i][8]=='Y')*2;
       if(!strncmp("objects=",args[i],8))
          recurse_objects=!!(args[i][8]=='Y')*2;
      }

    PrintMessage(Debug,"Recursive Fetch options: depth=%d limit='%s' force=%d",
                        recurse_depth,recurse_limit,recurse_force);
    PrintMessage(Debug,"Recursive Fetch options: stylesheets=%d images=%d frames=%d scripts=%d objects=%d location=%d",
                        recurse_stylesheets,recurse_images,recurse_frames,recurse_scripts,recurse_objects,recurse_location);

    free(args[0]);
    free(args);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Set the default recursive url options to decide what needs fetching recursively.

  URL *Url The URL that is being fetched.

  Header *head The header for the request for the URL.
  ++++++++++++++++++++++++++++++++++++++*/

void DefaultRecurseOptions(URL *Url,Header *head)
{
 char *value;

 /* Default values */

 recurse_stylesheets=ConfigBooleanURL(FetchStyleSheets,Url)*2;
 recurse_images=ConfigBooleanURL(FetchImages,Url)*2;
 recurse_frames=ConfigBooleanURL(FetchFrames,Url)*2;
 recurse_scripts=ConfigBooleanURL(FetchScripts,Url)*2;
 recurse_objects=ConfigBooleanURL(FetchObjects,Url)*2;

 recurse_location=MAX_RECURSE_LOCATION;

 /* Updated values depending on where the request came from */

 if((value=GetHeader2(head,"Pragma","wwwoffle-stylesheets=")))
    recurse_stylesheets=atoi(value+21);
 if((value=GetHeader2(head,"Pragma","wwwoffle-images=")))
    recurse_images=atoi(value+16);
 if((value=GetHeader2(head,"Pragma","wwwoffle-frames=")))
    recurse_frames=atoi(value+16);
 if((value=GetHeader2(head,"Pragma","wwwoffle-scripts=")))
    recurse_scripts=atoi(value+17);
 if((value=GetHeader2(head,"Pragma","wwwoffle-objects=")))
    recurse_objects=atoi(value+17);

 if((value=GetHeader2(head,"Pragma","wwwoffle-location=")))
    recurse_location=atoi(value+18);

 PrintMessage(Debug,"Default Recursive Fetch options: stylesheets=%d images=%d frames=%d scripts=%d objects=%d location=%d",
                     recurse_stylesheets,recurse_images,recurse_frames,recurse_scripts,recurse_objects,recurse_location);
}


/*++++++++++++++++++++++++++++++++++++++
  Replies with whether the page is to be forced to refresh.

  int RefreshForced Returns the force flag.
  ++++++++++++++++++++++++++++++++++++++*/

int RefreshForced(void)
{
 return(recurse_force);
}


/*++++++++++++++++++++++++++++++++++++++
  Fetch the images, etc from the just parsed page.

  int RecurseFetch Returns true if there are more to be fetched.

  URL *Url The URL that was fetched.
  ++++++++++++++++++++++++++++++++++++++*/

int RecurseFetch(URL *Url)
{
 char **list,*metarefresh;
 int more=0,old_recurse_location;
 int j;

 old_recurse_location=recurse_location;

 /* A Meta-Refresh header. */

 if(recurse_location && (metarefresh=MetaRefresh()))
   {
    URL *metarefreshUrl=SplitURL(metarefresh);

    recurse_location--;

    if(!metarefreshUrl->local && metarefreshUrl->Protocol)
      {
       char *refresh=NULL;

       if(recurse_depth>=0)
          if(!*recurse_limit || !strncmp(recurse_limit,metarefreshUrl->name,strlen(recurse_limit)))
             refresh=CreateRefreshPath(metarefreshUrl,recurse_limit,recurse_depth,
                                       recurse_force,
                                       recurse_stylesheets,recurse_images,recurse_frames,recurse_scripts,recurse_objects);

       PrintMessage(Debug,"Meta-Refresh=%s",refresh?refresh:metarefreshUrl->name);
       more+=request_url(metarefreshUrl,refresh,Url);
       if(refresh)
          free(refresh);
      }

    FreeURL(metarefreshUrl);
   }

 recurse_location=MAX_RECURSE_LOCATION;

 /* Any style sheets. */

 if(recurse_stylesheets && (list=GetReferences(RefStyleSheet)))
    for(j=0;list[j];j++)
      {
       URL *stylesheetUrl=SplitURL(list[j]);

       recurse_stylesheets--;

       if(!stylesheetUrl->local && stylesheetUrl->Protocol)
         {
          PrintMessage(Debug,"Style-Sheet=%s",stylesheetUrl->name);
          more+=request_url(stylesheetUrl,NULL,Url);
         }

       recurse_stylesheets++;

       FreeURL(stylesheetUrl);
      }

 /* Any images. */

 if(recurse_images && (list=GetReferences(RefImage)))
    for(j=0;list[j];j++)
      {
       URL *imageUrl=SplitURL(list[j]);

       recurse_images--;

       if(!imageUrl->local && imageUrl->Protocol)
         {
          if(!ConfigBooleanURL(FetchSameHostImages,Url) ||
             (!strcmp(Url->proto,imageUrl->proto) && !strcmp(Url->host,imageUrl->host)))
            {
             PrintMessage(Debug,"Image=%s",imageUrl->name);
             more+=request_url(imageUrl,NULL,Url);
            }
          else
             PrintMessage(Debug,"The image URL '%s' is on a different host.",imageUrl->name);
         }

       recurse_images++;

       FreeURL(imageUrl);
      }

 /* Any frames */

 if(recurse_frames && (list=GetReferences(RefFrame)))
    for(j=0;list[j];j++)
      {
       URL *frameUrl=SplitURL(list[j]);

       recurse_frames--;

       if(!frameUrl->local && frameUrl->Protocol)
         {
          char *refresh=NULL;

          if(recurse_depth>=0)
             if(!*recurse_limit || !strncmp(recurse_limit,frameUrl->name,strlen(recurse_limit)))
                refresh=CreateRefreshPath(frameUrl,recurse_limit,recurse_depth,
                                          recurse_force,
                                          recurse_stylesheets,recurse_images,recurse_frames,recurse_scripts,recurse_objects);

          PrintMessage(Debug,"Frame=%s",refresh?refresh:frameUrl->name);
          more+=request_url(frameUrl,refresh,Url);
          if(refresh)
             free(refresh);
         }

       recurse_frames++;

       FreeURL(frameUrl);
      }

 /* Any scripts. */

 if(recurse_scripts && (list=GetReferences(RefScript)))
    for(j=0;list[j];j++)
      {
       URL *scriptUrl=SplitURL(list[j]);

       recurse_scripts--;

       if(!scriptUrl->local && scriptUrl->Protocol)
         {
          PrintMessage(Debug,"Script=%s",scriptUrl->name);
          more+=request_url(scriptUrl,NULL,Url);
         }

       recurse_scripts++;

       FreeURL(scriptUrl);
      }

 /* Any Objects. */

 if(recurse_objects && (list=GetReferences(RefObject)))
    for(j=0;list[j];j++)
      {
       URL *objectUrl=SplitURL(list[j]);

       recurse_objects--;

       if(!objectUrl->local && objectUrl->Protocol)
         {
          PrintMessage(Debug,"Object=%s",objectUrl->name);
          more+=request_url(objectUrl,NULL,Url);
         }

       recurse_objects++;

       FreeURL(objectUrl);
      }

 if(recurse_objects && (list=GetReferences(RefInlineObject)))
    for(j=0;list[j];j++)
      {
       URL *objectUrl=SplitURL(list[j]);

       recurse_objects--;

       if(!objectUrl->local && objectUrl->Protocol)
         {
          char *refresh=NULL;

          if(recurse_depth>=0)
             if(!*recurse_limit || !strncmp(recurse_limit,objectUrl->name,strlen(recurse_limit)))
                refresh=CreateRefreshPath(objectUrl,recurse_limit,recurse_depth,
                                          recurse_force,
                                          recurse_stylesheets,recurse_images,recurse_frames,recurse_scripts,recurse_objects);

          PrintMessage(Debug,"InlineObject=%s",refresh?refresh:objectUrl->name);
          more+=request_url(objectUrl,refresh,Url);
          if(refresh)
             free(refresh);
         }

       recurse_objects++;

       FreeURL(objectUrl);
      }

 /* Any links */

 if(recurse_depth>0 && (list=GetReferences(RefLink)))
    for(j=0;list[j];j++)
      {
       URL *linkUrl=SplitURL(list[j]);

       recurse_depth--;

       if(!linkUrl->local && linkUrl->Protocol)
          if(!*recurse_limit || !strncmp(recurse_limit,linkUrl->name,strlen(recurse_limit)))
            {
             char *refresh=NULL;

             refresh=CreateRefreshPath(linkUrl,recurse_limit,recurse_depth,
                                       recurse_force,
                                       recurse_stylesheets,recurse_images,recurse_frames,recurse_scripts,recurse_objects);

             PrintMessage(Debug,"Link=%s",refresh);
             more+=request_url(linkUrl,refresh,Url);
             if(refresh)
                free(refresh);
            }

       recurse_depth++;

       FreeURL(linkUrl);
      }

 recurse_location=old_recurse_location;

 return(more);
}


/*++++++++++++++++++++++++++++++++++++++
  Fetch the relocated page.

  int RecurseFetchRelocation Returns true if there are more to be fetched.

  URL *Url The URL that was fetched.

  char *location The new location of the URL.
  ++++++++++++++++++++++++++++++++++++++*/

int RecurseFetchRelocation(URL *Url,char *location)
{
 int more=0;
 URL *locationUrl=SplitURL(location);

 if(recurse_location && !locationUrl->local && locationUrl->Protocol)
   {
    char *refresh=NULL;

    recurse_location--;

    if(recurse_depth>0)
       if(!*recurse_limit || !strncmp(recurse_limit,locationUrl->name,strlen(recurse_limit)))
          refresh=CreateRefreshPath(locationUrl,recurse_limit,recurse_depth,
                                    recurse_force,
                                    recurse_stylesheets,recurse_images,recurse_frames,recurse_scripts,recurse_objects);

    PrintMessage(Debug,"Location=%s",refresh?refresh:locationUrl->name);
    more+=request_url(locationUrl,refresh,Url);
    if(refresh)
       free(refresh);

    recurse_location++;
   }

 return(more);
}


/*++++++++++++++++++++++++++++++++++++++
  Make a request for a URL.

  int request_url Returns 1 on success, else 0.

  URL *Url The URL that was asked for.

  char *refresh The URL path that is required for refresh information.

  URL *refUrl The refering URL.
  ++++++++++++++++++++++++++++++++++++++*/

static int request_url(URL *Url,char *refresh,URL *refUrl)
{
 int retval=0;

 if(recurse_depth>0 && !ConfigBooleanURL(DontGetRecursive,Url))
    PrintMessage(Debug,"The URL '%s' matches one in the list not to get recursively.",Url->name);
 else if(ConfigBooleanMatchURL(DontGet,Url))
    PrintMessage(Debug,"The URL '%s' matches one in the list not to get.",Url->name);
 else
   {
    int new_outgoing=OpenOutgoingSpoolFile(0);

    if(new_outgoing==-1)
       PrintMessage(Warning,"Cannot open the new outgoing request to write.");
    else
      {
       URL *reqUrl;
       Header *new_request_head;
       char *head,str[32];

       init_io(new_outgoing);

       if(refUrl->pass && !strcmp(refUrl->host,Url->host))
          AddURLPassword(Url,refUrl->user,refUrl->pass);

       if(refresh)
          reqUrl=SplitURL(refresh);
       else
          reqUrl=Url;

       new_request_head=RequestURL(reqUrl,refUrl->name);

       if(recurse_force)
          AddToHeader(new_request_head,"Pragma","no-cache");

       sprintf(str,"wwwoffle-stylesheets=%d",recurse_stylesheets);
       AddToHeader(new_request_head,"Pragma",str);

       sprintf(str,"wwwoffle-images=%d",recurse_images);
       AddToHeader(new_request_head,"Pragma",str);

       sprintf(str,"wwwoffle-frames=%d",recurse_frames);
       AddToHeader(new_request_head,"Pragma",str);

       sprintf(str,"wwwoffle-scripts=%d",recurse_scripts);
       AddToHeader(new_request_head,"Pragma",str);

       sprintf(str,"wwwoffle-objects=%d",recurse_objects);
       AddToHeader(new_request_head,"Pragma",str);

       sprintf(str,"wwwoffle-location=%d",recurse_location);
       AddToHeader(new_request_head,"Pragma",str);

       head=HeaderString(new_request_head);
       if(write_string(new_outgoing,head)==-1)
          PrintMessage(Warning,"Cannot write to outgoing file; disk full?");

       finish_io(new_outgoing);
       CloseOutgoingSpoolFile(new_outgoing,reqUrl);

       retval=1;

       if(reqUrl!=Url)
          free(reqUrl);
       free(head);
       FreeHeader(new_request_head);
      }
   }

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Create the special path with arguments for doing a refresh with options.

  char *CreateRefreshPath Returns a new string.

  URL *Url The URL that is to be fetched (starting point).

  char *limit The limiting URL (to allow within subdir, or within host).

  int depth The depth of recursion.

  int force The option to force the refreshes.

  int stylesheets The option to fetch any stylesheets in the page.

  int images The option to fetch any images in the page.

  int frames The option to fetch any frames in the page.

  int scripts The option to fetch any scripts in the page.

  int objects The option to fetch any objects in the page.
  ++++++++++++++++++++++++++++++++++++++*/

char *CreateRefreshPath(URL *Url,char *limit,int depth,
                        int force,
                        int stylesheets,int images,int frames,int scripts,int objects)
{
 char *args;
 char *encurl,*enclimit=NULL;

 encurl=URLEncodeFormArgs(Url->name);
 if(limit)
    enclimit=URLEncodeFormArgs(limit);

 args=(char*)malloc(strlen(encurl)+(limit?strlen(enclimit):0)+128);

 strcpy(args,"/refresh-recurse/?");

 sprintf(&args[strlen(args)],"url=%s",encurl);

 if(depth>0)
    sprintf(&args[strlen(args)],";depth=%d",depth);
 else
    strcat(args,";depth=0");

 if(limit && depth>0)
    sprintf(&args[strlen(args)],";limit=%s",enclimit);

 if(force)
    strcat(args,";force=Y");

 if(stylesheets)
    strcat(args,";stylesheets=Y");
 if(images)
    strcat(args,";images=Y");
 if(frames)
    strcat(args,";frames=Y");
 if(scripts)
    strcat(args,";scripts=Y");
 if(objects)
    strcat(args,";objects=Y");

 free(encurl);
 if(limit)
    free(enclimit);

 return(args);
}
