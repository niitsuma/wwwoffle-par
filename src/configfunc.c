/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configfunc.c 1.42 2006/07/21 17:37:34 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Configuration item checking functions.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2004,2005,2006,2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "misc.h"
#include "errors.h"
#include "configpriv.h"
#include "config.h"
#include "sockets.h"
#include "headbody.h"


/*+ The local HTTP (or HTTPS) port number. +*/
static int localport=-1;

/*+ The local port protocol (HTTP or HTTPS). +*/
static char *localproto=NULL;


/*++++++++++++++++++++++++++++++++++++++
  Determine an email address to use as the FTP password.

  char *DefaultFTPPassWord Returns a best-guess password.
  ++++++++++++++++++++++++++++++++++++++*/

char *DefaultFTPPassWord(void)
{
 struct passwd *pwd;
 char *username,*fqdn,*password;

 pwd=getpwuid(getuid());

 if(!pwd)
    username="root";
 else
    username=pwd->pw_name;

 fqdn=GetFQDN();

 if(!fqdn)
    fqdn="";

 password=(char*)malloc(strlen(username)+strlen(fqdn)+4);
 sprintf(password,"%s@%s",username,fqdn);

 return(password);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the specified host and port number is allowed for SSL.

  int IsSSLAllowed Returns true if it is allowed.

  URL *Url The URL of the host (no path) to match.

  int cached A flag to indicate that caching is proposed.
  ++++++++++++++++++++++++++++++++++++++*/

int IsSSLAllowed(URL *Url,int cached)
{
 int i;
 char *hostport=Url->hostport;

 if(!Url->port)
   {
     hostport=(char*)alloca(strlen(Url->host)+sizeof(":443"));
     stpcpy(stpcpy(hostport,Url->host),":443");
   }

 if(cached)
   {
    if(SSLDisallowCache)
       for(i=0;i<SSLDisallowCache->nentries;++i)
          if(WildcardMatch(hostport,SSLDisallowCache->val[i].string))
            return 0;

    if(SSLAllowCache)
       for(i=0;i<SSLAllowCache->nentries;++i)
          if(WildcardMatch(hostport,SSLAllowCache->val[i].string))
            return 1;
   }
 else
   {
    if(SSLDisallowTunnel)
       for(i=0;i<SSLDisallowTunnel->nentries;++i)
          if(WildcardMatch(hostport,SSLDisallowTunnel->val[i].string))
            return 0;

    if(SSLAllowTunnel)
       for(i=0;i<SSLAllowTunnel->nentries;++i)
          if(WildcardMatch(hostport,SSLAllowTunnel->val[i].string))
            return 1;
   }

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the specified path is allowed to be used for CGIs.

  int IsCGIAllowed Returns true if it is allowed.

  const char *path The pathname to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsCGIAllowed(const char *path)
{
 int i;

 if(ExecCGI)
    for(i=0;i<ExecCGI->nentries;++i)
       if(WildcardMatch(path,ExecCGI->val[i].string))
         return 1;

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Set the local port number so that it can be used in local URLs.

  int port The port number.
  ++++++++++++++++++++++++++++++++++++++*/

void SetLocalPort(int port)
{
 localport=port;

 if(localport==ConfigInteger(HTTPS_Port))
    localproto="https";
 else
    localproto="http";
}


/*++++++++++++++++++++++++++++++++++++++
  Get the name of the first specified server in the Localhost section.

  char *GetLocalHost Returns the first named localhost.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetLocalHost(void)
{
 return strdup((LocalHost && LocalHost->nentries)?
	       LocalHost->key[0].string:
	       DEF_LOCALHOST);
}


/*++++++++++++++++++++++++++++++++++++++
  Get the name and port number of the first specified server in the Localhost section.

  char *GetLocalHostPort Returns the first named localhost and port number.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetLocalHostPort(void)
{
 char *localhost=((LocalHost && LocalHost->nentries)?
		  LocalHost->key[0].string:
		  DEF_LOCALHOST);

 return x_asprintf(strchr(localhost,':')?"[%s]:%d":"%s:%d",
		   localhost,localport);
}


/*++++++++++++++++++++++++++++++++++++++
  Get a URL for the first specified server in the Localhost section.

  char *GetLocalURL Returns a URL for the first named localhost and port number.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetLocalURL(void)
{
 char *localhost=((LocalHost && LocalHost->nentries)?
		  LocalHost->key[0].string:
		  DEF_LOCALHOST);

 return x_asprintf(strchr(localhost,':')?"%s://[%s]:%d":"%s://%s:%d",
		   localproto,localhost,localport);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is the localhost.

  int IsLocalHost Return true if the host is the local host.

  const URL *Url The URL that has been requested.
  ++++++++++++++++++++++++++++++++++++++*/

int IsLocalHost(const URL *Url)
{
 int http_port,https_port;
 int i;

 http_port=ConfigInteger(HTTP_Port);
 https_port=ConfigInteger(HTTPS_Port);

 if(Url->portnum!=http_port && Url->portnum!=https_port && (Url->port || http_port!=80 || https_port!=443))
    return(0);

 if(LocalHost)
    for(i=0;i<LocalHost->nentries;++i)
       if(!strcmp(LocalHost->key[i].string,Url->host))
         return 1;

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is in the local network.

  int IsLocalNetHost Return true if the host is on the local network.

  const char *host The name of the host to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

int IsLocalNetHost(const char *host)
{

 if(LocalHost) {
   int i;
   for(i=0;i<LocalHost->nentries;++i)
     if(!strcmp(LocalHost->key[i].string,host))
       return 1;
 }

 if(LocalNet) {
   int i;
   for(i=0;i<LocalNet->nentries;++i) {
     char *localnet=LocalNet->key[i].string;
     if(*localnet=='!') {
       if(WildcardMatch(host,localnet+1))
	 return 0;
     }
     else {
       if(WildcardMatch(host,localnet))
	 return 1;
     }
   }
 }

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is allowed to connect.

  int IsAllowedConnectHost Return true if it is allowed to connect.

  const char *host The name of the host to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

int IsAllowedConnectHost(const char *host)
{
 if(LocalHost) {
   int i;
   for(i=0;i<LocalHost->nentries;++i)
     if(!strcmp(LocalHost->key[i].string,host))
       return 1;
 }

 if(AllowedConnectHosts) {
   int i;
   for(i=0;i<AllowedConnectHosts->nentries;++i) {
     char *allowedhost=AllowedConnectHosts->key[i].string;
     if(*allowedhost=='!') {
       if(WildcardMatch(host,allowedhost+1))
	 return 0;
     }
     else {
       if(WildcardMatch(host,allowedhost))
	 return 1;
     }
   }
 }

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified username and password is allowed to connect.

  char *IsAllowedConnectUser Return the username if it is allowed to connect.

  const char *userpass The encoded username and password of the user to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

char *IsAllowedConnectUser(const char *userpass)
{
 char *isit=NULL;

 if(userpass)
   {
     const char *up=userpass;

     while(*up && *up!=' ') up++;
     while(*up==' ') up++;

     if(AllowedConnectUsers) {
       int i;

       for(i=0;i<AllowedConnectUsers->nentries;++i)
	 if(!strcmp(AllowedConnectUsers->key[i].string,up))
	   goto found;

       return NULL;
     }

   found:
     {
       char *colon;
       isit=(char *)Base64Decode((unsigned char *)up,NULL,NULL,0);
       colon=strchrnul(isit,':');
       *colon=0;
     }
   }

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the data for a URL can be compressed or not based on the MIME type or file extension.

  int NotCompressed Return 1 if the data is not to be compressed.

  const char *mime_type The MIME type of the data (may be NULL).

  const char *path The path of the URL (may be NULL).
  ++++++++++++++++++++++++++++++++++++++*/

int NotCompressed(const char *mime_type,const char *path)
{

  if(mime_type && DontCompressMIME) {
    int i;
    for(i=0;i<DontCompressMIME->nentries;++i)
      if(!strcmp(DontCompressMIME->val[i].string,mime_type))
	return 1;
  }

  if(path && DontCompressExt) {
    size_t pathlen=strlen(path);
    int i;
    for(i=0;i<DontCompressExt->nentries;++i) {
      char *ext = DontCompressExt->val[i].string;
      size_t extlen=strlen(ext);
      if(pathlen>extlen && !strcmp(ext,path+pathlen-extlen))
	return 1;
    }
  }

  return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the header line is to be sent to the server/client.

  char *CensoredHeader Returns the value to be inserted or NULL if it is to be removed.

  const URL *Url the URL that the request or reply is for/from.

  const char *key The key to check.

  char *val The default value to use.
  ++++++++++++++++++++++++++++++++++++++*/

char *CensoredHeader(ConfigItem confitem,const URL *Url,const char *key,char *val)
{
 char *new=val;

 if(confitem) {
   int i;
   for(i=0;i<confitem->nentries;++i)
     if(!strcasecmp(confitem->key[i].string,key))
       if(!confitem->url[i] || MatchUrlSpecification(confitem->url[i],Url))
	 {
	   if(!confitem->val[i].string || !strcmp(confitem->val[i].string,"yes"))
	     new=NULL;
	   else if(!strcmp(confitem->val[i].string,"no"))
	     ;
	   else
	     new=strdup(confitem->val[i].string);

	   break;
	 }
 }

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide what mime type to apply for a given file.

  char *WhatMIMEType Returns the MIME Type.

  const char *path The path of the file.
  ++++++++++++++++++++++++++++++++++++++*/

char *WhatMIMEType(const char *path)
{
  char *mimetype=NULL;

  if(MIMETypes) {
    size_t plen=strlen(path);
    size_t maxlen=0;
    int i;

    for(i=0;i<MIMETypes->nentries;++i) {
      char *mtype_key= MIMETypes->key[i].string;
      size_t keylen= strlen(mtype_key);

      if(plen>keylen && keylen>maxlen && !strcmp(mtype_key,path+plen-keylen))
	{mimetype=MIMETypes->val[i].string; maxlen=keylen;}
    }
  }

  return mimetype?mimetype:ConfigString(DefaultMIMEType);
}


/*++++++++++++++++++++++++++++++++++++++
  Get the URL that the current URL is aliased to.

  URL *GetAliasURL Returns a pointer to a new URL if the specified URL is aliased.

  const URL *Url The URL to check.
  ++++++++++++++++++++++++++++++++++++++*/

URL *GetAliasURL(const URL *Url)
{
 URL *newUrl=NULL;
 int i;

 if(Aliases)
    for(i=0;i<Aliases->nentries;++i)
      {
       UrlSpec *alias_key_spec=Aliases->key[i].urlspec;
       char *alias_path=HasUrlSpecPath(alias_key_spec)?UrlSpecPath(alias_key_spec):"";

       if(MatchUrlSpecificationProtoHostPort(alias_key_spec,Url) &&
          !strcmp_beg(Url->path,alias_path))
         {
          char *new_proto=NULL,*new_hostport=NULL,*new_path=NULL,*new_args=NULL;
	  UrlSpec *alias_val_spec=Aliases->val[i].urlspec;
	  int replaceflags=0;

          /* Sort out the aliased protocol. */

          if(HasUrlSpecProto(alias_val_spec)) {
	    new_proto=UrlSpecProto(alias_val_spec);
	    replaceflags |= REPLACEURLPROTO;
	  }

          /* Sort out the aliased host. */

          if(HasUrlSpecHost(alias_val_spec))
            {
	     if(UrlSpecPort(alias_val_spec)>0) {
	       new_hostport=(char*)alloca(strlen(UrlSpecHost(alias_val_spec))+1+MAX_INT_STR+1);
	       sprintf(new_hostport,"%s:%d",UrlSpecHost(alias_val_spec),UrlSpecPort(alias_val_spec));
	     }
	     else
	       new_hostport=UrlSpecHost(alias_val_spec);

	     replaceflags |= REPLACEURLHOSTPORT;
            }

          /* Sort out the aliased path. */

          if(HasUrlSpecPath(alias_val_spec))
            {
             size_t oldlen=strlen(alias_path);
             size_t newlen=strlen(UrlSpecPath(alias_val_spec));
	     int slashendold=(oldlen && alias_path[oldlen-1]=='/');
	     int slashendnew=(newlen && *(UrlSpecPath(alias_val_spec)+newlen-1)=='/');
	     char *p;

             new_path=(char*)alloca(newlen+strlen(Url->path)-oldlen+2);
             p=stpcpy(new_path,UrlSpecPath(alias_val_spec));

             if(slashendold && !slashendnew)
	       *p++='/';
             else if(!slashendold && slashendnew)
	       --p;

             stpcpy(p,Url->path+oldlen);

	     replaceflags |= REPLACEURLPATH;
            }

          /* Sort out the aliased arguments. */

          if(Url->args && *Url->args=='!')
            {
             char *pling2=strchr(Url->args+1,'!');

             if(pling2)
                new_args=pling2+1;
             else
                new_args=NULL;

	     replaceflags |= REPLACEURLARGS;
            }

          /* Create the new alias. */

          newUrl=MakeModifiedURL(Url,replaceflags,
				 new_proto,new_hostport,new_path,new_args,NULL,NULL);

          break;
         }
     }

 return(newUrl);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the integer value that applies to a ConfigItem.

  int ConfigInteger Returns the integer value.

  ConfigItem item The configuration item to check.
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigInteger(ConfigItem item)
{
 if(!item)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->url_type!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->nentries>1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->nentries==1)
    return(item->val[0].integer);
 else
    return(item->def_val.integer);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the string value that applies to a ConfigItem.

  char *ConfigString Returns the string value.

  ConfigItem item The configuration item to check.
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigString(ConfigItem item)
{
 if(!item)
    return(NULL);

 if(item->itemdef->url_type!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->nentries>1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->nentries==1)
    return(item->val[0].string);
 else
    return(item->def_val.string);
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a ConfigItem to find an integer value that applies to the specified URL.

  int ConfigIntegerURL Returns the integer value.

  ConfigItem item The configuration item to check.

  const URL *Url the URL to check for a match against (or NULL to get the match for all URLs).
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigIntegerURL(ConfigItem item,const URL *Url)
{
 int i;

 if(!item)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->url_type!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 for(i=0;i<item->nentries;++i)
   {
     if(!item->url[i] || (Url && MatchUrlSpecification(item->url[i],Url)))
       return(item->val[i].integer);
   }

 return(item->def_val.integer);
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a ConfigItem to find an string value that applies to the specified URL.

  char *ConfigStringURL Returns the string value.

  ConfigItem item The configuration item to check.

  const URL *Url the URL to check for a match against (or NULL to get the match for all URLs).
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigStringURL(ConfigItem item,const URL *Url)
{
 int i;

 if(!item)
    return(NULL);

 if(item->itemdef->url_type!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 for(i=0;i<item->nentries;++i)
   {
    if(!item->url[i])
       return(item->val[i].string);
    else if(Url && MatchUrlSpecification(item->url[i],Url))
       return(item->val[i].string);
   }

 return(item->def_val.string);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified URL is listed in this configuration item.

  int ConfigBooleanMatchURL Return true if it is in the list.

  ConfigItem item The configuration item to match.

  const URL *Url The URL to search the list for.
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigBooleanMatchURL(ConfigItem item,const URL *Url)
{
 int i;

 if(!item)
    return(0);

 if(item->itemdef->url_type!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 for(i=0;i<item->nentries;++i)
    if(MatchUrlSpecification(item->key[i].urlspec,Url))
       return(!item->key[i].urlspec->negated);

 return(0);
}


#ifndef CLIENT_ONLY
/*++++++++++++++++++++++++++++++++++++++
  Return the pattern that matches a line in a header.

  char *ConfigHeaderMatch Returns the string value of the pattern.

  ConfigItem item The configuration item to check.

  const URL *Url the URL to check for a match against (or NULL to get the match for all URLs).

  
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigHeaderMatch(ConfigItem item, const URL *Url, Header *header)
{
 int i;

 if(!item)
    return(NULL);

 if(item->itemdef->url_type!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 for(i=0;i<item->nentries;++i)
   if(!item->url[i] || (Url && MatchUrlSpecification(item->url[i],Url))) {
     char *pattern=item->val[i].string;

     if(pattern && MatchHeader(header,pattern))
       return(pattern);
   }

 return(NULL);
}

#endif
