/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configfunc.c 1.13 2002/08/21 14:28:30 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7b.
  Configuration item checking functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <pwd.h>
#include <unistd.h>
#include <sys/types.h>

#include "wwwoffle.h"
#include "misc.h"
#include "configpriv.h"
#include "config.h"
#include "proto.h"
#include "sockets.h"
#include "errors.h"


/*++++++++++++++++++++++++++++++++++++++
  Determine an email address to use as the FTP password.

  char *DefaultFTPPassword Returns a best-guess password.
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

  int IsSSLAllowedPort Returns true if it is allowed.

  char *host The hostname and port number to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsSSLAllowedPort(char *host)
{
 char *hoststr,*portstr;
 int port=0,isit=0;
 int i;

 SplitHostPort(host,&hoststr,&portstr);

 if(portstr)
    port=atoi(portstr);

 RejoinHostPort(host,hoststr,portstr);

 if(!portstr)
    return(0);

 if(SSLAllowPort)
    for(i=0;i<SSLAllowPort->nentries;i++)
       if(SSLAllowPort->val[i].integer==port)
         {isit=1;break;}

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the specified path is allowed to be used for CGIs.

  int IsCGIAllowed Returns true if it is allowed.

  char *path The pathname to check.
  ++++++++++++++++++++++++++++++++++++++*/

