/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/wwwoffles.c 2.320 2007/04/12 18:43:54 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  A server to fetch the required pages.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1996,97,98,99,2000,01,02,03,04,05,06,07 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2005,2006,2007 Paul A. Rombouts
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
#include <errno.h>

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
#include <signal.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "headbody.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "proto.h"
#include "document.h"
#include "version.h"

#define MAX_REDIRECT 8

/* pass on name of proxy user via global variable */
char* proxy_user=NULL;

static void uninstall_sighandlers(void);

inline static int is_wwwoffle_error_message(Header *head)
{
  return(head->note && (!strcasecmp(head->note,"WWWOFFLE Remote Host Error") ||
			!strcasecmp(head->note,"WWWOFFLE Requested Resource Gone")));
}

inline static int keepspoolfile(URL *Url,int backup)
{
  int retval=0;
  Header *spooled_head=SpooledPageHeader(Url,backup);
  if(spooled_head) {
    retval= (spooled_head->status==200 && !ConfigHeaderMatch(KeepCacheIfHeaderMatches,Url,spooled_head));
    FreeHeader(spooled_head);
  }
  return retval;
}


/*+ The mode of operation of the server. +*/
typedef enum _Mode
{
 None,                          /*+ Undecided. +*/

 Real,                          /*+ From server host to cache and client. +*/
 RealNoCache,                   /*+ From server host to client. +*/
 RealRefresh,                   /*+ Refresh the page, forced from index. +*/
 RealNoPassword,                /*+ From server host to cache, not using supplied password. +*/
 SpoolOrReal,                   /*+ Spool if already cached else Real. +*/

 Fetch,                         /*+ From server host to cache. +*/
 FetchNoPassword,               /*+ From server host to cache, not using supplied password. +*/

 Spool,                         /*+ From cache to client. +*/
 SpoolGet,                      /*+ Not in cache so record request in outgoing. +*/
 SpoolRefresh,                  /*+ Refresh the page, forced from refresh page. +*/
 SpoolPragma,                   /*+ Refresh the page, forced from client by 'Pragma: no-cache'. +*/

 SpoolInternal,                 /*+ The page has been internally generated in a temporary file. +*/
 InternalPage                   /*+ The page has been internally generated and sent to the client directly. +*/
}
Mode;


/*+ Variables to allow the modifications to take place on the data stream; +*/
static URL *modify_Url;         /*+ the URL that is being modified. +*/

/*+ Variables to allow the modifications to take place on the data stream; +*/
static const Protocol *modify_UrlProtocol; /*+ the Protocol for the URL that is being modified. +*/

/*+ Variables to allow the modifications to take place on the data stream; +*/
static int modify_read_fd,      /*+ the file descriptor to read from. +*/
           modify_write_fd,     /*+ the file descriptor to write to. +*/
           modify_copy_fd;      /*+ the file descriptor to copy the data to. +*/

/*+ Variables to allow the modifications to take place on the data stream; +*/
static ssize_t modify_err,      /*+ the error when writing. +*/
               modify_n;        /*+ the number of bytes read. +*/


/*++++++++++++++++++++++++++++++++++++++
  The main server program.

  int wwwoffles Returns the exit status.

  int online Whether the demon is online or not.

  int fetching Set to true if the request is to do a fetch.

  int client The file descriptor of the client.
  ++++++++++++++++++++++++++++++++++++++*/

int wwwoffles(int online,int fetching,int client)
{
 int outgoing_read=-1,outgoing_write=-1,spool=-1,tmpclient=-1,server=-1,is_server=0;
 int exitval=-1,fetch_again=0;
 URL *outgoingUrl=NULL;
 Header *request_head=NULL,*reply_head=NULL;
 Body   *request_body=NULL;
 int reply_status=-1;
 URL *Url=NULL,*Urlpw=NULL;
 const Protocol* UrlProtocol=NULL;
 Mode mode=None;
 int outgoing_exists=0,outgoing_exists_pw=0,createlasttimespoolfile=0;
 time_t spool_exists=0,spool_exists_pw=0;
 int conditional_request_ims=0,conditional_request_inm=0,not_modified_spool=0;
 int offline_request=1;
 int is_client_wwwoffle=0;
 int is_client_searcher=0;
#if USE_ZLIB
 int client_compression=0,request_compression=0,server_compression=0;
#endif
 int client_chunked=0,request_chunked=0,server_chunked=0;
 int redirect_count=0;


 /*----------------------------------------
   Initialise things, work out the mode from the options.
   ----------------------------------------*/

 uninstall_sighandlers();

 InitErrorHandler("wwwoffles",-1,-1); /* change name nothing else */

 head_only=0;

 if(online==1 && !fetching)
    mode=Real;
 else if(online!=0 && fetching)
    mode=Fetch;
 else if(online==-1 && !fetching)
    mode=SpoolOrReal;
 else if(!online && !fetching)
    mode=Spool;
 else
    PrintMessage(Fatal,"Started in a mode that is not allowed (online=%d, fetching=%d).",online,fetching);


 /*----------------------------------------
   mode = Spool, Real, SpoolOrReal or Fetch

   Set up the input file to read the request from, either client or stored in outgoing.
   ----------------------------------------*/

 /* Check the client file descriptor (client connection). */

 if(client==-1 && mode!=Fetch)
    PrintMessage(Fatal,"Cannot use client file descriptor %d.",client);

#if USE_GNUTLS

 /* Check if we are using http or https client connection. */

 if(mode!=Fetch)
   {
    char *host,*ip;
    int port;

    if(!SocketLocalName(client,&host,&ip,&port))
      {
       SetLocalPort(port);

       if(port==ConfigInteger(HTTPS_Port))
	 if(configure_io_gnutls(client,host?host:ip,1))
             PrintMessage(Fatal,"Cannot start SSL/TLS connection");
      }
   }

#endif

 /* Open up the outgoing file for reading the stored request from. */

 if(mode==Fetch)
   {
    outgoing_read=OpenExistingOutgoingSpoolFile(&outgoingUrl);

    if(outgoing_read==-1 || !outgoingUrl)
      {
       PrintMessage(Inform,"No more outgoing requests.");
       exitval=3; goto close_client;
      }

    init_io(outgoing_read);
   }


 /*----------------------------------------
   mode = Spool, Real, SpoolOrReal or Fetch

   Parse the request and make some checks on the type of request and request options.
   ----------------------------------------*/

 /* Get the URL from the request and read the header and body (if present). */

 if(mode==Real || mode==Spool || mode==SpoolOrReal)
    Url=ParseRequest(client,&request_head,&request_body);
 else /* if(mode==Fetch) */
   {
    Url=ParseRequest(outgoing_read,&request_head,&request_body);
    if(Url) FreeURL(Url);
    Url=outgoingUrl;
   }

#if USE_GNUTLS
 checkrequest:
#endif

 if(StderrLevel==ExtraDebug)
   {
     if(request_head)
       {
	 char *headerstring= HeaderString(request_head,NULL);
	 PrintMessage(ExtraDebug,"Incoming Request Head (from client)\n%s",headerstring);
	 free(headerstring);

	 if(request_body)
	   {
	     if(request_head->method && !strcmp(request_head->method,"POST")) /* only POST is guaranteed to be ASCII. */
	       PrintMessage(ExtraDebug,"Incoming Request Body (from client)\n%s",request_body->content);
	     else
	       PrintMessage(ExtraDebug,"Incoming Request Body (from client) is %d bytes of binary data",request_body->length);
	   }
       }
     else
       PrintMessage(ExtraDebug,"Incoming Request Head (from client) is empty");
   }

 /* Check that a URL was read, means a valid header was received. */

 if(!Url || !request_head)
   {
    PrintMessage(Warning,"Could not parse HTTP request (%s).",request_head?"Parse error":"Empty request"); /* Used in audit-usage.pl */

    if(mode!=Fetch)
       HTMLMessage(client,500,"WWWOFFLE Server Error",NULL,"ServerError",
                   "error",request_head?"Cannot parse the HTTP request":"The HTTP request was empty",
                   NULL);
    else  /* mode==Fetch */
       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [%s]\n",Url?Url->name:"URL",
                          request_head?"Cannot parse the HTTP request":"The HTTP request was empty");

    exitval=1; goto free_request_head_body;
   }

 /* Check if the client can use compression and work out which type. */

#if USE_ZLIB
 if(mode!=Fetch && ConfigBoolean(ReplyCompressedData))
   {
    HeaderList *accept_encodings=GetHeaderList(request_head,"Accept-Encoding");

    if(accept_encodings)
      {
       client_compression=AcceptWhichCompression(accept_encodings);
       FreeHeaderList(accept_encodings);

       if(StderrLevel>=0 && StderrLevel<=Debug)
	 {
	   char *accept_encoding= GetHeaderCombined(request_head,"Accept-Encoding");
	   PrintMessage(Debug,"Client can 'Accept-Encoding: %s' we choose to use %s encoding.",accept_encoding,
			client_compression==1?"deflate":client_compression==2?"gzip":"identity");
	   free(accept_encoding);
	 }
      }
   }
#endif

 /* Check if the client can use chunked encoding. */

 if(mode!=Fetch && ConfigBoolean(ReplyChunkedData) && !strcmp(request_head->version,"HTTP/1.1"))
   {
     client_chunked=1;

     PrintMessage(Debug,"Client is 'HTTP/1.1' we choose to use chunked encoding.");
   }

 /* Store the client's compression and chunked encoding preferences for message page generation. */

#if USE_ZLIB
 SetMessageOptions(client_compression,client_chunked);
#else
 SetMessageOptions(0,client_chunked);
