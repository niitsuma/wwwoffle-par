/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configedit.c 1.22 2002/08/04 10:27:13 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7e.
  The HTML interactive configuration editing pages.
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

#include <unistd.h>

#include "wwwoffle.h"
#include "misc.h"
#include "configpriv.h"
#include "config.h"
#include "errors.h"


static void ConfigurationIndexPage(int fd,/*@null@*/ char *url);
static void ConfigurationSectionPage(int fd,int section,/*@null@*/ char *url);
static void ConfigurationItemPage(int fd,int section,int item,/*@null@*/ char *url,/*@null@*/ Body *request_body);
static void ConfigurationURLPage(int fd,/*@null@*/ char *request_args);
static void ConfigurationAuthFail(int fd,char *url);

/*+ The list of configuration items to list on the configuration page for a URL. +*/
static ConfigItem *URLConfigItems[]={

 /* OnlineOptions */

 &RequestChanged,
 &RequestChangedOnce,
 &RequestExpired,
 &RequestNoCache,
 &RequestRedirection,
 &TryWithoutPassword,
 &IntrDownloadKeep,
 &IntrDownloadSize,
 &IntrDownloadPercent,
 &TimeoutDownloadKeep,
 &RequestCompressedData,

 /* OfflineOptions */

 &PragmaNoCache,
 &CacheControlNoCache,
 &ConfirmRequests,
 &DontRequestOffline,

 /* FetchOptions */

 &FetchStyleSheets,
 &FetchImages,
 &FetchWebbugImages,
 &FetchFrames,
 &FetchScripts,
 &FetchObjects,

 /* IndexOptions */

 &IndexListOutgoing,
 &IndexListLatest,
 &IndexListMonitor,
 &IndexListHost,
 &IndexListAny,

 /* ModifyHtml */

 &EnableHTMLModifications,
 &EnableModificationsOnline,
 &AddCacheInfo,
 &AnchorModifyBegin[0],
 &AnchorModifyBegin[1],
 &AnchorModifyBegin[2],
 &AnchorModifyEnd[0],
 &AnchorModifyEnd[1],
 &AnchorModifyEnd[2],
 &DisableHTMLScript,
 &DisableHTMLApplet,
 &DisableHTMLStyle,
 &DisableHTMLBlink,
 &DisableHTMLFlash,
 &DisableHTMLMetaRefresh,
 &DisableHTMLMetaRefreshSelf,
 &DisableHTMLDontGetAnchors,
 &DisableHTMLDontGetIFrames,
 &ReplaceHTMLDontGetImages,
 &ReplacementHTMLDontGetImage,
 &ReplaceHTMLWebbugImages,
 &ReplacementHTMLWebbugImage,
 &DemoroniseMSChars,
 &DisableAnimatedGIF,

 /* DontCache */

 &DontCache,

 /* DontGet */

 &DontGet,
 &DontGetReplacementURL,
 &DontGetRecursive,
 &DontGetLocation,

 /* CensorHeader */

 &SessionCookiesOnly,
 &CensorIncomingHeader,
 &RefererSelf,
 &RefererSelfDir,
 &CensorOutgoingHeader,

 /* FTPOptions */

 &FTPAuthUser,
 &FTPAuthPass,

 /* Proxy */

 &Proxies,
 &ProxyAuthUser,
 &ProxyAuthPass,
 &SSLProxy,

 /* Purge */

 &PurgeAges,
 &PurgeCompressAges
};

#define is_prefix(strlit,str) (!strncmp(strlit,str,strlitlen(strlit)))
#define is_proper_prefix(strlit,str) (!strncmp(strlit,str,strlitlen(strlit)) && (str)[strlitlen(strlit)])

/*++++++++++++++++++++++++++++++++++++++
  Send to the client one of the pages to configure WWWOFFLE using HTML.

  int fd The file descriptor of the client.

  URL *Url The Url that was requested.

  Body *request_body The body of the HTTP request.
  ++++++++++++++++++++++++++++++++++++++*/

