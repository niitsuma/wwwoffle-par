/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configedit.c 1.41 2005/10/11 18:34:15 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  The HTML interactive configuration editing pages.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007 Paul A. Rombouts
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
#include "errors.h"
#include "io.h"
#include "misc.h"
#include "configpriv.h"
#include "config.h"


static void ConfigurationIndexPage(int fd,/*@null@*/ char *url);
static void ConfigurationSectionPage(int fd,int section,/*@null@*/ char *url);
static void ConfigurationItemPage(int fd,int section,int item,URL *Url,/*@null@*/ char *url,/*@null@*/ Body *request_body);
static void ConfigurationEditURLPage(int fd,URL *Url,/*@null@*/ Body *request_body);
static void ConfigurationURLPage(int fd,char *url);
static void ConfigurationAuthFail(int fd,char *url);


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

 /* Extract the URL argument if any (ignoring all posted forms) */

 if(Url->args && *Url->args!='!')
    url=URLDecodeFormArgs(Url->args);

 /* Determine the page to show. */

 if(*newpath==0)
   {
    ConfigurationIndexPage(fd,url);
    goto free_return;
   }

 if(!strcmp(newpath,"editurl"))
   {
    ConfigurationEditURLPage(fd,Url,request_body);
    goto free_return;
   }

 if(!strcmp(newpath,"url") && url)
   {
    ConfigurationURLPage(fd,url);
    goto free_return;
   }

 for(s=0;s<CurrentConfig.nsections;++s)
   {
    size_t len=strlen(CurrentConfig.sections[s]->name);

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
                ConfigurationItemPage(fd,s,i,Url,url,request_body);
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
 unsigned long seekpos=0;
 int s;

 file=OpenLanguageFile("messages/README.CONF.txt");

 if(file==-1)
   {
    HTMLMessage(fd,500,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                "error","Cannot open README.CONF.txt",
                NULL);
    return;
   }

 init_io(file);

 line=read_line(file,line); /* TITLE ... */
 if(line) line=read_line(file,line); /* HEAD */
 if(line) line=read_line(file,line); /* comment */

 HTMLMessageHead(fd,200,"WWWOFFLE Configuration Page",
                 NULL);

 if(out_err==-1 || head_only) goto close_return;

 HTMLMessageBody(fd,"ConfigurationPage-Head",
                 "description",line,
                 NULL);

 if(out_err==-1) goto close_return;

 for(s=0;s<CurrentConfig.nsections;++s)
   {
    char *description=NULL;

    lseek(file,0,SEEK_SET);
    reinit_io(file);

    do
      {
       line=read_line(file,line);
       if(!line) goto output_body;
       chomp_str(line);
      }
    while(!(!strcmp_litbeg(line,"SECTION") && !strcmp(line+8,CurrentConfig.sections[s]->name)));

    tell_io(file,&seekpos,NULL);

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

 lseek(file,seekpos,SEEK_SET);  /* go back only to the start of the last section */
 reinit_io(file);

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
 finish_io(file);
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
 unsigned long seekpos=0;

 file=OpenLanguageFile("messages/README.CONF.txt");

 if(file==-1)
   {
    HTMLMessage(fd,500,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                "error","Cannot open README.CONF.txt",
                NULL);
    return;
   }

 init_io(file);

 HTMLMessageHead(fd,200,"WWWOFFLE Configuration Section Page",
                 NULL);

 if(out_err==-1 || head_only) goto close_return;

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
   tell_io(file,&seekpos,NULL);
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
    lseek(file,seekpos,SEEK_SET);  /* go back to the start of the required section */
    reinit_io(file);

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
 finish_io(file);
 close(file);
}

#define case_formarg(var,str)						\
 if(is_prefix(#var "=",str)) {						\
   if(var) free(var);							\
   (var)=TrimArgs(URLDecodeFormArgs((str)+strlitlen(#var "=")));	\
 }


#define case_nformarg(var,str)						\
 if(is_proper_prefix(#var "=",str)) {					\
   if(var) free(var);							\
   (var)=TrimArgs(URLDecodeFormArgs((str)+strlitlen(#var "=")));	\
 }


/*++++++++++++++++++++++++++++++++++++++
  The configuration page that handles an item in a section.

  int fd The file descriptor to write to.

  int section The section in the configuration file.

  int item The item within the section.

  URL *Url The URL of the page that is being requested.

  char *urlarg The URL specification from the URL argument.

  Body *request_body The body of the POST request containing the information.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationItemPage(int fd,int section,int item,URL *Url,char *urlarg,Body *request_body)
{
 ConfigSection *configsection=CurrentConfig.sections[section];
 ConfigItemDef *itemdef=&configsection->itemdefs[item];
 char *url=NULL;
 char *action=NULL,*entry=NULL;
 char *key=NULL,*val=NULL;
 int edit=0;
 int i;

 /* Parse the arguments if any */

 if(request_body && request_body->length)
   {
    char *form=URLRecodeFormArgs(request_body->content);
    char **args=SplitFormArgs(form);

    for(i=0;args[i];i++)
      {
	case_nformarg(url,args[i])   else 
	case_nformarg(key,args[i])   else
	case_nformarg(val,args[i])   else
	case_formarg(action,args[i]) else
	case_formarg(entry,args[i])  else
	PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",args[i],Url->name);
      }

    free(args[0]);
    free(args);
    free(form);
   }

 if(!url) url=urlarg;

 /* Display the page to edit the parameters */

 if(!action || (edit= !strcmp(action,"edit")))
   {
    char *line1=NULL,*line2=NULL,*template=NULL,*description=NULL;
    int file;
    char nentries[MAX_INT_STR+1],nallowed[sizeof "any"];
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
       HTMLMessage(fd,500,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
                   "error","Cannot open README.CONF.txt",
                   NULL);
       goto free_entry;
      }

    init_io(file);

    HTMLMessageHead(fd,200,"WWWOFFLE Configuration Item Page",
                    "Cache-Control","no-cache",
                    "Expires","0",
                    NULL);

    if(out_err==-1 || head_only) goto close_free;

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
    sprintf(nentries,"%d",(*itemdef->item)?(*itemdef->item)->nentries:0);

    strcpy(nallowed,(itemdef->same_key==0 && itemdef->url_type==0)?"1":"any");

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
    finish_io(file);
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

 if(url!=urlarg) free(url);
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
  The configuration page to edit the URL-SPECIFICATION and redirect to the real page.

  int fd The file descriptor to write to.

  URL *Url The URL of the page that is being requested.

  Body *request_body The body of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationEditURLPage(int fd,URL *Url,Body *request_body)
{
 char *url=NULL;
 char *proto=NULL,*host=NULL,*port=NULL,*path=NULL,*args=NULL;
 char *proto_other=NULL,*host_other=NULL,*port_other=NULL,*path_other=NULL,*args_other=NULL;

 /* Parse the arguments. */

 if(request_body && request_body->length)
   {
    char *form=URLRecodeFormArgs(request_body->content);
    char **arglist=SplitFormArgs(form);
    int i;

    for(i=0;arglist[i];i++)
      {
	case_nformarg(proto,arglist[i])        else
        case_nformarg(host,arglist[i])         else
        case_nformarg(port,arglist[i])         else
	case_nformarg(path,arglist[i])         else
	case_nformarg(args,arglist[i])         else

	case_nformarg(proto_other,arglist[i])  else
	case_nformarg(host_other,arglist[i])   else
	case_nformarg(port_other,arglist[i])   else
	case_nformarg(path_other,arglist[i])   else
	case_nformarg(args_other,arglist[i])   else
	PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",arglist[i],Url->name);
      }

    free(arglist[0]);
    free(arglist);
    free(form);
   }

 /* Sort out a URL from the mess of arguments. */

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

 if(args && !strcmp(args,"OTHER") && args_other)
   {free(args); args=args_other;}
 else
   {if(args_other) free(args_other);}
 if(args && *args && *args!='?') chrstr_prepend('?',&args);

 {
   int urlsize=sizeof("://")+strlen(proto)+strlen(host);
   if(port) urlsize+=strlen(port);
   urlsize+=strlen(path);
   if(args) urlsize+=strlen(args);

   url=(char*)malloc(urlsize);

   {
     char *p= stpcpy(stpcpy(stpcpy(url,proto),"://"),host);
     if(port) p=stpcpy(p,port);
     p=stpcpy(p,path);
     if(args) p=stpcpy(p,args);
   }
 }

 if(proto) free(proto);
 if(host)  free(host);
 if(port)  free(port);
 if(path)  free(path);
 if(args)  free(args);

 /* Redirect the client to it */

 {
   char *localurl=GetLocalURL();
   char *encurl=URLEncodeFormArgs(url);
   char relocate[strlen(localurl)+strlen(encurl)+sizeof("/configuration/url?")];

   stpcpy(stpcpy(stpcpy(relocate,localurl),"/configuration/url?"),encurl);

   HTMLMessage(fd,302,"WWWOFFLE Configuration Edit URL Redirect",relocate,"Redirect",
	       "location",relocate,
	       NULL);

   free(encurl);
   free(localurl);
 }

 free(url);
}


/*++++++++++++++++++++++++++++++++++++++
  The configuration page for a specific URL.

  int fd The file descriptor to write to.

  char *url The URL specification from the URL argument.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConfigurationURLPage(int fd,char *url)
{
 static char dummy_port[]=":";
 static char dummy_arg[]="?";
 int wildcard=0,file;

 /* Assume a "well-formed" URL, from the function above or a WWWOFFLE index.
    If it isn't then try and make something useful from it, at least don't crash. */

 {
   char *proto=NULL,*host=NULL,*port=NULL,*path=NULL,*args=NULL;
   char *h=url,*colon=strchr(url,':'),*slash,*ques,*pathbeg,*pathend;

   if(colon && *(colon+1)=='/' && *(colon+2)=='/')
     {
       proto=STRDUPA2(url,colon);
       h=colon+3;
     }

   slash=strchrnul(h,'/');
   ques=strchrnul(h,'?');
   pathend=ques;

   if(*ques)  /* ../path?... */
     {
       if(*(ques+1)==0)
	 args=dummy_arg;
       else
	 args=ques+1;
     }

   if(slash<pathend)
     {
       pathbeg=slash;
       path=STRSLICE(pathbeg,pathend);
     }
   else
     pathbeg=pathend;

   if(*h=='[')
     {
       colon=strchrnul(h+1,']');
       if(*colon) ++colon;
     }
   else
     colon=strchrnul(h,':');

   if(*colon==':' && colon<pathbeg)
     {
       if(colon+1==pathbeg)
	 port=dummy_port;
       else
	 port=STRDUPA2(colon+1,pathbeg);
     }
   else
     colon=pathbeg;

   host=STRDUPA2(h,colon);

   if(strchr(url,'*') || port==dummy_port || args==dummy_arg)
     wildcard=1;

   /* Display the HTML */

   file=OpenLanguageFile("messages/README.CONF.txt");

   if(file==-1)
     {
       HTMLMessage(fd,500,"WWWOFFLE Configuration Page Error",NULL,"ServerError",
		   "error","Cannot open README.CONF.txt",
		   NULL);
       return;
     }

   init_io(file);

   HTMLMessageHead(fd,200,"WWWOFFLE Configuration URL Page",
		   NULL);

   if(out_err==-1 || head_only) goto close_return;

   HTMLMessageBody(fd,"ConfigurationUrl-Head",
		   "wildcard",wildcard?"yes":"",
		   "url",url,
		   "proto",proto,
		   "host",host,
		   "port",port,
		   "path",path,
		   "args",args,
		   NULL);

   if(out_err==-1) goto close_return;
 }

 /* Loop through all of the ConfigItems and find those that take a URL argument. */

 {
   int s,i;
   char *line1=NULL,*line2=NULL;

   for(s=0;s<CurrentConfig.nsections;++s)
     {
       ConfigSection *section= CurrentConfig.sections[s];
       unsigned long seekpos=0;

       lseek(file,0,SEEK_SET);
       reinit_io(file);

       do
	 {
	   line1=read_line(file,line1);
	   if(!line1) goto list_items;
	   chomp_str(line1);
	 }
       while(!(!strcmp_litbeg(line1,"SECTION") && !strcmp(line1+8,section->name)));

       tell_io(file,&seekpos,NULL);

     list_items:
       for(i=0;i<section->nitemdefs;++i) {
	 ConfigItemDef *itemdef= &section->itemdefs[i];

	 if(itemdef->url_type || itemdef->key_type==UrlSpecification)
	   {
	     ConfigItem item= *itemdef->item;
	     char *template=NULL,*description=NULL,*current=NULL;

	     if(!seekpos) goto find_current;
	     lseek(file,seekpos,SEEK_SET);  /* go back only to the start of the current section */
	     reinit_io(file);

	     do
	       {
		 line1=read_line(file,line1);
		 if(!line1) goto find_current;
		 chomp_str(line1);
		 if(!strcmp_litbeg(line1,"SECTION")) goto find_current;
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

	   find_current:
	     if(!wildcard && item)
	       {
		 int j;

		 URL *Url=SplitURL(url);

		 if(itemdef->key_type==UrlSpecification) {
		   for(j=0;j<item->nentries;++j)
		     if(MatchUrlSpecification(item->key[j].urlspec,Url))
		       goto found_current;
		 }
		 else if(itemdef->url_type) {
		   if(itemdef->key_type!=String) {
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
		       alreadylisted: ;
		       }
		     }
		   }
		 }
		 goto skip_current;

	       found_current:
		 current=ConfigEntryString(item,j);
	       skip_current:
		 FreeURL(Url);
	       }

	     HTMLMessageBody(fd,"ConfigurationUrl-Body",
			     "section",section->name,
			     "item",itemdef->name,
			     "template",template,
			     "description",description,
			     "current",current,
			     NULL);

	     if(current) free(current);

	     if(out_err==-1) goto close_free_return;
	   }
       }
     }
   HTMLMessageBody(fd,"ConfigurationUrl-Tail",
		   NULL);

 close_free_return:
   if(line1) free(line1);
   if(line2) free(line2);
 }
close_return:
 finish_io(file);
 close(file);
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
 if(out_err!=-1 && !head_only)
   HTMLMessageBody(fd,"ConfigurationAuthFail",
		   "url",url,
		   NULL);
}
