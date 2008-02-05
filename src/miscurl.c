/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/miscurl.c 2.108 2006/02/10 18:35:10 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Miscellaneous HTTP / HTML Url Handling functions.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2004,2005,2007,2008 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "misc.h"
#include "errors.h"
#include "config.h"
#include "proto.h"
#include "sockets.h"


inline static const char *strchr_with_endptr(const char *str,const char *endptr,int c)
{
  return memchr(str,c,endptr-str);
}


/*++++++++++++++++++++++++++++++++++++++
  Split a URL into a protocol, hostname, path name and an argument list.

  URL *SplitURL Returns a URL structure containing the information.

  const char *url The name of the url to split.
  ++++++++++++++++++++++++++++++++++++++*/

URL *SplitURL(const char *url)
{
 URL *Url;
 const char *proto,*protoend,*user,*userend,*pass,*passend,*hostport,*hostportend,*path,*pathend,*args,*argsend;
 const char *colon,*slash,*at,*ques,*urlend;

 /* Remove any fragment identifiers */

 urlend=strchrnul(url,'#');

 /* Protocol = Url->proto */

 colon=strchr_with_endptr(url,urlend,':');
 slash=strchr_with_endptr(url,urlend,'/');
 at   =strchr_with_endptr(url,urlend,'@');

 proto=NULL; protoend=NULL;

 if(*url=='/')                     /* /dir/... (local) */
   ;
 else if(colon && slash && (colon+1)==slash) /* http:/[/]... */
   {
    proto=url; protoend=colon;
    url=slash+1;
    if(*url=='/') ++url;

    colon=strchr_with_endptr(url,urlend,':');
    slash=strchr_with_endptr(url,urlend,'/');
   }
 else if(colon && !isdigit(*(colon+1)) &&
         (!slash || colon<slash) &&
         (!at || (slash && slash<at)))  /* http:www.foo/...[:@]... */
   {
    proto=url; protoend=colon;
    url=colon+1;

    colon=strchr_with_endptr(url,urlend,':');
   }
 /* else ; */    /* www.foo:80/... */

 /* Username, Password = Url->user, Url->pass */

 user=NULL; userend=NULL;
 pass=NULL; passend=NULL;

 if(at) {
   if(url<at && (!slash || at<slash))
     {
       const char *at2;

       if(colon && colon<at)               /* user:pass@www.foo.com...[/]... */
	 {
	   user=url; userend=colon;
	   pass=colon+1; passend=at;
	   url=at+1;
	 }
       else if(colon && (at2=strchr_with_endptr(at+1,urlend,'@')) && /* user@host:pass@www.foo...[/]... */
	       colon<at2 && at2<slash)            /* [not actually valid, but common]    */
	 {
	   user=url; userend=colon;
	   pass=colon+1; passend=at2;
	   url=at2+1;
	 }
       else                               /* user@www.foo...[:/]... */
	 {
	   user=url; userend=at;
	   url=at+1;
	 }
     }
   else
     {
       if(at==url)             /* @www.foo... */
	 ++url;
     }
 }

 /* Hostname:port = Url->hostport, Pathname = Url->path, Arguments = Url->args */

 slash=strchr_with_endptr(url,urlend,'/');
 ques=strchr_with_endptr(url,urlend,'?');

 hostport=url; hostportend=urlend;
 path=NULL; pathend=urlend;
 args=NULL; argsend=urlend;

 if(ques)                       /* ../path?... */
   {
    hostportend=ques;
    args=ques+1;
    if(slash && slash<ques) {
      hostportend=slash;
      path=slash; pathend=ques;
    }
   }
 else if(slash) {
   hostportend=slash;
   path=slash;
 }


 /* Create the URL */

 {
   char *proto_str=NULL, *hostport_str=NULL, *path_str=NULL,
     *args_str=NULL, *user_str=NULL, *pass_str=NULL;

   if(proto) proto_str=STRDUPA2(proto,protoend);
   if(hostport<hostportend) hostport_str=STRDUPA2(hostport,hostportend);
   if(path) path_str=STRDUPA2(path,pathend);
   if(args) args_str=STRDUPA2(args,argsend);
   if(user) user_str=STRDUPA2(user,userend);
   if(pass) pass_str=STRDUPA2(pass,passend);

   Url=CreateURL(proto_str,hostport_str,path_str,args_str,user_str,pass_str);
 }

 return(Url);
}


