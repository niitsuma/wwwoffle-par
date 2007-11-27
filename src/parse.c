/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/parse.c 2.134 2006/06/25 14:25:11 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Functions to parse the HTTP requests.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2005,2006,2007 Paul A. Rombouts
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

#include <sys/stat.h>

#include "wwwoffle.h"
#include "version.h"
#include "io.h"
#include "misc.h"
#include "headbody.h"
#include "errors.h"
#include "config.h"

#ifndef CLIENT_ONLY

/*+ The time that the program went online. +*/
time_t OnlineTime=0;

/*+ The time that the program went offline. +*/
time_t OfflineTime=0;

/*+ Headers from a request that can be re-used in automatically generated requests. +*/
static const char* const reusable_headers[]={"User-Agent",
					     "Accept",
					     "Accept-Charset",
					     "Accept-Language",
					     "From",
					     "Proxy-Authorization"};

/*+ Headers that we do not allow the users to censor. +*/
static const char* const non_censored_headers[]={"Host",
						 "Connection",
						 "Proxy-Connection",
						 "Authorization",
						 "Content-Type",
						 "Content-Encoding",
						 "Content-Length",
						 "Transfer-Encoding"};

/*+ Headers that we cannot allow to be passed through WWWOFFLE. +*/
static const char* const deleted_http_headers[]={"If-Match",
						 "If-Range",
						 "Range",
						 "Upgrade",
						 "Keep-Alive",
						 "Accept-Encoding",
						 "TE"};

/*+ The headers from the request that are re-usable. +*/
static /*@only@*/ Header *reusable_header=NULL;

static void censor_header(ConfigItem confitem,const URL *Url,Header *head);  /* Added by Paul Rombouts */


/*++++++++++++++++++++++++++++++++++++++
  Parse the request to the server.

  URL *ParseRequest Returns the URL or NULL if it failed.

  int fd The file descriptor to read the request from.

  Header **request_head Return the header of the request.

  Body **request_body Return the body of the request.
  ++++++++++++++++++++++++++++++++++++++*/