#endif

 /* Store the client's language preferences for message page generation. */

 SetLanguage(request_head);

 /* Check if the client is actually the wwwoffle program. */

 is_client_wwwoffle=(GetHeader2(request_head,"Pragma","wwwoffle-client")!=NULL);

 /* Check the request's proxy authentication. */
 {
   char *proxy_auth=GetHeader(request_head,"Proxy-Authorization");

   if(proxy_user) free(proxy_user);
   proxy_user=IsAllowedConnectUser(proxy_auth);
   if(proxy_user)
     PrintMessage(Inform,"HTTP Proxy connection from user '%s'.",proxy_user); /* Used in audit-usage.pl */
   else if(!IsAllowedConnectAllUsers)
     {
       PrintMessage(Inform,"HTTP Proxy connection rejected from unauthenticated user."); /* Used in audit-usage.pl */

       if(mode==Fetch)
	 {
	   if(client!=-1)
	     write_formatted(client,"Cannot fetch %s [HTTP request had unauthenticated user]\n",Url->name);

	   exitval=1; goto clean_up;
	 }
       else /* mode!=Fetch */
	 {
	   HTMLMessageHead(client,407,"WWWOFFLE Proxy Authentication Required",
			   "Proxy-Authenticate","Basic realm=\"wwwoffle-proxy\"",
			   NULL);
	   if(out_err!=-1 && !head_only)
	     HTMLMessageBody(client,"ProxyAuthFail",
			     NULL);
	   mode=InternalPage; goto internalpage;
	 }
     }
 }

 /* Check for the request method used */

 if(!strcasecmp(request_head->method,"CONNECT"))
   {
    if(mode==Fetch)
      {
       PrintMessage(Warning,"A 'CONNECT' method request for '%s' cannot be handled in Fetch mode.",Url->hostport);

       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [HTTP method 'CONNECT' not supported in this mode]\n",Url->name);

       exitval=1; goto clean_up;
      }
    else if(mode==Spool && !IsLocalNetHost(Url->host) && !(USE_GNUTLS && ConfigBoolean(SSLEnableCaching)))
      {
       PrintMessage(Warning,"A 'CONNECT' method request for '%s' cannot be handled in Spool mode.",Url->hostport);

       HTMLMessage(client,500,"WWWOFFLE Server Error",NULL,"ServerError",
                   "error","SSL proxy connection while offline is not allowed.",
                   NULL);
       mode=InternalPage; goto internalpage;
      }
    else if(ConfigBooleanMatchURL(DontGet,Url))
      {
       PrintMessage(Inform,"The URL '%s' matches one in the list not to get.",Url->name);

       HTMLMessage(client,404,"WWWOFFLE Host Not Got",NULL,"HostNotGot",
                   "url",Url->name,
                   NULL);
       mode=InternalPage; goto internalpage;
      }
    else if((IsSSLAllowed(Url,0) && mode!=Spool) || IsLocalNetHost(Url->host)) /* tunnel SSL connection */
      {
       char *err;

       PrintMessage(Inform,"SSL(tunnel)='%s'.",Url->hostport); /* Used in audit-usage.pl */

       err=SSL_Open(Url);

       if(err && ConfigBoolean(ConnectRetry))
         {
          PrintMessage(Inform,"Waiting to try connection again.");
          sleep(10);
          free(err);
          err=SSL_Open(Url);
         }

       if(err)
         {
          HTMLMessage(client,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
                      "url",Url->hostport,
                      "reason",err,
                      "cache",NULL,
                      "backup",NULL,
                      NULL);
          free(err);
          mode=InternalPage; goto internalpage;
         }

       out_err=0;
       err=SSL_Request(client,Url,request_head);

       if(err)
         {
          HTMLMessage(client,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
                      "url",Url->hostport,
                      "reason",err,
                      "cache",NULL,
                      "backup",NULL,
                      NULL);
          free(err);
          mode=InternalPage; goto internalpage;
         }

       if(out_err==-1)
	 PrintMessage(Inform,"Error writing to client [%!s]; client disconnected?");
       else {
	 SSL_Transfer(client);
	 SSL_Close();
       }

       exitval=0; goto clean_up;
      }
#if USE_GNUTLS
    else if(ConfigBoolean(SSLEnableCaching) && IsSSLAllowed(Url,1)) /* cache SSL connection */
      {
       URL *httpsUrl;

       PrintMessage(Inform,"SSL(cached)='%s'.",Url->hostport); /* Used in audit-usage.pl */

       if(write_string(client,"HTTP/1.0 200 WWWOFFLE SSL OK\r\n\r\n")==-1) {
	 PrintMessage(Inform,"Error writing to client [%!s]; client disconnected?");
	 exitval=0; goto clean_up;
       }

       if(configure_io_gnutls(client,Url->host,IsLocalHost(Url)?1:2))
          PrintMessage(Fatal,"Could not initialise SSL connection to client.");

       SetLocalPort(ConfigInteger(HTTPS_Port));

       FreeURL(Url); Url=NULL;
       if(request_head)
	 FreeHeader(request_head);
       if(request_body)
	 FreeBody(request_body);

       httpsUrl=ParseRequest(client,&request_head,&request_body);

       if(httpsUrl) {
	 Url=MakeModifiedURL(httpsUrl,REPLACEURLPROTO,"https",NULL,NULL,NULL,NULL,NULL);
	 FreeURL(httpsUrl);
       }

       goto checkrequest;
      }
#endif
    else
      {
       PrintMessage(Warning,"A SSL proxy connection for %s was received but is not allowed.",Url->hostport);

       HTMLMessage(client,500,"WWWOFFLE Server Error",NULL,"ServerError",
                   "error","SSL proxy connection to specified host and port is not allowed.",
                   NULL);
       mode=InternalPage; goto internalpage;
      }
   }
 else if(!strcmp(request_head->method,"HEAD"))
   {
    if(mode==Fetch)
       strcpy(request_head->method,"GET");

    head_only=1;
   }
 else if(strcmp(request_head->method,"GET") &&
         strcmp(request_head->method,"POST") &&
         strcmp(request_head->method,"PUT"))
   {
    PrintMessage(Warning,"The requested method '%s' is not supported.",request_head->method); /* Used in audit-usage.pl */

    if(mode==Fetch)
      {
       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [The %s method is not supported]\n",Url->name,request_head->method);
       exitval=1; goto clean_up;
      }
    else /* mode!=Fetch */
      {
       HTMLMessage(client,501,"WWWOFFLE Method Unsupported",NULL,"MethodUnsupported",
                   "method",request_head->method,
                   "protocol","all",
                   NULL);
       mode=InternalPage; goto internalpage;
      }
   }

 /* Check caching is allowed for SSL connections when Fetching */

 if(mode==Fetch && !strcmp(Url->proto,"https"))
   {
    if(IsSSLAllowed(Url,0)) /* tunnel SSL connection */
      {
       PrintMessage(Warning,"A SSL request for %s was received but tunnelling only is allowed.",Url->hostport);

       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [HTTPS tunnelling not supported in this mode]\n",Url->name);

       exitval=1; goto clean_up;
      }
#if USE_GNUTLS
    else if(ConfigBoolean(SSLEnableCaching) && IsSSLAllowed(Url,1)) /* cache SSL connection */
       ;
#endif
    else
      {
       PrintMessage(Warning,"A SSL request for %s was received but not allowed.",Url->hostport);

       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [HTTPS not allowed for this host]\n",Url->name);

       exitval=1; goto clean_up;
      }
   }


 /*----------------------------------------
   mode = Spool, Real, SpoolOrReal or Fetch

   Modify the URL requested based on Alias and DontGet sections of the configuration file.
   ----------------------------------------*/

 PrintMessage(Inform,"URL='%s'%s.",Url->name,Url->user?" (With username/password)":""); /* Used in audit-usage.pl */
 PrintMessage(Debug,"proto='%s'; hostport='%s'; path='%s'; args='%s'; user:pass='%s:%s'.",
              Url->proto,Url->hostport,Url->path,Url->args,Url->user,Url->pass);

 /* Check for an alias. */

 {
   URL *aliasUrl;

   if((aliasUrl=GetAliasURL(Url)))
     {
       PrintMessage(Inform,"Aliased URL='%s'%s.",aliasUrl->name,aliasUrl->user?" (With username/password)":"");
       PrintMessage(Debug,"Aliased proto='%s'; hostport='%s'; path='%s'; args='%s'; user:pass='%s:%s'.",
		    aliasUrl->proto,aliasUrl->hostport,aliasUrl->path,aliasUrl->args,aliasUrl->user,aliasUrl->pass);

       if(mode==Fetch)
	 {
	   FreeURL(Url);
	   Url=aliasUrl;
	   ChangeURLInHeader(request_head,Url->file);
	 }
       else /* mode!=Fetch */
	 {
	   HTMLMessage(client,302,"WWWOFFLE Alias Redirect",aliasUrl->file,"Redirect",
		       "location",aliasUrl->file,
		       NULL);

	   FreeURL(aliasUrl);
	   mode=InternalPage; goto internalpage;
	 }
     }
 }

 /* Check for a DontGet URL and its replacement. */
 /* Must be here so that local URLs can be in the DontGet section. */

 if(ConfigBooleanMatchURL(DontGet,Url))
   {
    PrintMessage(Inform,"The URL '%s' matches one in the list not to get.",Url->name);

    if(mode==Fetch)
      {
       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [Not Got]\n",Url->name);

       exitval=0; goto clean_up;
      }
    else /* mode!=Fetch */
      {
	char *replace=ConfigStringURL(DontGetReplacementURL,Url);

	if(!replace)
	  {
	    HTMLMessage(client,404,"WWWOFFLE Host Not Got",NULL,"HostNotGot",
			"url",Url->name,
			NULL);
	    mode=InternalPage; goto internalpage;
	  }
	else
	  {
	    FreeURL(Url);
	    Url=SplitURL(replace);

	    PrintMessage(Inform,"Replaced URL='%s'%s.",Url->name,Url->user?" (With username/password)":"");
	    PrintMessage(Debug,"Replaced proto='%s'; hostport='%s'; path='%s'; args='%s'; user:pass='%s:%s'.",
			 Url->proto,Url->hostport,Url->path,Url->args,Url->user,Url->pass);
	  }
      }
   }

 /* Is the specified protocol valid? */

 if(!IsProtocolHandled(Url))
   {
    PrintMessage(Inform,"The protocol '%s' is not available.",Url->proto);

    if(mode==Fetch)
      {
       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [Protocol not available]\n",Url->name);
       exitval=1; goto clean_up;
      }
    else /* mode!=Fetch */
      {
       HTMLMessage(client,404,"WWWOFFLE Illegal Protocol",NULL,"IllegalProtocol",
                   "url",Url->name,
                   "protocol",Url->proto,
                   NULL);
       mode=InternalPage; goto internalpage;
      }
   }

 /* Can a POST or PUT request be made with this protocol? */

 if((!strcmp(request_head->method,"POST") && !IsPOSTHandled(Url)) ||
    (!strcmp(request_head->method,"PUT") && !IsPUTHandled(Url)))
   {
    PrintMessage(Warning,"The requested method '%s' is not supported for the %s protocol.",request_head->method,Url->proto);

    if(mode==Fetch)
      {
       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [The %s method is not supported for %s]\n",Url->name,request_head->method,Url->proto);
       exitval=1; goto clean_up;
      }
    else /* mode!=Fetch */
      {
       HTMLMessage(client,501,"WWWOFFLE Method Unsupported",NULL,"MethodUnsupported",
                   "method",request_head->method,
                   "protocol",Url->proto,
                   NULL);
       mode=InternalPage; goto internalpage;
      }
   }


 /*----------------------------------------
   mode = Spool, Real, SpoolOrReal or Fetch

   Handle the local URLs, all built-in web pages.
   ----------------------------------------*/

 /* If in Fetch mode then setup the default recursive fetch options. */

 if(mode==Fetch)
    DefaultRecurseOptions(Url,request_head);

 /* If a local URL handle it. */

 if(IsLocalHost(Url))
   local_url:
   {
    /* Refresh requests, recursive fetches, refresh options etc. */

    if(!strcmp_litbeg(Url->path,"/refresh"))
      {
       int recurse=0;
       URL *newUrl=RefreshPage(client,Url,request_body,&recurse);

       /* An internal page ( /refresh-options/ ) or some sort of error. */

       if(!newUrl)
         {
          if(mode==Fetch)
            {
             if(client!=-1)
                write_formatted(client,"Cannot fetch %s [Error with refresh URL]\n",Url->name);
             exitval=1; goto clean_up;
            }
          else /* mode!=Fetch */
            {
             mode=InternalPage; goto internalpage;
            }
         }

       /* A new URL to get to replace the original one. */

       PrintMessage(Inform,"Refresh newUrl='%s'%s.",newUrl->name,newUrl->user?" (With username/password)":"");
       PrintMessage(Debug,"Refresh proto='%s'; hostport='%s'; path='%s'; args='%s'; user:pass='%s:%s'.",
                    newUrl->proto,newUrl->hostport,newUrl->path,newUrl->args,newUrl->user,newUrl->pass);

       /* The new URL was originally a POST or PUT request. */

       if(newUrl->args && *newUrl->args=='!')
         {
          PrintMessage(Inform,"It is not possible to refresh a URL that used the POST/PUT method.");

          if(mode==Fetch)
            {
             if(client!=-1)
                write_formatted(client,"Cannot fetch %s [Reply from a POST/PUT method]\n",Url->name);
	     FreeURL(newUrl);
             exitval=1; goto clean_up;
            }
          else /* mode!=Fetch */
            {
             HTMLMessage(client,404,"WWWOFFLE Cant Refresh POST/PUT",NULL,"CantRefreshPosted",
                         "url",newUrl->name,
                         NULL);
	     FreeURL(newUrl);
             mode=InternalPage; goto internalpage;
            }
         }

       if(recurse==1)
         {
	   fetch_again=1;

	   if(mode==Fetch)
	     /* A recursive URL ( /refresh-recurse/ ) in Fetch mode is fetched. */
	     ;
	   else /* mode!=Fetch */
	     {
	       /* A recursive URL ( /refresh-recurse/ ) in other modes is unchanged. */
	       mode=SpoolGet;

	       FreeURL(newUrl);
	       newUrl=Url;
	     }
	 }

       /* A recursive URL ( /refresh-request/ ) in other modes is requested. */

       else if(recurse==-1)
         {
          fetch_again=1;

          mode=SpoolGet;

          FreeHeader(request_head);
          request_head=RequestURL(newUrl,NULL);
          if(request_body)
             FreeBody(request_body);
          request_body=NULL;
         }

       /* A non-recursive URL ( /refresh/ ) in spool mode needs refreshing (redirect to real page). */

       else if(mode==Spool)
          mode=SpoolRefresh;

       /* A non-recursive URL ( /refresh/ ) in online mode needs refreshing (redirect to real page). */

       else if(mode==Real || mode==SpoolOrReal)
         {
	  if(ConfigBooleanURL(KeepCacheIfNotFound,newUrl) && keepspoolfile(newUrl,0)) {
	    PrintMessage(Debug,"keep-cache-if-not-found is enabled for '%s', keeping the spool file as backup.",newUrl->name);
	    CreateBackupWebpageSpoolFile(newUrl,1);
	  }
	  else {
	    char *err=DeleteWebpageSpoolFile(newUrl,0);
	    if(err) free(err);
	  }

          if(is_client_wwwoffle)
             mode=Real;
          else
             mode=RealRefresh;
         }

       /* A non-recursive URL ( /refresh/ ) or wrong sort of recursive URL ( /refresh-request/ )
          in Fetch mode should not happen. */

       else /* if(mode==Fetch) */
         {
          if(client!=-1)
             write_formatted(client,"Cannot fetch %s [Error with refresh URL]\n",Url->name);
          exitval=1; goto clean_up;
         }

       /* Swap to the new URL if it is different. */

       if(Url!=newUrl)
         {
          if(Url->user)
             ChangePasswordURL(newUrl,Url->user,Url->pass);

          FreeURL(Url);
          Url=newUrl;

          PrintMessage(Inform,"Refresh URL='%s'%s.",Url->name,Url->user?" (With username/password)":"");
          PrintMessage(Debug,"Refresh proto='%s'; hostport='%s'; path='%s'; args='%s'; user:pass='%s:%s'.",
                       Url->proto,Url->hostport,Url->path,Url->args,Url->user,Url->pass);
         }
      }

    /* Cannot fetch a local page except for refresh requests handled above. */

    else if(mode==Fetch)
      {
       PrintMessage(Inform,"The request to fetch a page from the local host is ignored.");
       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [On local host]\n",Url->name);
       exitval=1; goto clean_up;
      }

    /* The index pages. */

    else if(!strcmp_litbeg(Url->path,"/index/")) /* mode!=Fetch */
      {
       IndexPage(client,Url);
       mode=InternalPage; goto internalpage;
      }

    /* The info pages. */

    else if(!strcmp_litbeg(Url->path,"/info/")) /* mode!=Fetch */
      {
       InfoPage(client,Url,request_head,request_body);
       mode=InternalPage; goto internalpage;
      }

    /* The control pages, deleting and URL replacements for wwwoffle program. */

    else if(!strcmp_litbeg(Url->path,"/control/")) /* mode!=Fetch */
      {
       ControlPage(client,Url,request_body);
       mode=InternalPage; goto internalpage;
      }

    /* The configuration editing pages. */

    else if(!strcmp_litbeg(Url->path,"/configuration/")) /* mode!=Fetch */
      {
       ConfigurationPage(client,Url,request_body);
       mode=InternalPage; goto internalpage;
      }

    /* The monitor request and options pages, (note that there is no trailing '/'). */

    else if(!strcmp_litbeg(Url->path,"/monitor")) /* mode!=Fetch */
      {
       MonitorPage(client,Url,request_body);
       mode=InternalPage; goto internalpage;
      }

    /* The search pages. */

    else if(!strcmp_litbeg(Url->path,"/search/")) /* mode!=Fetch */
      {
	if((tmpclient=SearchPage(client,Url,request_head,request_body))==-1)
	  {mode=InternalPage; goto internalpage;}
	else
	  {mode=SpoolInternal; goto spoolinternal;}
      }

#if USE_GNUTLS

    /* The https certificate pages. */

    else if(!strcmp_litbeg(Url->path,"/certificates/")) /* mode!=Fetch */
      {
       CertificatesPage(client,Url,request_head);
       mode=InternalPage; goto internalpage;
      }

#endif

    /* The local pages in the root or local directories. */

    else if(!strchr(Url->path+1,'/') || !strcmp_litbeg(Url->path,"/local/")) /* mode!=Fetch */
      {
	if((tmpclient=LocalPage(client,Url,request_head,request_body))==-1)
	  {mode=InternalPage; goto internalpage;}
	else
	  {mode=SpoolInternal; goto spoolinternal;}
      }

    /* Check for pages like '/http/www.foo/bar.html' and transform to 'http://www.foo/bar.html'. */

    else /* mode!=Fetch */
      {
       int i;
       char *proto_name; size_t strlen_proto;

       for(i=0;i<NProtocols;++i) {
	 proto_name= Protocols[i].name;
	 strlen_proto= strlen(proto_name);
	 if(!strncmp(Url->pathp+1,proto_name,strlen_proto) &&
	    Url->pathp[strlen_proto+1]=='/')
	   goto found_protocol;
       }

       /* Not found */

       PrintMessage(Inform,"The requested URL '%s' does not exist on the local server.",Url->pathp);
       HTMLMessage(client,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
		   "url",Url->name,
		   NULL);
       mode=InternalPage; goto internalpage;

      found_protocol:
       {
	 char url[strlen(Url->pathp)+2];
	 sprintf(url,"%s://%s",proto_name,Url->pathp+strlen_proto+2);
	 FreeURL(Url);
	 Url=SplitURL(url);
       }
      }

    /* Check for a DontGet URL and its replacement. (duplicated code from above.) */

    if(ConfigBooleanMatchURL(DontGet,Url))
      {
       PrintMessage(Inform,"The URL '%s' matches one in the list not to get.",Url->name);

       if(mode==Fetch)
         {
          if(client!=-1)
             write_formatted(client,"Cannot fetch %s [Not Got]\n",Url->name);

	  exitval=0; goto clean_up;
         }
       else /* mode!=Fetch */
	 {
	   char *replace=ConfigStringURL(DontGetReplacementURL,Url);

	   if(!replace)
	     {
	       HTMLMessage(client,404,"WWWOFFLE Host Not Got",NULL,"HostNotGot",
			   "url",Url->name,
			   NULL);
	       mode=InternalPage; goto internalpage;
	     }
	   else
	     {
	       FreeURL(Url);
	       Url=SplitURL(replace);

	       PrintMessage(Inform,"Replaced URL='%s'%s.",Url->name,Url->user?" (With username/password)":"");
	       PrintMessage(Debug,"Replaced proto='%s'; hostport='%s'; path='%s'; args='%s'; user:pass='%s:%s'.",
			    Url->proto,Url->hostport,Url->path,Url->args,Url->user,Url->pass);
	     }
	 }
      }

    /* Is the specified protocol valid? (duplicated code from above.) */

    if(!IsProtocolHandled(Url))
      {
       PrintMessage(Inform,"The protocol '%s' is not available.",Url->proto);

       if(mode==Fetch)
         {
          if(client!=-1)
             write_formatted(client,"Cannot fetch %s [Protocol not available]\n",Url->name);
          exitval=1; goto clean_up;
         }
       else /* mode!=Fetch */
         {
          HTMLMessage(client,404,"WWWOFFLE Illegal Protocol",NULL,"IllegalProtocol",
                      "url",Url->name,
                      "protocol",Url->proto,
                      NULL);
          mode=InternalPage; goto internalpage;
         }
      }

    /* Can a POST or PUT request be made with this protocol? (duplicated code from above.) */

    if((!strcmp(request_head->method,"POST") && !IsPOSTHandled(Url)) ||
       (!strcmp(request_head->method,"PUT") && !IsPUTHandled(Url)))
      {
       PrintMessage(Warning,"The requested method '%s' is not supported for the %s protocol.",request_head->method,Url->proto);

       if(mode==Fetch)
         {
          if(client!=-1)
             write_formatted(client,"Cannot fetch %s [The %s method is not supported for %s]\n",Url->name,request_head->method,Url->proto);
          exitval=1; goto clean_up;
         }
       else /* mode!=Fetch */
         {
          HTMLMessage(client,501,"WWWOFFLE Method Unsupported",NULL,"MethodUnsupported",
                      "method",request_head->method,
                      "protocol",Url->proto,
                      NULL);
          mode=InternalPage; goto internalpage;
         }
      }
   }

 /* The special case info pages based on the Referer header. */

 else if(mode!=Fetch) /* !Url->local */
   {
    char *referer=GetHeader(request_head,"Referer");

    if(referer && strstr(referer,"/info/request"))
      {
       URL *refUrl=SplitURL(referer);

       if(IsLocalHost(refUrl) && !strcmp_litbeg(refUrl->path,"/info/request"))
         {
          InfoPage(client,Url,request_head,request_body);
          FreeURL(refUrl);
          mode=InternalPage; goto internalpage;
         }

       FreeURL(refUrl);
      }
   }


 /*----------------------------------------
   mode = Spool, SpoolGet, SpoolRefresh, Real, RealRefresh, SpoolOrReal or Fetch

   Check for a username / password.
   ----------------------------------------*/

 if(Url->user && ConfigBooleanURL(TryWithoutPassword,Url))
   {
    Urlpw=Url;
    Url=MakeModifiedURL(Url,REPLACEURLUSER|REPLACEURLPASS,NULL,NULL,NULL,NULL,NULL,NULL);

    if(!Urlpw->pass)
      {
       if(mode==Fetch)
         {
          FreeURL(Urlpw);
          Urlpw=NULL;
         }
       else /* mode!=Fetch */
         {
          HTMLMessage(client,403,"WWWOFFLE Username Needs Password",NULL,"UserNeedsPass",
                      "url",Urlpw->name,
                      "user",Urlpw->user,
                      NULL);
          mode=InternalPage; goto internalpage;
         }
      }
   }


 /*----------------------------------------
   mode = Spool, SpoolGet, SpoolRefresh, Real, RealRefresh, SpoolOrReal or Fetch

   Check for an existing cached version, extra checks if there is a password.
   ----------------------------------------*/

 outgoing_exists=ExistsOutgoingSpoolFile(Url);
 spool_exists=ExistsWebpageSpoolFile(Url,0);

 if(Urlpw)
   {
    outgoing_exists_pw=ExistsOutgoingSpoolFile(Urlpw);
    spool_exists_pw=ExistsWebpageSpoolFile(Urlpw,0);

    /* In one of the spool modes we can only return one page, with or without. */

    if(mode==Spool || mode==SpoolGet || mode==SpoolOrReal || mode==SpoolRefresh)
      {
       if(spool_exists)
         {
          /* If the 401 page already exists without the password, then only try the password. */

          if(SpooledPageStatus(Url,0)==401)
            {
             FreeURL(Url);
             Url=Urlpw;
             Urlpw=NULL;
             spool_exists=spool_exists_pw;
             spool_exists_pw=0;
             outgoing_exists=outgoing_exists_pw;
             outgoing_exists_pw=0;
            }

          /* If a different page already exists without the password, then only try without the password. */

          else
            {
             FreeURL(Urlpw);
             Urlpw=NULL;
             spool_exists_pw=0;
             outgoing_exists_pw=0;
            }
         }
       else /* if(!spool_exists) */
         {
          /* If the password protected page exists then request the non-password version. */

          if(spool_exists_pw && !outgoing_exists)
            {
             int new_outgoing=OpenNewOutgoingSpoolFile();

             if(new_outgoing==-1)
                PrintMessage(Warning,"Cannot open the new outgoing request to write [%!s].");
             else
               {
		 ssize_t err;

		 init_io(new_outgoing);

		 {
		   size_t request_head_size;
		   char *head=HeaderString(request_head,&request_head_size);

		   if((err=write_data(new_outgoing,head,request_head_size))<0)
		     PrintMessage(Warning,"Cannot write to outgoing file [%!s]; disk full?");
		   free(head);
		 }
		 if(err>=0 && request_body)
                   if(write_data(new_outgoing,request_body->content,request_body->length)<0)
		     PrintMessage(Warning,"Cannot write to outgoing file [%!s]; disk full?");

		 finish_io(new_outgoing);
		 CloseNewOutgoingSpoolFile(new_outgoing,Url);
               }
            }

          /* Get the password protected version. */

          FreeURL(Url);
          Url=Urlpw;
          Urlpw=NULL;
          spool_exists=spool_exists_pw;
          spool_exists_pw=0;
          outgoing_exists=outgoing_exists_pw;
          outgoing_exists_pw=0;
         }
      }
    else if(mode==Fetch || mode==Real)
      {
       if(spool_exists)
         {
          /* If the 401 page already exists without the password, then only try the password. */

          if(SpooledPageStatus(Url,0)==401)
            {
             FreeURL(Url);
             Url=Urlpw;
             Urlpw=NULL;
             spool_exists=spool_exists_pw;
             spool_exists_pw=0;
             outgoing_exists=outgoing_exists_pw;
             outgoing_exists_pw=0;
            }

          /* If a different page already exists without the password, then only try without the password. */

          else
            {
             FreeURL(Urlpw);
             Urlpw=NULL;
             spool_exists_pw=0;
             outgoing_exists_pw=0;
            }
         }
      }

    /* Do nothing for the online refresh modes. */

    /* else if(mode==RealRefresh)
       ; */
   }


 /*----------------------------------------
   In Real or Fetch mode with a password Url then get page with
    no password then come here and try again with a password.
   ----------------------------------------*/

