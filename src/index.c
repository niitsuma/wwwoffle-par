/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/index.c 2.107 2005/11/09 19:48:24 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Generate an index of the web pages that are cached in the system.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2005,2007 Paul A. Rombouts
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
#include <fcntl.h>

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include "wwwoffle.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "proto.h"
#include "headbody.h"
#include "io.h"
#include "document.h"


/*+ A type to contain the information required to sort the files. +*/
typedef struct _FileIndex
{
 struct _FileIndex *next;	/*+ Pointer to the next item in the list. +*/
 URL *url;                      /*+ The URL of the file. +*/
 char *name;                    /*+ The name of the URL. +*/
 char *host;                    /*+ The host of the URL. +*/
 char *type;                    /*+ The type (file extension) of the URL. +*/
 time_t time;                   /*+ The time of the file (access or modification as appropriate). +*/
 int random;                    /*+ Number generated from the file name (used for random sort). +*/
}
FileIndex;

typedef struct {
  FileIndex *first;
  FileIndex **last;
  unsigned n;
}
filelist_t;

/*+ The method of sorting used. +*/
static const char* const sorttype[]={"none"  ,
                                     "mtime" ,
                                     "atime" ,
                                     "dated" ,
                                     "alpha" ,
                                     "domain",
                                     "type"  ,
                                     "random",
                                     "none"  };

/*+ An enumerated type for the sort modes. +*/
typedef enum _SortMode
{
 None,                          /*+ No sorting, natural order. +*/
 MTime,                         /*+ Sort by modification time +*/
 ATime,                         /*+ Sort by access time. +*/
 Dated,                         /*+ Sort by modification time with separators. +*/
 Alpha,                         /*+ Sort Alphabetically. +*/
 Domain,                        /*+ Sort by domain name. +*/
 Type,                          /*+ Sort by type (file extension). +*/
 Random,                        /*+ Sort by checksum of URL. +*/
 NSortModes                     /*+ The number of sort modes. +*/
}
SortMode;


typedef enum _index_t {
  indexmainpage,
  indexprotocol,
  indexhost,
  indexoutgoing,
  indexmonitor,
  indexlasttime,
  indexprevtime,
  indexlastout,
  indexprevout
}
index_t;

static const char* const index_head[]= {
  NULL,
  "IndexProtocol-Head",
  "IndexHost-Head",
  "IndexOutgoing-Head",
  "IndexMonitor-Head",
  "IndexLastTime-Head",
  "IndexLastTime-Head",
  "IndexLastOut-Head",
  "IndexLastOut-Head"
};
  
static const char* const index_body[]= {
  "IndexRoot-Body",
  "IndexProtocol-Body",
  "IndexHost-Body",
  "IndexOutgoing-Body",
  "IndexMonitor-Body",
  "IndexLastTime-Body",
  "IndexLastTime-Body",
  "IndexLastOut-Body",
  "IndexLastOut-Body"
};
  
static const char* const index_tail[]= {
  NULL,
  "IndexProtocol-Tail",
  "IndexHost-Tail",
  "IndexOutgoing-Tail",
  "IndexMonitor-Tail",
  "IndexLastTime-Tail",
  "IndexLastTime-Tail",
  "IndexLastOut-Tail",
  "IndexLastOut-Tail"
};
  
static ConfigItem* const indexlistconf[]= {
  NULL,
  NULL,
  &IndexListHost,
  &IndexListOutgoing,
  &IndexListMonitor,
  &IndexListLatest,
  &IndexListLatest,
  &IndexListLatest,
  &IndexListLatest
};


/*+ The current time. +*/
static time_t now;

static void IndexRoot(int fd);
static void IndexProtocol(int fd,char *proto,SortMode mode,int allopt);
static void IndexDir(index_t indextype,int fd,char *proto,char *name,SortMode mode,int allopt,int titleopt);

static void add_dir(filelist_t *l,char *name,SortMode mode);
static void add_file(filelist_t *l,char *name,SortMode mode);