int IsCGIAllowed(char *path)
{
 int isit=0;
 int i;

 if(ExecCGI)
    for(i=0;i<ExecCGI->nentries;i++)
       if(WildcardMatch(path,ExecCGI->val[i].string,0))
         {isit=1;break;}

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Get the first specified name of the server host.

  char *GetLocalHost Returns the first named localhost.

  int port If true then return the port as well.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetLocalHost(int port)
{
 char *localhost,*ret;

 if(LocalHost && LocalHost->nentries)
    localhost=LocalHost->key[0].string;
 else
    localhost=DEF_LOCALHOST;

 ret=(char*)malloc(strlen(localhost)+12);

 if(port)
   {
    if(strchr(localhost,':'))
       sprintf(ret,"[%s]:%d",localhost,ConfigInteger(HTTP_Port));
    else
       sprintf(ret,"%s:%d",localhost,ConfigInteger(HTTP_Port));
   }
 else
    strcpy(ret,localhost);

 return(ret);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is the localhost.

  int IsLocalHost Return true if the host is the local host.

  char *host The name of the host (and port number) to be checked.

  int port If true then check the port number as well.
  ++++++++++++++++++++++++++++++++++++++*/

int IsLocalHost(char *host,int port)
{
 char *hoststr,*portstr;
 int isit=0;
 int i;

 SplitHostPort(host,&hoststr,&portstr);

 if(LocalHost)
    for(i=0;i<LocalHost->nentries;i++)
       if(!strcmp(LocalHost->key[i].string,hoststr))
         {isit=1;break;}

 RejoinHostPort(host,hoststr,portstr);

 if(isit && port)
   {
    if((portstr && atoi(portstr)==ConfigInteger(HTTP_Port)) ||
       (!portstr && ConfigInteger(HTTP_Port)==80))
       ;
    else
       isit=0;
   }

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is in the local network.

  int IsLocalNetHost Return true if the host is on the local network.

  char *host The name of the host (and port number) to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

int IsLocalNetHost(char *host)
{
 char *hoststr,*portstr;
 int isit=0;
 int i;

 SplitHostPort(host,&hoststr,&portstr);

 if(LocalHost)
    for(i=0;i<LocalHost->nentries;i++)
       if(!strcmp(LocalHost->key[i].string,hoststr))
         {isit=1;break;}

 if(LocalNet && !isit)
   {
    for(i=0;i<LocalNet->nentries;i++)
       if(*LocalNet->key[i].string=='!')
         {
          if(WildcardMatch(hoststr,LocalNet->key[i].string+1,0))
            {isit=0;break;}
         }
       else
         {
          if(WildcardMatch(hoststr,LocalNet->key[i].string,0))
            {isit=1;break;}
         }
   }

 RejoinHostPort(host,hoststr,portstr);

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified hostname is allowed to connect.

  int IsAllowedConnectHost Return true if it is allowed to connect.

  char *host The name of the host to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

int IsAllowedConnectHost(char *host)
{
 char *hoststr,*portstr;
 int isit=0;
 int i;

 SplitHostPort(host,&hoststr,&portstr);

 if(LocalHost)
    for(i=0;i<LocalHost->nentries;i++)
       if(!strcmp(LocalHost->key[i].string,hoststr))
         {isit=1;break;}

 if(AllowedConnectHosts && !isit)
   {
    for(i=0;i<AllowedConnectHosts->nentries;i++)
       if(*AllowedConnectHosts->key[i].string=='!')
         {
          if(WildcardMatch(hoststr,AllowedConnectHosts->key[i].string+1,0))
            {isit=0;break;}
         }
       else
         {
          if(WildcardMatch(hoststr,AllowedConnectHosts->key[i].string,0))
            {isit=1;break;}
         }
   }

 RejoinHostPort(host,hoststr,portstr);

 return(isit);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified username and password is allowed to connect.

  char *IsAllowedConnectUser Return the username if it is allowed to connect.

  char *userpass The encoded username and password of the user to be checked.
  ++++++++++++++++++++++++++++++++++++++*/

char *IsAllowedConnectUser(char *userpass)
{
 char *isit;
 int i;

 if(AllowedConnectUsers)
    isit=NULL;
 else
    isit="anybody";

 if(AllowedConnectUsers && userpass)
   {
    char *up=userpass;

    while(*up!=' ') up++;
    while(*up==' ') up++;

    for(i=0;i<AllowedConnectUsers->nentries;i++)
       if(!strcmp(AllowedConnectUsers->key[i].string,up))
         {
          char *colon;
          int l;
          isit=Base64Decode(AllowedConnectUsers->key[i].string,&l);
          if((colon=strchr(isit,':')))
             *colon=0;
          break;
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
 int retval=0;
 int i;

 if(mime_type && DontCompressMIME)
    for(i=0;i<DontCompressMIME->nentries;i++)
       if(!strcmp(DontCompressMIME->val[i].string,mime_type))
         {retval=1;break;}

 if(path && DontCompressExt)
    for(i=0;i<DontCompressExt->nentries;i++)
       if(strlen(path)>strlen(DontCompressExt->val[i].string) &&
          !strcmp(DontCompressExt->val[i].string,path+strlen(path)-strlen(DontCompressExt->val[i].string)))
         {retval=1;break;}

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if the header line is to be sent to the server/browser.

  char *CensoredHeader Returns the value to be inserted or NULL if it is to be removed.

  URL *Url the URL that the request or reply is for/from.

  char *key The key to check.

  char *val The default value to use.
  ++++++++++++++++++++++++++++++++++++++*/

char *CensoredHeader(URL *Url,char *key,char *val)
{
 char *new=val;
 int i;

 if(CensorHeader)
    for(i=0;i<CensorHeader->nentries;i++)
       if(!strcasecmp(CensorHeader->key[i].string,key))
          if(!CensorHeader->url[i] || MatchUrlSpecification(CensorHeader->url[i],Url->proto,Url->host,Url->path,Url->args))
            {
             if(!CensorHeader->val[i].string)
                new=NULL;
             else if(!strcmp(CensorHeader->val[i].string,"yes"))
                new=NULL;
             else if(!strcmp(CensorHeader->val[i].string,"no"))
                ;
             else if(strcmp(CensorHeader->val[i].string,val))
               {
                new=(char*)malloc(strlen(CensorHeader->val[i].string)+1);
                strcpy(new,CensorHeader->val[i].string);
               }
             break;
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
 char *mimetype=ConfigString(DefaultMIMEType);
 int maxlen=0;
 int i;

 if(MIMETypes)
    for(i=0;i<MIMETypes->nentries;i++)
       if(strlen(path)>strlen(MIMETypes->key[i].string) &&
          strlen(MIMETypes->key[i].string)>maxlen &&
          !strcmp(MIMETypes->key[i].string,path+strlen(path)-strlen(MIMETypes->key[i].string)))
         {mimetype=MIMETypes->val[i].string;maxlen=strlen(mimetype);}

 return(mimetype);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if a specified protocol and host is aliased to another host.

  int IsAliased Returns non-zero if this is host is aliased to another host.

  char *proto The protocol to check.

  char *host The hostname to check.

  char *path The pathname to check.

  char **new_proto The protocol of the alias.

  char **new_host The hostname of the alias.

  char **new_path The pathname of the alias.
  ++++++++++++++++++++++++++++++++++++++*/

int IsAliased(char *proto,char *host,char *path,char **new_proto,char **new_host,char **new_path)
{
 int i;

 *new_proto=*new_host=*new_path=NULL;

 if(Aliases)
    for(i=0;i<Aliases->nentries;i++)
      {
       char *fake_path=Aliases->key[i].urlspec->path?UrlSpecPath(Aliases->key[i].urlspec):"";

       if(MatchUrlSpecification(Aliases->key[i].urlspec,proto,host,fake_path,NULL) &&
          !strncmp(fake_path,path,strlen(fake_path)))
         {
          if(Aliases->val[i].urlspec->proto)
            {
             *new_proto=(char*)malloc(strlen(UrlSpecProto(Aliases->val[i].urlspec))+1);
             strcpy(*new_proto,UrlSpecProto(Aliases->val[i].urlspec));
            }
          else
            {
             *new_proto=(char*)malloc(strlen(proto)+1);
             strcpy(*new_proto,proto);
            }

          if(Aliases->val[i].urlspec->host)
            {
             *new_host=(char*)malloc(strlen(UrlSpecHost(Aliases->val[i].urlspec))+8);
             strcpy(*new_host,UrlSpecHost(Aliases->val[i].urlspec));
             if(Aliases->val[i].urlspec->port>0)
                sprintf((*new_host)+strlen(*new_host),":%d",Aliases->val[i].urlspec->port);
            }
          else
            {
             *new_host=(char*)malloc(strlen(host)+1);
             strcpy(*new_host,host);
            }

          if(Aliases->val[i].urlspec->path)
            {
             int oldlen=strlen(fake_path);
             int newlen=strlen(UrlSpecPath(Aliases->val[i].urlspec));

             *new_path=(char*)malloc(newlen-oldlen+strlen(path)+1);
             if(newlen)
               {
                strcpy(*new_path,UrlSpecPath(Aliases->val[i].urlspec));
                if(newlen>1 && (*new_path)[newlen-1]=='/')
                   (*new_path)[newlen-1]=0;
               }
             else
                (*new_path)[0]=0;
             strcat(*new_path,path+oldlen);
            }
          else
            {
             *new_path=(char*)malloc(strlen(path)+1);
             strcpy(*new_path,path);
            }
         }
     }

 return(!!*new_proto);
}


/*++++++++++++++++++++++++++++++++++++++
  Return the integer value that applies to a ConfigItem.

  int ConfigInteger Returns the integer value.

  ConfigItem item The configuration item to check.
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigInteger(ConfigItem item)
{
#if CONFIG_VERIFY_ABORT
 if(!item)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->url_type!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->nentries>1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);
#endif

 if(item->nentries==1)
    return(item->val[0].integer);
 else
    return(item->def_val->integer);
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

#if CONFIG_VERIFY_ABORT
 if(item->itemdef->url_type!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->nentries>1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);
#endif

 if(item->nentries==1)
    return(item->val[0].string);
 else
    return(item->def_val->string);
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

#if CONFIG_VERIFY_ABORT
 if(!item)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->url_type!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);
#endif

 for(i=0;i<item->nentries;i++)
   {
    if(!item->url[i])
       return(item->val[i].integer);
    else if(Url && MatchUrlSpecification(item->url[i],Url->proto,Url->host,Url->path,Url->args))
       return(item->val[i].integer);
   }

 return(item->def_val->integer);
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

#if CONFIG_VERIFY_ABORT
 if(item->itemdef->url_type!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);
#endif

 for(i=0;i<item->nentries;i++)
   {
    if(!item->url[i])
       return(item->val[i].string);
    else if(Url && MatchUrlSpecification(item->url[i],Url->proto,Url->host,Url->path,Url->args))
       return(item->val[i].string);
   }

 if(item->def_val)
    return(item->def_val->string);
 else
    return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified URL is listed in this configuration item.

  int ConfigBooleanMatchURL Return true if it is in the list.

  URL *Url The URL to search the list for.
  ++++++++++++++++++++++++++++++++++++++*/

int ConfigBooleanMatchURL(ConfigItem item,URL *Url)
{
 int i;

 if(!item)
    return(0);

#if CONFIG_VERIFY_ABORT
 if(item->itemdef->url_type!=0)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

 if(item->itemdef->same_key!=1)
    PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);
#endif

 for(i=0;i<item->nentries;i++)
    if(MatchUrlSpecification(item->key[i].urlspec,Url->proto,Url->host,Url->path,Url->args))
       return(!item->key[i].urlspec->negated);

 return(0);
}