passwordagain:

 /* Reset the variables used for server data format. */

#if USE_ZLIB
 request_compression=0,server_compression=0;
#endif
 request_chunked=0,server_chunked=0;


 /*----------------------------------------
   mode = Spool, SpoolGet, SpoolRefresh, Real, RealRefresh, SpoolOrReal or Fetch

   Check if it needs to be cached, depends on mode and password status.
   ----------------------------------------*/

 if(IsLocalNetHost(Url->host))
   {
    /* Don't cache if in a normal mode */

    if(mode==Real || mode==Spool || mode==SpoolOrReal)
       mode=RealNoCache;

    /* Don't do anything if a refresh URL. */

    /* else if(mode==SpoolGet)
       ; */

    /* Give an error if in fetch mode. */

    else if(mode==Fetch)
      {
       PrintMessage(Inform,"The request to fetch a page from the local network host '%s' is ignored.",Url->hostport);
       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [On local network]\n",Url->name);
       exitval=1; goto clean_up;
      }

    /* Do nothing for the refresh modes, they will come through again. */

    /* else if(mode==RealRefresh || mode==SpoolRefresh)
       ; */
   }
 else if(ConfigBooleanMatchURL(DontCache,Url))
   {
    /* Don't cache if possibly online */

    if(mode==Real || mode==SpoolOrReal)
       mode=RealNoCache;

    /* Give an error if in fetch mode. */

    else if(mode==Fetch)
      {
       PrintMessage(Inform,"The request to fetch the page '%s' is ignored because it is not to be cached.",Url->name);
       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [Not cached]\n",Url->name);
       exitval=1; goto clean_up;
      }

    /* Give an error if in a spooling mode */

    else if(mode==Spool || mode==SpoolGet)
      {
       PrintMessage(Inform,"It is not possible to request a URL that is not cached when offline.");
       HTMLMessage(client,404,"WWWOFFLE Cant Spool Not Cached",NULL,"HostNotCached",
                   "url",Url->name,
                   NULL);
       mode=InternalPage; goto internalpage;
      }

    /* Do nothing for the refresh modes, they will come through again. */

    /* else if(mode==RealRefresh || mode==SpoolRefresh)
       ; */
   }

 /* If a HEAD request when online then don't cache */

 if((mode==Real || mode==RealRefresh) && head_only)
   {
    mode=RealNoCache;
   }

 /* If not caching then only use the password version. */

 if(mode==RealNoCache && Urlpw)
   {
    FreeURL(Url);
    Url=Urlpw;
    Urlpw=NULL;
    spool_exists=spool_exists_pw;
    spool_exists_pw=0;
    outgoing_exists=outgoing_exists_pw;
    outgoing_exists_pw=0;
   }


 /*----------------------------------------
   mode = Spool, SpoolGet, SpoolRefresh, Real, RealRefresh, SpoolOrReal or Fetch

   Special handling for POST/PUT requests and cached replies.
   ----------------------------------------*/

 /* Check if it was a POST/PUT method. */

 if(!strcmp(request_head->method,"POST") ||
    !strcmp(request_head->method,"PUT"))
   {
    /* In spool mode they can only be requests to get a new page, not requests from the cache. */

    if(mode==Spool)
       mode=SpoolGet;
   }

 /* Check if the URL indicates the request for */

 else if(Url->args && *Url->args=='!')
   {
    /* In fetch mode they cannot be fetched. */

    if(mode==Fetch)
      {
       PrintMessage(Inform,"It is not possible to fetch a URL that used the POST/PUT method.");
       if(client!=-1)
          write_formatted(client,"Cannot fetch %s [Reply from a POST/PUT method]\n",Url->name);
       exitval=1; goto clean_up;
      }

    /* If they don't already exist then they can't be requested. */

    else if(!spool_exists && !outgoing_exists) /* mode!=Fetch */
      {
       PrintMessage(Inform,"It is not possible to request a URL that used the POST/PUT method.");
       HTMLMessage(client,404,"WWWOFFLE Cant Refresh POST/PUT",NULL,"CantRefreshPosted",
                   "url",Url->name,
                   NULL);
       mode=InternalPage; goto internalpage;
      }

    /* In all other cases they must be requests for a cached reply. */

    else /* mode!=Fetch */
       mode=Spool;
   }


 /*----------------------------------------
   mode = Spool, SpoolGet, SpoolRefresh, Real, RealRefresh, RealNoCache, SpoolOrReal or Fetch

   Check if it is htdig or mnogosearch (udmsearch) or WDG HTML validator and don't let it request any URLs.
   ----------------------------------------*/

 {
  char *user_agent=GetHeader(request_head,"User-Agent");
  if(user_agent)
     is_client_searcher=!strcasecmp_litbeg(user_agent,"htdig") ||
                        !strcasecmp_litbeg(user_agent,"mnoGoSearch") ||
                        !strcasecmp_litbeg(user_agent,"UdmSearch") ||
                        !strcasecmp_litbeg(user_agent,"WDG_Validator");
 }

 if(is_client_searcher)
   {
    /* If it exists then we must supply the cached version. */

    if(spool_exists)
       mode=Spool;

    /* If not then deny the request. */

    else
      {
       PrintMessage(Inform,"URL unavailable to be searched.");
       HTMLMessageHead(client,404,"WWWOFFLE Not Searched",
		       "Content-Length","0",
                       NULL);
       mode=InternalPage; goto internalpage;
      }
   }


 /*----------------------------------------
   mode = Spool, SpoolGet, SpoolRefresh, Real, RealRefresh, RealNoCache, SpoolOrReal or Fetch

   Check if a refresh is needed based on pragma, request changes, autodial.
   ----------------------------------------*/

 conditional_request_ims=(GetHeader(request_head,"If-Modified-Since")!=NULL);
 conditional_request_inm=(GetHeader(request_head,"If-None-Match")!=NULL);

 {
   int lockcreated;

 /* If in Real mode and there is a lockfile (cannot create new one) then just spool the cache version. */

 if((mode==Real || mode==SpoolOrReal) && (lockcreated=CreateLockWebpageSpoolFile(Url))<=0)
   {
     if(lockcreated<0) {
       char *errmsg=GetPrintMessage(Warning,"Cannot create a lock file in the spool directory [%!s].");
       HTMLMessage(client,500,"WWWOFFLE Server Error",NULL,"ServerError",
                   "error",errmsg,
                   NULL);
       free(errmsg);
       mode=InternalPage; goto internalpage;
     }
     else
       mode=Spool;
   }

 /* If in Fetch mode and there is a lockfile (cannot create new one) then we don't need to fetch again. */

 else if(mode==Fetch && (lockcreated=CreateLockWebpageSpoolFile(Url))<=0)
   {
     if(lockcreated==0)
       PrintMessage(Debug,"Already fetching URL.");
    if(client!=-1)
       write_formatted(client,
		       lockcreated<0?"Cannot fetch %s [Creating lockfile failed]\n":
		                     "Cannot fetch %s [Already fetching]\n",
		       Url->name);
    exitval=(fetch_again?4:0); goto clean_up;
   }

 /* If a forced refresh then change to an appropriate mode. */

 else if(!is_client_searcher && (RefreshForced() || RequireForced(request_head,Url,online)))
   {
    if(conditional_request_ims)
       RemoveFromHeader(request_head,"If-Modified-Since");
    if(conditional_request_inm)
       RemoveFromHeader(request_head,"If-None-Match");

    /* In spool mode either force a new request if cached or make a new request. */

    if(mode==Spool || mode==SpoolGet)
      {
       if(spool_exists && !ConfigBooleanURL(ConfirmRequests,Url))
          mode=SpoolPragma;
       else
          mode=SpoolGet;
      }

    /* In autodial mode we need to make a real request. */

    else if(mode==SpoolOrReal)
      {
       if(head_only)
          mode=RealNoCache;
       else
          mode=Real;
      }

    /* In real or fetch or nocache or refresh modes make no other changes. */

    /* else if(mode==Real || mode==SpoolRefresh || mode==RealRefresh || mode==RealNoCache || mode==Fetch)
       ; */
   }

 /* In Fetch mode when not a forced refresh. */

 else if(mode==Fetch)
   {
    if(conditional_request_ims)
       RemoveFromHeader(request_head,"If-Modified-Since");
    if(conditional_request_inm)
       RemoveFromHeader(request_head,"If-None-Match");

    /* If there is already a cached version then open it. */

    if(spool_exists)
      {
	spool=OpenWebpageSpoolFile(1,Url);

	if(spool!=-1)
	  {
	    char *ims=NULL,*inm=NULL;
	    init_io(spool);

	    /* If changes are needed then get them and add the conditional headers. */

	    if(RequireChanges(spool,Url,&ims,&inm))
	      {
		if(inm) {
		  AddToHeader(request_head,"If-None-Match",inm);
		  PrintMessage(Debug,"Requesting URL (Conditional request with If-None-Match: %s).",inm);
		  free(inm);
		}
		if(ims) {
		  AddToHeader(request_head,"If-Modified-Since",ims);
		  PrintMessage(Debug,"Requesting URL (Conditional request with If-Modified-Since: %s).",ims);
		  free(ims);
		}
	      }

	    /* If no change is needed but is a recursive request, parse the document and fetch the images / links. */

	    else if(fetch_again)
	      {
		lseek(spool,0,SEEK_SET);
		reinit_io(spool);

		if(ParseDocument(spool,Url,0))
		  RecurseFetch(Url);

		finish_io(spool);
		close(spool);
		spool=-1;

		DeleteLockWebpageSpoolFile(Url);

		exitval=4; goto clean_up;
	      }

	    /* Otherwise just exit. */

	    else
	      {
		finish_io(spool);
		close(spool);
		spool=-1;

		DeleteLockWebpageSpoolFile(Url);

		exitval=0; goto clean_up;
	      }

	    finish_io(spool);
	    close(spool);
	    spool=-1;
	  }
      }
   }

 /* In Real mode when not a forced refresh. */

 else if(mode==Real)
   {
     /* If there is already a cached version then open it. */

     if(!spool_exists || (spool=OpenWebpageSpoolFile(1,Url))==-1)
       {
	 if(conditional_request_ims)
	   RemoveFromHeader(request_head,"If-Modified-Since");
	 if(conditional_request_inm)
	   RemoveFromHeader(request_head,"If-None-Match");
       }
     else
       {
	 char *ims=NULL,*inm=NULL;
	 init_io(spool);

	 /* If changes are needed then get them and add the conditional headers. */

	 if(RequireChanges(spool,Url,&ims,&inm))
	   {
	     if(conditional_request_ims || conditional_request_inm) {
	       lseek(spool,0,SEEK_SET);
	       reinit_io(spool);

	       /* We may want to send a not-modified header to the client when the
		  server also replies "Not Modified".
		  Check this with the present request_head, because the
		  original conditional request headers from the client
		  are going to be replaced.
	       */

	       not_modified_spool= !IsModified(spool,request_head);
	     }
	     
	     /* Remove the conditional headers only after calling IsModified(). */

	     if(conditional_request_ims)
	       RemoveFromHeader(request_head,"If-Modified-Since");
	     if(conditional_request_inm)
	       RemoveFromHeader(request_head,"If-None-Match");
	     if(inm) {
	       AddToHeader(request_head,"If-None-Match",inm);
	       PrintMessage(Debug,"Requesting URL (Conditional request with If-None-Match: %s).",inm);
	       free(inm);
	     }
	     if(ims) {
	       AddToHeader(request_head,"If-Modified-Since",ims);
	       PrintMessage(Debug,"Requesting URL (Conditional request with If-Modified-Since: %s).",ims);
	       free(ims);
	     }
	   }

	 /* Otherwise just use the spooled version. */

	 else
	   {
	     mode=Spool;
	     DeleteLockWebpageSpoolFile(Url);

	     if(conditional_request_ims || conditional_request_inm)
	       {
		 lseek(spool,0,SEEK_SET);
		 reinit_io(spool);

		 /* Return a not-modified header if it isn't modified. */

		 if(!IsModified(spool,request_head))
		   {
		     HTMLMessageHead(client,304,"WWWOFFLE Not Modified",
				     "Content-Length","0",
				     NULL);
		     mode=InternalPage; goto internalpage;
		   }
	       }

	     /* Remove the headers only after calling IsModified(). */

	     if(conditional_request_ims)
	       RemoveFromHeader(request_head,"If-Modified-Since");
	     if(conditional_request_inm)
	       RemoveFromHeader(request_head,"If-None-Match");
	   }

	 finish_io(spool);
	 close(spool);
	 spool=-1;
       }
   }

 else if(mode==SpoolOrReal || mode==Spool || mode==RealRefresh || mode==SpoolRefresh || mode==SpoolGet)
   {
     /* If in autodial mode when not a forced refresh. */

     if(mode==SpoolOrReal)
       {
	 /* If it is cached then use Spool mode. */

	 if(spool_exists)
	   {
	     mode=Spool;
	     DeleteLockWebpageSpoolFile(Url);
	   }

	 /* If not then use Real mode. */

	 else
	   {
	     if(head_only)
	       mode=RealNoCache;
	     else
	       mode=Real;
	   }
       }

     /* If in Spool mode when not a forced refresh. */

     if(mode==Spool)
       {
	 /* If the cached version does not exist then get it. */

	 if(!spool_exists)
	   mode=SpoolGet;

	 /* If the cached version does exist and a conditional request then see if it is modified. */

	 else if(conditional_request_ims || conditional_request_inm)
	   {
	     spool=OpenWebpageSpoolFile(1,Url);

	     if(spool!=-1)
	       {
		 init_io(spool);

		 /* Return a not-modified header if it isn't modified. */

		 if(!IsModified(spool,request_head))
		   {
		     HTMLMessageHead(client,304,"WWWOFFLE Not Modified",
				     "Content-Length","0",
				     NULL);
		     mode=InternalPage; goto internalpage;
		   }

		 finish_io(spool);
		 close(spool);
		 spool=-1;
	       }
	   }
       }

     /* Remove the conditional headers only after calling IsModified(). */

     if(conditional_request_ims)
       RemoveFromHeader(request_head,"If-Modified-Since");
     if(conditional_request_inm)
       RemoveFromHeader(request_head,"If-None-Match");
   }

 /* In no cached mode do nothing. */

 /* else if(mode==RealNoCache)
   ; */

 }

 /* If offline but making a request then change HEAD to GET. */

 if(mode==SpoolGet && head_only)
   {
    strcpy(request_head->method,"GET");
   }

 /*----------------------------------------
   mode = Spool, SpoolGet, SpoolPragma, SpoolRefresh, Real, RealRefresh, RealNoCache or Fetch

   Set up the spool file, outgoing file and server connection as appropriate.
   ----------------------------------------*/

 UrlProtocol=GetProtocol(Url);

 /* Set up the file descriptor for the spool file (to write). */

 if(mode==Real || mode==Fetch)
   {
    if(spool_exists)
      CreateBackupWebpageSpoolFile(Url,0);
    else
      spool_exists=ExistsWebpageSpoolFile(Url,1);

    spool=OpenWebpageSpoolFile(0,Url);

    if(spool==-1)
      {
       char *errmsg=GetPrintMessage(Warning,"Cannot open the spooled web page to write [%!s].");
       DeleteLockWebpageSpoolFile(Url);

       if(mode==Fetch)
         {
          if(client!=-1)
             write_formatted(client,"Internal Error %s [Cannot open spool file]\n",Url->name);
	  free(errmsg);
          exitval=1; goto clean_up;
         }
       else /* mode!=Fetch */
         {
          HTMLMessage(client,500,"WWWOFFLE Server Error",NULL,"ServerError",
                      "error",errmsg,
                      NULL);
	  free(errmsg);
          mode=InternalPage; goto internalpage;
         }
      }

    init_io(spool);

    createlasttimespoolfile=1;
   }

 /* Set up the file descriptor for the spool file (to read). */

 else if(mode==Spool || mode==SpoolPragma)
   {
    spool=OpenWebpageSpoolFile(1,Url);

    if(spool==-1)
      {
       char *errmsg=GetPrintMessage(Warning,"Cannot open the spooled web page to read [%!s].");
       HTMLMessage(client,500,"WWWOFFLE Server Error",NULL,"ServerError",
                   "error",errmsg,
                   NULL);
       free(errmsg);
       mode=InternalPage; goto internalpage;
      }

    init_io(spool);
   }

 /* Set up the outgoing file (to write). */

 offline_request=!ConfigBooleanURL(DontRequestOffline,Url);

 if((offline_request || online) &&
    !outgoing_exists &&
    (mode==SpoolRefresh ||
     ((mode==SpoolGet || mode==SpoolPragma) &&
      (!ConfigBooleanURL(ConfirmRequests,Url) || is_client_wwwoffle || IsLocalHost(Url)))))
   {
    outgoing_write=OpenNewOutgoingSpoolFile();

    if(outgoing_write==-1)
      {
       char *errmsg=GetPrintMessage(Warning,"Cannot open the outgoing request to write [%!s].");
       HTMLMessage(client,500,"WWWOFFLE Server Error",NULL,"ServerError",
                   "error",errmsg,
                   NULL);
       free(errmsg);
       mode=InternalPage; goto internalpage;
      }

    init_io(outgoing_write);
   }

 errno=0;

 /* Open the connection to the server host. */

 if(mode==Real || mode==RealNoCache || mode==Fetch)
   {
    char *err=(UrlProtocol->open)(Url);

    /* Retry if the option is set. */

    if(err && ConfigBoolean(ConnectRetry))
      {
       PrintMessage(Inform,"Waiting to try connection again.");
       sleep(10);
       free(err);
       err=(UrlProtocol->open)(Url);
      }

    /* In case of an error ... */

    if(err)
      {
       /* Store the error in the cache. */

       if(mode==Real || mode==Fetch)
         {
          lseek(spool,0,SEEK_SET);
          ftruncate(spool,0);
          reinit_io(spool);

          HTMLMessage(spool,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
                      "url",Url->name,
                      "reason",err,
                      "cache","yes",
                      "backup",spool_exists?"yes":NULL,
                      NULL);

          finish_io(spool);
          close(spool);
	  spool=-1;

          DeleteLockWebpageSpoolFile(Url);
         }

       /* In Fetch mode print message and exit. */

       if(mode==Fetch)
         {
          if(client!=-1)
             write_formatted(client,"Fetch Failure %s [Server Connection Failed]\n",Url->name);
          free(err);
          exitval=1; goto clean_up;
         }

       /* Write the error to the client. */

       else /* if(mode==Real || mode==RealNoCache) */
         {
          HTMLMessage(client,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
                      "url",Url->name,
                      "reason",err,
                      "cache",NULL,
                      "backup",spool_exists?"yes":NULL,
                      NULL);
          free(err);
          mode=InternalPage; goto internalpage;
         }
      }
   }


 /*----------------------------------------
   mode = Spool, SpoolGet, SpoolPragma, SpoolRefresh, Real, RealRefresh, RealNoCache or Fetch

   Make the request to the server.
   ----------------------------------------*/

 /* Modify the request header. */

 if(mode==SpoolRefresh || mode==Real || mode==RealRefresh || mode==RealNoCache || mode==Fetch)
   {
    /* Censor / Cannonicalise URL / POST & PUT URL hacking / HTTP-1.1 etc. */

    ModifyRequest(Url,request_head);

    /* Add the compression request header */

#if USE_ZLIB
    {
      int compress_spec=ConfigIntegerURL(RequestCompressedData,Url);
      if(compress_spec && !NotCompressed(NULL,Url->path))
	{
	  request_compression=1;
	  AddToHeader(request_head,"Accept-Encoding",
		      (compress_spec==1) ? "x-deflate, deflate, identity; q=0.1":
		      (compress_spec==2) ? "x-gzip, gzip, identity; q=0.1":
		                           "x-gzip; q=1.0, gzip; q=1.0, x-deflate; q=0.9, deflate; q=0.9, identity; q=0.1");
	}
    }
#endif

    /* Add the chunked encoding header and modify the connection header */

    if(ConfigBooleanURL(RequestChunkedData,Url))
      {
       request_chunked=1;

       ChangeVersionInHeader(request_head,"HTTP/1.1");
       AddToHeader(request_head,"TE","chunked");

       /* The following line is required by HTTP/1.1 specification.  The "bad-behaviour"
          PHP script may stop WWWOFFLE access to web sites if it is not present.
          If the line is included then IIS servers don't recognise the "Connection: close"
          header and keep WWWOFFLE waiting until there is a connection timeout.
       */
       AddToHeader(request_head,"Connection","TE");
      }
   }

 /* Display a message if fetching. */

 if(mode==Fetch && client!=-1)
    write_formatted(client,"Fetching %s ...\n",Url->name);

 /* Write request to remote server. */

 if(mode==Real || mode==RealNoCache || mode==Fetch)
   {
    char *err=(UrlProtocol->request)(Url,request_head,request_body);

    is_server=1;

    /* In case of an error ... */

    if(err)
      {
       /* Store the error in the cache. */

       if(mode==Real || mode==Fetch)
         {
          lseek(spool,0,SEEK_SET);
          ftruncate(spool,0);
          reinit_io(spool);

          HTMLMessage(spool,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
                      "url",Url->name,
                      "reason",err,
                      "cache","yes",
                      "backup",spool_exists?"yes":NULL,
                      NULL);

          finish_io(spool);
          close(spool);
	  spool=-1;

          DeleteLockWebpageSpoolFile(Url);
         }

       /* In Fetch mode print message and exit. */

       if(mode==Fetch)
         {
          if(client!=-1)
             write_formatted(client,"Fetch Failure %s [Server Connection Error]\n",Url->name);
          free(err);

          exitval=1; goto clean_up;
         }

       /* Write the error to the client. */

       else /* if(mode==Real || mode==RealNoCache) */
         {
          HTMLMessage(client,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
                      "url",Url->name,
                      "reason",err,
                      "cache",NULL,
                      "backup",spool_exists?"yes":NULL,
                      NULL);
          free(err);
          mode=InternalPage; goto internalpage;
         }
      }
   }

 /* Write request to outgoing file. */

 else if((offline_request || online) &&
         !outgoing_exists &&
         (mode==SpoolRefresh ||
          ((mode==SpoolGet || mode==SpoolPragma) &&
           (!ConfigBooleanURL(ConfirmRequests,Url) || is_client_wwwoffle || IsLocalHost(Url)))))
   {
     ssize_t err;
     {
       size_t request_head_size;
       char *head=HeaderString(request_head,&request_head_size);

       err=write_data(outgoing_write,head,request_head_size);
       free(head);
     }

    /* Write the error to the client. */

    if(err<0)
      {
       char *errmsg=GetPrintMessage(Warning,"Cannot write the outgoing request [%!s].");
       HTMLMessage(client,500,"WWWOFFLE Server Error",NULL,"ServerError",
                   "error",errmsg,
                   NULL);
       free(errmsg);
       mode=InternalPage; goto internalpage;
      }

    if(request_body)
       if(write_data(outgoing_write,request_body->content,request_body->length)<0)
          PrintMessage(Warning,"Cannot write to outgoing file [%!s]; disk full?");
   }


 /*----------------------------------------
   mode = Spool, SpoolGet, SpoolPragma, SpoolRefresh, Real, RealRefresh, RealNoCache or Fetch

   Check the reply from the server or from the cache.
   ----------------------------------------*/

 /* reply_head=NULL; */

 /* Read the reply from the server. */

 if(mode==Real || mode==RealNoCache || mode==Fetch)
   {
    char *headerpattern;

    /* Get the header */

    server=UrlProtocol->readhead(&reply_head);

    is_server=1;

    /* Check for a "100 Continue" status which shouldn't happen, but might.  */

    if(reply_head && reply_head->status==100)
      {
	if(StderrLevel==ExtraDebug) {
	  char *headerstring=HeaderString(reply_head,NULL);
          PrintMessage(ExtraDebug,"Incoming Reply Head (from server/proxy)\n%s",headerstring);
	  free(headerstring);
	}

	FreeHeader(reply_head);
	server=UrlProtocol->readhead(&reply_head);
      }

    /* In case of error ... */

    if(!reply_head)
      {
       int saved_errno=errno;
       char *errmsg=GetPrintMessage(Warning,errno?"Error reading reply header from remote host [%!s].":"Could not parse reply header from remote host.");
       if(saved_errno == ETIMEDOUT) {
	 free(errmsg);
	 errmsg=strdup("TimeoutReply");
       }

       /* Write the error to the cache. */

       if(mode==Real || mode==Fetch)
         {
          lseek(spool,0,SEEK_SET);
          ftruncate(spool,0);
          reinit_io(spool);

          HTMLMessage(spool,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
                      "url",Url->name,
                      "reason",errmsg,
                      "cache","yes",
                      "backup",spool_exists?"yes":NULL,
                      NULL);

          finish_io(spool);
          close(spool);
	  spool=-1;

          DeleteLockWebpageSpoolFile(Url);
         }

       /* In Fetch mode print message and exit. */

       if(mode==Fetch)
         {
          if(client!=-1)
             write_formatted(client,"Fetch Failure %s [Server Reply Error]\n",Url->name);
	  free(errmsg);
          exitval=1; goto clean_up;
         }

       /* Write the error to the client. */

       else /* if(mode==Real || mode==RealNoCache) */
         {
          HTMLMessage(client,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
                      "url",Url->name,
                      "reason",errmsg,
                      "cache",NULL,
                      "backup",spool_exists?"yes":NULL,
                      NULL);
	  free(errmsg);
          mode=InternalPage; goto internalpage;
         }
      }

    if(StderrLevel==ExtraDebug) {
      char *headerstring=HeaderString(reply_head,NULL);
      PrintMessage(ExtraDebug,"Incoming Reply Head (from server/proxy)\n%s",headerstring);
      free(headerstring);
    }

    /* Check for compression header */

#if USE_ZLIB
    {
      char *content_encoding=NULL;

      if(request_compression && (content_encoding=GetHeader(reply_head,"Content-Encoding")))
	server_compression=WhichCompression(content_encoding);
      else
	server_compression=0;

      if(server_compression)
	{
	  PrintMessage(Inform,"Server has used 'Content-Encoding: %s'.",content_encoding); /* Used in audit-usage.pl */
	  RemoveFromHeader(reply_head,"Content-Encoding");
	  configure_io_zlib(server,server_compression,-1);
	}
    }
#endif

    /* Check for chunked encoding header */
    {
      char *transfer_encoding=NULL;
      server_chunked=(request_chunked && (transfer_encoding=GetHeader2(reply_head,"Transfer-Encoding","chunked")));

      if(server_chunked)
	{
	  PrintMessage(Inform,"Server has used 'Transfer-Encoding: %s'.",transfer_encoding); /* Used in audit-usage.pl */
	  RemoveFromHeader(reply_head,"Transfer-Encoding");
	  configure_io_chunked(server,1,-1);
	}
    }

    /* Handle status codes */

    reply_status=reply_head->status;

    /* If the page has not changed ... */

    if(reply_status==304)
      {
       PrintMessage(Inform,"Cache Access Status='Unmodified on Server'."); /* Used in audit-usage.pl */

       /* Restore the backup version of the page if writing to the cache. */

       if(mode==Fetch || mode==Real)
         {
          finish_io(spool);
	  close(spool);
	  spool=-1;

          RestoreBackupWebpageSpoolFile(Url);
          TouchWebpageSpoolFile(Url,0);
          createlasttimespoolfile=0;

          DeleteLockWebpageSpoolFile(Url);
         }

       /* If fetching then parse the document for links. */

       if(mode==Fetch)
         {
          if(client!=-1)
             write_formatted(client,"Not fetching %s [Page Unchanged]\n",Url->name);

          if(fetch_again)
            {
             spool=OpenWebpageSpoolFile(1,Url);

             if(spool!=-1)
	       {
                init_io(spool);

		if(ParseDocument(spool,Url,0))
		  RecurseFetch(Url);

                finish_io(spool);
		close(spool);
		spool=-1;
	       }
            }

          exitval=(fetch_again?4:0); goto clean_up;
         }

       /* If in Real mode then tell the client or spool the page to the client. */

       else if(mode==Real)
         {
          if(not_modified_spool)
            {
             HTMLMessageHead(client,304,"WWWOFFLE Not Modified",
			     "Content-Length","0",
                             NULL);
             mode=InternalPage; goto internalpage;
            }
          else
            {
             spool=OpenWebpageSpoolFile(1,Url);
             if(spool!=-1)
                init_io(spool);

             mode=Spool;
            }
         }
      }

    /* If fetching a page that requires a password then change mode. */

    else if(mode==Fetch && reply_status==401 && Urlpw)
      {
       mode=FetchNoPassword;
       if(client!=-1)
          write_formatted(client,"Fetching More %s [URL with password]\n",Url->name);
      }

    /* If in Real mode and a page that requires a password then change mode. */

    else if(mode==Real && reply_status==401 && Urlpw)
      {
       mode=RealNoPassword;
      }

    /* If the status is an error but we want to keep existing page. */

    else if((mode==Fetch || mode==Real) &&
	    (headerpattern=NULL,(reply_status>=300 && ConfigBooleanURL(KeepCacheIfNotFound,Url)) ||
	     (headerpattern=ConfigHeaderMatch(KeepCacheIfHeaderMatches,Url,reply_head)))
	    && keepspoolfile(Url,1))
      {
	if(headerpattern)
	  PrintMessage(Debug,"Reply header for '%s' matches '%s', keeping backup spool file.",Url->name,headerpattern);
	else
	  PrintMessage(Debug,"Reply status for '%s' was %d, keeping backup spool file.",Url->name,reply_status);

	lseek(spool,0,SEEK_SET);
	ftruncate(spool,0);
	reinit_io(spool);

	{
	  char status[MAX_INT_STR+1];

	  sprintf(status,"%d",reply_status);
	  HTMLMessage(spool,410,"WWWOFFLE Requested Resource Gone",NULL,"KeepCache",
		      "url",Url->name,
		      "replystatus",status,
		      "replynote",reply_head->note,
		      "headerpattern",headerpattern,
		      NULL);
	}
	DeleteLockWebpageSpoolFile(Url);

	finish_io(spool);
	close(spool);
	spool=-1;
	createlasttimespoolfile=0;

	if(mode==Fetch)
	  {
	    if(client!=-1)
	      write_formatted(client,"Not fetching %s [Keeping cached version]\n",Url->name);
	    exitval=0; goto clean_up;
	  }
	else /* if(mode==Real) */
	  mode=RealNoCache;
      }

    /* If fetching and the page has moved, request the new one. */

    else if(mode==Fetch && (reply_status==301 || reply_status==302))
      {
       URL *newUrl=MovedLocation(Url,reply_head);

       if(client!=-1)
          write_formatted(client,"Fetching More %s [Page Moved]\n",Url->name);

       if(!newUrl)
          PrintMessage(Warning,"Cannot parse the reply for the new location.");
       else {
          fetch_again+=RecurseFetchRelocation(Url,newUrl);
	  FreeURL(newUrl);
       }
      }
   }

 /* Close the outgoing file if any. */

 if(outgoing_read>=0)
   {
    finish_io(outgoing_read);
    close(outgoing_read);
    outgoing_read=-1;
   }

 if(outgoing_write>=0)
   {
    finish_io(outgoing_write);
    CloseNewOutgoingSpoolFile(outgoing_write,Url);
    outgoing_write=-1;
   }


 /*----------------------------------------
   mode = Spool, SpoolGet, SpoolPragma, SpoolRefresh, Real, RealRefresh, RealNoCache, RealNoPassword, Fetch or FetchNoPassword

   The main handling of the body data in the reply.
   ----------------------------------------*/

 /* Check the modified request to get the log messages correct. */

 conditional_request_ims=(GetHeader(request_head,"If-Modified-Since")!=NULL);
 conditional_request_inm=(GetHeader(request_head,"If-None-Match")!=NULL);

 /* When reading from the server and writing to the client and possibly the cache. */

 if(mode==Real || mode==RealNoCache || mode==RealNoPassword)
   {
    ssize_t err=0,spool_err=0,n=0;
    unsigned long bytes_start,content_length=CUNDEF;
    int modify=0;
    char buffer[IO_BUFFER_SIZE];

    /* Print a message for auditing. */

    if(mode==RealNoCache)
       PrintMessage(Inform,"Cache Access Status='Not Cached'."); /* Used in audit-usage.pl */
    else /* mode==Real || mode==RealNoPassword */
      {
       if(!spool_exists)
          PrintMessage(Inform,"Cache Access Status='New Page'."); /* Used in audit-usage.pl */
       else if(conditional_request_ims || conditional_request_inm)
          PrintMessage(Inform,"Cache Access Status='Modified on Server'."); /* Used in audit-usage.pl */
       else
          PrintMessage(Inform,"Cache Access Status='Forced Reload'."); /* Used in audit-usage.pl */
      }

    /* Generate the header and write it to the cache unmodified. */

    if(mode!=RealNoCache) {
      size_t headlen;
      char *head;
      /* Add a WWWOFFLE timestamp */
      AddToHeader(reply_head,"wwwoffle-cache-date",RFC822Date(time(NULL),1));
      head=HeaderString(reply_head,&headlen);
      if((spool_err=write_data(spool,head,headlen))<0)
	PrintMessage(Warning,"Cannot write to cache file [%!s]; disk full?");
      free(head);
    }

    /* Fix up the reply head from the server before sending to the client. */

    ModifyReply(Url,reply_head);

    /* Check if the HTML modifications are to be performed. */

    if(mode==Real && !head_only &&
       !is_client_wwwoffle &&
       !is_client_searcher &&
       /* ConfigBooleanURL(EnableHTMLModifications,Url) && */
       !GetHeader2(request_head,"Cache-Control","no-transform"))
      {
       char *content_encoding;

       if(ConfigBooleanURL(EnableHTMLModifications,Url) &&
          (GetHeader2(reply_head,"Content-Type","text/html") ||
           GetHeader2(reply_head,"Content-Type","application/xhtml")))
          modify=1;
       else if(ConfigBooleanURL(DisableAnimatedGIF,Url) &&
               GetHeader2(reply_head,"Content-Type","image/gif"))
          modify=2;

       /* If the reply uses compression and we are modifying the content then don't (shouldn't happen). */

       if(modify && (content_encoding=GetHeader(reply_head,"Content-Encoding")))
          if(WhichCompression(content_encoding))
             modify=0;
      }

    {
      char *lenstr=GetHeader(reply_head,"Content-Length");
      if(lenstr) {
	char *endptr;
	long len;
	errno=0;
	len=strtol(lenstr,&endptr,10);
	if(*endptr || errno)
	  PrintMessage(Warning,"Ignoring unparsable Content-Length (%s) in reply from host '%s'.",lenstr,Url->hostport);
	else if(len<0)
	  PrintMessage(Warning,"Ignoring negative Content-Length (%s) in reply from host '%s'.",lenstr,Url->hostport);
	else
	  content_length=len;

	if(server_compression || modify || content_length==CUNDEF)
	  RemoveFromHeader(reply_head,"Content-Length");
      }
    }

    /* Set up compression header for the client if available and required. */

#if USE_ZLIB
    if(client_compression && !head_only && mode!=RealNoPassword)
      {
	/* If it is not to be compressed then don't */

	if(GetHeader(reply_head,"Content-Encoding") ||
	   NotCompressed(GetHeader(reply_head,"Content-Type"),NULL) ||
	   (content_length!=CUNDEF && !server_compression && content_length<=MINCOMPRSIZE))
	  client_compression=0;

          /* Add the compression header for the client. */

	else {
	  RemoveFromHeader(reply_head,"Content-Length");

	  AddToHeader(reply_head,"Content-Encoding",(client_compression==1)?"deflate":"gzip");
	  PrintMessage(Debug,"Using 'Content-Encoding: %s' for the client.",
		       client_compression==1?"deflate":"gzip");
	}
      }
#endif

    /* Set up chunked encoding header for the client if required. */

    if(client_chunked && !head_only && mode!=RealNoPassword)
      {
	/* If the length is already known don't bother with chunked encoding */
	if(GetHeader(reply_head,"Content-Length"))
	  client_chunked=0;
	else
	  {
	    ChangeVersionInHeader(reply_head,"HTTP/1.1");
	    AddToHeader(reply_head,"Transfer-Encoding","chunked");

	    PrintMessage(Debug,"Using 'Transfer-Encoding: chunked' for the client.");
	  }
      }

    /* Write the header to the client if this is the actual reply. */

    if(mode!=RealNoPassword)
      {
	size_t headlen;
	char *head=HeaderString(reply_head,&headlen);
	if(StderrLevel==ExtraDebug)
	  PrintMessage(ExtraDebug,"Outgoing Reply Head (to client)\n%s",head);

	if((err=write_data(client,head,headlen))<0)
	  PrintMessage(Inform,"Error writing to client [%!s]; client disconnected?");

	free(head);
      }

    if(server!=-1) /* can happen if ftp built-in error message */
       tell_io(server,&bytes_start,NULL);

    if(err>=0) {
      /* Initialise the client compression. */

#if USE_ZLIB
      if(client_compression && !head_only && mode!=RealNoPassword)
	configure_io_zlib(client,-1,client_compression);
#endif

      /* Initialise the client chunked encoding. */

      if(client_chunked && !head_only && mode!=RealNoPassword)
	configure_io_chunked(client,-1,1);

      /* Don't read more than the announced content length */
      if(content_length!=CUNDEF)
	configure_io_content_length(server,content_length);

      /* While there is data to read ... */

      if(modify)
	{
	  char *content_type=GetHeader(reply_head,"Content-Type");

	  PrintMessage(Debug,"Modifying page content of type '%s'",content_type);

	  modify_Url=Url;
	  modify_UrlProtocol=UrlProtocol;
	  modify_read_fd=-1;
	  modify_write_fd=client;
	  modify_copy_fd=(spool_err>=0?spool:-1);
	  modify_err=0;
	  modify_n=0;

	  if(modify==1)
	    OutputHTMLWithModifications(Url,spool,content_type);
	  else if(modify==2)
	    OutputGIFWithModifications();

	  n=modify_n;
	  err=modify_err;
	  if(modify_copy_fd==-1) spool_err=-1;
	}
      else
	for(;;)
	  {
	    n=(UrlProtocol->readbody)(buffer,IO_BUFFER_SIZE);
	    if(n<0)
	      PrintMessage(Warning,"Error reading reply body from remote host [%!s].");
	    if(n<=0)
	      break;

	    /* Write the data to the cache. */

	    if(mode!=RealNoCache && spool_err>=0)
	      if((spool_err=write_data(spool,buffer,n))<0)
                PrintMessage(Warning,"Cannot write to cache file [%!s]; disk full?");

	    /* Write the data to the client if it wants the body now. */

	    if(mode!=RealNoPassword && !head_only) {
	      if((err=write_data(client,buffer,n))<0) {
		PrintMessage(Inform,"Error writing to client [%!s]; client disconnected?");
		break;
	      }
	    }
	    else if(spool_err<0)
	      break;
	  }
    }

    /* If writing to the cache ... */

    if(mode!=RealNoCache)
      {
       /* In case of error writing to client decide if to continue reading from server. */

       if(err<0 && spool_err>=0 && server!=-1)
         {
	  int intr_percent=ConfigIntegerURL(IntrDownloadPercent,Url);
          unsigned long intr_size=(unsigned long)ConfigIntegerURL(IntrDownloadSize,Url)<<10;
	  unsigned long bytes_end,bytes;

	  /* if(server!=-1) */ /* can happen if ftp built-in error message */
	    tell_io(server,&bytes_end,NULL);
	  bytes=bytes_end-bytes_start;

	  PrintMessage(Debug,"Content-Length=%ld, bytes=%lu, intr-size=%lu intr-percent=%d.",
		       (long)content_length,bytes,intr_size,intr_percent);

	  if((content_length!=CUNDEF)?
	     (content_length<intr_size || (100*(double)bytes/(double)content_length)>intr_percent):
	     (bytes<intr_size))
	    {
	      PrintMessage(Debug,"Continuing download from server.");

	      for(;;)
		{
		  n=(UrlProtocol->readbody)(buffer,IO_BUFFER_SIZE);
		  if(n<0)
		    PrintMessage(Warning,"Error reading reply body from remote host [%!s].");
		  if(n<=0)
		    break;

		  /* Write the data to the cache. */

		  if((spool_err=write_data(spool,buffer,n))<0) {
		    PrintMessage(Warning,"Cannot write to cache file [%!s]; disk full?");
		    break;
		  }

		  if(content_length==CUNDEF) {
		    tell_io(server,&bytes_end,NULL);
		    bytes=bytes_end-bytes_start;
		    if(!(bytes<intr_size))
		      break;
		  }
		}

	      if(n==0)
		err=0;
	    }
	 }

       /* If there is an error with the client and we don't keep partial pages write the error to the cache. */

       if(err<0 && !ConfigBooleanURL(IntrDownloadKeep,Url))
	 {
	  lseek(spool,0,SEEK_SET);
	  ftruncate(spool,0);
	  reinit_io(spool);

	  HTMLMessage(spool,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
		      "url",Url->name,
		      "reason","ClientClose",
		      "cache","yes",
		      "backup",spool_exists?"yes":NULL,
		      NULL);
	 }

       /* If there is an error with the server and we don't keep partial pages write the error to the cache. */

       else if(n<0 && !ConfigBooleanURL(TimeoutDownloadKeep,Url))
         {
	   char *errmsg=errno==ERRNO_USE_IO_ERRNO?strdup("DataCorrupt"):
	                errno==ETIMEDOUT?strdup("TimeoutTransfer"):
	                x_asprintf("Error reading reply body from remote host [%s].",strerror(errno));

          lseek(spool,0,SEEK_SET);
          ftruncate(spool,0);
          reinit_io(spool);

          HTMLMessage(spool,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
                      "url",Url->name,
                      "reason",errmsg,
                      "cache","yes",
                      "backup",spool_exists?"yes":NULL,
                      NULL);
	  free(errmsg);
         }

       /* Otherwise ... */

       else
         {
          ftruncate(spool,lseek(spool,0,SEEK_CUR));

          /* In case of error change the cache status code. */

          if(n<0 || err<0)
            {
             lseek(spool,offsetof_status(reply_head),SEEK_SET);
             reinit_io(spool);
             write_all(spool,"503",3);
            }

          /* Delete the backup if there is one. */

          if(spool_exists)
             DeleteBackupWebpageSpoolFile(Url);
         }

       DeleteLockWebpageSpoolFile(Url);
      }

    /* Finish with the client compression. */

#if USE_ZLIB
    if(client_compression)
      if(configure_io_zlib(client,-1,0)<0)
	PrintMessage(Inform,"Error compressing reply body for client [%!s].");
#endif

    /* Finish with the client chunked encoding */

    if(client_chunked)
      if(configure_io_chunked(client,-1,0)<0)
	PrintMessage(Inform,"Error chunk-encoding reply body for client [%!s].");
   }

 /* When reading from the server and writing to the cache. */

 else if(mode==Fetch || mode==FetchNoPassword)
   {
    ssize_t spool_err=0,n=0;

    /* Write a message for auditing. */

    if(!spool_exists)
       PrintMessage(Inform,"Cache Access Status='New Page'."); /* Used in audit-usage.pl */
    else if(conditional_request_ims || conditional_request_inm)
       PrintMessage(Inform,"Cache Access Status='Modified on Server'."); /* Used in audit-usage.pl */
    else
       PrintMessage(Inform,"Cache Access Status='Forced Reload'."); /* Used in audit-usage.pl */

    /* Write the header to the cache. */

    {
      size_t reply_head_size;
      char *head;
      /* Add a WWWOFFLE timestamp */
      AddToHeader(reply_head,"wwwoffle-cache-date",RFC822Date(time(NULL),1));
      head=HeaderString(reply_head,&reply_head_size);
      if((spool_err=write_data(spool,head,reply_head_size))<0)
	PrintMessage(Warning,"Cannot write to cache file [%!s]; disk full?");
      free(head);
    }

    {
      char *lenstr=GetHeader(reply_head,"Content-Length");
      if(lenstr) {
	/* Don't read more than the announced content length */
	char *endptr;
	long len;
	errno=0;
	len=strtol(lenstr,&endptr,10);
	if(*endptr || errno)
	  PrintMessage(Warning,"Ignoring unparsable Content-Length (%s) in reply from host '%s'.",lenstr,Url->hostport);
	else if(len<0)
	  PrintMessage(Warning,"Ignoring negative Content-Length (%s) in reply from host '%s'.",lenstr,Url->hostport);
	else
	  configure_io_content_length(server,len);
      }
    }

    /* While the server has data write it to the cache. */

    if(spool_err>=0) {
      char buffer[IO_BUFFER_SIZE];

      for(;;) {
	n=(UrlProtocol->readbody)(buffer,IO_BUFFER_SIZE);
	if(n<0)
	  PrintMessage(Warning,"Error reading reply body from remote host [%!s].");
	if(n<=0)
	  break;

	if((spool_err=write_data(spool,buffer,n))<0) {
	  PrintMessage(Warning,"Cannot write to cache file [%!s]; disk full?");
	  break;
	}
      }
    }

    /* If there is an error with the server and we don't keep partial pages write the error to the cache. */

    if(n<0)
      {
	char *errstr=strdup(errno==ERRNO_USE_IO_ERRNO?"DataCorrupt":
			    errno==ETIMEDOUT?"Timeout":
			    strerror(errno));
	if(ConfigBooleanURL(TimeoutDownloadKeep,Url)) {
	  ftruncate(spool,lseek(spool,0,SEEK_CUR));

	  if(spool_exists)
	    DeleteBackupWebpageSpoolFile(Url);
	}
	else {
	  char *errmsg=errno==ERRNO_USE_IO_ERRNO?strdup("DataCorrupt"):
	               errno==ETIMEDOUT?strdup("TimeoutTransfer"):
	               x_asprintf("Error reading reply body from remote host [%s].",errstr);
	  lseek(spool,0,SEEK_SET);
	  ftruncate(spool,0);
	  reinit_io(spool);

	  HTMLMessage(spool,503,"WWWOFFLE Remote Host Error",NULL,"RemoteHostError",
		      "url",Url->name,
		      "reason",errmsg,
		      "cache","yes",
		      "backup",spool_exists?"yes":NULL,
		      NULL);
	  free(errmsg);
	}

	if(client!=-1)
          write_formatted(client,"Fetch Failure %s [%s]\n",Url->name,errstr);

	free(errstr);
      }

    else if(spool_err<0)
      {
	if(client!=-1)
	  write_formatted(client,"Cache Failure %s [%s]\n",Url->name,strerror(errno));
	if(spool_exists)
	  DeleteBackupWebpageSpoolFile(Url);
      }

    /* If we finished OK then delete the backup. */

    else
      {
	ftruncate(spool,lseek(spool,0,SEEK_CUR));

	if(spool_exists)
	  DeleteBackupWebpageSpoolFile(Url);
	if(client!=-1)
	  write_formatted(client,"Fetch Success %s\n",Url->name);
      }

    DeleteLockWebpageSpoolFile(Url);

    /* If the page was OK then fetch the links and images. */

    if(spool_err>=0 && n>=0 && reply_status>=200 && reply_status<400)
      {
       lseek(spool,0,SEEK_SET);
       reinit_io(spool);

       if(ParseDocument(spool,Url,0))
         {
          int links=RecurseFetch(Url);

          if(client!=-1 && links)
             write_formatted(client,"Fetching More %s [%d Extra URLs]\n",Url->name,links);

          fetch_again+=links;
         }
      }
   }

 /* If reading the page from the cache to the client. */

 else if(mode==Spool || mode==SpoolPragma)
   {
    /* If the lock file exists ... */

    if(ExistsLockWebpageSpoolFile(Url))
      {
       int t=0;

       /* If we are online then wait for it to become unlocked. */

       if(online)
         {
          t=ConfigInteger(SocketTimeout)/6;

          PrintMessage(Inform,"Waiting for the page to be unlocked.");

          while(--t>0 && ExistsLockWebpageSpoolFile(Url))
             sleep(1);

          if(t<=0)
             PrintMessage(Inform,"Timed out waiting for the page to be unlocked.");
         }

       /* Show an error message if still locked. */

       if(t<=0)
         {
          HTMLMessage(client,503,"WWWOFFLE File Locked",NULL,"FileLocked",
                      "url",Url->name,
                      NULL);
          mode=InternalPage; goto internalpage;
         }
      }

    /* Get the header from the cache. */

    if(reply_head) {FreeHeader(reply_head); /* reply_head=NULL; */ }
    reply_status=ParseReply(spool,&reply_head);

    PrintMessage(Inform,"Cache Access Status='Cached Page Used'."); /* Used in audit-usage.pl */

    /* If the page is not empty ... */

    if(reply_head)
      {
	struct stat buf;
	ssize_t err=0;
	int modify=0,spool_compression=0;
	unsigned long size=0,content_length=0;

	if(StderrLevel==ExtraDebug)
	  {
	    char *headerstring=HeaderString(reply_head,NULL);
	    PrintMessage(ExtraDebug,"Spooled Reply Head (from cache)\n%s",headerstring);
	    free(headerstring);
	  }

	if(!fstat(spool,&buf)) {
	  unsigned long spool_head_size;
	  size=buf.st_size;
	  tell_io(spool,&spool_head_size,NULL);
	  content_length=size-spool_head_size;
	}
	else
	  PrintMessage(Warning,"Cannot stat spool file for '%s' [%!s].",Url->name);

       /* If the page is a previous error message then delete the cached one and restore the backup. */

       if(is_wwwoffle_error_message(reply_head) || size==0)
         {
          char *err=DeleteWebpageSpoolFile(Url,0);
          RestoreBackupWebpageSpoolFile(Url);
          if(err) free(err);
         }

       /* If the cached file is compressed then prepare to uncompress it. */

#if USE_ZLIB
       {
	 char *content_encoding;

	 if((content_encoding=GetHeader(reply_head,"Content-Encoding")) && RemoveFromHeader2(reply_head,"Pragma","wwwoffle-compressed"))
	   {
             PrintMessage(Debug,"Spooled page has 'Content-Encoding: %s'.",content_encoding);
             RemoveFromHeader(reply_head,"Content-Encoding");
             configure_io_zlib(spool,2,-1);
	     spool_compression=1;
	   }
       }
#endif

       /* Fix up the reply head from the cache before sending to the client. */

       ModifyReply(Url,reply_head);

       /* Decide if we need to modify the content. */

       if(!head_only &&
	  !is_client_wwwoffle &&
          !is_client_searcher &&
          /* ConfigBooleanURL(EnableHTMLModifications,Url) && */
          !GetHeader2(request_head,"Cache-Control","no-transform"))
         {
          char *content_encoding;

          if(ConfigBooleanURL(EnableHTMLModifications,Url) &&
             (GetHeader2(reply_head,"Content-Type","text/html") ||
              GetHeader2(reply_head,"Content-Type","application/xhtml")))
             modify=1;
          else if(ConfigBooleanURL(DisableAnimatedGIF,Url) &&
                  GetHeader2(reply_head,"Content-Type","image/gif"))
             modify=2;

          /* If the spooled page uses compression and we are modifying the content then don't (e.g. very old WWWOFFLE cache). */

          if(modify && (content_encoding=GetHeader(reply_head,"Content-Encoding")))
             if(WhichCompression(content_encoding))
                modify=0;
         }

       /* Set up compression header for the client if available and required. */

#if USE_ZLIB
       if(client_compression && !head_only)
         {
          /* If it is not to be compressed then don't */

          if(GetHeader(reply_head,"Content-Encoding") ||
             NotCompressed(GetHeader(reply_head,"Content-Type"),NULL) ||
	     (size && !spool_compression && content_length<=MINCOMPRSIZE))
	    client_compression=0;

          /* Add the compression header for the client. */

          else
            {
	     AddToHeader(reply_head,"Content-Encoding",
			 (client_compression==1)?"deflate":"gzip");

             PrintMessage(Debug,"Using 'Content-Encoding: %s' for the client.",
                          client_compression==1?"deflate":"gzip");
            }
         }
#endif

       /* Set up chunked encoding header for the client if required. */

       if(client_chunked && !head_only)
         {
	   /* If the content length is already known don't bother with chunked encoding */
	   if(size && !spool_compression && !modify && !client_compression)
	     client_chunked=0;
	   else
	     {
	       ChangeVersionInHeader(reply_head,"HTTP/1.1");
	       AddToHeader(reply_head,"Transfer-Encoding","chunked");

	       PrintMessage(Debug,"Using 'Transfer-Encoding: chunked' for the client.");
	     }
	 }

       /* Write the header to the client. */

       if(size && !spool_compression && !modify && !client_compression) {
	   char length[MAX_INT_STR+1];
	   sprintf(length,"%lu",content_length);
	   ReplaceOrAddInHeader(reply_head,"Content-Length",length);
       }
       else
	 RemoveFromHeader(reply_head,"Content-Length");

       {
	 size_t reply_head_size;
	 char *head=HeaderString(reply_head,&reply_head_size);

	 if(StderrLevel==ExtraDebug)
	   PrintMessage(ExtraDebug,"Outgoing Reply Head (to client)\n%s",head);
	 if((err=write_data(client,head,reply_head_size))<0)
	   PrintMessage(Inform,"Error writing to client [%!s]; client disconnected?");
	 free(head);
       }

       /* If we are doing the body as well then read that from the file and send to the client. */

       if(err>=0 && !head_only)
	 {
	   /* Initialise the client compression. */

#if USE_ZLIB
	   if(client_compression)
	     configure_io_zlib(client,-1,client_compression);
#endif

	   /* Initialise the client chunked encoding. */

	   if(client_chunked)
	     configure_io_chunked(client,-1,1);

	   /* Modify the content or don't and output it to the client. */

	   if(modify)
	     {
	       char *content_type=GetHeader(reply_head,"Content-Type");

	       PrintMessage(Debug,"Modifying page content of type '%s'",content_type);

	       modify_Url=NULL;
	       modify_UrlProtocol=NULL;
	       modify_read_fd=spool;
	       modify_write_fd=client;
	       modify_copy_fd=-1;
	       modify_err=0;
	       modify_n=0;

	       if(modify==1)
		 OutputHTMLWithModifications(Url,spool,content_type);
	       else if(modify==2)
		 OutputGIFWithModifications();
	     }
	   else
	     {
	       ssize_t n;
	       char buffer[IO_BUFFER_SIZE];

	       for(;;) {
		 n=read_data(spool,buffer,IO_BUFFER_SIZE);
		 if(n<0)
		   PrintMessage(Warning,"Error reading spool file for '%s' [%!s].",Url->name);
		 if(n<=0)
		   break;
		 if(write_data(client,buffer,n)<0)
		   {
		     PrintMessage(Inform,"Error writing to client [%!s]; disconnected?");
		     break;
		   }
	       }
	     }
	 }
 
       /* Finish with uncompressing from the cache. */

#if USE_ZLIB
       if(spool_compression)
	 if(configure_io_zlib(spool,0,-1)<0)
	   PrintMessage(Warning,"Error uncompressing spool file for '%s' [%!s].",Url->name);
#endif

       /* Finish with the client compression. */

#if USE_ZLIB
       if(client_compression)
	 if(configure_io_zlib(client,-1,0)<0)
	   PrintMessage(Inform,"Error compressing reply body for client [%!s].");
#endif

       /* Finish with the client chunked encoding */

       if(client_chunked)
	 if(configure_io_chunked(client,-1,0)<0)
	   PrintMessage(Inform,"Error chunk-encoding reply body for client [%!s].");
      }
    else
      {
	char *err=DeleteWebpageSpoolFile(Url,0);
	PrintMessage(Warning,"Spooled Reply Head (from cache) is empty; deleting it.");
	if(err) free(err);
      }
   }

 /* If the page is to be requested. */

 else if(mode==SpoolGet)
   {
    /* If offline requests are allowed or we are online give the appropriate message to the client. */

    if(offline_request || online)
      {
       if(ConfigBooleanURL(ConfirmRequests,Url) && !IsLocalHost(Url) && !outgoing_exists)
          HTMLMessage(client,404,"WWWOFFLE Confirm Request",NULL,"ConfirmRequest",
                      "url",Url->name,
                      NULL);
       else if(fetch_again)
	 {
	  char *hash=HashOutgoingSpoolFile(Url);
          HTMLMessage(client,404,"WWWOFFLE Refresh Will Get",NULL,"RefreshWillGet",
                      "url",Url->name,
                      "hash",hash,
                      NULL);
	  free(hash);
	 }
       else if(outgoing_exists)
          HTMLMessage(client,404,"WWWOFFLE Will Get",NULL,"WillGet",
                      "url",Url->name,
                      "hash",NULL,
                      NULL);
       else
	 {
	  char *hash=HashOutgoingSpoolFile(Url);
          HTMLMessage(client,404,"WWWOFFLE Will Get",NULL,"WillGet",
                      "url",Url->name,
                      "hash",hash,
                      NULL);
	  free(hash);
	 }
      }

    /* Else refuse the request. */

    else
       HTMLMessage(client,404,"WWWOFFLE Refused Request",NULL,"RefusedRequest",
                   "url",Url->name,
                   NULL);

    mode=InternalPage;
   }

 /* If the request is for a refresh page when online or offline then redirect the client. */

 else /* if(mode==RealRefresh || mode==SpoolRefresh) */
   {
    char *proxyablelink=ProxyableLink(Url);
    HTMLMessage(client,302,"WWWOFFLE Refresh Redirect",proxyablelink,"Redirect",
                "location",proxyablelink,
                NULL);

    mode=InternalPage;
   }


 /*----------------------------------------
   mode = SpoolInternal, Real, RealNoCache, RealNoPassword, Fetch or FetchNoPassword

   Output the data to the client if we haven't already sent it.
   ----------------------------------------*/


 /* If we are handling an internally generated page in a temporary file ... */

 if(mode==SpoolInternal)
   spoolinternal:
   {
    off_t size;
    unsigned long content_length;

    if(reply_head) {
       FreeHeader(reply_head);
       reply_head=NULL;
    }

    /* Work out the size of the file. */

    if((size=lseek(tmpclient,0,SEEK_CUR))==(off_t)-1 || lseek(tmpclient,0,SEEK_SET)==(off_t)-1) {
      char *errmsg=GetPrintMessage(Warning,"Cannot read temporary file [%!s].");
      HTMLMessage(client,500,"WWWOFFLE Server Error",NULL,"ServerError",
		  "error",errmsg,
		  NULL);
      free(errmsg);

      mode=InternalPage; goto internalpage;
    }
    init_io(tmpclient);

    /* Read the head back in again. */

    reply_status=ParseReply(tmpclient,&reply_head);

    if(reply_head)
      {
       ssize_t err=0;

       {
	 unsigned long reply_head_size;
	 tell_io(tmpclient,&reply_head_size,NULL);
	 content_length=size-reply_head_size;
       }

       if(!reply_head->version)  /* If status line was missing, construct one according to CGI specification */
	 {
	   char *val;

	   reply_head->version=strdup("HTTP/1.0");

	   if((val=GetHeader(reply_head,"Status")))
	     {
	       reply_head->status=atoi(val);
	       while(*val && isdigit(*val)) ++val;  /* skip status code */
	       while(*val && isspace(*val)) ++val;  /* skip whitespace */
	       reply_head->note=strdup(val);
	       RemoveFromHeader(reply_head,"Status");
	     }
	   else if((val=GetHeader(reply_head,"Location")))
	     {
	       char *p=val;

	       while(*p && (isalnum(*p) || *p=='+' || *p=='-' || *p=='.')) ++p;
	       if(p>val && *p==':')   /* Does it look like a URL? */
		 {
		   reply_head->status=302;
		   reply_head->note=strdup("WWWOFFLE Redirect");
		 }
	       else  /* Try to serve up a local page instead */
		 {
		   finish_io(tmpclient);
		   CloseTempSpoolFile(tmpclient);
		   tmpclient=-1;

		   FreeURL(Url);
		   Url=SplitURL(val);

		   if(IsLocalHost(Url))
		     {
		       if((++redirect_count)<=MAX_REDIRECT)
			 {
			   PrintMessage(Debug,"While in mode SpoolInternal the reply head contains redirection to a local page '%s'.",val);

			   FreeHeader(reply_head); reply_head=NULL;
			   createlasttimespoolfile=0;

			   /* restore original mode */
			   if(online==1 && !fetching)       mode=Real;
			   else if(online!=0 && fetching)   mode=Fetch;
			   else if(online==-1 && !fetching) mode=SpoolOrReal;
			   else if(!online && !fetching)    mode=Spool;

			   goto local_url;
			 }
		       else
			 {
			   PrintMessage(Warning,"After %d redirections the maximum local redirection count has been exceeded, last redirection was to a page '%s'.",redirect_count,val);
			   HTMLMessage(client,500,"WWWOFFLE Server Error",NULL,"ServerError",
				       "error","WWWOFFLE Maximum Local Redirection Count Exceeded.",
				       NULL);

			   mode=InternalPage; goto internalpage;
			 }
		     }
		   else
		     { /* We should never get here */
		       PrintMessage(Warning,"While in mode SpoolInternal the reply head contains redirection to a page '%s' that looks neither like a URL nor like a path to a local page.",val);
		       HTMLMessage(client,404,"WWWOFFLE Page Not Found",NULL,"PageNotFound",
				   "url",Url->name,
				   NULL);

		       mode=InternalPage; goto internalpage;
		     }
		 }
	     }
	   else
	     {
	       reply_head->status=200;
	       reply_head->note=strdup("WWWOFFLE OK");
	     }

	   if(!GetHeader(reply_head,"Server"))
	     AddToHeader(reply_head,"Server","WWWOFFLE/" WWWOFFLE_VERSION);

	   if(!GetHeader(reply_head,"Date"))
	     AddToHeader(reply_head,"Date",RFC822Date(time(NULL),1));

	 }

       /* Modify the reply as appropriate. */

       ModifyReply(Url,reply_head);

       /* Set up compression header for the client if available and required. */

#if USE_ZLIB
       if(client_compression && !head_only)
         {
          /* If it is not to be compressed then don't */

          if(GetHeader(reply_head,"Content-Encoding") ||
             NotCompressed(GetHeader(reply_head,"Content-Type"),NULL) ||
	     content_length<=MINCOMPRSIZE)
	    client_compression=0;

          /* Add the compression header for the client. */

          else
            {
	     AddToHeader(reply_head,"Content-Encoding",
			 (client_compression==1)?"deflate":"gzip");

             PrintMessage(Debug,"Using 'Content-Encoding: %s' for the client.",
                          client_compression==1?"deflate":"gzip");
            }
         }
#endif

       /* Set up chunked encoding header for the client if required. */

       if(client_chunked && !head_only)
         {
	   /* If the content length is already known don't bother with chunked encoding */
	   if(!client_compression)
	     client_chunked=0;
	   else
	     {
	       ChangeVersionInHeader(reply_head,"HTTP/1.1");
	       AddToHeader(reply_head,"Transfer-Encoding","chunked");

	       PrintMessage(Debug,"Using 'Transfer-Encoding: chunked' for the client.");
	     }
	 }

       /* Put in the correct content length. */

       if(!client_compression) {
	   char length[MAX_INT_STR+1];
	   sprintf(length,"%lu",content_length);
	   ReplaceOrAddInHeader(reply_head,"Content-Length",length);
       }
       else
	 RemoveFromHeader(reply_head,"Content-Length");

       /* Write the header to the client. */

       {
	 size_t headlen;
	 char *head=HeaderString(reply_head,&headlen);

	 if(StderrLevel==ExtraDebug)
	   PrintMessage(ExtraDebug,"Outgoing Reply Head (to client):\n%s",head);
	 if((err=write_data(client,head,headlen))<0)
	   PrintMessage(Inform,"Error writing to client [%!s]; client disconnected?");

	 free(head);
       }

       /* If we are doing the body as well then read that from the file and send it to the client. */

       if(err>=0 && !head_only)
	 {
	   ssize_t n;
	   char buffer[IO_BUFFER_SIZE];

	   /* Initialise the client compression. */

#if USE_ZLIB
	   if(client_compression)
	     configure_io_zlib(client,-1,client_compression);
#endif

	   /* Initialise the client chunked encoding. */

	   if(client_chunked)
	     configure_io_chunked(client,-1,1);

	   for(;;) {
	     n=read_data(tmpclient,buffer,IO_BUFFER_SIZE);
	     if(n<0)
	       PrintMessage(Warning,"Error reading temporary spool file for '%s' [%!s].",Url->name);
	     if(n<=0)
	       break;
	     if(write_data(client,buffer,n)<0) {
	       PrintMessage(Inform,"Error writing to client [%!s]; client disconnected?");
	       break;
	     }
	   }
	 }

       /* Finish with the client compression. */

#if USE_ZLIB
       if(client_compression)
	 if(configure_io_zlib(client,-1,0)<0)
	   PrintMessage(Inform,"Error compressing reply body for client [%!s].");
#endif

       /* Finish with the client chunked encoding */

       if(client_chunked)
	 if(configure_io_chunked(client,-1,0)<0)
	   PrintMessage(Inform,"Error chunk-encoding reply body for client [%!s].");
      }
    else
       PrintMessage(Warning,"Outgoing Reply Head (to client) is empty.");
   }