void ConfigurationPage(int fd,URL *Url,Body *request_body)
{
 int s,i;
 char *url=NULL;
 char *newpath=Url->path+strlitlen("/configuration/");

 /* Check the authorisation. */

 if(ConfigString(PassWord))
   {
    if(!Url->pass)
      {
       ConfigurationAuthFail(fd,Url->pathp);
       return;
      }
    else if(strcmp(Url->pass,ConfigString(PassWord)))
      {
       ConfigurationAuthFail(fd,Url->pathp);
       return;
      }
   }

 /* Parse the arguments if any */

 if(Url->args)
   {
    char **args,*pling;

    if(*Url->args=='!' && (pling=strstr(Url->args+1,"!POST")))
      {
       args=STRDUP3(Url->args+1,pling,SplitFormArgs);
      }
    else
       args=SplitFormArgs(Url->args);

    for(i=0;args[i];i++)
      {
       if(is_proper_prefix("url=",args[i]))
          url=URLDecodeFormArgs(args[i]+strlitlen("url="));
      }

    free(args[0]);
    free(args);
   }

 /* Determine the page to show. */

 if(*newpath==0)
   {
    ConfigurationIndexPage(fd,url);
    goto free_return;
   }

 if(!strcmp(newpath,"url"))
   {
    ConfigurationURLPage(fd,Url->args);
    goto free_return;
   }

 for(s=0;s<CurrentConfig.nsections;++s)
   {
    int len=strlen(CurrentConfig.sections[s]->name);

    if(!strncmp(newpath,CurrentConfig.sections[s]->name,len))
      {
       char *subpath=newpath+len;
       if(*subpath==0)
         {
          ConfigurationSectionPage(fd,s,url);
          goto free_return;
         }
       else if(*subpath=='/')
	 {
	  int no_name=!strcmp(subpath+1,"no-name");
          for(i=0;i<CurrentConfig.sections[s]->nitemdefs;++i)
             if(!strcmp(CurrentConfig.sections[s]->itemdefs[i].name,subpath+1) ||
                (no_name && *CurrentConfig.sections[s]->itemdefs[i].name==0))
               {
                ConfigurationItemPage(fd,s,i,url,request_body);
                goto free_return;
               }
	 }
      }
   }

 HTMLMessage(fd,404,"WWWOFFLE Illegal Configuration Page",NULL,"ConfigurationIllegal",
             "url",Url->pathp,
             NULL);
free_return:
 if(url) free(url);
}


/*++++++++++++++++++++++++++++++++++++++
  The configuration page index that lists the sections.

  int fd The file descriptor to write to.

  char *url The URL specification from the URL argument.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationIndexPage(int fd,char *url)
{
 char *line=NULL;
 int file;
 int s;

 file=OpenLanguageFile("messages/README.CONF.txt");

 if(file==-1)
   {
    HTMLMessage(fd,200,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                "error","Cannot open README.CONF.txt",
                NULL);
    return;
   }


 line=read_line(file,line); /* TITLE ... */
 if(line) line=read_line(file,line); /* HEAD */
 if(line) line=read_line(file,line); /* comment */

 HTMLMessageHead(fd,200,"WWWOFFLE Configuration Page",
                 NULL);

 if(out_err==-1) goto close_return;

 HTMLMessageBody(fd,"ConfigurationPage-Head",
                 "description",line,
                 NULL);

 if(out_err==-1) goto close_return;

 for(s=0;s<CurrentConfig.nsections;++s)
   {
    char *description=NULL;

    lseek(file,0,SEEK_SET);
    init_buffer(file);

    do
      {
       line=read_line(file,line);
       if(!line) goto output_body;
       chomp_str(line);
      }
    while(!(!strcmp_litbeg(line,"SECTION") && !strcmp(line+8,CurrentConfig.sections[s]->name)));

    line=read_line(file,line);
    if(line) {
      chomp_str(line);
      description=line;
    }

   output_body:
    HTMLMessageBody(fd,"ConfigurationPage-Body",
                    "section",CurrentConfig.sections[s]->name,
                    "description",description,
                    "url",url,
                    NULL);

    if(out_err==-1) goto close_return;
   }

 lseek(file,0,SEEK_SET);
 init_buffer(file);

 do
   {
    line=read_line(file,line);
    if(!line) goto output_tail;
   }
 while(strcmp_litbeg(line,"TAIL"));

 line=read_line(file,line); /* comment */

