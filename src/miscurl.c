/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/miscurl.c 2.86 2004/09/01 18:02:34 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8d.
  Miscellaneous HTTP / HTML Url Handling functions.
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
#include <ctype.h>

#include "misc.h"
#include "config.h"
#include "proto.h"


/*++++++++++++++++++++++++++++++++++++++
  Split a URL into a protocol, hostname, path name and an argument list.

  URL *SplitURL Returns a URL structure containing the information.

  char *url The name of the url to split.
  ++++++++++++++++++++++++++++++++++++++*/

URL *SplitURL(const char *url)
{
 URL *Url=(URL*)malloc(sizeof(URL));
 const char *colon,*slash,*at,*ques,*semi,*pathbeg,*pathend;

 /* Protocol */

 colon=strchr(url,':');
 slash=strchrnul(url,'/');
 at   =strchr(url,'@');

 if(*url=='/')                     /* /dir/... (local) */
   {
    Url->proto=strdup("http");
   }
 else if(colon && *slash && (colon+1)==slash) /* http://... */
   {
    Url->proto=STRDUP2(url,colon);
    url=slash+1;
    if(*url=='/') ++url;

    colon=strchr(url,':');
    slash=strchrnul(url,'/');
   }
 else if(colon && !isdigit(*(colon+1)) &&
         (colon<slash) &&
         (!at || (at>slash)))  /* http:www.foo.com/...[:@]... */
   {
    Url->proto=STRDUP2(url,colon);
    url=colon+1;

    colon=strchr(url,':');
   }
 else                                   /* www.foo.com:80/... */
   {
    Url->proto=strdup("http");
   }

 downcase(Url->proto);

 Url->Protocol=NULL;

 {
   int i;
   for(i=0;i<NProtocols;++i)
     if(!strcmp(Protocols[i].name,Url->proto))
       {Url->Protocol=&Protocols[i]; break;}
 }

 /* Password */

 Url->user=NULL;
 Url->pass=NULL;

 if(at) {
   if(url<at && at<slash)
     {
       char *at2;

       if(colon && colon<at)               /* user:pass@www.foo.com...[/]... */
	 {
	   Url->user=STRDUP3(url,colon,URLDecodeGeneric);
	   Url->pass=STRDUP3(colon+1,at,URLDecodeGeneric);
	   url=at+1;
	 }
       else if(colon && (at2=strchr(at+1,'@')) && /* user@host:pass@www.foo.com...[/]... */
	       colon<at2 && at2<slash)            /* [not actually valid, but common]    */
	 {
	   Url->user=STRDUP3(url,colon,URLDecodeGeneric);
	   Url->pass=STRDUP3(colon+1,at2,URLDecodeGeneric);
	   url=at2+1;
	 }
       else                               /* user@www.foo.com...[:/]... */
	 {
	   Url->user=STRDUP3(url,at,URLDecodeGeneric);
	   url=at+1;
	 }
     }
   else
     {
       if(at==url)             /* @www.foo.com... */
	 ++url;
     }
 }

 /* Parameters */

 semi=strchrnul(url,';');
 ques=strchrnul(url,'?');
 pathend=ques;

 Url->params=NULL;

 if(semi<ques) /* ../path;...[?]... */
   {
     pathend=semi;
     Url->params=STRDUP3(semi+1,ques,URLRecodeFormArgs);
   }

 /* Arguments */

 if(*ques)                       /* ../path?... */
   {
    Url->args=URLRecodeFormArgs(ques+1);
    URLReplaceAmp(Url->args);
   }
 else
    Url->args=NULL;

 /* Hostname */

 if(*url=='/')              /* /path/... (local) */
   {
    Url->local=1;
    pathbeg=url;
    Url->hostport=GetLocalHost(1);
   }
 else                           /* www.foo.com... */
   {
    Url->local=0;

    if(slash<pathend) /* www.foo.com/...[?]... */
      pathbeg=slash;
    else                        /* www.foo.com[?]... */
      pathbeg=pathend;

    Url->hostport=STRDUP2(url,pathbeg);

    if(Url->Protocol==&Protocols[0] && IsLocalHostPort(Url->hostport))
      {
	free(Url->hostport);
	Url->hostport=GetLocalHost(1);
	Url->local=1;
      }
   }

 /* Pathname */

 if(pathbeg<pathend)
   {
     char *temppath=STRDUP3(pathbeg,pathend,URLDecodeGeneric);
     CanonicaliseName(temppath);
     Url->path=URLEncodePath(temppath);
     free(temppath);
   }
 else
   Url->path=strdup("/");

 /* Hostname (cont) */

 {
   char *canonicalisedhost=CanonicaliseHost(Url->hostport);
   if(canonicalisedhost!=Url->hostport) {
     free(Url->hostport);
     Url->hostport=canonicalisedhost;
   }
 }

 {
   char *p=Url->hostport;
   int defport=(Url->Protocol?Url->Protocol->defport:80);

   Url->host=p;
   Url->port=NULL;
   Url->portnum=defport;

   if(*p=='[') {
     ++p;
     while(*p) {
       if(*p == ']') {
	 Url->host=STRDUP2(Url->hostport+1,p);
	 ++p;
	 break;
       }
       ++p;
     }
   }
   else
     while(*p && *p!=':') ++p;

   if(*p==':') {
     Url->portnum=atoi(p+1);
     if(Url->portnum==defport) {
       *p=0;
     }	 
     else {
       if(Url->host==Url->hostport)
	 Url->host=STRDUP2(Url->hostport,p);
       Url->port=p+1;
     }
   }
 }

 /* Canonicalise the URL. */

 {
   char *p=(char*)malloc(strlen(Url->proto)+strlitlen("://")+
                         strlen(Url->hostport)+
                         strlen(Url->path)+
                         (Url->params?strlen(Url->params)+1:0)+
                         (Url->args?strlen(Url->args)+1:0)+
                         1);

   Url->name=p;

   p=stpcpy(stpcpy(p,Url->proto),"://");

   Url->hostp=p;

   p=stpcpy(p,Url->hostport);

   Url->pathp=p;

   p=stpcpy(p,Url->path);

   Url->pathendp=p;

   if(Url->params && *Url->params) {
     *p++=';';
     p=stpcpy(p,Url->params);
   }

   if(Url->args && *Url->args) {
     *p++='?';
     p=stpcpy(p,Url->args);
   }
 }

 if(Url->user)
   {
    char *encuser=URLEncodePassword(Url->user);
    char *encpass=(Url->pass)?URLEncodePassword(Url->pass):NULL;
    char *p=(char*)malloc(strlen(Url->name)+
			  strlen(encuser)+
			  (encpass?strlen(encpass)+1:0)+2);

    Url->file=p;

    p=mempcpy(p,Url->name,Url->hostp-Url->name);

    p=stpcpy(p,encuser);

    free(encuser);

    if(encpass) {
      *p++=':';
      p=stpcpy(p,encpass);

      free(encpass);
    }

    *p++='@';
    stpcpy(p,Url->hostp);
   }
 else
    Url->file=Url->name;

 if(Url->Protocol && !Url->Protocol->proxyable)
   {
    char *localhost=GetLocalHost(1);
    Url->link= x_asprintf("http://%s/%s/%s",localhost,Url->proto,Url->hostp);
    free(localhost);
   }
 else
    Url->link=Url->name;

 Url->dir=Url->hostport;

#if defined(__CYGWIN__)
 if(strchr(Url->hostport,':'))
   {
    char *p,*q;
    Url->dir=(char*)malloc(strlen(Url->hostport)+1);

    for(p=Url->dir,q=Url->hostport; *q; ++p,++q) {
       if(*q==':') *p='!'; else *p=*q;
    }

    *p=0;
   }
#endif

 Url->hash=NULL;

 /* end */

 return(Url);
}