internalpage:
clean_up:
 /*----------------------------------------
   mode = InternalPage, SpoolInternal, Real, RealNoCache, RealNoPassword, Fetch or FetchNoPassword

   Tidy up and exit.
   ----------------------------------------*/

 if(is_server) {
   /* Delete the outgoing spool file if we just got it from the server. */

   if(outgoing_exists)
     {
       char *err=DeleteOutgoingSpoolFile(Url);
       if(err) free(err);
       outgoing_exists=0;
     }

   /* If we had a connection to a server then close it. */

   (UrlProtocol->close)();
   is_server=0;
 }

 /* If there is a temporary file open then close it. */

 if(tmpclient>=0) {
    finish_io(tmpclient);
    CloseTempSpoolFile(tmpclient);
    tmpclient=-1;
 }

 /* Close the outgoing file if any. */

 if(outgoing_read>=0)
   {
    finish_io(outgoing_read);
    close(outgoing_read);
    outgoing_read=-1;
   }

 if(outgoing_write>=0)
   {
    finish_io(outgoing_write);
    /* Close and remove (temporary) outgoing file. */
    CloseNewOutgoingSpoolFile(outgoing_write,NULL);
    outgoing_write=-1;
   }

 /* If there is a spool file open then close it. */

 if(spool>=0)
   {
    if(finish_io(spool)<0)
      PrintMessage(Warning,"Cannot finish writing to cache file [%!s].");
    if(close(spool))
      PrintMessage(Warning,"Cannot close cache file [%!s].");
    spool=-1;
   }

 /* Create an entry in the last time spool directory (if appropriate). */

 if(createlasttimespoolfile) CreateLastTimeSpoolFile(Url);

 /* If we were searched then reset the cache file time. */

 if(is_client_searcher && request_head && spool_exists)
    TouchWebpageSpoolFile(Url,spool_exists);

 /* If there is a reply head free it. */

 if(reply_head) {
    FreeHeader(reply_head);
    reply_head=NULL;
 }

 /* If we have fetched a version of a URL without a password then fetch the real one. */

 if(exitval==-1 && (mode==RealNoPassword || mode==FetchNoPassword))
   {
    FreeURL(Url);
    Url=Urlpw;
    Urlpw=NULL;

    spool_exists=spool_exists_pw;
    spool_exists_pw=0;
    outgoing_exists=outgoing_exists_pw;
    outgoing_exists_pw=0;
    createlasttimespoolfile=0;

    if(mode==RealNoPassword)
       mode=Real;
    else
       mode=Fetch;

    goto passwordagain;
   }

 /* Free the original URL */

 FreeURL(Url);
 if(Urlpw) FreeURL(Urlpw);

 /* If there is a request head and body then free them. */