URL *ParseRequest(int fd,Header **request_head,Body **request_body)
{
 URL *Url=NULL;
 char *line=NULL,*val;
 unsigned i;

 *request_head=NULL;
 *request_body=NULL;

 if(reusable_header) FreeHeader(reusable_header);
 CreateHeader("GET reusable HTTP/1.0",1,&reusable_header);

 while((line=read_line(fd,line)))
   {
    if(!*request_head) /* first line */
      {
       if(CreateHeader(line,1,request_head)<=0 || !(*request_head)->url)
	 goto free_return_null;

       Url=SplitURL((*request_head)->url);
      }
    else
      if(!AddToHeaderRaw(*request_head,line))
	break;
   }

 /* Timeout or Connection lost? */
 
 if(!line || !Url || !*request_head) {
   PrintMessage(Warning,"Nothing to read from the wwwoffle proxy socket; timed out or connection lost? [%!s].");
   goto free_return_null;
 }

 free(line); line=NULL;

 
 /* Find re-usable headers (for recursive requests) */

 for(i=0;i<sizeof(reusable_headers)/sizeof(char*);i++)
   CopyHeader(*request_head,reusable_header,reusable_headers[i]);

 /* Check for firewall operation. */

 if((val=GetHeader(*request_head,"Host")) && strcasecmp((*request_head)->method,"CONNECT"))
   {
    URL *oldUrl=Url;
    Url=MakeModifiedURL(oldUrl,REPLACEURLHOSTPORT,NULL,val,NULL,NULL,NULL,NULL);
    FreeURL(oldUrl);
   }

 /* Check for passwords */

 if((val=GetHeader(*request_head,"Authorization")))
   {
    char *p=val;

    while(*p && *p!=' ') ++p;
    while(*p && *p==' ') ++p;
    if(*p)
      {
	size_t userpass_size=base64declenmax(strlen(p))+1;
	char userpass[userpass_size];
	char *user,*pass;

	pass=user=(char *)Base64Decode((unsigned char *)p,NULL,(unsigned char *)userpass,userpass_size);
	while(*pass && *pass!=':') ++pass;

	if(*pass) *pass++=0; else pass=NULL;

	ChangePasswordURL(Url,user,pass);
      }
   }

 if(!strcmp((*request_head)->method,"POST") ||
    !strcmp((*request_head)->method,"PUT"))
   {
    if((val=GetHeader(*request_head,"Content-Length")))
      {
       int length=atoi(val),nread;

       if(length<0)
         {
          PrintMessage(Warning,"POST or PUT request must have a non-negative Content-Length header.");
	  goto free_return_null;
	 }

       *request_body=CreateBody(length);

       for(nread=0;nread<length;) {
	 int m=read_data(fd,&(*request_body)->content[nread],length-nread);
	 if(m<0) {
	   PrintMessage(Warning,"Error reading body of POST or PUT request [%!s].");
	   goto free_return_null;
	 }
	 if(m==0)
	   break;
	 nread += m;
       }

       if(nread!=length) {
	 PrintMessage(Warning,"POST or PUT request must have same data length as specified in Content-Length header (%d compared to %d).",length,nread);
	 goto free_return_null;
       }

       (*request_body)->content[length]=0;
      }
    else if(GetHeader2(*request_head,"Transfer-Encoding","chunked"))
      {
	int length,lenalloc;

       PrintMessage(Debug,"Client has used chunked encoding.");
       configure_io_chunked(fd,1,-1);

       *request_body=NULL;

       for(length=0,lenalloc=0;;) {
	 int m;
	 if(length>=lenalloc) {
	   lenalloc = length + IO_BUFFER_SIZE;
	   *request_body=ReallocBody(*request_body,lenalloc);
	 }
	 m=read_data(fd,&(*request_body)->content[length],lenalloc-length);
	 if(m<0) {
	   PrintMessage(Warning,"Error reading body of POST or PUT request [%!s].");
	   goto free_return_null;
	 }
	 if(m==0)
	   break;
	 length += m;
       }

       *request_body=ReallocBody(*request_body,length);
       (*request_body)->content[length]=0;
      }
    else
      {
       PrintMessage(Warning,"POST or PUT request must have Content-Length header or use chunked encoding.");
       goto free_return_null;
      }

    {
      md5hash_t h;
      char buf[base64enclen(sizeof(md5hash_t))+1];
      size_t argssize;

      MakeHash((*request_body)->content,(*request_body)->length,&h);
      hashbase64encode(&h,(unsigned char *)buf,sizeof(buf));
      argssize=strlen((*request_head)->method)+base64enclen(sizeof(md5hash_t))+MAX_HEX_STR+sizeof("!:.");
      if(Url->args) argssize += strlen(Url->args)+1;
      {
	char args[argssize];
	URL *oldUrl;
      
	sprintf(Url->args?stpcpy(stpcpy(args,"!"),Url->args):args,
		"!%s:%s.%08lx",(*request_head)->method,buf,(long)time(NULL));
	oldUrl=Url;
	Url=MakeModifiedURL(oldUrl,REPLACEURLARGS,NULL,NULL,NULL,args,NULL,NULL);
	FreeURL(oldUrl);
      }
    }
   }

 return(Url);

free_return_null:
 if(line) free(line);
 if(Url) FreeURL(Url);
 return NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if a forced refresh of the URL is required based on the header from the client.

  int RequireForced Returns 1 if the page should be refreshed.

  const Header *request_head The head of the HTTP request to modify.

  const URL *Url The URL that is being requested.

  int online The online or offline status.
  ++++++++++++++++++++++++++++++++++++++*/

int RequireForced(const Header *request_head,const URL *Url,int online)
{
 int retval=0;
 char* maxage_val;

 if(online)
   {
    if(ConfigBooleanURL(PragmaNoCacheOnline,Url) &&
       GetHeader2(request_head,"Pragma","no-cache"))
      {
       PrintMessage(Debug,"Requesting URL (Pragma: no-cache).");
       retval=1;
      }
    else if(ConfigBooleanURL(CacheControlNoCacheOnline,Url) &&
       GetHeader2(request_head,"Cache-Control","no-cache"))
      {
       PrintMessage(Debug,"Requesting URL (Cache-Control: no-cache).");
       retval=1;
      }
    else if(ConfigBooleanURL(CacheControlMaxAge0Online,Url) &&
	    (maxage_val=GetHeader2Val(request_head,"Cache-Control","max-age")) &&
	    atol(maxage_val)==0)
      {
       PrintMessage(Debug,"Requesting URL (Cache-Control: max-age=0).");
       retval=1;
      }
    else if(ConfigBooleanURL(CookiesForceRefreshOnline,Url) &&
       GetHeader(request_head,"Cookie"))
      {
       PrintMessage(Debug,"Requesting URL (Cookie:).");
       retval=1;
      }
   }
 else
   {
    if(ConfigBooleanURL(PragmaNoCacheOffline,Url) &&
       GetHeader2(request_head,"Pragma","no-cache"))
      {
       PrintMessage(Debug,"Requesting URL (Pragma: no-cache).");
       retval=1;
      }
    else if(ConfigBooleanURL(CacheControlNoCacheOffline,Url) &&
       GetHeader2(request_head,"Cache-Control","no-cache"))
      {
       PrintMessage(Debug,"Requesting URL (Cache-Control: no-cache).");
       retval=1;
      }
    else if(ConfigBooleanURL(CacheControlMaxAge0Offline,Url) &&
	    (maxage_val=GetHeader2Val(request_head,"Cache-Control","max-age")) &&
	    atol(maxage_val)==0)
      {
       PrintMessage(Debug,"Requesting URL (Cache-Control: max-age=0).");
       retval=1;
      }
   }

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Examine the spooled file to see if it needs to be updated.

  int RequireChanges Returns 1 if the file needs changes made, 0 if not.

  int fd The file descriptor of the spooled file.

  const URL *Url The URL that is being requested.

  char **ims,char **inm If not NULL, these will contain the value fields
  of If-Modified-Since and If-None-Match header lines, that can be used
  to modify the request to ask for changes since the spooled file.

  ++++++++++++++++++++++++++++++++++++++*/

int RequireChanges(int fd,const URL *Url,char **ims,char **inm)
{
 struct stat buf;
 int status,retval=0;
 Header *spooled_head=NULL;

 status=ParseReply(fd,&spooled_head);

 if(status==0 || fstat(fd,&buf))
   {
    PrintMessage(Debug,"Requesting URL (Empty or no status).");
    retval=1;
   }
 else if(status<200 || status>=400)
   {
    PrintMessage(Debug,"Requesting URL (Error status (%d)).",status);
    retval=1;
   }
 else
   {
    time_t now=time(NULL);
    int temp_redirection = ((status==301 || status==302 || status==303 || status==307) && ConfigBooleanURL(RequestRedirection,Url));

    if(temp_redirection || ConfigBooleanURL(RequestExpired,Url))
      {
       char *expires,*maxage_val,*date;

       if((maxage_val=GetHeader2Val(spooled_head,"Cache-Control","max-age")) &&
          (date=GetHeader(spooled_head,"Date")))
          {
           time_t then=DateToTimeT(date);
           long maxage=atol(maxage_val);

           if((now-then)>maxage)
             {
	      char maxage_str[MAXDURATIONSIZE];

	      DurationToString_r(maxage,maxage_str);
              PrintMessage(Debug,"Requesting URL (Cache-Control expiry time of %s from '%s').",maxage_str,date);
              retval=1;
	      goto cleanup_return;
             }
          }
       else if((expires=GetHeader(spooled_head,"Expires")))
         {
          time_t when=DateToTimeT(expires);

          if(when<=now)
            {
             PrintMessage(Debug,"Requesting URL (Expiry date of '%s').",expires);
             retval=1;
	     goto cleanup_return;
            }
         }
       else if(temp_redirection)
	 {
	   PrintMessage(Debug,"Requesting URL (Redirection status %d).", status);
	   retval = 1;
	   goto cleanup_return;
	 }
      }

    if(ConfigBooleanURL(RequestNoCache,Url))
      {
       char *head,*val;

       if(GetHeader2(spooled_head,head="Pragma"       ,val="no-cache") ||
          GetHeader2(spooled_head,head="Cache-Control",val="no-cache"))
         {
          PrintMessage(Debug,"Requesting URL (No cache header '%s: %s').",head,val);
          retval=1;
	  goto cleanup_return;
         }
      }

    if(ConfigBooleanURL(RequestChangedOnce,Url) && buf.st_mtime>OnlineTime)
      {
	PrintMessage(Debug,"Not requesting URL (Only once per online session).");
	retval=0;
      }
    else 
      {
       time_t requestchanged=ConfigIntegerURL(RequestChanged,Url);

       if(requestchanged<0 || (now-buf.st_mtime)<requestchanged)
         {
	  char age[MAXDURATIONSIZE],config_age[MAXDURATIONSIZE];

	  DurationToString_r(now-buf.st_mtime,age);
	  DurationToString_r(requestchanged,config_age);
          PrintMessage(Debug,"Not requesting URL (Last changed %s ago, config is %s).", age,config_age);
          retval=0;
         }
       else if(!ConfigBooleanURL(RequestConditional,Url))
          retval=1;
       else
         {
	   char *date;
	   char *lastmodified=GetHeader(spooled_head,"Last-Modified");
	   char *etag;

	   if(inm && (etag=GetHeader(spooled_head,"Etag")) &&
	      (ConfigBooleanURL(ValidateWithEtag,Url) ||
               !(lastmodified && (date=GetHeader(spooled_head,"Date")) &&
                 (DateToTimeT(date)-DateToTimeT(lastmodified))>=60  ) ) )
	     {
	       *inm=strdup(etag);
	     }

	   if(ims)
	     *ims=strdup(lastmodified?lastmodified:
			 (date=GetHeader(spooled_head,"Date"))?date:
			 (date=GetHeader(spooled_head,"wwwoffle-cache-date"))?date:
			 RFC822Date(buf.st_mtime,1));

	   retval=1;
         }
      }
   }

cleanup_return:
 if(spooled_head)
    FreeHeader(spooled_head);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the spooled page has been modified for a conditional request.

  int IsModified Returns 1 if the file has been modified, 0 if not.

  int fd The file descriptor of the spooled file.

  const Header *request_head The head of the HTTP request to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsModified(int fd,const Header *request_head)
{
 int is_modified=1;
 Header *spooled_head=NULL;

 ParseReply(fd,&spooled_head);

 if(spooled_head)
   {
    int check_time=1;

    /* Check the entity tags */

    {
      char *etag_val=GetHeader(spooled_head,"Etag");
      if(etag_val)
	{
	  HeaderList *inm_vals=GetHeaderList(request_head,"If-None-Match");
	  if(inm_vals)
	    {
	      int i;

	      check_time=0;

	      for(i=0;i<inm_vals->n;++i)
		{
		  char *inm_val=inm_vals->item[i].val;
		  if((*inm_val=='*' && !*(inm_val+1)) || !strcmp(etag_val,inm_val))
		    {is_modified=0; check_time=1; break;}
		}

	      FreeHeaderList(inm_vals);
	    }
	}
    }

    /* Check the If-Modified-Since header if there are no matching Etags */

    if(check_time)
      {
       char *ims_val=GetHeader(request_head,"If-Modified-Since");

       if(ims_val)
         {
          time_t since=DateToTimeT(ims_val);
          char *modified=GetHeader(spooled_head,"Last-Modified");

	  if(!modified) {
	    modified=GetHeader(spooled_head,"Date");
	    if(!modified)
	      modified=GetHeader(spooled_head,"wwwoffle-cache-date");
	  }

          if(modified)
            {
             time_t modtime=DateToTimeT(modified);

             is_modified=(!modtime || modtime>since);
            }
          else
            {
             struct stat buf;

             if(!fstat(fd,&buf))
               {
                time_t modtime=buf.st_mtime;

                is_modified=(modtime>since);
               }
            }
         }
      }

    FreeHeader(spooled_head);
   }

 return(is_modified);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the location that the URL has been moved to.

  URL *MovedLocation Returns the new URL.

  const URL *Url The original URL.

  const Header *reply_head The head of the original HTTP reply.
  ++++++++++++++++++++++++++++++++++++++*/

URL *MovedLocation(const URL *Url,const Header *reply_head)
{
 char *location;
 URL *new;

 location=GetHeader(reply_head,"Location");

 if(!location)
    return(NULL);

 new=LinkURL(Url,location);

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a new request for a page.

  Header *RequestURL Ask for a page.

  const URL *Url The URL to get.

  const URL *refererUrl The Refering URL or NULL if none.
  ++++++++++++++++++++++++++++++++++++++*/

Header *RequestURL(const URL *Url,const URL *refererUrl)
{
 Header *new;

 {
   char top[sizeof("GET  HTTP/1.0")+strlen(Url->name)];
   stpcpy(stpcpy(stpcpy(top,"GET "),Url->name)," HTTP/1.0");
   CreateHeader(top,1,&new);
 }

 if(Url->user)
   {
     size_t userpasslen=strlen(Url->user)+(Url->pass?strlen(Url->pass):0)+1;
     char userpass[userpasslen+1];
     {
       char *p=stpcpy(userpass,Url->user);
       *p++=':';
       if(Url->pass) stpcpy(p,Url->pass); else *p=0;
     }
     {
       size_t encuserpasslen=base64enclen(userpasslen);
       char auth[sizeof("Basic ")+encuserpasslen];
       Base64Encode((unsigned char *)userpass,userpasslen,(unsigned char *)stpcpy(auth,"Basic "),encuserpasslen+1);
       AddToHeader(new,"Authorization",auth);
     }
   }

 if(refererUrl)
    AddToHeader(new,"Referer",refererUrl->name);

 if(reusable_header)
   CopyHeader(reusable_header,new,NULL);

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Finish with the reusable headers.
  ++++++++++++++++++++++++++++++++++++++*/

void FinishParse(void)
{
  if(reusable_header) {
    FreeHeader(reusable_header);
    reusable_header=NULL;
  }
}


/*++++++++++++++++++++++++++++++++++++++
  Modify the request taking into account censoring of header and modified URL.

  const URL *Url The actual URL.

  Header *request_head The head of the HTTP request possibly with a different URL.
  ++++++++++++++++++++++++++++++++++++++*/

void ModifyRequest(const URL *Url,Header *request_head)
{
 unsigned j;
 char *referer;

 /* Modify the top line of the header. */

 ChangeURLInHeader(request_head,Url->name);

 /* Remove the false arguments from POST/PUT URLs. */

 if(!strcmp(request_head->method,"POST") ||
    !strcmp(request_head->method,"PUT"))
   RemovePlingFromUrl(request_head->url);

 if(request_head->version && !strcmp(request_head->version,"HTTP/1.1"))
   request_head->version[7]='0';   /* "HTTP/1.1" -> "HTTP/1.0" */

 /* Add a host header */

 ReplaceOrAddInHeader(request_head,"Host",Url->hostport);

 /* Add a Connection / Proxy-Connection header */

 ReplaceOrAddInHeader(request_head,"Connection","close");
 ReplaceOrAddInHeader(request_head,"Proxy-Connection","close");

 /* Check the authorisation header. */

 if(Url->user)
   {
     size_t userpasslen=strlen(Url->user)+(Url->pass?strlen(Url->pass):0)+1;
     char userpass[userpasslen+1];
     {
       char *p=stpcpy(userpass,Url->user);
       *p++=':';
       if(Url->pass) stpcpy(p,Url->pass); else *p=0;
     }
     {
       size_t encuserpasslen=base64enclen(userpasslen);
       char auth[sizeof("Basic ")+encuserpasslen];
       Base64Encode((unsigned char *)userpass,userpasslen,(unsigned char *)stpcpy(auth,"Basic "),encuserpasslen+1);
       ReplaceOrAddInHeader(request_head,"Authorization",auth);
     }
   }
 else
   RemoveFromHeader(request_head,"Authorization");


 /* Remove some headers */

 for(j=0;j<sizeof(deleted_http_headers)/sizeof(char*);j++)
    RemoveFromHeader(request_head,deleted_http_headers[j]);

 RemoveFromHeader2(request_head,"Pragma","wwwoffle");

 /* Fix the Referer header */

 if((referer=GetHeader(request_head,"Referer")))
   RemovePlingFromUrl(referer);

 /* Modified by Paul Rombouts: different implementation of "referer-self" option */
 {
   char *newval, *optionname;

   if (ConfigBooleanURL(RefererSelfDir,Url)) {
     char *p=Url->pathendp;
     while(--p>=(Url->pathp) && *p!='/');
     ++p;
     newval=STRDUPA2(Url->name,p);
     optionname="RefererSelfDir";
   }
   else if(ConfigBooleanURL(RefererSelf,Url)) {
     newval=Url->name;
     optionname="RefererSelf";
   }
   else
     goto dontrefertoself;

   PrintMessage(Debug,"CensorRequestHeader (%s) replaced '%s' by '%s'.",optionname,referer?referer:"(none)",newval);
   ReplaceOrAddInHeader(request_head,"Referer",newval);
 dontrefertoself: ;
 }

 /* Force the insertion of a User-Agent header */

 if(ConfigBooleanURL(ForceUserAgent,Url) && !GetHeader(request_head,"User-Agent"))
   {
    PrintMessage(Debug,"CensorRequestHeader (ForceUserAgent) inserted '%s'.","WWWOFFLE/" WWWOFFLE_VERSION);
    AddToHeader(request_head,"User-Agent","WWWOFFLE/" WWWOFFLE_VERSION);
   }

 /* Censor the header */

 censor_header(CensorOutgoingHeader,Url,request_head);
}


/*++++++++++++++++++++++++++++++++++++++
  Change the request to one that contains an authorisation string if required.

  const URL *proxyUrl The URL of the proxy.

  Header *request_head The HTTP request head.
  ++++++++++++++++++++++++++++++++++++++*/

void MakeRequestProxyAuthorised(const URL *proxyUrl,Header *request_head)
{
 if(ProxyAuthUser && ProxyAuthPass)
   {
    char *user,*pass;

    user=ConfigStringURL(ProxyAuthUser,proxyUrl);
    pass=ConfigStringURL(ProxyAuthPass,proxyUrl);

    if(user && pass)
      {
       size_t userpasslen=strlen(user)+strlen(pass)+1;
       char userpass[userpasslen+1];

       {char *p=stpcpy(userpass,user); *p++=':'; stpcpy(p,pass); }
       {
	 size_t encuserpasslen=base64enclen(userpasslen);
	 char auth[sizeof("Basic ")+encuserpasslen];
	 Base64Encode((unsigned char *)userpass,userpasslen,(unsigned char *)stpcpy(auth,"Basic "),encuserpasslen+1);
	 ReplaceOrAddInHeader(request_head,"Proxy-Authorization",auth);
       }
       return;
      }
   }

 RemoveFromHeader(request_head,"Proxy-Authorization");
}


/*++++++++++++++++++++++++++++++++++++++
  Change the request from one to a proxy to a normal one.

  const URL *Url The URL of the request.

  Header *request_head The head of the HTTP request.
  ++++++++++++++++++++++++++++++++++++++*/

void MakeRequestNonProxy(const URL *Url,Header *request_head)
{
 /* Remove the full URL and replace it with just the path and args. */

 ChangeURLInHeader(request_head,Url->pathp);

 /* Remove the false arguments from POST/PUT URLs. */

 if(!strcmp(request_head->method,"POST") ||
    !strcmp(request_head->method,"PUT"))
   RemovePlingFromUrl(request_head->url);

 /* Remove the proxy connection & authorization headers. */

 RemoveFromHeader(request_head,"Proxy-Connection");
 RemoveFromHeader(request_head,"Proxy-Authorization");
}
#endif


/*++++++++++++++++++++++++++++++++++++++
  Parse the reply from the server.

  int ParseReply Return the numeric status of the reply.

  int fd The file descriptor to read from.

  Header **reply_head Return the head of the HTTP reply.
  ++++++++++++++++++++++++++++++++++++++*/

int ParseReply(int fd,Header **reply_head)
{
 char *line=NULL;

 *reply_head=NULL;

 while((line=read_line(fd,line)))
   {
    if(!*reply_head)   /* first line */
      {
       if(!CreateHeader(line,0,reply_head))
          break;
      }
    else
      if(!AddToHeaderRaw(*reply_head,line))
	break;
   }

 if(line) free(line);

 if(*reply_head)
    return((*reply_head)->status);
 else
    return(0);
}


#ifndef CLIENT_ONLY
/*++++++++++++++++++++++++++++++++++++++
  Find the status of a spooled page.

  int SpooledPageStatus Returns the status number.

  URL *Url The URL to check.

  int backup A flag to indicate that the backup file is to be used.
  ++++++++++++++++++++++++++++++++++++++*/

int SpooledPageStatus(URL *Url,int backup)
{
 int status=0;
 int spool;

 if(backup)
    spool=OpenBackupWebpageSpoolFile(Url);
 else
    spool=OpenWebpageSpoolFile(1,Url);

 if(spool!=-1)
   {
    char *reply;

    init_io(spool);

    reply=read_line(spool,NULL);

    if(reply)
      {
       sscanf(reply,"%*s %d",&status);
       free(reply);
      }

    finish_io(spool);
    close(spool);
   }

 return(status);
}


/*++++++++++++++++++++++++++++++++++++++
  Get the header of a spooled page.

  Header *SpooledPageHeader Returns the spooled header.

  URL *Url The URL of the page.

  int backup A flag to indicate that the backup file is to be used.
  ++++++++++++++++++++++++++++++++++++++*/

Header *SpooledPageHeader(URL *Url,int backup)
{
 Header *spooled_head=NULL;
 int spool;

 if(backup)
    spool=OpenBackupWebpageSpoolFile(Url);
 else
    spool=OpenWebpageSpoolFile(1,Url);

 if(spool!=-1)
   {

    init_io(spool);

    ParseReply(spool,&spooled_head);

    finish_io(spool);
    close(spool);
   }

 return(spooled_head);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide which type of content compression was used.

  int WhichCompression Returns the compression method, 1 for deflate and 2 for gzip.

  char *content_encoding The string representing the content encoding the client/server used.
  ++++++++++++++++++++++++++++++++++++++*/

int WhichCompression(char *content_encoding)
{
  return (!strcmp(content_encoding,"deflate") || !strcmp(content_encoding,"x-deflate"))? 1 :
         (!strcmp(content_encoding,"gzip") || !strcmp(content_encoding,"x-gzip"))      ? 2 :
         0;
}

/*++++++++++++++++++++++++++++++++++++++
  Decide which compression that we can use for the reply to the client.

  int AcceptWhichCompression Returns the compression method, 1 for deflate and 2 for gzip.

  HeaderList *list The list representing the content-encodings the client/server accepts.
  ++++++++++++++++++++++++++++++++++++++*/

int AcceptWhichCompression(HeaderList *list)
{
 float q_deflate=0,q_gzip=0,q_identity=1;
 int i;

 for(i=0;i<list->n;++i)
   if(list->item[i].qval>0)
      {
       if(!strcmp(list->item[i].val,"deflate") || !strcmp(list->item[i].val,"x-deflate"))
          q_deflate=list->item[i].qval;
       else if(!strcmp(list->item[i].val,"gzip") || !strcmp(list->item[i].val,"x-gzip"))
          q_gzip=list->item[i].qval;
       else if(!strcmp(list->item[i].val,"identity"))
          q_identity=list->item[i].qval;
      }


 /* Deflate is a last resort, see comment in iozlib.c. */

 return (q_identity>q_gzip && q_identity>q_deflate) ? 0:
        (q_gzip>0)                                  ? 2:
        (q_deflate>0)                               ? 1:
                                                      0;
}


/*++++++++++++++++++++++++++++++++++++++
  Modify the reply taking into account censoring of the header.

  const URL *Url The URL that this reply comes from.

  Header *reply_head The head of the HTTP reply.
  ++++++++++++++++++++++++++++++++++++++*/

void ModifyReply(const URL *Url,Header *reply_head)
{
 if(reply_head->version && !strcmp(reply_head->version,"HTTP/1.1"))
   reply_head->version[7]='0';   /* "HTTP/1.1" -> "HTTP/1.0" */

 /* Add a Connection / Proxy-Connection header */

 ReplaceOrAddInHeader(reply_head,"Connection","close");
 ReplaceOrAddInHeader(reply_head,"Proxy-Connection","close");

 /* Send errors instead when we see Location headers that send the client to a blocked page. */
 
 if(!IsLocalHost(Url) && ConfigBooleanURL(DontGetLocation,Url))
   {
    char *location;

    if((location=GetHeader(reply_head,"Location")))
      {
       URL *locUrl=SplitURL(location);
 
       if(ConfigBooleanMatchURL(DontGet,locUrl))
         {
	  int locerror=1;
	  char *replace=ConfigStringURL(DontGetReplacementURL,locUrl);
	  if(replace) {
	    URL *replaceUrl=SplitURL(replace);
	    if(IsLocalHost(replaceUrl) &&
	       (!strchr(replaceUrl->path+1,'/') || !strcmp_litbeg(replaceUrl->path,"/local/")) &&
	       IsCGIAllowed(replaceUrl->path))
	      {
		/* If the replacement is (potentially) a CGI executable script, no location error. */
		locerror=0;
	      }
	    FreeURL(replaceUrl);
	  }

	  if(locerror) {
	    reply_head->status=404;
	    RemoveFromHeader(reply_head,"Location");
	  }
         }
 
       FreeURL(locUrl);
      }
   }
 
 /* Delete the "expires" field from "Set-Cookie:" server headers.
    Most browsers will not store such cookies permanently and forget them in between sessions.
    An exception is made for an expire date in the past. These should cause browsers
    to delete cookies and we don't want to prevent that */

 if(!IsLocalHost(Url) && ConfigBooleanURL(SessionCookiesOnly,Url)) {
   KeyValueNode *line;
   for(line=reply_head->line; line; line=line->next) {
     if(!strcasecmp(line->key,"Set-Cookie")) {
       char *str=line->val,*p,*q;

       for(p=str;;) {
	 for(;;++p) {if(!*p) goto nextline; if(*p==';') break;}
	 q=p;
	 do {if(!*++p) goto nextline;} while(isspace(*p));
	 if(!strcasecmp_litbeg(p,"expires")) {
	   int comma=0;
	   p+=strlitlen("expires");
	   for(;;++p) {if(!*p) goto nextline; if(!isspace(*p)) break;}
	   if(*p!='=') continue;
	   do {if(!*++p) goto chop_line;} while(isspace(*p));
	   {
	     char *s=p;
	     while(*p!=';' && (*p!=',' || !(comma++))) {if(!*++p) break;}
	     if(s<p && STRDUP3(s,p,DateToTimeT)<time(NULL)) continue;
	   }
	   if(!*p) goto chop_line;
	   {char *t=p; p=q; while((*q++=*t++));}
	 }
       }

     chop_line:
       while(--q>=str && isspace(*q));
       *++q=0;

     }
   nextline: ;
   }
 }

 /* Remove headers used by WWWOFFLE for internal use only. */

 RemoveFromHeader(reply_head,"wwwoffle-cache-date");

 /* Censor the header */

 censor_header(CensorIncomingHeader,Url,reply_head);
}

/*
  censor_header removes or replaces header lines as specifed in the
  CensorIncomingHeader or CensorOutgoingHeader section of the configuration file.

  const URL *url: the requested URL.
  Header *head: the header to censor.
*/

static void censor_header(ConfigItem confitem,const URL *Url,Header *head)
{
 KeyValueNode *prev,*line,*p,*l;
 unsigned j;
 char *censor;

 prev=NULL;
 line=head->line;
 while(line)
   {
    for(j=0;j<sizeof(non_censored_headers)/sizeof(char*);++j)
       if(!strcasecmp(non_censored_headers[j],line->key))
          goto nextline;

    if((censor=CensoredHeader(confitem,Url,line->key,line->val)))
      {
       if(censor!=line->val)
         {
          PrintMessage(Debug,"CensorHeader replaced '%s: %s' by '%s: %s'.",line->key,line->val,line->key,censor);
          free(line->val);
          line->val=censor;

	  /* remove any remaining values with the same key */
	  p= line;
	  l= line->next;
	  while(l) {
	    if(!strcasecmp(l->key,line->key)) {
	      PrintMessage(Debug,"CensorHeader removed '%s: %s'.",l->key,l->val);
	      l=RemoveLineFromHeader(head,l,p);
	    }
	    else {
	      p= l;
	      l= l->next;
	    }
	  }
	 }
      }
    else
      {
       PrintMessage(Debug,"CensorHeader removed '%s: %s'.",line->key,line->val);
       line=RemoveLineFromHeader(head,line,prev);
       goto skipline;
      }
   nextline:
    prev=line;
    line=line->next;
   skipline: ;
   }
}
#endif