static void dated_separator(int fd,FileIndex *fi,int *lastdays,int *lasthours);

static int sort_alpha(FileIndex *a,FileIndex *b);
static int sort_type(FileIndex *a,FileIndex *b);
static int sort_domain(FileIndex *a,FileIndex *b);
static int sort_time(FileIndex *a,FileIndex *b);
static int sort_random(FileIndex *a,FileIndex *b);
static void listsort(FileIndex **l, int (*cmp)(FileIndex *,FileIndex *));
static char *webpagetitle(URL *Url);


/*++++++++++++++++++++++++++++++++++++++
  Check if the specified URL is to be indexed.

  int is_indexed Return true if it is to be indexed.

  URL *Url The URL to check.

  index_t indextype The type of index that is being checked.
  ++++++++++++++++++++++++++++++++++++++*/

inline static int is_indexed(URL *Url,index_t indextype)
{
 return(ConfigBooleanURL(IndexListAny,Url) && ConfigBooleanURL(*indexlistconf[indextype],Url));
}


/*++++++++++++++++++++++++++++++++++++++
  Generate an index of the pages that are in the cache.

  int fd The file descriptor to write the output to.

  URL *Url The URL that specifies the path to generate the index for.
  ++++++++++++++++++++++++++++++++++++++*/

void IndexPage(int fd,URL *Url)
{
 char *newpath;
 char *proto=NULL,*host="";
 SortMode sort=NSortModes,s;
 index_t indextype;
 int delopt=0,refopt=0,monopt=0,allopt=0,confopt=0,infoopt=0,titleopt=0;
 int i;

 if(!strcmp(Url->path,"/index/url"))
   {
    if(Url->args)
      {
       char *url=URLDecodeFormArgs(Url->args);
       URL *indexUrl=SplitURL(url);
       char *localurl=GetLocalURL();
       char *relocate=x_asprintf("%s/index/%s/%s?sort=%s",localurl,indexUrl->proto,indexUrl->hostport,sorttype[Alpha]);
       HTMLMessage(fd,302,"WWWOFFLE Index URL Redirect",relocate,"Redirect",
                   "location",relocate,
                   NULL);

       free(relocate);
       free(localurl);
       FreeURL(indexUrl);
       free(url);
      }
    else
       HTMLMessage(fd,404,"WWWOFFLE Illegal Index Page",NULL,"IndexIllegal",
                   "url",Url->pathp,
                   NULL);

    return;
   }

 newpath=strdupa(Url->path+strlitlen("/index/"));

 {
   char *p=strchrnul(newpath,0)-1;
   if(p>=newpath && *p=='/') *p=0;

   p=strchr(newpath,'/');
   if(p) {*p=0; host=p+1;}
 }

 proto=newpath;

 {
   int protocol=0,nprev;

   for(i=0;i<NProtocols;++i)
     if(!strcmp(proto,Protocols[i].name))
       {protocol=i+1; break;}

   indextype=
     protocol                           ? (*host ? indexhost: indexprotocol):
     !strcmp(proto,"outgoing")          ? indexoutgoing:
     !strcmp(proto,"monitor")           ? indexmonitor:
     !strcmp(proto,"lasttime")          ? indexlasttime:
     !strcmp_litbeg(proto,"prevtime") &&
       (nprev=atoi(proto+strlitlen("prevtime")))>=0 &&
       nprev<=NUM_PREVTIME_DIR          ? indexprevtime:
     !strcmp(proto,"lastout")           ? indexlastout:
     !strcmp_litbeg(proto,"prevout") &&
       (nprev=atoi(proto+strlitlen("prevout")))>=0 &&
       nprev<=NUM_PREVTIME_DIR          ? indexprevout:
                                          indexmainpage;
 }

 if(Url->args)
   {
    char **args,**a;

    args=SplitFormArgs(Url->args);

    for(a=&args[0];*a;a++)
      {
       if(!strcmp_litbeg(*a,"sort="))
         {
          for(s=0;s<NSortModes;s++)
             if(!strcmp(*a+strlitlen("sort="),sorttype[s]))
               {sort=s;break;}
         }
       else if(!strcmp(*a,"delete"))
          delopt=1;
       else if(!strcmp(*a,"refresh"))
          refopt=1;
       else if(!strcmp(*a,"monitor"))
          monopt=1;
       else if(!strcmp(*a,"all"))
          allopt=1;
       else if(!strcmp(*a,"config"))
          confopt=1;
       else if(!strcmp(*a,"info"))
          infoopt=1;
       else if(!strcmp(*a,"title"))
          titleopt=1;
       else if(!strcmp(*a,"titleonly"))
          titleopt=2;
       else
          PrintMessage(Warning,"Unexpected argument '%s' seen decoding form data for URL '%s'.",*a,Url->name);
      }

    free(args[0]);
    free(args);
   }

 now=time(NULL);

 if((*host && (strchr(host,'/') || !strcmp(host,"..") || !strcmp(host,"."))) ||
    (indextype==indexmainpage && Url->path[7]) ||
    (indextype!=indexhost && *host))
    HTMLMessage(fd,404,"WWWOFFLE Illegal Index Page",NULL,"IndexIllegal",
                "url",Url->pathp,
                NULL);
 else if(sort==NSortModes)
   {
    char *localurl=GetLocalURL();
    char *relocate=x_asprintf("%s%s?sort=%s",localurl,Url->path,sorttype[0]);

    HTMLMessage(fd,302,"WWWOFFLE Unknown Sort Index Redirect",relocate,"Redirect",
                "location",relocate,
                NULL);

    free(relocate);
    free(localurl);
   }
 else
   {
    HTMLMessageHead(fd,200,"WWWOFFLE Index",
                    NULL);
    if(out_err==-1 || head_only) return;
    HTMLMessageBody(fd,"Index-Head",
                    "type",indextype==indexprevtime?"prevtime":indextype==indexprevout?"prevout":proto,
                    "subtype",indextype==indexprevtime?proto+8:indextype==indexprevout?proto+7:host,
                    "sort",sorttype[sort],
                    "delete" ,delopt?";delete":"",
                    "refresh",refopt?";refresh":"",
                    "monitor",monopt?";monitor":"",
                    "all"    ,allopt?";all":"",
                    "config" ,confopt?";config":"",
                    "info"   ,infoopt?";info":"",
                    "title"  ,titleopt?(titleopt==2?";titleonly":";title"):"",
                    NULL);

    if(out_err==-1) return;

    switch (indextype) {
    case indexoutgoing:
    case indexmonitor:
    case indexlasttime:
    case indexprevtime:
    case indexlastout:
    case indexprevout:
      IndexDir(indextype,fd,NULL,proto,sort,allopt,titleopt);
      break;

    case indexprotocol:
      IndexProtocol(fd,proto,sort,allopt);
      break;

    case indexhost:
      IndexDir(indextype,fd,proto,host,sort,allopt,titleopt);
      break;

    default:
      IndexRoot(fd);
    }

    if(out_err==-1) return;

    HTMLMessageBody(fd,"Index-Tail",
                    NULL);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Index the root of the cache.

  int fd The file descriptor to write to.
  ++++++++++++++++++++++++++++++++++++++*/

static void IndexRoot(int fd)
{
 HTMLMessageBody(fd,"IndexRoot-Body",NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Index the hosts for one protocol in the cache.

  int fd The file descriptor to write to.

  char *proto The protocol to index.

  SortMode mode The sort mode to use.

  int allopt The option to show all pages including unlisted ones.
  ++++++++++++++++++++++++++++++++++++++*/

static void IndexProtocol(int fd,char *proto,SortMode mode,/*@unused@*/ int allopt)
{
 DIR *dir;
 struct dirent* ent;
 char total[MAX_INT_STR+1];
 int lastdays=0,lasthours=0;
 filelist_t l; FileIndex *p;

 /* Open the spool directory. */

 if(chdir(proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not indexed.",proto);
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open spool directory '%s' [%!s]; index failed.",proto);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read spool directory '%s' [%!s]; index failed.",proto);closedir(dir);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 /* Get all of the host sub-directories. */

 l.first= NULL;
 l.last= &l.first;
 l.n= 0;

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    add_dir(&l,ent->d_name,mode);
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 /* Sort the files. */

 {
   int (*sortfun)(FileIndex *,FileIndex *);

   switch (mode) {
   case MTime:
   case ATime:
   case Dated:  sortfun=sort_time;   break;
   case Alpha:
   case Type:   sortfun=sort_alpha;  break;
   case Domain: sortfun=sort_domain; break;
   case Random: sortfun=sort_random; break;
   default:     sortfun=NULL;
   }

   if(sortfun) listsort(&l.first,sortfun);
 }

 /* Output the page. */

 sprintf(total,"%u",l.n);

 HTMLMessageBody(fd,"IndexProtocol-Head",
                 "proto",proto,
                 "total",total,
                 NULL);

 p=l.first;
 while(p)
   {
    FileIndex *next;

    if(out_err!=-1) {
      if(mode==Dated)
	dated_separator(fd,p,&lastdays,&lasthours);

      HTMLMessageBody(fd,"IndexProtocol-Body",
		      "host",p->name,
		      NULL);
    }
    free(p->name);
    next=p->next;
    free(p);
    p=next;
   }

 if(out_err==-1) return;

 HTMLMessageBody(fd,"IndexProtocol-Tail",
                 NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Create an index of the pages in a subdirectory.

  index_t indextype The type of index.

  int fd The file descriptor to write to.

  char *proto The protocol to index (or NULL if the directory is a special one).

  char *name The name of the subdirectory.

  SortMode mode The method to use to sort the names.

  int allopt The option to show all pages including unlisted ones.
  ++++++++++++++++++++++++++++++++++++++*/


static void IndexDir(index_t indextype,int fd,char *proto,char *name,SortMode mode,int allopt,int titleopt)
{
 DIR *dir;
 struct dirent* ent;
 unsigned nindexed=0;
 char total[MAX_INT_STR+1],indexed[MAX_INT_STR+1],notindexed[MAX_INT_STR+1],
      date[MAXDATESIZE];
 int lastdays=0,lasthours=0;
 struct stat buf;
 filelist_t l; FileIndex *p;

 /* Change to the protocol directory. */

 if(proto && chdir(proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not indexed.",proto);
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 /* Open the subdirectory. */

#if defined(__CYGWIN__)
 if(proto) {
   char *p;
   for(p=name;*p;++p)
     if(*p==':')
       *p='!';
 }
#endif

 if(chdir(name))
   {PrintMessage(Warning,"Cannot change to directory '%s%s%s' [%!s]; not indexed.",proto?proto:"",proto?"/":"",name);if(proto) ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory '%s%s%s' [%!s]; not indexed.",proto?proto:"",proto?"/":"",name);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read directory '%s%s%s' [%!s]; not indexed.",proto?proto:"",proto?"/":"",name);closedir(dir);ChangeBackToSpoolDir();
   HTMLMessageBody(fd,"IndexError-Body",NULL);return;}

#if defined(__CYGWIN__)
 if(proto) {
   char *p;
   for(p=name;*p;++p)
     if(*p=='!')
       *p=':';
 }
#endif

 switch (indextype) {
 case indexlasttime:
 case indexprevtime:
 case indexlastout:
 case indexprevout:
   if(stat(".timestamp",&buf))
     strcpy(date,"?");
   else
     RFC822Date_r(buf.st_mtime,0,date);
 default:;
 }

 /* Add all of the file names. */

 l.first= NULL;
 l.last= &l.first;
 l.n= 0;

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    add_file(&l,ent->d_name,mode);
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 /* Sort the files. */

 {
   int (*sortfun)(FileIndex *,FileIndex *);

   switch (mode) {
   case MTime:
   case ATime:
   case Dated:  sortfun=sort_time;   break;
   case Alpha:  sortfun=sort_alpha;  break;
   case Type:   sortfun=sort_type;   break;
   case Domain: sortfun=(indextype==indexhost?sort_alpha:sort_domain); break;
   case Random: sortfun=sort_random; break;
   default:     sortfun=NULL;
   }

   if(sortfun) listsort(&l.first,sortfun);
 }

 /* Output the page. */

 sprintf(total,"%u",l.n);

 switch (indextype) {
 case indexhost:
   HTMLMessageBody(fd,index_head[indextype],
		   "proto",proto,
		   "host",name,
		   "total",total,
		   NULL);
   break;

 case indexoutgoing:
 case indexmonitor:
   HTMLMessageBody(fd,index_head[indextype],
		   "total",total,
		   NULL);
   break;

 case indexlasttime:
 case indexprevtime:
 case indexlastout:
 case indexprevout:
   HTMLMessageBody(fd,index_head[indextype],
		   "number",name+((indextype==indexlasttime || indextype==indexprevtime)?8:7),
		   "total",total,
		   "date",date,
		   NULL);
   break;
 default:;
 }

 p=l.first;
 while(p)
   {
     FileIndex *next;

     if(out_err!=-1 && (allopt || is_indexed(p->url,indextype)))
      {
       {
	 char *item=NULL;

	 if(titleopt) {
	   item=webpagetitle(p->url);
	   if(titleopt==2 && !item)
	     goto free_Url;  /* Skip items without a title if titleopt==2. */
	 }
	 if(!item)    item=HTML_url(indextype==indexhost?p->url->pathp:p->url->name);

	 if(mode==Dated)
	   dated_separator(fd,p,&lastdays,&lasthours);

	 if(indextype==indexmonitor) {
	   int last,next;
	   char laststr[MAX_INT_STR+3+1],nextstr[MAX_INT_STR+3+1];

	   MonitorTimes(p->url,&last,&next);

	   if(last>=(366*24))
	     sprintf(laststr,"365+");
	   else
	     sprintf(laststr,"%d:%d",last/24,last%24);
	   if(next>=(366*24))
	     sprintf(nextstr,"365+");
	   else
	     sprintf(nextstr,"%d:%d",next/24,next%24);

	   HTMLMessageBody(fd,index_body[indextype],
			   "url",p->url->name,
			   "link",ProxyableLink(p->url),
			   "item",item,
			   "last",laststr,
			   "next",nextstr,
			   NULL);
	 }
	 else
	   HTMLMessageBody(fd,index_body[indextype],
			   "url",p->url->name,
			   "link",ProxyableLink(p->url),
			   "item",item,
			   NULL);

	 free(item);
       }

       nindexed++;
      }

   free_Url:
    FreeURL(p->url);
    next=p->next;
    free(p);
    p=next;
   }

 if(out_err==-1) return;

 sprintf(indexed   ,"%u",nindexed);
 sprintf(notindexed,"%u",l.n-nindexed);

 HTMLMessageBody(fd,index_tail[indextype],
                 "indexed",indexed,
                 "notindexed",notindexed,
                 NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Add a directory to the list.

  filelist_t *l The list to add to.

  char *name The name of the directory.

  SortMode mode The sort mode.
  ++++++++++++++++++++++++++++++++++++++*/

static void add_dir(filelist_t *l,char *name,SortMode mode)
{
 struct stat buf;
 FileIndex *new;

 if(stat(name,&buf))
   {PrintMessage(Inform,"Cannot stat directory '%s' [%!s]; race condition?",name);return;}

 if(!S_ISDIR(buf.st_mode))
   return;

 new=(FileIndex*)malloc(sizeof(FileIndex));

 new->next=NULL;
 new->url=NULL;
 new->name=strdup(name);

#if defined(__CYGWIN__)
 {
   char *p;
   for(p=new->name;*p;++p)
     if(*p=='!')
       *p=':';
 }
#endif

 if(mode==Domain)
   new->host=new->name;
 else if(mode==Type)
   new->type="";
 else if(mode==MTime || mode==Dated)
   new->time=buf.st_mtime;
 else if(mode==ATime)
   new->time=buf.st_atime;

 *(l->last)=new;
 l->last= &(new->next);
 ++(l->n);
}


/*++++++++++++++++++++++++++++++++++++++
  Add a file to the list.

  filelist_t *l The list to add to.

  char *name The name of the file.

  SortMode mode The sort mode.
  ++++++++++++++++++++++++++++++++++++++*/

static void add_file(filelist_t *l,char *name,SortMode mode)
{
 URL *Url;
 struct stat buf;
 FileIndex *new;

 if(!*name || (*name!='D' && *name!='O') || name[strlen(name)-1]=='~')
    return;

 if(stat(name,&buf))
   {PrintMessage(Inform,"Cannot stat file '%s' [%!s]; race condition?",name); return;}

 if(!S_ISREG(buf.st_mode))
   return;

 Url=FileNameToURL(name);

 if(!Url)
    return;

 new=(FileIndex*)malloc(sizeof(FileIndex));

 new->next=NULL;
 new->url=Url;
 new->name=Url->name;

 if(mode==Domain)
   new->host=Url->hostport;
 else if(mode==Type)
   {
     char *p=strchrnul(Url->path,0);

     new->type="";

     while(--p>=Url->path && *p!='/')
       if(*p=='.') {
	 new->type=p;
	 break;
       }
   }
 else if(mode==MTime || mode==Dated)
   new->time=buf.st_mtime;
 else if(mode==ATime)
   new->time=buf.st_atime;
 else if(mode==Random)
   new->random=name[1]+256*(int)name[2]+65536*(int)name[3]+16777216*(int)name[4];

 *(l->last)=new;
 l->last= &(new->next);
 ++(l->n);
}


/*++++++++++++++++++++++++++++++++++++++
  Write out a date separator for the dated format if needed.

  int fd The file to write to.

  int file The number of the file in the list.

  int *lastdays The age of the previous file in days.

  int *lasthours The age of the previous file in hours.
  ++++++++++++++++++++++++++++++++++++++*/

static void dated_separator(int fd,FileIndex *fi,int *lastdays,int *lasthours)
{
 int days=(now-fi->time)/(24*3600),hours=(now-fi->time)/3600;

 if(*lastdays!=-1 && *lastdays<days)
   {
    char daystr[MAX_INT_STR+1];
    sprintf(daystr,"%d",days-*lastdays);

    HTMLMessageBody(fd,"IndexSeparator-Body",
                    "days",daystr,
                    NULL);
   }
 else if(*lasthours!=-1 && *lasthours<(hours-1))
   {
    HTMLMessageBody(fd,"IndexSeparator-Body",
                    "days","",
                    NULL);
   }

 *lastdays=days;
 *lasthours=hours;
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into alphabetical order.

  int sort_alpha Returns the comparison of the pointers to strings.

  FileIndex *a The first FileIndex.

  FileIndex *b The second FileIndex.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_alpha(FileIndex *a,FileIndex *b)
{
 char *an=(a)->name;
 char *bn=(b)->name;

 return(strcmp(an,bn));
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into type (file extension) order.

  int sort_type Returns the comparison of the pointers to strings.

  FileIndex *a The first FileIndex.

  FileIndex *b The second FileIndex.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_type(FileIndex *a,FileIndex *b)
{
 char *at=(a)->type;
 char *bt=(b)->type;
 int chosen;

 /* Compare the type */

 if((chosen=strcasecmp(at,bt))) return chosen;

 /* Fallback to alphabetical */

 return(sort_alpha(a,b));
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into domain order.

  int sort_domain Returns the comparison of the pointers to strings.

  FileIndex *a The first FileIndex.

  FileIndex *b The second FileIndex.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_domain(FileIndex *a,FileIndex *b)
{
 char *ap=strchrnul((a)->host,0);
 char *bp=strchrnul((b)->host,0);
 int chosen;

 /* Compare longer and longer domain parts */

 do {
   while(--ap>=(a)->host && *ap!='.' && *ap!='/');
   while(--bp>=(b)->host && *bp!='.' && *bp!='/');

   if((chosen=strcmp(ap+1,bp+1))) return chosen;
 }
 while(ap>=(a)->host && bp>=(b)->host);

 /* Fallback to alphabetical */

 return(sort_alpha(a,b));
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into time order.

  int sort_time Returns the comparison of the times.

  FileIndex *a The first FileIndex.

  FileIndex *b The second FileIndex.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_time(FileIndex *a,FileIndex *b)
{
 time_t at=(a)->time;
 time_t bt=(b)->time;

 /* Compare the time stamps or fallback to alphabetical */

 return(at<bt?1:at>bt?-1:sort_alpha(a,b));
}


/*++++++++++++++++++++++++++++++++++++++
  Used to sort the files into random order.

  int sort_random Returns the comparison of the random numbers generated from the file name.

  FileIndex *a The first FileIndex.

  FileIndex *b The second FileIndex.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_random(FileIndex *a,FileIndex *b)
{
 int ar=(a)->random;
 int br=(b)->random;

 return(br-ar);
}


/* Added by Paul Rombouts */

/*++++++++++++++++++++++++++++++++++++++
  Sort a list using a merge sort algorithm.

  FileIndex **l The list to sort.

  int (*cmp)(FileIndex *a,FileIndex *b)  The comparison function to use.
  ++++++++++++++++++++++++++++++++++++++*/

static void listsort(FileIndex **l, int (*cmp)(FileIndex *,FileIndex *))
{
  if(*l) {
    unsigned int m;

    for(m=1;; m *= 2) {
      unsigned int nmerge=0,i,j;
      FileIndex *p,*q=*l,**s= l, **t;

      do {
	++nmerge;
	p=q;
	i=m;
	do {
	  t= &q->next;
	  q= *t;
	} while(--i && q);

	if(!q) break;
      
	i=j=m;
	for(;;) {
	  if(cmp(p,q) <= 0) {
	    *s= p;
	    s= &p->next;
	    p= *s;
	    --i;
	    if(!i) {
	      *s= q;
	      do {s= &q->next; q= *s;} while(--j && q);
	      break;
	    }
	  }
	  else { /* cmp(p,q) > 0 */
	    *s= q;
	    s= &q->next;
	    q= *s;
	    --j;
	    if(!j || !q) {
	      *s= p;
	      *t= q;
	      s= t;
	      break;
	    }
	  }
	}
      } while(q);

      if(nmerge<=1) break;
    }
  }
}


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

/*++++++++++++++++++++++++++++++++++++++
  Find the title in spooled webpage.

  char *webpagetitle Returns the title.

  URL *Url The URL of the webpage.
  ++++++++++++++++++++++++++++++++++++++*/
static char *webpagetitle(URL *Url)
{
 char *title=NULL;
 int fd;

 {
   char *urldir=URLToDirName(Url);
   char name[strlen(Url->proto)+strlen(urldir)+base64enclen(sizeof(md5hash_t))+sizeof("//D")];
   char *p;

   p=stpcpy(name,Url->proto);
   *p++='/';
   p=stpcpy(p,urldir);
   *p++='/';
   *p++='D';
   GetHash(Url,p,base64enclen(sizeof(md5hash_t))+1);

   fd=open(name,O_RDONLY|O_BINARY);
 }

 if(fd!=-1) {
   int status;
   Header *spooled_head=NULL;

   init_io(fd);
   status=ParseReply(fd,&spooled_head);

   if(spooled_head) {
     if(status==200) {
       if((title= GetHeader(spooled_head,"Title")))
	 title= HTMLString(title,0,NULL);
       else if(GetHeader2(spooled_head,"Content-Type","text/html"))
	 title= HTML_title(fd);
     }

     FreeHeader(spooled_head);
   }

   finish_io(fd);
   close(fd);
 }

 return title;
}

