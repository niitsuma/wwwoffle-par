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

URL *SplitURL(char *url)
{
 URL *Url=(URL*)malloc(sizeof(URL));
 char *copyurl,*mallocurl=malloc(strlen(url)+2);
 int i=0,n=0;
 char *colon,*slash,*at,*ques,*semi,*temp,root[2];

 copyurl=mallocurl;
 strcpy(copyurl,url);

 /* Protocol */

 colon=strchr(copyurl,':');
 slash=strchr(copyurl,'/');
 at   =strchr(copyurl,'@');

 if(slash==copyurl)                     /* /dir/... (local) */
   {
    Url->proto=(char*)malloc(5);
    strcpy(Url->proto,"http");
   }
 else if(colon && slash && (colon+1)==slash) /* http://... */
   {
    *colon=0;
    Url->proto=(char*)malloc(colon-copyurl+1);
    strcpy(Url->proto,copyurl);
    copyurl=slash+1;
    if(*copyurl=='/')
       copyurl++;

    colon=strchr(copyurl,':');
    slash=strchr(copyurl,'/');
   }
 else if(colon && !isdigit(*(colon+1)) &&
         (!slash || colon<slash) &&
         (!at || (slash && at>slash)))  /* http:www.foo.com/...[:@]... */
   {
    *colon=0;
    Url->proto=(char*)malloc(colon-copyurl+1);
    strcpy(Url->proto,copyurl);
    copyurl=colon+1;

    colon=strchr(copyurl,':');
   }
 else                                   /* www.foo.com:80/... */
   {
    Url->proto=(char*)malloc(5);
    strcpy(Url->proto,"http");
   }

 for(i=0;Url->proto[i];i++)
    Url->proto[i]=tolower(Url->proto[i]);

 Url->Protocol=NULL;

 for(i=0;i<NProtocols;i++)
    if(!strcmp(Protocols[i].name,Url->proto))
       Url->Protocol=&Protocols[i];

 /* Password */

 if(at && at>copyurl && (!slash || slash>at))
   {
    char *at2;

    if(colon && at>colon)               /* user:pass@www.foo.com...[/]... */
      {
       *colon=0;
       Url->user=URLDecodeGeneric(copyurl);
       *at=0;
       Url->pass=URLDecodeGeneric(colon+1);
       copyurl=at+1;
      }
    else if(colon && (at2=strchr(at+1,'@')) && /* user@host:pass@www.foo.com...[/]... */
            at2>colon && at2<slash)            /* [not actually valid, but common]    */
      {
       *colon=0;
       Url->user=URLDecodeGeneric(copyurl);
       *at2=0;
       Url->pass=URLDecodeGeneric(colon+1);
       copyurl=at2+1;
      }
    else                               /* user@www.foo.com...[:/]... */
      {
       *at=0;
       Url->user=URLDecodeGeneric(copyurl);
       Url->pass=NULL;
       copyurl=at+1;
      }
   }
 else
   {
    if(at==copyurl)             /* @www.foo.com... */
       copyurl++;

    Url->user=NULL;
    Url->pass=NULL;
   }

 /* Parameters */

 semi=strchr(copyurl,';');
 ques=strchr(copyurl,'?');

 Url->params=NULL;

 if(semi && (!ques || semi<ques)) /* ../path;...[?]... */
   {
    *semi++=0;

    if(ques)
       *ques=0;

    if(*semi)
       Url->params=URLRecodeFormArgs(semi);
   }

 /* Arguments */

 if(ques)                       /* ../path?... */
   {
    *ques++=0;
    Url->args=URLRecodeFormArgs(ques);
    URLReplaceAmp(Url->args);
   }
 else
    Url->args=NULL;

 /* Hostname */

 if(*copyurl=='/')              /* /path/... (local) */
   {
    Url->host=GetLocalHost(1);
    Url->local=1;
   }
 else                           /* www.foo.com... */
   {
    Url->host=copyurl;
    Url->local=0;

    if(slash && (!ques || slash<ques)) /* www.foo.com/...[?]... */
       copyurl=slash;
    else                        /* www.foo.com[?]... */
      {root[0]='/';root[1]=0;copyurl=root;}
   }

 /* Pathname */

 temp=URLDecodeGeneric(copyurl);
 CanonicaliseName(temp);
 Url->path=URLEncodePath(temp);
 free(temp);

 /* Hostname (cont) */

 if(!Url->local)
   {
    *copyurl=0;
    copyurl=Url->host;
    Url->host=(char*)malloc(strlen(copyurl)+1);
    strcpy(Url->host,copyurl);
   }

 for(i=0;Url->host[i];i++)
    if(!isalnum(Url->host[i]) && Url->host[i]!=':' && Url->host[i]!='-' &&
       Url->host[i]!='.' && Url->host[i]!='[' && Url->host[i]!=']')
      {Url->host[i]=0;break;}

 if(!Url->host[0])
   {
    Url->host=GetLocalHost(1);
    Url->local=1;
   }

 if(*Url->host=='[')
   {
    for(i=0;Url->host[i] && Url->host[i]!=']';i++)
       ;
    i++;
   }
 else
    for(i=0;Url->host[i] && Url->host[i]!=':';i++)
       ;

 if(Url->host[i]==':')
    if(atoi(&Url->host[i+1])==(Url->Protocol?Url->Protocol->defport:80))
       Url->host[i]=0;

 if(!Url->local && IsLocalHost(Url->host,1) && Url->Protocol && Url->Protocol==&Protocols[0])
   {
    free(Url->host);
    Url->host=GetLocalHost(1);
    Url->local=1;
   }

 temp=Url->host;
 Url->host=CanonicaliseHost(Url->host);
 if(Url->host!=temp)
    free(temp);

 /* Canonicalise the URL. */

 Url->name=(char*)malloc(strlen(Url->proto)+
                         strlen(Url->host)+
                         strlen(Url->path)+
                         (Url->params?strlen(Url->params):0)+
                         (Url->args?strlen(Url->args):0)+
                         8);

 strcpy(Url->name,Url->proto);
 n=strlen(Url->proto);
 strcpy(Url->name+n,"://");
 n+=3;

 Url->hostp=Url->name+n;

 strcpy(Url->name+n,Url->host);
 n+=strlen(Url->host);

 Url->pathp=Url->name+n;

 strcpy(Url->name+n,Url->path);
 n+=strlen(Url->path);

 if(Url->params && *Url->params)
   {
    strcpy(Url->name+n,";");
    strcpy(Url->name+n+1,Url->params);
    n+=strlen(Url->params)+1;
   }

 if(Url->args && *Url->args)
   {
    strcpy(Url->name+n,"?");
    strcpy(Url->name+n+1,Url->args);
   }

 if(Url->user)
   {
    char *encuserpass;

    Url->file=(char*)malloc(strlen(Url->name)+
                            3*strlen(Url->user)+
                            (Url->pass?3*strlen(Url->pass):0)+
                            8);

    n=Url->hostp-Url->name;
    strncpy(Url->file,Url->name,n);

    encuserpass=URLEncodePassword(Url->user);

    strcpy(Url->file+n,encuserpass);
    n+=strlen(encuserpass);

    free(encuserpass);

    if(Url->pass)
      {
       encuserpass=URLEncodePassword(Url->pass);

       strcpy(Url->file+n,":");
       strcpy(Url->file+n+1,encuserpass);
       n+=strlen(encuserpass)+1;

       free(encuserpass);
      }

    strcpy(Url->file+n,"@");
    strcpy(Url->file+n+1,Url->hostp);
   }
 else
    Url->file=Url->name;

 if(Url->Protocol && !Url->Protocol->proxyable)
   {
    char *localhost=GetLocalHost(1);
    Url->link=(char*)malloc(strlen(Url->name)+strlen(localhost)+8);
    sprintf(Url->link,"http://%s/%s/%s",localhost,Url->proto,Url->hostp);
    free(localhost);
   }
 else
    Url->link=Url->name;

 Url->dir=Url->host;

#if defined(__CYGWIN__)
 if(strchr(Url->host,':'))
   {
    Url->dir=(char*)malloc(strlen(Url->host)+1);

    for(i=0;Url->host[i];i++)
       if(Url->host[i]==':')
          Url->dir[i]='!';
       else
          Url->dir[i]=Url->host[i];

    Url->dir[i]=0;
   }
#endif

 /* end */

 free(mallocurl);

 return(Url);
}


