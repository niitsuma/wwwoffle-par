/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configmisc.c 1.18 2002/08/21 14:28:30 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7e.
  Configuration file data management functions.
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

#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#include "misc.h"
#include "configpriv.h"
#include "errors.h"
#include "wwwoffle.h"


/* Local functions */

static /*@null@*/ char* sprintf_key_or_value(ConfigType type,KeyOrValue key_or_val);
static /*@null@*/ char* sprintf_url_spec(UrlSpec *urlspec);
static /*@null@*/ char *strstrn(const char *phaystack, const char *pneedle, size_t n);
static /*@null@*/ char *strcasestrn(const char *phaystack, const char *pneedle, size_t n);


/*+ The backup version of the config file. +*/
static ConfigItem **BackupConfig;


/*++++++++++++++++++++++++++++++++++++++
  Set the configuration file default values.
  ++++++++++++++++++++++++++++++++++++++*/

void DefaultConfigFile(void)
{
 int s,i;

 for(s=0;s<CurrentConfig.nsections;s++)
   for(i=0;i<CurrentConfig.sections[s]->nitemdefs;i++) {
     ConfigItem *item_p=CurrentConfig.sections[s]->itemdefs[i].item;
     if(CurrentConfig.sections[s]->itemdefs[i].def_val) {

       ConfigItem item=*item_p=(ConfigItem)malloc(sizeof(struct _ConfigItem));

       item->itemdef=&CurrentConfig.sections[s]->itemdefs[i];
       item->nentries=0;
       item->url=NULL;
       item->key=NULL;
       item->val=NULL;

#if CONFIG_VERIFY_ABORT
       {
	 char *errmsg;
	 if(CurrentConfig.sections[s]->itemdefs[i].key_type!=Fixed)
	   PrintMessage(Fatal,"Configuration file error at %s:%d",__FILE__,__LINE__);

	 if((errmsg=ParseKeyOrValue(CurrentConfig.sections[s]->itemdefs[i].def_val,CurrentConfig.sections[s]->itemdefs[i].val_type,&(item->def_val))))
	   PrintMessage(Fatal,"Configuration file error at %s:%d; %s",__FILE__,__LINE__,errmsg);
       }
#else
       ParseKeyOrValue(CurrentConfig.sections[s]->itemdefs[i].def_val,CurrentConfig.sections[s]->itemdefs[i].val_type,&(item->def_val));
#endif
     }
     else
       *item_p=NULL;
   }

#if CONFIG_DEBUG_DUMP
 DumpConfigFile();
#endif
}


/*++++++++++++++++++++++++++++++++++++++
  Save the old values in case the re-read of the file fails.
  ++++++++++++++++++++++++++++++++++++++*/

void CreateBackupConfigFile(void)
{
 int s,i;

 /* Create a backup of all of the sections. */

 BackupConfig=(ConfigItem**)malloc(CurrentConfig.nsections*sizeof(ConfigItem*));

 for(s=0;s<CurrentConfig.nsections;++s)
   {
    BackupConfig[s]=(ConfigItem*)malloc(CurrentConfig.sections[s]->nitemdefs*sizeof(ConfigItem));

    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;++i)
      {
       BackupConfig[s][i]=*CurrentConfig.sections[s]->itemdefs[i].item;
      }
   }

 /* Restore the default values */

 DefaultConfigFile();
}


/*++++++++++++++++++++++++++++++++++++++
  Restore the old values if the re-read of the file failed.
  ++++++++++++++++++++++++++++++++++++++*/

void RestoreBackupConfigFile(void)
{
 int s,i;

 /* Restore all of the sections. */

 for(s=0;s<CurrentConfig.nsections;++s)
   {
    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;++i)
      {
       FreeConfigItem(*CurrentConfig.sections[s]->itemdefs[i].item);

       *CurrentConfig.sections[s]->itemdefs[i].item=BackupConfig[s][i];
      }

    free(BackupConfig[s]);
   }

 free(BackupConfig);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the old values if the re-read of the file succeeded.

  int restore_startup Set to true if the StartUp section is to be restored.
  ++++++++++++++++++++++++++++++++++++++*/