/*++++++++++++++++++++++++++++++++++++++
  Change the password in an existing URL.

  URL *Url The URL to add the username and password to.

  char *user The username.

  char *pass The password.

  If user and pass are NULL, the username & password are removed.
  ++++++++++++++++++++++++++++++++++++++*/

void ChangeURLPassword(URL *Url,char *user,char *pass)
{
 if(Url->user) {
   free(Url->user);
   Url->user=NULL;
 }

 if(user)
   Url->user=strdup(user);

 if(Url->pass) {
   free(Url->pass);
   Url->pass=NULL;
 }

 if(pass)
   Url->pass=strdup(pass);

 if(Url->file!=Url->name)
    free(Url->file);

 if(user) {
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

 if(Url->hash) {
   free(Url->hash);
   Url->hash=NULL;
 }
}


/*++++++++++++++++++++++++++++++++++++++
  Copy a URL to newly allocated memory.

  URL *Url The URL to copy.
  ++++++++++++++++++++++++++++++++++++++*/

URL *CopyURL(URL *Url)
{
  URL *copy=(URL*)malloc(sizeof(URL));

  copy->name=strdup(Url->name);

  copy->link=(Url->link!=Url->name)?strdup(Url->link):copy->name;

  copy->file=(Url->file!=Url->name)?strdup(Url->file):copy->name;

  copy->hostp=copy->name+(Url->hostp-Url->name);
  copy->pathp=copy->name+(Url->pathp-Url->name);
  copy->pathendp=copy->name+(Url->pathendp-Url->name);

  copy->proto=strdup(Url->proto);
  copy->hostport=strdup(Url->hostport);
  copy->host=(Url->host!=Url->hostport)?strdup(Url->host):copy->hostport;
  copy->port=(Url->port)?copy->hostport+(Url->port-Url->hostport):NULL;
  copy->portnum=Url->portnum;
  copy->path=strdup(Url->path);
  copy->params=(Url->params)?strdup(Url->params):NULL;
  copy->args=(Url->args)?strdup(Url->args):NULL;

  copy->Protocol=Url->Protocol;

  copy->user=(Url->user)?strdup(Url->user):NULL;
  copy->pass=(Url->pass)?strdup(Url->pass):NULL;

  copy->dir=(Url->dir!=Url->hostport)?strdup(Url->dir):copy->hostport;

  copy->hash=(Url->hash)?strdup(Url->hash):NULL;

  copy->local=Url->local;

  return copy;
}


/*++++++++++++++++++++++++++++++++++++++
  Free the memory in a URL.

  URL *Url The URL to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeURL(URL *Url)
{
 if(Url->link!=Url->name)
    free(Url->link);

 if(Url->file!=Url->name)
    free(Url->file);

 if(Url->dir!=Url->hostport)
    free(Url->dir);

 free(Url->name);

 free(Url->proto);
 if(Url->host!=Url->hostport) free(Url->host);
 free(Url->hostport);
 free(Url->path);
 if(Url->params) free(Url->params);
 if(Url->args)   free(Url->args);

 if(Url->user)   free(Url->user);
 if(Url->pass)   free(Url->pass);

 if(Url->hash)   free(Url->hash);

 free(Url);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a url reference from a page to an absolute one.

  char *LinkURL Return the linked to URL or the original link if unchanged.

  URL *Url The page that we are looking at.

  char *link The link from the page.
  ++++++++++++++++++++++++++++++++++++++*/

char *LinkURL(URL *Url,char *link)
{
 char *new=link;
 char *colon=strchr(link,':');
 char *slash=strchr(link,'/');

 if((colon && slash && colon<slash) ||
    /* "mailto:" doesn't follow the rule of ':' before '/'. */
    !strcasecmp_litbeg(link,"mailto:"))
    ;
 else if(*link=='/')
   {
     if(*(link+1)=='/')
       {
	 new=(char*)malloc(strlen(Url->proto)+strlen(link)+2);
	 sprintf(new,"%s:%s",Url->proto,link);
       }
     else
       {
	 new=(char*)malloc(strlen(Url->proto)+strlen(Url->hostport)+strlen(link)+4);
	 sprintf(new,"%s://%s%s",Url->proto,Url->hostport,link);
       }
   }
 else
   {
    char *p,*q;
    new=(char*)malloc(strlen(Url->proto)+strlen(Url->hostport)+strlen(Url->path)+strlen(link)+4);
    p=stpcpy(stpcpy(stpcpy(new,Url->proto),"://"),Url->hostport);
    q=stpcpy(p,Url->path);

    if(*link)
      {
	while(--q>new)
	  if(*q=='/')
	    break;

	stpcpy(q+1,link);

	CanonicaliseName(p);
      }
   }

 return(new);
}


/*++++++++++++++++++++++++++++++++++++++
  Canonicalise a hostname by converting to lower case, filling in the zeros for IPv6 and converting decimal to dotted quad.

  char *CanonicaliseHost Returns the original argument or a new malloced one.

  char *host The original host address.
  ++++++++++++++++++++++++++++++++++++++*/

char *CanonicaliseHost(char *host)
{
 int hasletter=0,hasdot=0,hascolon=0;

 {
   char *p;
   for(p=host;*p;++p) {
     if(isalpha(*p)) {
       ++hasletter;
       *p=tolower(*p);
     }
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
    char *ps,*pe,*port=NULL;

    ps=host;
    pe=strchrnul(host,0)-1;

    if(*host=='[') {
      ++ps;
      if(!(pe=strchr(ps,']'))) return(host);
      if(*(pe+1)==':') {
	port=pe+2;
	if(!*port) port=NULL;
      }
      --pe;
    }

    for(;;) {
      if(!*ps || *ps==']') goto fill_in_zeros;
      if(cs>ce) return(host);
      if(*ps==':') break;
      {
	unsigned int ipv4[4]={0,0,0,0};

	if(sscanf(ps,"%u.%u.%u.%u",&ipv4[0],&ipv4[1],&ipv4[2],&ipv4[3])==4)
	  {
	    if(cs>=ce) return(host);
	    ipv6[cs++]=ipv4[0]*256+ipv4[1];
	    ipv6[cs++]=ipv4[2]*256+ipv4[3];
	  }
	else if(sscanf(ps,"%x",&ipv6[cs])==1)
          ++cs;
	else
	  return(host);

	for(;;) {
	  ++ps;
	  if(!*ps || *ps==']') goto fill_in_zeros;
	  if(*ps==':') {++ps; break;}
	}
      }
    }
      
    while(pe>ps && *pe!=':') {
      if(cs>ce) return(host);
      do {--pe; if(pe<ps) return(host);} while(*pe!=':');
      {
	unsigned int ipv4[4]={0,0,0,0};

	if(sscanf(pe+1,"%u.%u.%u.%u",&ipv4[0],&ipv4[1],&ipv4[2],&ipv4[3])==4)
	  {
	    if(cs>=ce) return(host);
	    ipv6[ce--]=ipv4[2]*256+ipv4[3];
	    ipv6[ce--]=ipv4[0]*256+ipv4[1];
	  }
	else if(sscanf(pe+1,"%x",&ipv6[ce])==1)
	  --ce;
	else
	  return(host);

	--pe;
      }
    }

   fill_in_zeros:
    for(;cs<=ce;cs++)
       ipv6[cs]=0;

    for(cs=0;cs<8;cs++)
       ipv6[cs]&=0xffff;

    return port?
      x_asprintf("[%x:%x:%x:%x:%x:%x:%x:%x]:%u",
		 ipv6[0],ipv6[1],ipv6[2],ipv6[3],ipv6[4],ipv6[5],ipv6[6],ipv6[7],
		 (unsigned int)strtoul(port,NULL,0)&0xffff):
      x_asprintf("[%x:%x:%x:%x:%x:%x:%x:%x]",
		 ipv6[0],ipv6[1],ipv6[2],ipv6[3],ipv6[4],ipv6[5],ipv6[6],ipv6[7]);
   }
 else if(hasletter)
    return(host);
 else
   {
    unsigned int ipv4[4];
    char *port;

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

    return port?
      x_asprintf("%u.%u.%u.%u:%u",ipv4[0],ipv4[1],ipv4[2],ipv4[3],
		 (unsigned int)strtoul(port,NULL,0)&0xffff):
      x_asprintf("%u.%u.%u.%u",ipv4[0],ipv4[1],ipv4[2],ipv4[3]);
   }
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
