/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configfunc.c 1.17 2004/01/17 16:23:46 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Configuration item checking functions.
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

#include <pwd.h>
#include <sys/types.h>
#include <unistd.h>

#include "misc.h"
#include "errors.h"
#include "configpriv.h"
#include "config.h"
#include "sockets.h"


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
  Decide if the specified port number is allowed for SSL.

  int IsSSLAllowedPort Returns true if it is allowed.

  int port  The port number to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsSSLAllowedPort(int port)
{
 if(SSLAllowPort) {
   int i;
   for(i=0;i<SSLAllowPort->nentries;++i)
     if(SSLAllowPort->val[i].integer==port)
       return 1;
 }

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the specified path is allowed to be used for CGIs.

  int IsCGIAllowed Returns true if it is allowed.

  char *path The pathname to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsCGIAllowed(char *path)
{
 int i;

 if(ExecCGI)
    for(i=0;i<ExecCGI->nentries;++i)
       if(WildcardMatch(path,ExecCGI->val[i].string))
         return 1;

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Get the first specified name of the server host.

  char *GetLocalHost Returns the first named localhost.

  int port If true then return the port as well.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetLocalHost(int port)
{
 char *localhost,*ret,*p,*colon=NULL,portstr[12];
 int lenhost,lenport=0,lenret;

 if(LocalHost && LocalHost->nentries)
    localhost=LocalHost->key[0].string;
 else
    localhost=DEF_LOCALHOST;

 lenret=lenhost=strlen(localhost);
 if(port) {
   lenport=sprintf(portstr,"%d",ConfigInteger(HTTP_Port));
   lenret+=1+lenport;
   colon=strchr(localhost,':');
   if(colon) lenret+=2;
 }

 ret=p=(char*)malloc(lenret+1);

 if(colon) *p++='[';
 p=mempcpy(p,localhost,lenhost);
 if(colon) *p++=']';
 if(port) {*p++=':'; p=mempcpy(p,portstr,lenport);}
 *p=0;

 return(ret);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is the localhost.

  int IsLocalHostPort Return true if the host is the local host, and the port is our http port.

  char *hostport The name of the host (and port number) to be checked.

  ++++++++++++++++++++++++++++++++++++++*/

int IsLocalHostPort(char *hostport)
{
 char *hoststr,*portstr;
 int hostlen;

 SplitHostPort(hostport,&hoststr,&hostlen,&portstr);

 if(LocalHost) {
   int i;
   for(i=0;i<LocalHost->nentries;++i) {
     char *localhost=LocalHost->key[i].string;
     if(!strncasecmp(localhost,hoststr,hostlen) && !localhost[hostlen])
       return(portstr?atoi(portstr)==ConfigInteger(HTTP_Port):
	              ConfigInteger(HTTP_Port)==80);
   }
 }

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is in the local network.

  int IsLocalNetHost Return true if the host is on the local network.

  char *host The name of the host (without port number) to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

int IsLocalNetHost(char *host)
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
  Check if the specified hostname is in the local network.

  int IsLocalNetHostPort Return true if the host is on the local network.

  char *hostport The name of the host (and port number) to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

int IsLocalNetHostPort(char *hostport)
{
 char *hoststr,*portstr; int hostlen;

 SplitHostPort(hostport,&hoststr,&hostlen,&portstr);

 if(LocalHost) {
   int i;
   for(i=0;i<LocalHost->nentries;++i) {
     char *localhost=LocalHost->key[i].string;
     if(!strncmp(localhost,hoststr,hostlen) && !localhost[hostlen])
       return 1;
   }
 }

 if(LocalNet) {
   int i;
   for(i=0;i<LocalNet->nentries;++i) {
     char *localnet=LocalNet->key[i].string;
     if(*localnet=='!') {
       if(WildcardMatchN(hoststr,hostlen,localnet+1))
	 return 0;
     }
     else {
       if(WildcardMatchN(hoststr,hostlen,localnet))
	 return 1;
     }
   }
 }

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is allowed to connect.

  int IsAllowedConnectHost Return true if it is allowed to connect.

  char *host The name of the host (without port number) to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

int IsAllowedConnectHost(char *host)
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
     char * allowedhost=AllowedConnectHosts->key[i].string;
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

  char *userpass The encoded username and password of the user to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

char *IsAllowedConnectUser(char *userpass)
{
 char *isit;

 if(AllowedConnectUsers)
   isit=NULL;
 else
   isit="anybody";

 if(userpass)
   {
     char *up=userpass;

     while(*up!=' ') up++;
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
       int l;
       isit=Base64Decode(up,&l);
       colon=strchrnul(isit,':');
       *colon=0;
     }
   }

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the data for a URL can be compressed or not based on the MIME type or file extension.

  int NotCompressed Return 1 if the data is not to be compressed.

  char *mime_type The MIME type of the data (may be NULL).

  char *path The path of the URL (may be NULL).
  ++++++++++++++++++++++++++++++++++++++*/

int NotCompressed(char *mime_type,char *path)
{

  if(mime_type && DontCompressMIME) {
    int i;
    for(i=0;i<DontCompressMIME->nentries;++i)
      if(!strcmp(DontCompressMIME->val[i].string,mime_type))
	return 1;
  }

  if(path && DontCompressExt) {
    int i;
    for(i=0;i<DontCompressExt->nentries;++i) {
      char *ext = DontCompressExt->val[i].string;
      int strlen_diff=strlen(path)-strlen(ext);
      if(strlen_diff>0 && !strcmp(ext,path+strlen_diff))
	return 1;
    }
  }

  return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the header line is to be sent to the server/client.

  char *CensoredHeader Returns the value to be inserted or NULL if it is to be removed.

  URL *Url the URL that the request or reply is for/from.

  char *key The key to check.

  char *val The default value to use.
  ++++++++++++++++++++++++++++++++++++++*/

char *CensoredHeader(ConfigItem confitem,URL *Url,char *key,char *val)
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
	   else if(strcmp(confitem->val[i].string,val))
	     new=strdup(confitem->val[i].string);

	   break;
	 }
 }

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide what mime type to apply for a given file.

  char *WhatMIMEType Returns the MIME Type.

  char *path The path of the file.
  ++++++++++++++++++++++++++++++++++++++*/

char *WhatMIMEType(char *path)
{
  char *mimetype=NULL;

  if(MIMETypes) {
    int plen=strlen(path);
    int maxlen=0;
    int i;

    for(i=0;i<MIMETypes->nentries;++i) {
      char *mtype_key= MIMETypes->key[i].string;
      int keylen= strlen(mtype_key);

      if(plen>keylen && keylen>maxlen && !strcmp(mtype_key,path+plen-keylen))
	{mimetype=MIMETypes->val[i].string; maxlen=keylen;}
    }
  }

  return mimetype?mimetype:ConfigString(DefaultMIMEType);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if a specified protocol and host is aliased to another host.

  int IsAliased Returns non-zero if this is host is aliased to another host.

  char *proto The protocol to check.

  char *hostport The hostname to check.

  char *path The pathname to check.

  char **new_proto The protocol of the alias.

  char **new_hostport The hostname of the alias.

  char **new_path The pathname of the alias.
  ++++++++++++++++++++++++++++++++++++++*/

int IsAliased(char *proto,char *hostport,char *path,char **new_proto,char **new_hostport,char **new_path)
{
 int i;

 *new_proto=*new_hostport=*new_path=NULL;

 if(Aliases)
    for(i=0;i<Aliases->nentries;++i)
      {
       UrlSpec *alias_key_spec=Aliases->key[i].urlspec;
       char *fake_path=alias_key_spec->path?UrlSpecPath(alias_key_spec):"";

       if(MatchUrlSpecificationProtoHostPort(alias_key_spec,proto,hostport) &&
          !strcmp_beg(path,fake_path))
         {
	  UrlSpec *alias_val_spec=Aliases->val[i].urlspec;

          if(alias_val_spec->proto)
            {
             *new_proto=strdup(UrlSpecProto(alias_val_spec));
            }
          else
            {
             *new_proto=strdup(proto);
            }

          if(alias_val_spec->host)
            {
             if(alias_val_spec->port>0)
	       *new_hostport=x_asprintf("%s:%d",UrlSpecHost(alias_val_spec),alias_val_spec->port);
	     else
	       *new_hostport=strdup(UrlSpecHost(alias_val_spec));
            }
          else
            {
             *new_hostport=strdup(hostport);
            }

          if(alias_val_spec->path)
            {
             int oldlen=strlen(fake_path);
             int newlen=strlen(UrlSpecPath(alias_val_spec));
	     char *p= (char*)malloc(strlen(path)-oldlen+newlen+1);

             *new_path=p;
             p=stpcpy(p,UrlSpecPath(alias_val_spec));
             stpcpy(p,path+oldlen);
            }
          else
            {
             *new_path=strdup(path);
            }

	  return 1;
         }
     }

 return 0;
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

  URL *Url the URL to check for a match against (or NULL to get the match for all URLs).
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigIntegerURL(ConfigItem item,URL *Url)
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
    if(!item->url[i])
       return(item->val[i].integer);
    else if(Url && MatchUrlSpecification(item->url[i],Url))
       return(item->val[i].integer);
   }

 return(item->def_val.integer);
}

/*++++++++++++++++++++++++++++++++++++++
  Search through a ConfigItem to find an integer value that applies to the specified protocol, host (and port).

  int ConfigIntegerURL Returns the integer value.

  ConfigItem item The configuration item to check.

  char *proto  The protocol to match.
  char *hostport  The host (and port number) to match.
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigIntegerProtoHostPort(ConfigItem item,char *proto,char *hostport)
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
    if(!item->url[i])
       return(item->val[i].integer);
    else if(MatchUrlSpecificationProtoHostPort(item->url[i],proto,hostport))
       return(item->val[i].integer);
   }

 return(item->def_val.integer);
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a ConfigItem to find an string value that applies to the specified URL.

  char *ConfigStringURL Returns the string value.

  ConfigItem item The configuration item to check.

  URL *Url the URL to check for a match against (or NULL to get the match for all URLs).
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigStringURL(ConfigItem item,URL *Url)
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
  Search through a ConfigItem to find an string value that applies to the specified protocol, host (and port).

  char *ConfigStringProtoHostPort Returns the string value.

  ConfigItem item The configuration item to check.

  char *proto  The protocol to match.
  char *hostport  The host (and port number) to match.
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigStringProtoHostPort(ConfigItem item,char *proto,char *hostport)
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
    else if(MatchUrlSpecificationProtoHostPort(item->url[i],proto,hostport))
       return(item->val[i].string);
   }

 return(item->def_val.string);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified URL is listed in this configuration item.

  int ConfigBooleanMatchURL Return true if it is in the list.

  ConfigItem item The configuration item to match.

  URL *Url The URL to search the list for.
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigBooleanMatchURL(ConfigItem item,URL *Url)
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

/*++++++++++++++++++++++++++++++++++++++
  Check if the specified protocol and host matches a URL-SPEC listed in this configuration item.

  int ConfigBooleanMatchProtoHostPort Return true if it is in the list.

  char *proto  The protocol to match.
  char *hostport  The host (and port number) to match.
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigBooleanMatchProtoHostPort(ConfigItem item,char *proto,char *hostport)
{
 int i;

 if(!item)
    return(0);

 if(item->itemdef->url_type!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 for(i=0;i<item->nentries;++i)
    if(MatchUrlSpecificationProtoHostPort(item->key[i].urlspec,proto,hostport))
       return(!item->key[i].urlspec->negated);

 return(0);
}