free_request_head_body:
 if(request_head)
    FreeHeader(request_head);
 if(request_body)
    FreeBody(request_body);

 /* If there is a client then close it down cleanly. */

close_client:
 if(client>=0)
   {
    if(mode==Fetch)
      {
       finish_io(client);
       CloseSocket(client);
      }
    else /* mode!=Fetch */
      {
       unsigned long r,w;

       if(finish_tell_io(client,&r,&w)<0)
	 PrintMessage(Inform,"Error writing to client: finishing IO failed [%!s].");

       PrintMessage(Inform,"Client bytes; %d Read, %d Written.",r,w); /* Used in audit-usage.pl */

       ShutdownSocket(client);
      }
   }

 /* Tidy up messages and header parsing. */

 FinishMessages();
 FinishParse();

 /* Clean up language preferences. */
 SetLanguage(NULL);

 /* proxy_user is an malloc-ed string */

 if(proxy_user) {free(proxy_user); proxy_user=NULL;}

 /* If we need to fetch more then tell the parent process. */

 return (exitval!=-1)?exitval:fetch_again?4:0;
}


/*++++++++++++++++++++++++++++++++++++++
  Read data from the server/cache and perhaps write a copy to the cache when modifying content.

  ssize_t wwwoffles_read_data Returns the number of bytes read.

  char *data The data buffer to fill in.

  size_t len The length of the buffer to fill in.

  This function is used as a callback from gifmodify.c and htmlmodify.l
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t wwwoffles_read_data(char *data,size_t len)
{
 if(modify_err==-1)
    return(0);

 if(modify_Url)
    modify_n=(modify_UrlProtocol->readbody)(data,len);
 else
    modify_n=read_data(modify_read_fd,data,len);

 if(modify_n>0)
   {
    /* Write the data to the cache. */

    if(modify_copy_fd!=-1)
      if(write_data(modify_copy_fd,data,modify_n)==-1) {
          PrintMessage(Warning,"Cannot write to cache file; disk full?");
	  modify_copy_fd=-1;
      }
   }

 return(modify_n);
}


