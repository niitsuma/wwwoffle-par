/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/refresh.c 2.79 2004/09/28 16:25:30 amb Exp $

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
#include "headbody.h"
#include "errors.h"
#include "config.h"
#include "document.h"


#ifndef CLIENT_ONLY
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

static void ParseRecurseOptions(URL *Url);

static /*@null@*/ char *RefreshFormParse(int fd,URL *Url,/*@null@*/ char *request_args,/*@null@*/ Body *request_body);

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
    if((newurl=RefreshFormParse(fd,Url,Url->args,request_body)))
       *recurse=-1;
   }
 else if(!strcmp("/refresh/",Url->path) && url)
   {
    newurl=strdup(url);
   }
 else if(!strcmp("/refresh-recurse/",Url->path) && Url->args)
   {
    *recurse=1;
    ParseRecurseOptions(Url);
    newurl=strdup(recurse_url);
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

  URL *Url The URL of the form that is being processed.

  char *request_args The arguments of the requesting URL.

  Body *request_body The body of the HTTP request sent by the client.
  ++++++++++++++++++++++++++++++++++++++*/

static char *RefreshFormParse(int fd,URL *Url,char *request_args,Body *request_body)
{
 int i;
 char **args,*url=NULL,*limit=NULL,*method=NULL;
 int depth=0,force=0;
 int stylesheets=0,images=0,frames=0,scripts=0,objects=0;
 URL *refUrl;
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
    if(!strcmp_litbeg(args[i],"url="))
       url=TrimArgs(URLDecodeFormArgs(args[i]+strlitlen("url=")));
    else if(!strcmp_litbeg(args[i],"depth="))
       depth=atoi(args[i]+strlitlen("depth="));
    else if(!strcmp_litbeg(args[i],"method="))
       method=args[i]+strlitlen("method=");
    else if(!strcmp_litbeg(args[i],"force="))
       force=(args[i][strlitlen("force=")]=='Y');
    else if(!strcmp_litbeg(args[i],"stylesheets="))
       stylesheets=(args[i][strlitlen("stylesheets=")]=='Y');
    else if(!strcmp_litbeg(args[i],"images="))
       images=(args[i][strlitlen("images=")]=='Y');
    else if(!strcmp_litbeg(args[i],"frames="))
       frames=(args[i][strlitlen("frames=")]=='Y');
    else if(!strcmp_litbeg(args[i],"scripts="))
       scripts=(args[i][strlitlen("scripts=")]=='Y');
    else if(!strcmp_litbeg(args[i],"objects="))
       objects=(args[i][strlitlen("objects=")]=='Y');
    else
       PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",args[i],Url->name);
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

 refUrl=SplitURL(url);
 free(url);

 if(!strcmp(method,"any"))
   limit=strdup("");
 else if(!strcmp(method,"proto"))
   {
    limit=strndup(refUrl->name,refUrl->hostp-refUrl->name);
   }
 else if(!strcmp(method,"host"))
   {
    limit=(char*)malloc((refUrl->pathp-refUrl->name)+2);
    {char *p=mempcpy(limit,refUrl->name,refUrl->pathp-refUrl->name); *p++='/'; *p=0; }
   }
 else if(!strcmp(method,"dir"))
   {
    char *p=refUrl->pathendp;
    while(--p>=refUrl->pathp && *p!='/');
    limit=strndup(refUrl->name,p+1-refUrl->name);
   }
 else
    depth=0;

 free(args[0]);
 free(args);

 new_url=CreateRefreshPath(refUrl,limit,depth,force,stylesheets,images,frames,scripts,objects);

 if(limit)
    free(limit);

 FreeURL(refUrl);

 return(new_url);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the recursive url options to decide what needs fetching recursively.

  URL *Url The URL containing the options to use, encoding the depth and other things.
  ++++++++++++++++++++++++++++++++++++++*/

static void ParseRecurseOptions(URL *Url)
{
 if(recurse_url) free(recurse_url);
 recurse_url=NULL;
 recurse_depth=-1;
 recurse_limit="";

 if(Url->args)
   {
    char **args;
    int i;

    recurse_stylesheets=0;
    recurse_images=0;
    recurse_frames=0;
    recurse_scripts=0;
    recurse_objects=0;

    args=SplitFormArgs(Url->args);

    for(i=0;args[i];i++)
      {
       if(!strcmp_litbeg(args[i],"url="))
          recurse_url=TrimArgs(URLDecodeFormArgs(args[i]+strlitlen("url=")));
       else if(!strcmp_litbeg(args[i],"depth="))
         {recurse_depth=atoi(args[i]+strlitlen("depth=")); if(recurse_depth<0) recurse_depth=0;}
       else if(!strcmp_litbeg(args[i],"limit="))
          recurse_limit=TrimArgs(URLDecodeFormArgs(args[i]+strlitlen("limit=")));
       else if(!strcmp_litbeg(args[i],"force="))
          recurse_force=(args[i][strlitlen("force=")]=='Y');
       else if(!strcmp_litbeg(args[i],"stylesheets="))
          recurse_stylesheets=(args[i][strlitlen("stylesheets=")]=='Y')*2;
       else if(!strcmp_litbeg(args[i],"images="))
          recurse_images=(args[i][strlitlen("images=")]=='Y')*2;
       else if(!strcmp_litbeg(args[i],"frames="))
          recurse_frames=(args[i][strlitlen("frames=")]=='Y')*2;
       else if(!strcmp_litbeg(args[i],"scripts="))
          recurse_scripts=(args[i][strlitlen("scripts=")]=='Y')*2;
       else if(!strcmp_litbeg(args[i],"objects="))
          recurse_objects=(args[i][strlitlen("objects=")]=='Y')*2;
       else
          PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",args[i],Url->name);
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

 if((value=GetHeader2Val(head,"Pragma","wwwoffle-stylesheets")))
    recurse_stylesheets=atoi(value);
 if((value=GetHeader2Val(head,"Pragma","wwwoffle-images")))
    recurse_images=atoi(value);
 if((value=GetHeader2Val(head,"Pragma","wwwoffle-frames")))
    recurse_frames=atoi(value);
 if((value=GetHeader2Val(head,"Pragma","wwwoffle-scripts")))
    recurse_scripts=atoi(value);
 if((value=GetHeader2Val(head,"Pragma","wwwoffle-objects")))
    recurse_objects=atoi(value);

 if((value=GetHeader2Val(head,"Pragma","wwwoffle-location")))
    recurse_location=atoi(value);

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
          if(!*recurse_limit || !strcmp_beg(metarefreshUrl->name,recurse_limit))
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
             if(!*recurse_limit || !strcmp_beg(frameUrl->name,recurse_limit))
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
             if(!*recurse_limit || !strcmp_beg(objectUrl->name,recurse_limit))
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
          if(!*recurse_limit || !strcmp_beg(linkUrl->name,recurse_limit))
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
       if(!*recurse_limit || !strcmp_beg(locationUrl->name,recurse_limit))
          refresh=CreateRefreshPath(locationUrl,recurse_limit,recurse_depth,
                                    recurse_force,
                                    recurse_stylesheets,recurse_images,recurse_frames,recurse_scripts,recurse_objects);

    PrintMessage(Debug,"Location=%s",refresh?refresh:locationUrl->name);
    more+=request_url(locationUrl,refresh,Url);
    if(refresh)
       free(refresh);

    recurse_location++;
   }

 FreeURL(locationUrl);
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

       if(refUrl->pass && !strcmp(refUrl->hostport,Url->hostport))
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

       head=HeaderString(new_request_head,NULL);
       if(write_string(new_outgoing,head)==-1)
          PrintMessage(Warning,"Cannot write to outgoing file; disk full?");

       finish_io(new_outgoing);
       CloseOutgoingSpoolFile(new_outgoing,reqUrl);

       retval=1;

       if(reqUrl!=Url)
          FreeURL(reqUrl);
       free(head);
       FreeHeader(new_request_head);
      }
   }

 return(retval);
}
#endif


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
 char depthstr[12];
 int len;

 encurl=URLEncodeFormArgs(Url->name);

 sprintf(depthstr,"%d",depth>0?depth:0);

 len=strlitlen("/refresh-recurse/?url=;depth=")+strlen(encurl)+strlen(depthstr);

 if(limit && depth>0) {
   enclimit=URLEncodeFormArgs(limit);
   len+=strlitlen(";limit=")+strlen(enclimit);
 }
 
 if(force)       len+=strlitlen(";force=Y");
 if(stylesheets) len+=strlitlen(";stylesheets=Y");
 if(images)      len+=strlitlen(";images=Y");
 if(frames)      len+=strlitlen(";frames=Y");
 if(scripts)     len+=strlitlen(";scripts=Y");
 if(objects)     len+=strlitlen(";objects=Y");
 
 args=(char*)malloc(len+1);

 {
   char *p;
   p=stpcpy(stpcpy(stpcpy(stpcpy(args,"/refresh-recurse/?url="),encurl),";depth="),depthstr);

   if(limit && depth>0)
     p=stpcpy(stpcpy(p,";limit="),enclimit);

   if(force)       p=stpcpy(p,";force=Y");
   if(stylesheets) p=stpcpy(p,";stylesheets=Y");
   if(images)      p=stpcpy(p,";images=Y");
   if(frames)      p=stpcpy(p,";frames=Y");
   if(scripts)     p=stpcpy(p,";scripts=Y");
   if(objects)     p=stpcpy(p,";objects=Y");
 }

 free(encurl);
 if(enclimit)
    free(enclimit);

 return(args);
}