/*++++++++++++++++++++++++++++++++++++++
  Make a modified URL data structure from an existing URL
  and a set of alternative component parts.

  URL *MakeModifiedURL Returns a newly allocated URL.

  const URL *Url The Url to copy and modify (may be NULL).

  int modflags Set of flags indicating which components should be replaced.

  const char *proto The alternative protocol (may be NULL).

  const char *hostport The alternative host and port (may be NULL).

  const char *path The alternative path (may be NULL).

  const char *args The alternative args (may be NULL).

  const char *user The alternative username (may be NULL);

  const char *pass The alternative password (may be NULL);
  ++++++++++++++++++++++++++++++++++++++*/

URL *MakeModifiedURL(const URL *Url,int modflags,const char *proto,const char *hostport,const char *path,const char *args,const char *user,const char *pass)
{
 URL *newUrl=(URL*)malloc(sizeof(URL));

 if(!Url) modflags=REPLACEURLALL;

 /* proto = Url->proto */

 if(modflags&REPLACEURLPROTO) {
   newUrl->proto= strdup(proto?proto:"http");
   downcase(newUrl->proto);
 }
 else
   newUrl->proto=strdup(Url->proto);

 /* hostport = Url->hostport */

 if(modflags&REPLACEURLHOSTPORT) {
   char *temp,*p;

   if(hostport)
     temp=URLDecodeGeneric(hostport);
   else {
     char *localhostport=GetLocalHostPort();
     temp=URLDecodeGeneric(localhostport);
     free(localhostport);
   }

   for(p=temp;*p;++p)
     if(isalpha(*p))
       *p=tolower(*p);
     else if(!isalnum(*p) && *p!=':' && *p!='-' &&
	     *p!='.' && *p!='[' && *p!=']')
       {*p=0;break;}

   newUrl->hostport=CanonicaliseHost(temp);

   free(temp);
 }
 else
   newUrl->hostport=strdup(Url->hostport);

 /* path = Url->path */

 if(modflags&REPLACEURLPATH) {
   char *temp;
   temp= URLDecodeGeneric(path?path:"/");
   CanonicaliseName(temp);
   newUrl->path=URLEncodePath(temp);

   free(temp);
 }
 else
   newUrl->path=strdup(Url->path);

 /* args = Url->args */

 if(modflags&REPLACEURLARGS) { 
   if(args && *args)
     {
       newUrl->args=URLRecodeFormArgs(args);
       URLReplaceAmp(newUrl->args);
     }
   else
     newUrl->args=NULL;
 }
 else
   newUrl->args= (Url->args)?strdup(Url->args):NULL;

 /* user, pass = Url->user, Url->pass */

 if(modflags&REPLACEURLUSER)
   newUrl->user= user?URLDecodeGeneric(user):NULL;    /* allow empty usernames */
 else
   newUrl->user=(Url->user)?strdup(Url->user):NULL;

 if(modflags&REPLACEURLPASS)
   newUrl->pass=(pass && *pass)?URLDecodeGeneric(pass):NULL;
 else
   newUrl->pass=(Url->pass)?strdup(Url->pass):NULL;

 if(modflags) {

   /* Hostname, port = Url->host, Url->port */

   {
     char *colon;

     if(*newUrl->hostport=='[')
       {
	 char *square=strchr(newUrl->hostport,']');
	 colon= square? strchr(square+1,':') : NULL;
       }
     else
       colon=strchr(newUrl->hostport,':');

     if(colon)
       {
	 int defport=DefaultPort(newUrl);
	 if(defport && atoi(colon+1)==defport)
	   *colon=0;
       }

     if(colon && *colon)
       {
	 newUrl->host=STRDUP2(newUrl->hostport,colon);
	 newUrl->port=colon+1;
	 newUrl->portnum=atoi(newUrl->port);
       }
     else
       {
	 newUrl->host=newUrl->hostport;
	 newUrl->port=NULL;
	 newUrl->portnum=DefaultPort(newUrl);
       }
   }

   /* Canonical URL = Url->name (and pointers Url->hostp, Url->pathp, Url->pathendp). */

   {
     char *p=(char*)malloc(strlen(newUrl->proto)+strlitlen("://")+
			   strlen(newUrl->hostport)+
			   strlen(newUrl->path)+
			   (newUrl->args?strlen(newUrl->args)+1:0)+
			   1);

     newUrl->name=p;

     p=stpcpy(stpcpy(p,newUrl->proto),"://");

     newUrl->hostp=p;

     p=stpcpy(p,newUrl->hostport);

     newUrl->pathp=p;

     p=stpcpy(p,newUrl->path);

     newUrl->pathendp=p;

     if(newUrl->args) {
       *p++='?';
       p=stpcpy(p,newUrl->args);
     }
   }

   /* File name = Url->file */

   if(newUrl->user)
     {
       char *encuser=URLEncodePassword(newUrl->user);
       char *encpass=(newUrl->pass)?URLEncodePassword(newUrl->pass):NULL;
       char *p=(char*)malloc(strlen(newUrl->name)+
			     strlen(encuser)+
			     (encpass?strlen(encpass)+1:0)+2);

       newUrl->file=p;

       p=mempcpy(p,newUrl->name,newUrl->hostp-newUrl->name);

       p=stpcpy(p,encuser);

       free(encuser);

       if(encpass) {
	 *p++=':';
	 p=stpcpy(p,encpass);

	 free(encpass);
       }

       *p++='@';
       stpcpy(p,newUrl->hostp);
     }
   else
     newUrl->file=newUrl->name;

#ifdef __CYGWIN__
   /* Host directory = Url->private_dir - Private data */

   newUrl->private_dir=NULL;
#endif

   /* Cache filename = Url->private_file - Private data */

   /* newUrl->private_file=NULL; */

   /* Proxyable link = Url->private_link - Private data */

   newUrl->private_link=NULL;

   newUrl->hashvalid=0;
   newUrl->addrvalid=0;
 }
 else {
   /* Clone Url without modifications */

   newUrl->host=(Url->host!=Url->hostport)?strdup(Url->host):newUrl->hostport;
   newUrl->port=(Url->port)?newUrl->hostport+(Url->port-Url->hostport):NULL;
   newUrl->portnum=Url->portnum;

   newUrl->name=strdup(Url->name);
   newUrl->hostp=newUrl->name+(Url->hostp-Url->name);
   newUrl->pathp=newUrl->name+(Url->pathp-Url->name);
   newUrl->pathendp=newUrl->name+(Url->pathendp-Url->name);

   newUrl->file=(Url->file!=Url->name)?strdup(Url->file):newUrl->name;

#ifdef __CYGWIN__
   newUrl->private_dir=Url->private_dir?((Url->private_dir!=Url->hostport)?strdup(Url->private_dir):newUrl->hostport):NULL;
#endif

   /* newUrl->private_file=NULL; */
   newUrl->private_link= Url->private_link?((Url->private_link!=Url->name)?strdup(Url->private_link):newUrl->name):NULL;

   newUrl->hash=Url->hash;
   newUrl->addr=Url->addr;
   newUrl->hashvalid=Url->hashvalid;
   newUrl->addrvalid=Url->addrvalid;
 }

 return(newUrl);
}