/*++++++++++++++++++++++++++++++++++++++
  Add a password to an existing URL.

  URL *Url The URL to add the username and password to.

  char *user The username.

  char *pass The password.
  ++++++++++++++++++++++++++++++++++++++*/

void AddURLPassword(URL *Url,char *user,char *pass)
{
 int n;
 char *encuserpass;

 if(Url->user)
    free(Url->user);

 Url->user=(char*)malloc(strlen(user)+1);
 strcpy(Url->user,user);

 if(Url->pass)
    free(Url->pass);
 Url->pass=NULL;

 if(pass)
   {
    Url->pass=(char*)malloc(strlen(pass)+1);
    strcpy(Url->pass,pass);
   }

 Url->file=(char*)malloc(strlen(Url->name)+
                         3*strlen(user)+
                         (pass?3*strlen(pass):0)+8);

 n=Url->hostp-Url->name;
 strncpy(Url->file,Url->name,n);

 encuserpass=URLEncodePassword(Url->user);

 strcpy(Url->file+n,encuserpass);
 n+=strlen(encuserpass);

 free(encuserpass);

 if(Url->pass)
   {
    encuserpass=URLEncodePassword(Url->pass);

    strcpy(Url->file+n,":");
    strcpy(Url->file+n+1,encuserpass);
    n+=strlen(encuserpass)+1;

    free(encuserpass);
   }

 strcpy(Url->file+n,"@");
 strcpy(Url->file+n+1,Url->hostp);
}