void PurgeBackupConfigFile(int restore_startup)
{
 int s,i;

 /* Purge all of the sections and restore StartUp if needed. */

 for(s=0;s<CurrentConfig.nsections;++s)
   {
    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;++i)
      {
       if(s==0 && restore_startup)
         {
	   FreeConfigItem(*CurrentConfig.sections[s]->itemdefs[i].item);

	   *CurrentConfig.sections[s]->itemdefs[i].item=BackupConfig[s][i];
	 }
       else
	 FreeConfigItem(BackupConfig[s][i]);
      }
    free(BackupConfig[s]);
   }

 free(BackupConfig);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the values in the config file.
  ++++++++++++++++++++++++++++++++++++++*/

void PurgeConfigFile(void)
{
 int s,i;

 /* Purge all of the sections. */

 for(s=0;s<CurrentConfig.nsections;++s)
   {
    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;++i)
      {
       FreeConfigItem(*CurrentConfig.sections[s]->itemdefs[i].item);
      }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Free a ConfigItem list.

  ConfigItem item The item to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeConfigItem(ConfigItem item)
{

 if(!item)
    return;

 if(item->url) {
   int i;

   for(i=0;i<item->nentries;++i)
     FreeUrlSpecification(item->url[i]);

   free(item->url);
 }

 if(item->key) {
   FreeKeysOrValues(item->key,item->itemdef->key_type,item->nentries);
   free(item->key);
 }

 if(item->val) {
   FreeKeysOrValues(item->val,item->itemdef->val_type,item->nentries);
   free(item->val);
 }

 FreeKeysOrValues(&(item->def_val),item->itemdef->val_type,1);

 free(item);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a Key or Value.

  KeyOrValue *keyval The key or value to free.

  ConfigType type The type of key or value.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeKeysOrValues(KeyOrValue *keyval,ConfigType type, int n)
{
 int i;

 switch(type)
   {
    /* None or Fixed */

   case Fixed:
   case None:

    /* Integer */

   case CfgMaxServers:
   case CfgMaxFetchServers:
   case CfgLogLevel:
   case Boolean:
   case PortNumber:
   case AgeDays:
   case TimeSecs:
   case CacheSize:
   case FileSize:
   case Percentage:
   case UserId:
   case GroupId:
   case FileMode:
   case CompressSpec:
    break;

    /* String */

   case String:
   case PathName:
   case FileExt:
   case MIMEType:
   case HostOrNone:
   case Host:
   case HostAndPortOrNone:
   case HostAndPort:
   case UserPass:
   case Url:
   for(i=0;i<n;++i)
     if(keyval[i].string)
       free(keyval[i].string);
    break;

   case UrlSpecification:
   for(i=0;i<n;++i)
     FreeUrlSpecification(keyval[i].urlspec);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Check if a URL matches a URL-SPECIFICATION in the config file.

  int MatchUrlSpecification returns 1 if true else 0.

  UrlSpec *spec The URL-SPECIFICATION.

  URL *Url  The URL to match.
  ++++++++++++++++++++++++++++++++++++++*/

int MatchUrlSpecification(UrlSpec *spec,URL *Url)
{
  return((!spec->proto || !strcmp(UrlSpecProto(spec),Url->proto)) &&
	 (!spec->host || WildcardMatch(Url->host,UrlSpecHost(spec))) &&
	 (spec->port==-1 || (Url->port? Url->portnum==spec->port : spec->port==0)) &&
	 (!spec->path || (spec->nocase? WildcardCaseMatch(Url->path,UrlSpecPath(spec)):WildcardMatch(Url->path,UrlSpecPath(spec)))) &&
	 (!spec->params || (Url->params? (spec->nocase? WildcardCaseMatch(Url->params,UrlSpecParams(spec)):WildcardMatch(Url->params,UrlSpecParams(spec))) : !*UrlSpecParams(spec))) &&
	 (!spec->args || (Url->args? (spec->nocase? WildcardCaseMatch(Url->args,UrlSpecArgs(spec)):WildcardMatch(Url->args,UrlSpecArgs(spec))) : !*UrlSpecArgs(spec))));
}

/*++++++++++++++++++++++++++++++++++++++
  Check if a protocol, host and port match a URL-SPECIFICATION in the config file.

  int MatchUrlSpecificationProtoHostPort returns 1 if true else 0.

  UrlSpec *spec The URL-SPECIFICATION.

  char *proto The protocol.

  char *hostport The host (and port).

  ++++++++++++++++++++++++++++++++++++++*/

int MatchUrlSpecificationProtoHostPort(UrlSpec *spec,char *proto,char *hostport)
{
  char *hoststr,*portstr; int hostlen;

  if((spec->proto && strcmp(UrlSpecProto(spec),proto)) ||
     (spec->path) || (spec->params) || (spec->args))
    return 0;

  SplitHostPort(hostport,&hoststr,&hostlen,&portstr);

  return((!spec->host || WildcardMatchN(hoststr,hostlen,UrlSpecHost(spec))) &&
	 (spec->port==-1 || (portstr? atoi(portstr)==spec->port : spec->port==0)));
}

/*++++++++++++++++++++++++++++++++++++++
  Do a match using a wildcard specified with '*' in it.

  int WildcardMatch returns 1 if there is a match.

  char *string The fixed string that is being matched.

  char *pattern The pattern to match against.
  ++++++++++++++++++++++++++++++++++++++*/

int WildcardMatch(const char *string,const char *pattern)
{
  int len_patt;
  const char *midstr, *endstr;
  const char *pattstr, *starp=strchr(pattern,'*');

  if(!starp) return(!strcmp(string,pattern));

  len_patt=starp-pattern;
  if(strncmp(string,pattern,len_patt)) return 0;
  midstr=string+len_patt;

  while(pattstr=starp+1,starp=strchr(pattstr,'*'))
    {
      const char *match;
      len_patt = starp-pattstr;
      if(!(match = strstrn(midstr,pattstr,len_patt))) return 0;
      midstr=match+len_patt;
    }

  endstr= strchrnul(midstr,0)-strlen(pattstr);
  if(midstr>endstr) return 0;
  return(!strcmp(endstr,pattstr));
}

/*++++++++++++++++++++++++++++++++++++++
  Do a case-insensitive match using a wildcard specified with '*' in it.

  int WildcardCaseMatch returns 1 if there is a match.

  char *string The fixed string that is being matched.

  char *pattern The pattern to match against.
  ++++++++++++++++++++++++++++++++++++++*/

int WildcardCaseMatch(const char *string,const char *pattern)
{
  int len_patt;
  const char *midstr, *endstr;
  const char *pattstr, *starp=strchr(pattern,'*');

  if(!starp) return(!strcasecmp(string,pattern));

  len_patt=starp-pattern;
  if(strncasecmp(string,pattern,len_patt)) return 0;
  midstr=string+len_patt;

  while(pattstr=starp+1,starp=strchr(pattstr,'*'))
    {
      const char *match;
      len_patt = starp-pattstr;
      if(!(match = strcasestrn(midstr,pattstr,len_patt))) return 0;
      midstr=match+len_patt;
    }

  endstr= strchrnul(midstr,0)-strlen(pattstr);
  if(midstr>endstr) return 0;
  return(!strcasecmp(endstr,pattstr));
}


/*++++++++++++++++++++++++++++++++++++++
  Match part of a string against a pattern with '*'s in it.

  int WildcardMatchN returns 1 if there is a match.

  char *string The fixed string that is being matched.
  int stringlen The length of the part of string to match. (This part must not contain a null char)

  char *pattern The pattern to match against.
  ++++++++++++++++++++++++++++++++++++++*/

int WildcardMatchN(const char *string,int stringlen,const char *pattern)
{
  int len_patt;
  const char *midstr, *endstr;
  const char *pattstr, *starp=strchr(pattern,'*');

  if(!starp) return(!strncmp(string,pattern,stringlen) && !pattern[stringlen]);

  len_patt=starp-pattern;
  if(len_patt>stringlen) return 0;
  if(strncmp(string,pattern,len_patt)) return 0;
  midstr=string+len_patt;

  while(pattstr=starp+1,starp=strchr(pattstr,'*'))
    {
      const char *match;
      len_patt = starp-pattstr;
      if(!(match = strstrn(midstr,pattstr,len_patt))) return 0;
      midstr=match+len_patt;
    }

  len_patt=strlen(pattstr);
  endstr= string+stringlen-len_patt;
  if(midstr>endstr) return 0;
  return(!strncmp(endstr,pattstr,len_patt));
}


#if CONFIG_DEBUG_DUMP

/*++++++++++++++++++++++++++++++++++++++
  Remove the old values if the re-read of the file succeeded.
  ++++++++++++++++++++++++++++++++++++++*/

void DumpConfigFile(void)
{
 int s,i,e;

 fprintf(stderr,"CONFIGURATION FILE\n");

 for(s=0;s<CurrentConfig.nsections;s++)
   {
    fprintf(stderr,"  Section %s\n",CurrentConfig.sections[s]->name);

    for(i=0;i<CurrentConfig.sections[s]->nitemdefs;i++)
      {
       if(*CurrentConfig.sections[s]->itemdefs[i].name)
          fprintf(stderr,"    Item %s\n",CurrentConfig.sections[s]->itemdefs[i].name);
       else
          fprintf(stderr,"    Item [default]\n");

       if(*CurrentConfig.sections[s]->itemdefs[i].item)
          for(e=0;e<(*CurrentConfig.sections[s]->itemdefs[i].item)->nentries;e++)
            {
             char *string=ConfigEntryString(*CurrentConfig.sections[s]->itemdefs[i].item,e);
             fprintf(stderr,"      %s\n",string);
             free(string);
            }
      }
   }
}

#endif


/*++++++++++++++++++++++++++++++++++++++
  Return the string that represents the Configuration type.

  char *ConfigTypeString Returns a static string.

  ConfigType type The configuration type.
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigTypeString(ConfigType type)
{
 switch(type)
   {
   case Fixed:
    return "Fixed";              /* key */
   case None:
    return "None";                         /* val */
   case CfgMaxServers:
    return "CfgMaxServers";                /* val */
   case CfgMaxFetchServers:
    return "CfgMaxFetchServers";           /* val */
   case CfgLogLevel:
    return "CfgLogLevel";                  /* val */
   case Boolean:
    return "Boolean";                      /* val */
   case PortNumber:
    return "PortNumber";                   /* val */
   case AgeDays:
    return "AgeDays";                      /* val */
   case TimeSecs:
    return "TimeSecs";                     /* val */
   case CacheSize:
    return "CacheSize";                    /* val */
   case FileSize:
    return "FileSize";                     /* val */
   case Percentage:
    return "Percentage";                   /* val */
   case UserId:
    return "UserId";                       /* val */
   case GroupId:
    return "GroupId";                      /* val */
   case CompressSpec:
     return "CompressionSpecification";    /* var */
   case String:
    return "String";             /* key */ /* val */
   case PathName:
    return "PathName";                     /* val */
   case FileExt:
    return "FileExt";            /* key */ /* val */
   case FileMode:
    return "FileMode";                     /* val */
   case MIMEType:
    return "MIMEType";                     /* val */
   case Host:
    return "Host";               /* key */
   case HostOrNone:
    return "HostOrNone";                   /* val */
   case HostAndPort:
    return "HostAndPort";
   case HostAndPortOrNone:
    return "HostAndPortOrNone";            /* val */
   case UserPass:
    return "UserPass";           /* key */
   case Url:
    return "Url";                          /* val */
   case UrlSpecification:
    return "UrlSpecification";   /* key */ /* val */
   }

 /*@notreached@*/

 return("??Unknown??");
};


/*++++++++++++++++++++++++++++++++++++++
  Convert a Configuration entry into a canonical printable string.

  char *ConfigEntryString Returns a malloced string.

  ConfigItem item The configuration item.

  int which Which particular entry in the ConfigItem to print.
  ++++++++++++++++++++++++++++++++++++++*/

char *ConfigEntryString(ConfigItem item,int which)
{
 char *url=NULL,*key=NULL,*val=NULL;
 char *string;

 /* Get the sub-strings */

 ConfigEntryStrings(item,which,&url,&key,&val);

 /* Create the string */

 string=MakeConfigEntryString(item->itemdef,url,key,val);

 if(url) free(url);
 if(key) free(key);
 if(val) free(val);

 /* Send the results back */

 return(string);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a Configuration entry into a canonical printable string.

  ConfigItem item The configuration item.

  int which Which particular entry in the ConfigItem to print.

  char **url Returns the URL string.

  char **key Returns the key string.

  char **val Returns the value string.
  ++++++++++++++++++++++++++++++++++++++*/

void ConfigEntryStrings(ConfigItem item,int which,char **url,char **key,char **val)
{
 /* Handle the URL */

 if(item->url && item->url[which])
    *url=sprintf_url_spec(item->url[which]);
 else
    *url=NULL;

 /* Handle the key */
 {
   ConfigType key_type=item->itemdef->key_type;
   *key=sprintf_key_or_value(key_type,(key_type==Fixed)?(KeyOrValue)item->itemdef->name:item->key[which]);
 }

 /* Handle the value */

 if(item->itemdef->val_type!=None)
    *val=sprintf_key_or_value(item->itemdef->val_type,item->val[which]);
 else
    *val=NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Make a Configuration entry string from supplied arguments.

  char *MakeConfigEntryString Returns a malloced string.

  ConfigItemDef *itemdef The configuration item definition.

  char *url Specifies the URL string.

  char *key Specifies the key string.

  char *val Specifies the val string.
  ++++++++++++++++++++++++++++++++++++++*/
/* Rewritten by Paul Rombouts */
char *MakeConfigEntryString(ConfigItemDef *itemdef,char *url,char *key,char *val)
{
 int length=0;
 char *string,*p;

 /* compute the length */

 if(url) length+=strlitlen("<> ")+strlen(url);

 if(key) length+=strlen(key);

 if(itemdef->val_type!=None) {
   length+=strlitlen(" = ");

   if(val)
     length+=strlen(val);
 }

 string=p=(char*)malloc(length+1);

 /* Handle the URL */

 if(url) {
   *p++='<';
   p=stpcpy(p,url);
   *p++='>';
   *p++=' ';
 }

 /* Handle the key */

 if(key) p=stpcpy(p,key);

 /* Handle the value */

 if(itemdef->val_type!=None) {
   p=stpcpy(p," = ");

   if(val)
     p=stpcpy(p,val);
 }

 *p=0;

 /* Send the result back */
 return(string);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a KeyOrValue type into a string.

  char* sprintf_key_or_value Return the newly malloced string.

  ConfigType type The type of the KeyOrValue.

  KeyOrValue key_or_val The KeyOrValue.
  ++++++++++++++++++++++++++++++++++++++*/

static char* sprintf_key_or_value(ConfigType type,KeyOrValue key_or_val)
{
 switch(type)
   {
    /* None or Fixed */

   case Fixed:
     return strdup(key_or_val.string);

   case None:
     return NULL;

    /* Integer */

   case Boolean:
     return strdup(key_or_val.integer?"yes":"no");

   case CfgMaxServers:
   case CfgMaxFetchServers:
   case PortNumber:
   case CacheSize:
   case FileSize:
   case Percentage:
     return x_asprintf("%d",key_or_val.integer);

   case CfgLogLevel:
     return strdup((key_or_val.integer==Debug)?     "debug":
		   (key_or_val.integer==Inform)?    "info":
		   (key_or_val.integer==Important)? "important":
		   (key_or_val.integer==Warning)?   "warning":
		   (key_or_val.integer==Fatal)?     "fatal":
		                                    "unknown");

   case UserId:
    {
     struct passwd *pwd=getpwuid(key_or_val.integer);
     return pwd? strdup(pwd->pw_name) : x_asprintf("%d",key_or_val.integer);
    }

   case GroupId:
    {
     struct group *grp=getgrgid(key_or_val.integer);
     return grp? strdup(grp->gr_name) : x_asprintf("%d",key_or_val.integer);
    }

   case FileMode:
     return x_asprintf("0%o",key_or_val.integer);

   case AgeDays:
    {
     int days=key_or_val.integer;

     return (days==0)?     strdup("0"):
            (days%365==0)? x_asprintf("%dy",days/365):
            (days%30==0)?  x_asprintf("%dm",days/30):
            (days%7==0)?   x_asprintf("%dw",days/7):
                           x_asprintf("%d",days);
    }

   case TimeSecs:
    {
     int seconds=key_or_val.integer;

     return (seconds==0)?             strdup("0"):
            (seconds%(3600*24*7)==0)? x_asprintf("%dw",seconds/(3600*24*7)):
            (seconds%(3600*24)==0)?   x_asprintf("%dd",seconds/(3600*24)):
            (seconds%(3600)==0)?      x_asprintf("%dh",seconds/(3600)):
            (seconds%(60)==0)?        x_asprintf("%dm",seconds/(60)):
                                      x_asprintf("%d",seconds);
    }

   case CompressSpec:
     return strdup((key_or_val.integer==0 ) ? "no":
		   (key_or_val.integer==1 ) ? "deflate":
		   (key_or_val.integer==2 ) ? "gzip":
		                              "yes");

    /* String */

   case String:
   case PathName:
   case FileExt:
   case MIMEType:
   case HostOrNone:
   case Host:
   case HostAndPortOrNone:
   case HostAndPort:
   case UserPass:
   case Url:
     return strdup(key_or_val.string? key_or_val.string : "");

    /* Url Specification */

   case UrlSpecification:
     return sprintf_url_spec(key_or_val.urlspec);
   }

 return NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a URL-SPECIFICATION into a string.

  char* sprintf_url_spec Return the new string.

  UrlSpec urlspec The URL-SPECIFICATION to convert.
  ++++++++++++++++++++++++++++++++++++++*/

static char* sprintf_url_spec(UrlSpec *urlspec)
{
 int length=0;
 char *string,*p;
 char portstr[12];

 if(!urlspec) return(NULL);

 /* First compute the length */

 if(urlspec->negated) length+=strlitlen("!");
 if(urlspec->nocase) length+=strlitlen("~");

 length+= (urlspec->proto ? strlen(UrlSpecProto(urlspec)) : strlitlen("*")) + strlitlen("://");

 length+= (urlspec->host ? strlen(UrlSpecHost(urlspec)) : strlitlen("*"));;

 if(urlspec->port!=-1) {
   length+=strlitlen(":");
   if(urlspec->port!=0)
     length+= sprintf(portstr,"%d",urlspec->port);
 }

 if(urlspec->path) {
   if(*UrlSpecPath(urlspec)=='*') length+=strlitlen("/");
   length+= strlen(UrlSpecPath(urlspec));
 }
 else
   length+=strlitlen("/*");

 if(urlspec->args)
   length+= strlitlen("?") + strlen(UrlSpecArgs(urlspec));

 /* Now construct the actual string */

 string=p=(char*)malloc(length+1);

 if(urlspec->negated) *p++='!';
 if(urlspec->nocase) *p++='~';

 p=stpcpy(stpcpy(p,urlspec->proto?UrlSpecProto(urlspec):"*"),"://");

 p=stpcpy(p,urlspec->host?UrlSpecHost(urlspec):"*");

 if(urlspec->port!=-1) {
   *p++=':';
   if(urlspec->port!=0)
     p=stpcpy(p,portstr);
 }

 if(urlspec->path) {
   if(*UrlSpecPath(urlspec)=='*') *p++='/';
   p=stpcpy(p,UrlSpecPath(urlspec));
 }
 else
   p=stpcpy(p,"/*");

 if(urlspec->args) {
   *p++='?';
   p=stpcpy(p,UrlSpecArgs(urlspec));
 }

 return(string);
}


/* Return the offset of one string within another.
   Copyright (C) 1994, 1996, 1997, 2000 Free Software Foundation, Inc.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, write to the Free
   Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
   02111-1307 USA.  */

/*
 * My personal strstr() implementation that beats most other algorithms.
 * Until someone tells me otherwise, I assume that this is the
 * fastest implementation of strstr() in C.
 * I deliberately chose not to comment it.  You should have at least
 * as much fun trying to understand it, as I had to write it :-).
 *
 * Stephen R. van den Berg, berg@pool.informatik.rwth-aachen.de	*/

/* strstrn() is a variation of strstr() that only tries to find
   the first n characters of "needle" in "haystack".
   If strlen(needle) happens to be less than n, strstrn() behaves 
   exactly like strstr().
   Modifications made by Paul Rombouts <p.a.rombouts@home.nl>.
*/

typedef unsigned chartype;

static char *strstrn(const char *phaystack, const char *pneedle, size_t n)
{
  register const unsigned char *haystack, *needle;
  register chartype b, c;
  const unsigned char *needle_end;

  haystack = (const unsigned char *) phaystack;
  needle = (const unsigned char *) pneedle;
  needle_end = needle+n;

  if (needle != needle_end && (b = *needle) != '\0' )
    {
      haystack--;				/* possible ANSI violation */
      do
	{
	  c = *++haystack;
	  if (c == '\0')
	    goto ret0;
	}
      while (c != b);

      if (++needle == needle_end || (c = *needle) == '\0')
	goto foundneedle;
      ++needle;
      goto jin;

      for (;;)
        {
          register chartype a;
	  register const unsigned char *rhaystack, *rneedle;

	  do
	    {
	      a = *++haystack;
	      if (a == '\0')
		goto ret0;
	      if (a == b)
		break;
	      a = *++haystack;
	      if (a == '\0')
		goto ret0;
shloop:
	      ;
	    }
          while (a != b);

jin:	  a = *++haystack;
	  if (a == '\0')
	    goto ret0;

	  if (a != c)
	    goto shloop;

	  rhaystack = haystack-- + 1;
	  if(needle == needle_end) goto foundneedle;
	  rneedle = needle;
	  a = *rneedle;

	  if (*rhaystack == a)
	    do
	      {
		if (a == '\0')
		  goto foundneedle;
		++rhaystack;
		if(++needle == needle_end) goto foundneedle;
		a = *needle;
		if (*rhaystack != a)
		  break;
		if (a == '\0')
		  goto foundneedle;
		++rhaystack;
		if(++needle == needle_end) goto foundneedle;
		a = *needle;
	      }
	    while (*rhaystack == a);

	  needle = rneedle;		/* took the register-poor approach */

	  if (a == '\0')
	    break;
        }
    }
foundneedle:
  return (char*) haystack;
ret0:
  return 0;
}


static char *strcasestrn(const char *phaystack, const char *pneedle, size_t n)
{
  register const unsigned char *haystack, *needle;
  register chartype b, c;
  const unsigned char *needle_end;

  haystack = (const unsigned char *) phaystack;
  needle = (const unsigned char *) pneedle;
  needle_end = needle+n;

  if (needle != needle_end && (b = tolower(*needle)) != '\0' )
    {
      haystack--;				/* possible ANSI violation */
      do
	{
	  c = *++haystack;
	  if (c == '\0')
	    goto ret0;
	}
      while (tolower(c) != (int) b);

      if (++needle == needle_end || (c = tolower(*needle)) == '\0')
	goto foundneedle;
      ++needle;
      goto jin;

      for (;;)
        {
          register chartype a;
	  register const unsigned char *rhaystack, *rneedle;

	  do
	    {
	      a = *++haystack;
	      if (a == '\0')
		goto ret0;
	      if (tolower(a) == (int) b)
		break;
	      a = *++haystack;
	      if (a == '\0')
		goto ret0;
shloop:
	      ;
	    }
          while (tolower(a) != (int) b);

jin:	  a = *++haystack;
	  if (a == '\0')
	    goto ret0;

	  if (tolower(a) != (int) c)
	    goto shloop;

	  rhaystack = haystack-- + 1;
	  if(needle == needle_end) goto foundneedle;
	  rneedle = needle;
	  a = tolower(*rneedle);

	  if (tolower(*rhaystack) == (int) a)
	    do
	      {
		if (a == '\0')
		  goto foundneedle;
		++rhaystack;
		if(++needle == needle_end) goto foundneedle;
		a = tolower(*needle);
		if (tolower(*rhaystack) != (int) a)
		  break;
		if (a == '\0')
		  goto foundneedle;
		++rhaystack;
		if(++needle == needle_end) goto foundneedle;
		a = tolower(*needle);
	      }
	    while (tolower(*rhaystack) == (int) a);

	  needle = rneedle;		/* took the register-poor approach */

	  if (a == '\0')
	    break;
        }
    }
foundneedle:
  return (char*) haystack;
ret0:
  return 0;
}