/*++++++++++++++++++++++++++++++++++++++
  Change the password in an existing URL.

  URL *Url The URL to add the username and password to.

  const char *user The username.

  const char *pass The password.

  If user and pass are NULL, the username & password are removed.
  ++++++++++++++++++++++++++++++++++++++*/

void ChangePasswordURL(URL *Url,const char *user,const char *pass)
{
 if(Url->user) {
   free(Url->user);
   Url->user=NULL;
 }

 if(Url->pass) {
   free(Url->pass);
   Url->pass=NULL;
 }

 if(user) /* allow empty usernames */
    Url->user=strdup(user);

 if(pass && *pass)
    Url->pass=strdup(pass);

 if(Url->file!=Url->name)
    free(Url->file);

 if(Url->user) {
   char *encuser=URLEncodePassword(Url->user);
   char *encpass=(Url->pass)?URLEncodePassword(Url->pass):NULL;
   char *p=(char*)malloc(strlen(Url->name)+
                         strlen(encuser)+
                         (encpass?strlen(encpass)+1:0)+2);
   Url->file=p;
   p=mempcpy(p,Url->name,Url->hostp-Url->name);

   p=stpcpy(p,encuser);

   free(encuser);

   if(encpass)
     {
       *p++=':';
       p=stpcpy(p,encpass);

       free(encpass);
     }

   *p++='@';
   stpcpy(p,Url->hostp);
 }
 else
   Url->file=Url->name;

 /* Since we have changed the "file" field, a possible cached hash value must 
    be invalidated */

 Url->hashvalid=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Free the memory in a URL.

  URL *Url The URL to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeURL(URL *Url)
{
#ifdef __CYGWIN__
 if(Url->private_dir && Url->private_dir!=Url->hostport)
    free(Url->private_dir);
#endif

 if(Url->private_link && Url->private_link!=Url->name)
    free(Url->private_link);

 if(Url->file!=Url->name)
    free(Url->file);

 free(Url->name);

 if(Url->host!=Url->hostport)
    free(Url->host);

 if(Url->proto)    free(Url->proto);
 if(Url->hostport) free(Url->hostport);
 if(Url->path)     free(Url->path);
 if(Url->args)     free(Url->args);

 if(Url->user)     free(Url->user);
 if(Url->pass)     free(Url->pass);

 free(Url);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a url reference from a page to an absolute one.

  URL *LinkURL Returns a new URL that refers to the link.

  const URL *Url The page that we are looking at.

  const char *link The link from the page.
  ++++++++++++++++++++++++++++++++++++++*/

URL *LinkURL(const URL *Url,const char *link)
{
 URL *newUrl;
 const char *new;
 char *colon=strchr(link,':');
 char *slash=strchr(link,'/');

 if((colon && slash && colon<slash) ||
    /* "mailto:" doesn't follow the rule of ':' before '/'. */
    !strcasecmp_litbeg(link,"mailto:") ||
    /* "javascript:" doesn't follow the rule of ':' before '/'. */
    !strcasecmp_litbeg(link,"javascript:"))
   {
    new=link;
   }
 else if(*link=='#')
   {
    new=Url->name;
   }
 else if(*link=='/')
   {
     if(*(link+1)=='/')
       {
	 char *newstr=(char*)alloca(strlen(Url->proto)+strlen(link)+2);
	 sprintf(newstr,"%s:%s",Url->proto,link);
	 new=newstr;
       }
     else
       {
	 char *newstr=(char*)alloca(strlen(Url->proto)+strlen(Url->hostport)+strlen(link)+4);
	 sprintf(newstr,"%s://%s%s",Url->proto,Url->hostport,link);
	 new=newstr;
       }
   }
 else
   {
    char *p,*q;
    char *newstr=(char*)alloca(strlen(Url->proto)+strlen(Url->hostport)+strlen(Url->path)+strlen(link)+4);
    p=stpcpy(stpcpy(stpcpy(newstr,Url->proto),"://"),Url->hostport);
    q=stpcpy(p,Url->path);

    if(*link)
      {
	while(--q>p)
	  if(*q=='/')
	    break;

	stpcpy(q+1,link);

	CanonicaliseName(p);
      }

    new=newstr;
   }

 newUrl=SplitURL(new);

 return(newUrl);
}


/*++++++++++++++++++++++++++++++++++++++
  Canonicalise a hostname by converting to lower case, filling in the zeros for IPv6 and converting decimal to dotted quad.

  char *CanonicaliseHost Returns the a newly allocated canonical string for the host.

  const char *host The original host address.
  ++++++++++++++++++++++++++++++++++++++*/

char *CanonicaliseHost(const char *host)
{
 char *newhost;
 int hasletter=0,hasdot=0,hascolon=0;

 {
   const char *p;
   for(p=host;*p;++p) {
     if(isalpha(*p))
       ++hasletter;
     else if(*p=='.')
       ++hasdot;
     else if(*p==':')
       ++hascolon;
   }
 }

 if(*host=='[' || hascolon>=2)
   {
    unsigned int ipv6[8];
    int cs=0,ce=7;
    const char *ps,*pe,*port=NULL;

    ps=host;
    pe=strchrnul(host,0)-1;

    if(*host=='[') {
      ++ps;
      if(!(pe=strchr(ps,']'))) goto error_return;
      if(*(pe+1)==':') {
	port=pe+2;
	if(!*port) port=NULL;
      }
      --pe;
    }

    for(;;) {
      if(!*ps || *ps==']') goto fill_in_zeros;
      if(cs>ce) goto error_return;
      if(*ps==':') break;
      {
	unsigned int ipv4[4]={0,0,0,0};

	if(sscanf(ps,"%u.%u.%u.%u",&ipv4[0],&ipv4[1],&ipv4[2],&ipv4[3])==4)
	  {
	    if(cs>=ce) goto fill_in_zeros;
	    ipv6[cs++]=ipv4[0]*256+ipv4[1];
	    ipv6[cs++]=ipv4[2]*256+ipv4[3];
	  }
	else if(sscanf(ps,"%x",&ipv6[cs])==1)
          ++cs;
	else
	  goto error_return;

	for(;;) {
	  ++ps;
	  if(!*ps || *ps==']') goto fill_in_zeros;
	  if(*ps==':') {++ps; break;}
	}
      }
    }

    while(pe>ps && *pe!=':') {
      if(cs>ce) goto error_return;
      do {--pe; if(pe<ps) goto error_return;} while(*pe!=':');
      {
	unsigned int ipv4[4]={0,0,0,0};

	if(sscanf(pe+1,"%u.%u.%u.%u",&ipv4[0],&ipv4[1],&ipv4[2],&ipv4[3])==4)
	  {
	    if(cs>=ce) goto error_return;
	    ipv6[ce--]=ipv4[2]*256+ipv4[3];
	    ipv6[ce--]=ipv4[0]*256+ipv4[1];
	  }
	else if(sscanf(pe+1,"%x",&ipv6[ce])==1)
	  --ce;
	else
	  goto error_return;

	--pe;
      }
    }

   fill_in_zeros:
    for(;cs<=ce;cs++)
       ipv6[cs]=0;

    for(cs=0;cs<8;cs++)
       ipv6[cs]&=0xffff;

    newhost= port?
      x_asprintf("[%x:%x:%x:%x:%x:%x:%x:%x]:%u",
		 ipv6[0],ipv6[1],ipv6[2],ipv6[3],ipv6[4],ipv6[5],ipv6[6],ipv6[7],
		 (unsigned int)strtoul(port,NULL,0)&0xffff):
      x_asprintf("[%x:%x:%x:%x:%x:%x:%x:%x]",
		 ipv6[0],ipv6[1],ipv6[2],ipv6[3],ipv6[4],ipv6[5],ipv6[6],ipv6[7]);
   }
 else if(hasletter) {
   newhost=strdup(host);
   downcase(newhost);
 }
 else
   {
    unsigned int ipv4[4];
    const char *port;

    if(hasdot==0)
      {
       unsigned long decimal=strtoul(host,NULL,0);

       ipv4[3]=decimal&0xff; decimal>>=8;
       ipv4[2]=decimal&0xff; decimal>>=8;
       ipv4[1]=decimal&0xff; decimal>>=8;
       ipv4[0]=decimal&0xff;
      }
    else
      {
       ipv4[0]=ipv4[1]=ipv4[2]=ipv4[3]=0;
       sscanf(host,"%u.%u.%u.%u",&ipv4[0],&ipv4[1],&ipv4[2],&ipv4[3]);

       ipv4[0]&=0xff;
       ipv4[1]&=0xff;
       ipv4[2]&=0xff;
       ipv4[3]&=0xff;
      }

    port=strchr(host,':');
    if(port) {
      ++port;
      if(!*port)
	port=NULL;
    }

    newhost= port?
      x_asprintf("%u.%u.%u.%u:%u",ipv4[0],ipv4[1],ipv4[2],ipv4[3],
		 (unsigned int)strtoul(port,NULL,0)&0xffff):
      x_asprintf("%u.%u.%u.%u",ipv4[0],ipv4[1],ipv4[2],ipv4[3]);
   }

 return(newhost);

error_return:
 /* If we were to do this properly, we would
    throw some kind of exception. */
 return(strdup(host));
}


/*++++++++++++++++++++++++++++++++++++++
  Canonicalise a file name by removing '/../', '/./' and '//' references.

  char *name The original name, returned modified inplace.

  The same function is used in WWWOFFLE and cxref with changes for files or URLs.
  ++++++++++++++++++++++++++++++++++++++*/

void CanonicaliseName(char *name)
{
 char *match,*name2;

 match=name;
 while((match=strstr(match,"/./")) || !strncmp(match=name,"./",2))
   {
    char *prev=match, *next=match+2;
    while((*prev++=*next++));
   }

#if 0 /* as used in cxref */

 match=name;
 while((match=strstr(match,"//")))
   {
    char *prev=match, *next=match+1;
    while((*prev++=*next++));
   }

#endif

 match=name2=name;
 while((match=strstr(match,"/../")))
   {
    char *prev=match, *next=match+4;
    if((prev-name2)==2 && !strncmp(name2,"../",3))
      {name2+=3;match++;continue;}
    while(prev>name2 && *--prev!='/');
    match=prev;
    if(*prev=='/')prev++;
    while((*prev++=*next++));
   }

 match=&name[strlen(name)-2];
 if(match>=name && !strcmp(match,"/."))
    *match=0;

 match=&name[strlen(name)-3];
 if(match>=name && !strcmp(match,"/.."))
   {
    if(match==name)
       *++match=0;
    else
       while(match>name && *--match!='/')
          *match=0;
   }

#if 0 /* as used in cxref */

 match=&name[strlen(name)-1];
 if(match>name && !strcmp(match,"/"))
    *match=0;

 if(!*name)
    *name='.',*(name+1)=0;

#else /* as used in wwwoffle */

 if(!*name || !strncmp(name,"../",3))
    *name='/',*(name+1)=0;

#endif
}