/*++++++++++++++++++++++++++++++++++++++
  Free the memory in a URL.

  URL *Url The URL to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeURL(URL *Url)
{
 if(Url->name!=Url->link)
    free(Url->link);

 if(Url->name!=Url->file)
    free(Url->file);

 if(Url->dir!=Url->host)
    free(Url->dir);

 free(Url->name);

 if(Url->proto)  free(Url->proto);
 if(Url->host)   free(Url->host);
 if(Url->path)   free(Url->path);
 if(Url->params) free(Url->params);
 if(Url->args)   free(Url->args);

 if(Url->user)   free(Url->user);
 if(Url->pass)   free(Url->pass);

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

 if(colon && slash && colon<slash)
    ;
 else if(*link=='/' && *(link+1)=='/')
   {
    new=(char*)malloc(strlen(Url->proto)+strlen(link)+2);
    sprintf(new,"%s:%s",Url->proto,link);
   }
 else if(*link=='/')
   {
    new=(char*)malloc(strlen(Url->proto)+strlen(Url->host)+strlen(link)+4);
    sprintf(new,"%s://%s%s",Url->proto,Url->host,link);
   }
 else if(!strncasecmp(link,"mailto:",7))
    /* "mailto:" doesn't follow the rule of ':' before '/'. */ ;
 else
   {
    int j;
    new=(char*)malloc(strlen(Url->proto)+strlen(Url->host)+strlen(Url->path)+strlen(link)+4);
    sprintf(new,"%s://%s%s",Url->proto,Url->host,Url->path);

    if(*link)
      {
       for(j=strlen(new)-1;j>0;j--)
          if(new[j]=='/')
             break;

       strcpy(new+j+1,link);

       CanonicaliseName(new+strlen(Url->proto)+3+strlen(Url->host));
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
 int i;

 for(i=0;host[i];i++)
   {
    if(isalpha(host[i]))
      {
       hasletter++;
       host[i]=tolower(host[i]);
      }
    else if(host[i]=='.')
       hasdot++;
    else if(host[i]==':')
       hascolon++;
   }

 if(*host=='[' || hascolon>=2)
   {
    unsigned int ipv6[8],port=0;
    int cs=0,ce=7;
    char *newhost=(char*)malloc(48),*ps,*pe;

    ps=host;
    if(*host=='[')
       ps++;

    while(*ps && *ps!=':' && *ps!=']')
      {
       unsigned int ipv4[4];
       ipv4[0]=ipv4[1]=ipv4[2]=ipv4[3]=0;

       if(cs<=6 && sscanf(ps,"%u.%u.%u.%u",&ipv4[0],&ipv4[1],&ipv4[2],&ipv4[3])==4)
         {
          ipv6[cs++]=ipv4[0]*256+ipv4[1];
          ipv6[cs++]=ipv4[2]*256+ipv4[3];
         }
       else if(cs<=7 && sscanf(ps,"%x",&ipv6[cs])==1)
          cs++;
       else if(*(ps+1)==':' || *(ps+1)==']')
          break;

       while(*ps && *ps!=':' && *ps!=']')
          ps++;
       ps++;
      }

    pe=host+strlen(host)-1;

    if(*host=='[')
      {
       while(pe>host && *pe!=']')
          pe--;
       if(*(pe+1)==':')
          port=strtoul(pe+2,NULL,0);
       pe--;
      }

    if(*pe!=':')
       do
         {
          unsigned int ipv4[4];
          ipv4[0]=ipv4[1]=ipv4[2]=ipv4[3]=0;

          while(pe>host && *pe!=':')
             pe--;

          if(ce>=1 && sscanf(pe+1,"%u.%u.%u.%u",&ipv4[0],&ipv4[1],&ipv4[2],&ipv4[3])==4)
            {
             ipv6[ce--]=ipv4[2]*256+ipv4[3];
             ipv6[ce--]=ipv4[0]*256+ipv4[1];
            }
          else if(ce>=0 && sscanf(pe+1,"%x",&ipv6[ce])==1)
             ce--;
          else
             break;

          pe--;
         }
       while(pe>host && *pe!=':');

    for(;cs<=ce;cs++)
       ipv6[cs]=0;

    for(cs=0;cs<8;cs++)
       ipv6[cs]&=0xffff;

    if(port)
       sprintf(newhost,"[%x:%x:%x:%x:%x:%x:%x:%x]:%u",
               ipv6[0],ipv6[1],ipv6[2],ipv6[3],ipv6[4],ipv6[5],ipv6[6],ipv6[7],port&0xffff);
    else
       sprintf(newhost,"[%x:%x:%x:%x:%x:%x:%x:%x]",
               ipv6[0],ipv6[1],ipv6[2],ipv6[3],ipv6[4],ipv6[5],ipv6[6],ipv6[7]);

    return(newhost);
   }
 else if(hasletter)
    return(host);
 else
   {
    unsigned int ipv4[4],port=0;
    char *colon=strchr(host,':');
    char *newhost=(char*)malloc(24);

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

    if(colon && *(colon+1))
       port=strtoul(colon+1,NULL,0);

    if(port)
       sprintf(newhost,"%u.%u.%u.%u:%u",ipv4[0],ipv4[1],ipv4[2],ipv4[3],port&0xffff);
    else
       sprintf(newhost,"%u.%u.%u.%u",ipv4[0],ipv4[1],ipv4[2],ipv4[3]);

    return(newhost);
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


/*++++++++++++++++++++++++++++++++++++++
  Split a hostname from a port, possibly an IPv6 address like '[01:23::78]:80'.

  char *hostport Specifies the host and port combination, modified by the function.

  char **host Returns a pointer to the host part (e.g. '01:23::78').

  char **port Returns a pointer to the port part (e.g. '80').
  ++++++++++++++++++++++++++++++++++++++*/

void SplitHostPort(char *hostport,char **host,char **port)
{
 char *colon;

 *port=NULL;

 if(*hostport=='[')             /* IPv6 */
   {
    char *square=strchr(hostport,']');

    *host=hostport+1;

    if(square)
      {
       *square=0;
       hostport=square+1;
      }
    else
      {
       *host=hostport;
       return;
      }
   }
 else                           /* IPv4 or name */
    *host=hostport;

 colon=strchr(hostport,':');

 if(colon)
   {
    *colon=0;
    *port=colon+1;
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Rejoin a hostname and a port that was split by SplitHostPort().

  char *hostport Specifies the host and port combination, modified by the function.

  char *host Specifies the host part.

  char *port Specifies the port part.
  ++++++++++++++++++++++++++++++++++++++*/

void RejoinHostPort(char *hostport,char *host,char *port)
{
 if(*hostport=='[' && host!=hostport) /* IPv6 */
    hostport[strlen(hostport)]=']';

 if(port)
    *(port-1)=':';
}