output_tail:
 HTMLMessageBody(fd,"ConfigurationPage-Tail",
                 "description",line,
                 NULL);

close_return:
 if(line) free(line);
 close(file);
}


/*++++++++++++++++++++++++++++++++++++++
  The configuration page that handles a section.

  int fd The file descriptor to write to.

  int section The section in the configuration file.

  char *url The URL specification from the URL argument.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationSectionPage(int fd,int section,char *url)
{
 int i;
 char *line1=NULL,*line2=NULL,*description=NULL;
 int file;
 off_t seekpos=0;

 file=OpenLanguageFile("messages/README.CONF.txt");

 if(file==-1)
   {
    HTMLMessage(fd,200,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                "error","Cannot open README.CONF.txt",
                NULL);
    return;
   }


 HTMLMessageHead(fd,200,"WWWOFFLE Configuration Section Page",
                 NULL);

 if(out_err==-1) goto close_return;

 do
   {
     line1=read_line(file,line1);
     if(!line1) goto output_head;
     chomp_str(line1);
   }
 while(!(!strcmp_litbeg(line1,"SECTION") && !strcmp(line1+8,CurrentConfig.sections[section]->name)));

 line1=read_line(file,line1);
 if(line1) {
   chomp_str(line1);
   description=line1;
   seekpos=buffered_seek_cur(file);
 }

output_head:
 HTMLMessageBody(fd,"ConfigurationSection-Head",
                 "section",CurrentConfig.sections[section]->name,
                 "description",description,
                 "nextsection",section==CurrentConfig.nsections-1?
                               CurrentConfig.sections[0]->name:
                               CurrentConfig.sections[section+1]->name,
                 "prevsection",section==0?
                               CurrentConfig.sections[CurrentConfig.nsections-1]->name:
                               CurrentConfig.sections[section-1]->name,
                 "url",url,
                 NULL);

 if(out_err==-1) goto close_return;

 for(i=0;i<CurrentConfig.sections[section]->nitemdefs;++i)
   {
    char *template=NULL;
    description=NULL;

    if(!seekpos) goto output_body;
    lseek(file,seekpos,SEEK_SET);
    init_buffer(file);

    do
      {
       line1=read_line(file,line1);
       if(!line1) goto output_body;
       chomp_str(line1);
       if(!strcmp_litbeg(line1,"SECTION")) goto output_body;
      }
    while(!(!strcmp_litbeg(line1,"ITEM") && !strcmp(line1+5,CurrentConfig.sections[section]->itemdefs[i].name)));

    line1=read_line(file,line1);
    if(line1) {
      chomp_str(line1);
      template=line1;
      line2=read_line(file,line2);
      if(line2) {
	chomp_str(line2);
	description=line2;
      }
    }

   output_body:
    HTMLMessageBody(fd,"ConfigurationSection-Body",
                    "item",CurrentConfig.sections[section]->itemdefs[i].name,
                    "template",template,
                    "description",description,
                    NULL);

    if(out_err==-1) goto close_return;
   }

 HTMLMessageBody(fd,"ConfigurationSection-Tail",
                 NULL);

close_return:
 if(line1)
    free(line1);
 if(line2)
    free(line2);
 close(file);
}


/*++++++++++++++++++++++++++++++++++++++
  The configuration page that handles an item in a section.

  int fd The file descriptor to write to.

  int section The section in the configuration file.

  int item The item within the section.

  char *url The URL specification from the URL argument.

  Body *request_body The body of the POST request containing the information.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationItemPage(int fd,int section,int item,char *url,Body *request_body)
{
 ConfigSection *configsection=CurrentConfig.sections[section];
 ConfigItemDef *itemdef=&configsection->itemdefs[item];
 char *oldurl=url;
 char *action=NULL,*entry=NULL;
 char *key=NULL,*val=NULL;
 int edit=0;
 int i;

 /* Parse the arguments if any */

 if(request_body)
   {
    char *form=URLRecodeFormArgs(request_body->content);
    char **args=SplitFormArgs(form);

    for(i=0;args[i];i++)
      {
       if(is_proper_prefix("url=",args[i]))
          url=URLDecodeFormArgs(args[i]+strlitlen("url="));
       else if(is_proper_prefix("key=",args[i]))
          key=URLDecodeFormArgs(args[i]+strlitlen("key="));
       else if(is_proper_prefix("val=",args[i]))
          val=URLDecodeFormArgs(args[i]+strlitlen("val="));
       else if(is_prefix("action=",args[i]))
          action=URLDecodeFormArgs(args[i]+strlitlen("action="));
       else if(is_prefix("entry=",args[i]))
          entry=URLDecodeFormArgs(args[i]+strlitlen("entry="));
      }

    free(args[0]);
    free(args);
    free(form);
   }

 /* Display the page to edit the parameters */

 if(!action || (edit= !strcmp(action,"edit")))
   {
    char *line1=NULL,*line2=NULL,*template=NULL,*description=NULL;
    int file;
    char nentries[12],nallowed[4];
    char *entry_url=NULL,*entry_key=NULL,*entry_val=NULL;

    if(edit && entry)
      {
       for(i=0;i<(*itemdef->item)->nentries;++i)
         {
          char *thisentry=ConfigEntryString(*itemdef->item,i);

          if(!strcmp(thisentry,entry))
            {
             ConfigEntryStrings(*itemdef->item,i,&entry_url,&entry_key,&entry_val);
             free(thisentry);
             break;
            }

          free(thisentry);
         }
      }

    /* Display the HTML */

    file=OpenLanguageFile("messages/README.CONF.txt");

    if(file==-1)
      {
       HTMLMessage(fd,200,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                   "error","Cannot open README.CONF.txt",
                   NULL);
       goto free_entry;
      }


    HTMLMessageHead(fd,200,"WWWOFFLE Configuration Item Page",
                    NULL);

    if(out_err==-1) goto close_free;

    do
      {
       line1=read_line(file,line1);
       if(!line1) goto output_head;
       chomp_str(line1);
      }
    while(!(!strcmp_litbeg(line1,"SECTION") && !strcmp(line1+8,configsection->name)));

    do
      {
       line1=read_line(file,line1);
       if(!line1) goto output_head;
       chomp_str(line1);
       if(!strcmp_litbeg(line1,"SECTION")) goto output_head;
      }
    while(!(!strcmp_litbeg(line1,"ITEM") && !strcmp(line1+5,itemdef->name)));

    line1=read_line(file,line1);
    if(line1) {
      chomp_str(line1);
      template=line1;
      line2=read_line(file,line2);
      if(line2) {
	chomp_str(line2);
	description=line2;
      }
    }

   output_head:
    if(*itemdef->item)
       sprintf(nentries,"%d",(*itemdef->item)->nentries);
    else
       strcpy(nentries,"0");

    if(itemdef->same_key==0 &&
       itemdef->url_type==0)
       strcpy(nallowed,"1");
    else
       strcpy(nallowed,"any");

    HTMLMessageBody(fd,"ConfigurationItem-Head",
                    "section",configsection->name,
                    "item",itemdef->name,
                    "template",template,
                    "description",description,
                    "nextitem",item==configsection->nitemdefs-1?
                               configsection->itemdefs[0].name:
                               configsection->itemdefs[item+1].name,
                    "previtem",item==0?
                               configsection->itemdefs[configsection->nitemdefs-1].name:
                               configsection->itemdefs[item-1].name,

                    "url_type",itemdef->url_type?"yes":"",
                    "key_type",ConfigTypeString(itemdef->key_type),
                    "val_type",ConfigTypeString(itemdef->val_type),

                    "def_key",itemdef->key_type==Fixed?
                              itemdef->name:"",
                    "def_val",itemdef->def_val,

                    "url",edit?entry_url:url,
                    "key",edit?entry_key:(!itemdef->url_type &&
                                          itemdef->key_type==UrlSpecification &&
                                          itemdef->val_type==None)?url:key,
                    "val",edit?entry_val:val,

                    "entry",entry,

                    "nentries",nentries,
                    "nallowed",nallowed,
                    NULL);

    if(out_err==-1) goto close_free;

    /* No items present => no list */

    if(!*itemdef->item ||
       (*itemdef->item)->nentries==0)
       ;

    /* Only one entry allowed => one item */

    else if(itemdef->same_key==0 &&
            itemdef->url_type==0)
      {
       char *string=ConfigEntryString(*itemdef->item,0);

       HTMLMessageBody(fd,"ConfigurationItem-Body",
		       "thisentry",string,
		       "entry",string,
		       NULL);

       free(string);
       if(out_err==-1) goto close_free;
      }

    /* Several entries allowed => list */

    else
      {
       for(i=0;i<(*itemdef->item)->nentries;++i)
         {
          char *string=ConfigEntryString(*itemdef->item,i);

	  
	  HTMLMessageBody(fd,"ConfigurationItem-Body",
			  "thisentry",string,
			  NULL);

          free(string);
	  if(out_err==-1) goto close_free;
         }
      }

    HTMLMessageBody(fd,"ConfigurationItem-Tail",
		    NULL);

   close_free:
    if(line1)     free(line1);
    if(line2)     free(line2);
    close(file);
   free_entry:
    if(entry_url) free(entry_url);
    if(entry_key) free(entry_key);
    if(entry_val) free(entry_val);
   }

    /* Display the page with the results of editing the parameters. */

 else
   {
    char *errmsg=NULL;
    char *newentry=NULL;

    if(url || key || val)
       newentry=MakeConfigEntryString(itemdef,url,key,val);

    if(!strcmp(action,"insert"))
       errmsg=ModifyConfigFile(section,item,newentry,NULL ,NULL ,NULL );
    else if(!strcmp(action,"before") && entry)
       errmsg=ModifyConfigFile(section,item,newentry,NULL ,NULL ,entry);
    else if(!strcmp(action,"replace") && entry)
       errmsg=ModifyConfigFile(section,item,newentry,NULL ,entry,NULL );
    else if(!strcmp(action,"after") && entry)
       errmsg=ModifyConfigFile(section,item,newentry,entry,NULL ,NULL );
    else if(!strcmp(action,"delete") && entry)
       errmsg=ModifyConfigFile(section,item,NULL    ,NULL ,entry,NULL );
    else
      {
       errmsg=strdup("The specified form action was invalid or an existing entry parameter was missing.");
      }

    /* Display the result */

    HTMLMessage(fd,200,"WWWOFFLE Configuration Change Page",NULL,"ConfigurationChange",
                "section",configsection->name,
                "item",itemdef->name,
                "action",action,
                "oldentry",entry,
                "newentry",newentry,
                "errmsg",errmsg,
                NULL);

    if(errmsg) free(errmsg);

    if(newentry) free(newentry);
   }

 if(url!=oldurl) free(url);
 if(key)    free(key);
 if(val)    free(val);
 if(action) free(action);
 if(entry)  free(entry);
}