/*++++++++++++++++++++++++++++++++++++++
  Write data to the client and handle client socket closing.

  ssize_t wwwoffles_write_data Returns the number of bytes written.

  const char *data The data to write.

  size_t len The number of bytes to write.

  This function is used as a callback from gifmodify.c and htmlmodify.l
  ++++++++++++++++++++++++++++++++++++++*/

ssize_t wwwoffles_write_data(const char *data,size_t len)
{
 /* Write the data to the client if it wants the body now. */

 if(modify_err!=-1)
    modify_err=write_buffer_data(modify_write_fd,data,len); /* expect to make lots of small writes */

 return(len);
}


/*++++++++++++++++++++++++++++++++++++++
  Uninstall the signal handlers.
  ++++++++++++++++++++++++++++++++++++++*/

static void uninstall_sighandlers(void)
{
 struct sigaction action;

 /* SIGCHLD */
 action.sa_handler = SIG_DFL;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGCHLD, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot uninstall SIGCHLD handler.");

 /* SIGINT, SIGQUIT, SIGTERM */
 action.sa_handler = SIG_DFL;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGINT, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot uninstall SIGINT handler.");
 if(sigaction(SIGQUIT, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot uninstall SIGQUIT handler.");
 if(sigaction(SIGTERM, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot uninstall SIGTERM handler.");

 /* SIGHUP */
 action.sa_handler = SIG_DFL;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGHUP, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot uninstall SIGHUP handler.");

 /* SIGPIPE */
 action.sa_handler = SIG_IGN;
 sigemptyset(&action.sa_mask);
 action.sa_flags = 0;
 if(sigaction(SIGPIPE, &action, NULL) != 0)
    PrintMessage(Warning, "Cannot ignore SIGPIPE.");
}