inline static char *chrstr_dup(char c, const char *str)
{
  int str_len=strlen(str);
  char *result=(char *)malloc(str_len+2);
  *result=c;
  memcpy(result+1,str,str_len+1);
  return result;
}

inline static void chrstr_prepend(char c, char **str)
{
  char *oldstr=*str;
  *str=chrstr_dup(c,oldstr);
  free(oldstr);
}


/*++++++++++++++++++++++++++++++++++++++
  The configuration page for a specific URL.

  int fd The file descriptor to write to.

  char *request_args The arguments to the configuration page.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationURLPage(int fd,char *request_args)
{
 char *url=NULL; URL *Url=NULL;
 char *proto=NULL,*host=NULL,*port=NULL,*path=NULL,*params=NULL,*args=NULL;
 char *proto_other=NULL,*host_other=NULL,*port_other=NULL,*path_other=NULL,*params_other=NULL,*args_other=NULL;
 int file;
 char *line1=NULL,*line2=NULL;
 int configitem,s,last_s=-1,seekpos=0;

 /* Parse the arguments. */

 if(request_args)
   {
    char *form,**arglist;
    int i;

    form=URLRecodeFormArgs(request_args);
    arglist=SplitFormArgs(form);

    for(i=0;arglist[i];i++)
      {
       if(is_proper_prefix("url=",arglist[i]))
          url=URLDecodeFormArgs(arglist[i]+strlitlen("url="));

       else if(is_proper_prefix("proto=",arglist[i]))
          proto=URLDecodeFormArgs(arglist[i]+strlitlen("proto="));
       else if(is_proper_prefix("host=",arglist[i]))
          host=URLDecodeFormArgs(arglist[i]+strlitlen("host="));
       else if(is_proper_prefix("port=",arglist[i]))
          port=URLDecodeFormArgs(arglist[i]+strlitlen("port="));
       else if(is_proper_prefix("path=",arglist[i]))
          path=URLDecodeFormArgs(arglist[i]+strlitlen("path="));
       else if(is_proper_prefix("params=",arglist[i]))
          params=URLDecodeFormArgs(arglist[i]+strlitlen("params="));
       else if(is_proper_prefix("args=",arglist[i]))
          args=URLDecodeFormArgs(arglist[i]+strlitlen("args="));

       else if(is_proper_prefix("proto_other=",arglist[i]))
          proto_other=URLDecodeFormArgs(arglist[i]+strlitlen("proto_other="));
       else if(is_proper_prefix("host_other=",arglist[i]))
          host_other=URLDecodeFormArgs(arglist[i]+strlitlen("host_other="));
       else if(is_proper_prefix("port_other=",arglist[i]))
          port_other=URLDecodeFormArgs(arglist[i]+strlitlen("port_other="));
       else if(is_proper_prefix("path_other=",arglist[i]))
          path_other=URLDecodeFormArgs(arglist[i]+strlitlen("path_other="));
       else if(is_proper_prefix("params_other=",arglist[i]))
          params_other=URLDecodeFormArgs(arglist[i]+strlitlen("params_other="));
       else if(is_proper_prefix("args_other=",arglist[i]))
          args_other=URLDecodeFormArgs(arglist[i]+strlitlen("args_other="));
      }

    free(arglist[0]);
    free(arglist);
    free(form);
   }

 /* Sort out a URL from the mess of arguments. */

 if(url)
   {
    URL *Url=SplitURL(url);

    if(proto_other)free(proto_other);
    if(host_other) free(host_other);
    if(port_other) free(port_other);
    if(path_other) free(path_other);
    if(params_other) free(params_other);
    if(args_other) free(args_other);

    if(proto) free(proto); proto=strdup(Url->proto);

    if(host) free(host); host=strdup(Url->host);

    if(Url->port)
      {
       if(port) free(port); port=chrstr_dup(':',Url->port);
      }

    if(path) free(path); path=strdup(Url->path);

    if(Url->params)
      {
       if(params) free(params); params=chrstr_dup(';',Url->params);
      }

    if(Url->args)
      {
       if(args) free(args); args=chrstr_dup('?',Url->args);
      }

    FreeURL(Url);
   }
 else
   {
    int urlsize;

    if(proto && !strcmp(proto,"OTHER") && proto_other)
      {free(proto); proto=proto_other;}
    else
      {if(proto_other) free(proto_other);}
    if(!proto) proto=strdup("*");

    if(host && !strcmp(host,"OTHER") && host_other)
      {free(host); host=host_other;}
    else
      {if(host_other) free(host_other);}
    if(!host) host=strdup("*");

    if(port && !strcmp(port,"OTHER") && port_other)
      {free(port); port=port_other;}
    else
      {if(port_other) free(port_other);}
    if(port && *port && *port!=':') chrstr_prepend(':',&port);

    if(path && !strcmp(path,"OTHER") && path_other)
      {free(path); path=path_other;}
    else
      {if(path_other) free(path_other);}
    if(path) {if(*path!='/') chrstr_prepend('/',&path);}
    else path=strdup("/*");

    if(params && !strcmp(params,"OTHER") && params_other)
      {free(params); params=params_other;}
    else
      {if(params_other) free(params_other);}
    if(params && *params && *params!=';') chrstr_prepend(';',&params);

    if(args && !strcmp(args,"OTHER") && args_other)
      {free(args); args=args_other;}
    else
      {if(args_other) free(args_other);}
    if(args && *args && *args!='?') chrstr_prepend('?',&args);

    urlsize=4;
    urlsize+=strlen(proto);
    urlsize+=strlen(host);
    if(port) urlsize+=strlen(port);
    urlsize+=strlen(path);
    if(params) urlsize+=strlen(params);
    if(args) urlsize+=strlen(args);

    url=(char*)malloc(urlsize);

    {
      char *p;
      p=stpcpy(stpcpy(stpcpy(url,proto),"://"),host);
      if(port) p=stpcpy(p,port);
      p=stpcpy(p,path);
      if(params) p=stpcpy(p,params);
      if(args) p=stpcpy(p,args);
    }
   }

 if(!strchr(url,'*'))
   Url=SplitURL(url);


 /* Display the HTML */

 file=OpenLanguageFile("messages/README.CONF.txt");

 if(file==-1)
   {
    HTMLMessage(fd,200,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                "error","Cannot open README.CONF.txt",
                NULL);

    goto free_return;
   }

 HTMLMessageHead(fd,200,"WWWOFFLE Configuration URL Page",
                 NULL);

 if(out_err==-1) goto close_free_return;

 HTMLMessageBody(fd,"ConfigurationUrl-Head",
                 "wildcard",Url?"":"yes",
                 "url",url,
                 "proto",proto,
                 "host",host,
                 "port",port,
                 "path",path,
                 "params",params,
                 "args",args,
                 NULL);

 if(out_err==-1) goto close_free_return;

 /* Loop through all of the specified ConfigItems. */

 for(configitem=0;configitem<sizeof(URLConfigItems)/sizeof(URLConfigItems[0]);++configitem)
   {
    int i;
    char *template=NULL,*description=NULL,*current=NULL;

    for(s=0,i=0;s<CurrentConfig.nsections;++s)
      for(i=0;i<CurrentConfig.sections[s]->nitemdefs;++i)
	if(URLConfigItems[configitem]==CurrentConfig.sections[s]->itemdefs[i].item)
	  goto found_configitem;

    continue;

   found_configitem:
    if(s!=last_s)
      {
       last_s=s;

       lseek(file,0,SEEK_SET);
       init_buffer(file);
       seekpos=0;

       do
         {
	   line1=read_line(file,line1);
	   if(!line1) goto find_current;
	   chomp_str(line1);
	 }
       while(!(!strcmp_litbeg(line1,"SECTION") && !strcmp(line1+8,CurrentConfig.sections[s]->name)));

       line1=read_line(file,line1); /* skip section description */
       if(!line1) goto find_current;
       seekpos=buffered_seek_cur(file);
      }
    else
      {
       if(!seekpos) goto find_current;
       lseek(file,seekpos,SEEK_SET);
       init_buffer(file);
      }

    do
      {
       line1=read_line(file,line1);
       if(!line1) goto find_current;
       chomp_str(line1);
       if(!strcmp_litbeg(line1,"SECTION")) goto find_current;
      }
    while(!(!strcmp_litbeg(line1,"ITEM") && !strcmp(line1+5,CurrentConfig.sections[s]->itemdefs[i].name)));

    line1=read_line(file,line1);
    if(line1) {
      chomp_str(line1);
      template=line1;
      line2=read_line(file,line2);
      if(line2) {
	chomp_str(line2);
	description=line2;
      }
    }

   find_current:
    if(Url)
      {
	ConfigItem item=*URLConfigItems[configitem];

	if(item) {
	  int j;

	  if(item->itemdef->key_type==UrlSpecification) {
	    for(j=0;j<item->nentries;++j)
	      if(MatchUrlSpecification(item->key[j].urlspec,Url))
		goto found_current;
	  }
	  else if(item->itemdef->url_type) {
	    if(item->itemdef->key_type!=String) {
	      for(j=0;j<item->nentries;++j) {
		if(!item->url[j] || MatchUrlSpecification(item->url[j],Url))
		  goto found_current;
	      }
	    }
	    else { /* this is more complicated, we may need to list more that one entry */
	      int len_current=0,nl=0;
	      char *listed[item->nentries];

	      for(j=0;j<item->nentries;++j) {
		if(!item->url[j] || MatchUrlSpecification(item->url[j],Url)) {
		  char *entrystr; int len_entry,k;

		  for(k=0;k<nl;++k) {
		    if(!strcasecmp(listed[k],item->key[j].string))
		      goto alreadylisted;
		  }

		  entrystr=ConfigEntryString(item,j);
		  len_entry=strlen(entrystr);
		  if(nl==0) {
		    current=entrystr;
		    len_current=len_entry;
		  }
		  else {
		    current=(char*)realloc(current,len_current+len_entry+2);
		    current[len_current]='\n';
		    memcpy(&current[len_current+1],entrystr,len_entry+1);
		    len_current+=len_entry+1;
		    free(entrystr);
		  }
		  listed[nl++]=item->key[j].string;
		alreadylisted:
		}
	      }
	    }
	  }
	  goto skip_current;

	found_current:
	  current=ConfigEntryString(item,j);
	skip_current:
	}
      }

    HTMLMessageBody(fd,"ConfigurationUrl-Body",
		    "section",CurrentConfig.sections[s]->name,
		    "item",CurrentConfig.sections[s]->itemdefs[i].name,
		    "template",template,
		    "description",description,
		    "current",current,
		    NULL);

    if(current) free(current);

    if(out_err==-1) goto close_free_return;
   }

 HTMLMessageBody(fd,"ConfigurationUrl-Tail",
                 NULL);

close_free_return:
 if(line1) free(line1);
 if(line2) free(line2);
 close(file);
free_return:
 if(proto) free(proto);
 if(host)  free(host);
 if(port)  free(port);
 if(path)  free(path);
 if(params)  free(params);
 if(args)  free(args);
 if(url)   free(url);
 if(Url)   FreeURL(Url);

}


/*++++++++++++++++++++++++++++++++++++++
  Inform the user that the authorisation failed.

  int fd The file descriptor to write to.

  char *url The specified path.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationAuthFail(int fd,char *url)
{
 HTMLMessageHead(fd,401,"WWWOFFLE Authorisation Failed",
                 "WWW-Authenticate","Basic realm=\"control\"",
                 NULL);
 if(out_err!=-1)
   HTMLMessageBody(fd,"ConfigurationAuthFail",
		   "url",url,
		   NULL);
}
