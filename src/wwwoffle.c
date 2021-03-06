/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9f.
  A user level program to interact with the server.
  ******************/ /******************
  Written by Andrew M. Bishop.
  Modified by Paul A. Rombouts.

  This file Copyright 1996-2009 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2004,2005,2006,2007,2008 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "config.h"
#include "proto.h"
#include "errors.h"
#include "sockets.h"
#include "document.h"
#include "version.h"


#ifndef PATH_MAX
/*+ The maximum pathname length in characters. +*/
#define PATH_MAX 4096
#endif


/*+ The action to perform. +*/
typedef enum _Action
{
 None,                          /*+ Undecided. +*/

 Online,                        /*+ Tell the server that we are online. +*/
 Autodial,                      /*+ Tell the server that we are in autodial mode. +*/
 Offline,                       /*+ Tell the server that we are offline. +*/

 Fetch,                         /*+ Tell the server to fetch the requested pages. +*/

 Config,                        /*+ Tell the server to re-read the configuration file. +*/

 Dump,                          /*+ Tell the server to dump the configuration file. +*/

 CycleLog,                      /*+ Tell the server to close and re-open the log file. +*/

 Purge,                         /*+ Tell the server to purge pages. +*/

 Status,                        /*+ Find out from the server the current status. +*/

 Kill,                          /*+ Tell the server to exit. +*/

 Get,                           /*+ Tell the server to GET pages. +*/
 Post,                          /*+ Tell the server to POST a page. +*/
 Put,                           /*+ Tell the server to PUT a page. +*/

 Output,                        /*+ Get a page from the server and output it. +*/
 OutputWithHeader               /*+ Get a page and the headers from the server and output it. +*/
}
Action;


/* Local functions */

static void usage(int verbose);

static void add_url_list(URL **links);
static void add_url_file(char *url_file);


/* Local variables */

/*+ The list of URLs or files. +*/
static /*@null@*/ char **url_file_list=NULL;

/*+ The number of URLs or files. +*/
static int n_url_file_list=0;


/*++++++++++++++++++++++++++++++++++++++
  The main program.
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc, char** argv)
{
 int i;
 int recursive_mode=0,depth=0;
 int force=0;
 int stylesheets=0,images=0,frames=0,iframes=0,scripts=0,objects=0,nothing=0;
 char *config_file=NULL;
 int exitval=0;

 Action action=None;

 char *env=NULL;
 char *host=NULL;
 int port=0;

 /* Parse the command line options */

 if(argc==1)
    usage(0);

 for(i=1;i<argc;i++)
   {
    /* Main controlling options */

    if(!strcmp(argv[i],"-h") || !strcmp(argv[i],"--help"))
       usage(1);

    if(!strcmp(argv[i],"--version"))
       usage(2);

    if(!strcmp(argv[i],"-o"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Output;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-O"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=OutputWithHeader;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-post"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Post;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-put"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Put;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-on") || !strcmp(argv[i],"-online"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Online;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-auto") || !strcmp(argv[i],"-autodial"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Autodial;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-off") || !strcmp(argv[i],"-offline"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Offline;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-fetch"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Fetch;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-config"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Config;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-dump"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Dump;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-cyclelog"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=CycleLog;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-purge"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Purge;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-status"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Status;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-kill"))
      {
       if(action!=None)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Kill;
       argv[i]=NULL;
       continue;
      }

    /* Get options */

    if(!strcmp(argv[i],"-F"))
      {
       if(action!=None && action!=Get)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Get;
       if(force)
         {fprintf(stderr,"wwwoffle: Only one '-F' argument may be given.\n"); exit(1);}
       force=1;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp_litbeg(argv[i],"-g"))
      {
       if(action!=None && action!=Get)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Get;
       if(strchr(argv[i]+2,'S'))
         {
          if(stylesheets)
            {fprintf(stderr,"wwwoffle: Only one '-gS' argument may be given.\n"); exit(1);}
          stylesheets=1;
         }
       else if(strchr(argv[i]+2,'i'))
         {
          if(images)
            {fprintf(stderr,"wwwoffle: Only one '-gi' argument may be given.\n"); exit(1);}
          images=1;
         }
       else if(strchr(argv[i]+2,'f'))
         {
          if(frames)
            {fprintf(stderr,"wwwoffle: Only one '-gf' argument may be given.\n"); exit(1);}
          frames=1;
         }
       else if(strchr(argv[i]+2,'s'))
         {
          if(scripts)
            {fprintf(stderr,"wwwoffle: Only one '-gs' argument may be given.\n"); exit(1);}
          scripts=1;
         }
       else if(strchr(argv[i]+2,'o'))
         {
          if(objects)
            {fprintf(stderr,"wwwoffle: Only one '-go' argument may be given.\n"); exit(1);}
          objects=1;
         }
       else if(argv[i][2]==0)
         {
          if(stylesheets || images || frames || scripts || objects)
            {fprintf(stderr,"wwwoffle: Cannot have '-g' and '-g[Sifso]' together.\n"); exit(1);}
          else if(nothing)
            {fprintf(stderr,"wwwoffle: Only one '-g' argument can be given.\n"); exit(1);}
          nothing=1;
         }
       else
         {fprintf(stderr,"wwwoffle: The '-g' option does allow '%c' following it.\n",argv[i][2]); exit(1);}

       argv[i]=NULL;
       continue;
      }

    if(!strcmp_litbeg(argv[i],"-R") || !strcmp_litbeg(argv[i],"-r") || !strcmp_litbeg(argv[i],"-d"))
      {
       if(action!=None && action!=Get)
         {fprintf(stderr,"wwwoffle: Only one command at a time.\n\n");usage(0);}
       action=Get;

       if(depth)
         {fprintf(stderr,"wwwoffle: Only one '-d', '-r' or '-R' argument may be given.\n"); exit(1);}

       if(argv[i][1]=='d')
          recursive_mode=1;
       else if(argv[i][1]=='r')
          recursive_mode=2;
       else /* argv[i][1]=='R' */
          recursive_mode=3;

       if(argv[i][2])
          depth=atoi(&argv[i][2]);
       else if(i<(argc-1) && (atoi(argv[i+1]) || argv[i+1][0]=='0'))
          depth=atoi(argv[i+1]);
       else
          depth=1;
       if(depth<0)
         {fprintf(stderr,"wwwoffle: The '-%c' argument may only be followed by non-negative integer.\n",argv[i][1]); exit(1);}

       if(!argv[i][2] && i<(argc-1) && (atoi(argv[i+1]) || argv[i+1][0]=='0'))
          argv[i++]=NULL;

       argv[i]=NULL;
       continue;
      }

    /* Server & port number / config file options */

    if(!strcmp(argv[i],"-p"))
      {
       char *hoststr,*portstr; size_t hostlen;

       if(++i>=argc)
         {fprintf(stderr,"wwwoffle: The '-p' argument requires a hostname and optionally a port number.\n"); exit(1);}

       if(config_file)
         {fprintf(stderr,"wwwoffle: The '-p' and '-c' options cannot be used together.\n"); exit(1);}

       SplitHostPort(argv[i],&hoststr,&hostlen,&portstr);

       if(portstr)
         {
          port=atoi(portstr);

          if(port<=0 || port>=65536)
            {fprintf(stderr,"wwwoffle: The port number %d in -p option '%s' is invalid.\n",port,argv[i]); exit(1);}
         }

       if(host) free(host);
       host=strndup(hoststr,hostlen);
       argv[i-1]=NULL;
       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-c"))
      {
       if(++i>=argc)
         {fprintf(stderr,"wwwoffle: The '-c' argument requires a configuration file name.\n"); exit(1);}

       if(host)
         {fprintf(stderr,"wwwoffle: The '-p' and '-c' options cannot be used together.\n"); exit(1);}

       config_file=argv[i];

       argv[i-1]=NULL;
       argv[i]=NULL;
       continue;
      }

    /* Unknown options */

    if(argv[i][0]=='-' && argv[i][1])
      {
       fprintf(stderr,"wwwoffle: Unknown option '%s'.\n\n",argv[i]);
       usage(0);
      }

    /* URLs */

    add_url_file(argv[i]);
   }

 /* Check special conditions depending on the mode */

 if(action==None)
    action=Get;

 if((action==Post || action==Put) && n_url_file_list!=1)
   {
    fprintf(stderr,"wwwoffle: The -post and -put options require exactly one URL.\n\n");
    usage(0);
   }
 else if((action==Output || action==OutputWithHeader) && n_url_file_list!=1)
   {
    fprintf(stderr,"wwwoffle: The -o and -O options require exactly one URL.\n\n");
    usage(0);
   }
 else if(action==Get && n_url_file_list==0)
   {
    fprintf(stderr,"wwwoffle: No URLs were specified to request.\n\n");
    usage(0);
   }
 else if(action!=Get && action!=Output && action!=OutputWithHeader && n_url_file_list!=0)
   {
    fprintf(stderr,"wwwoffle: The -online, -autodial, -offline, -fetch, -config, -dump, -cyclelog,\n"
                   "          -purge, -status and -kill options must have no other arguments.\n\n");
    usage(0);
   }

 /* Check the environment variable. */

 if(!config_file && !host && (env=getenv("WWWOFFLE_PROXY")))
   {
    if(*env=='/')
       config_file=env;
    else
      {
       char *hoststr,*portstr; size_t hostlen;

       SplitHostPort(env,&hoststr,&hostlen,&portstr);
       if(host) free(host);
       host=strndup(hoststr,hostlen);

       if(portstr)
         {
	  char *colon=strchr(portstr,':');

	  if(!colon || action==Get || action==Post || action==Put || action==Output || action==OutputWithHeader)
	    port=atoi(portstr);
	  else
	    port=atoi(colon+1);

          if(port<=0 || port>=65536)
            {fprintf(stderr,"wwwoffle: The port number %d in WWWOFFLE_PROXY variable '%s' is invalid.\n",port,env); exit(1);}
         }
      }
   }

 /* Load the configuration file. */

 InitErrorHandler("wwwoffle",0,1);

 InitConfigurationFile(config_file);

 if(config_file)
   {
    init_io(STDERR_FILENO);

    if(ReadConfigurationFile(STDERR_FILENO))
       PrintMessage(Fatal,"Error in configuration file '%s'.",config_file);

    finish_io(STDERR_FILENO);
   }

 /* Check the host and port specified. */

 if(!host)
    host=GetLocalHost();

 if(!port)
   {
    if(action==Get || action==Post || action==Put || action==Output || action==OutputWithHeader)
       port=ConfigInteger(HTTP_Port);
    else
       port=ConfigInteger(WWWOFFLE_Port);
   }

 /* The connections to the WWWOFFLE server. */

 if(action!=Get && action!=Post && action!=Put && action!=Output && action!=OutputWithHeader)
   {
    int socket;
    char *line=NULL;

    socket=OpenClientSocket(host,port);

    if(socket==-1)
       PrintMessage(Fatal,"Cannot open connection to wwwoffle server %s port %d.",host,port);

    init_io(socket);

    /* Send the message. */

    if(ConfigString(PassWord))
       write_formatted(socket,"WWWOFFLE PASSWORD %s\r\n",ConfigString(PassWord));

    write_string(socket,(action==Online)?  "WWWOFFLE ONLINE\r\n":
			(action==Autodial)?"WWWOFFLE AUTODIAL\r\n":
			(action==Offline)? "WWWOFFLE OFFLINE\r\n":
			(action==Fetch)?   "WWWOFFLE FETCH\r\n":
			(action==Config)?  "WWWOFFLE CONFIG\r\n":
			(action==Dump)?    "WWWOFFLE DUMP\r\n":
			(action==CycleLog)?"WWWOFFLE CYCLELOG\r\n":
			(action==Purge)?   "WWWOFFLE PURGE\r\n":
			(action==Status)?  "WWWOFFLE STATUS\r\n":
			(action==Kill)?    "WWWOFFLE KILL\r\n":
				           "WWWOFFLE BOGUS\r\n");

    while((line=read_line(socket,line)))
      {
       fputs(line,stdout);
       fflush(stdout);

       if(!strcmp_litbeg(line,"WWWOFFLE Incorrect Password"))
          exitval=3;

       else if(action==Online && !strcmp_litbeg(line,"WWWOFFLE Already Online"))
          exitval=1;
       else if(action==Autodial && !strcmp_litbeg(line,"WWWOFFLE Already in Autodial Mode"))
          exitval=1;
       else if(action==Offline && !strcmp_litbeg(line,"WWWOFFLE Already Offline"))
          exitval=1;
       else if(action==Fetch && (!strcmp_litbeg(line,"WWWOFFLE Already Fetching") ||
				 !strcmp_litbeg(line,"WWWOFFLE Must be online or autodial to fetch")))
          exitval=1;
       else if(action==Config && !strcmp_litbeg(line,"Configuration file syntax error"))
          exitval=1;
       else if(action==Purge && (!strcmp_litbeg(line,"WWWOFFLE Already Purging") ||
				 !strcmp_litbeg(line,"WWWOFFLE Purge prematurely aborted")))
          exitval=1;
       else if(action==CycleLog && !strcmp_litbeg(line,"WWWOFFLE Has No Log File"))
          exitval=1;
      }

    finish_io(socket);
    CloseSocket(socket);
   }

 /* The connections to the http proxy. */

 else if(action==Get)
   {
    URL *Url;
    struct stat buf;

    for(i=0;i<n_url_file_list;i++)
       if(strcmp(url_file_list[i],"-") && stat(url_file_list[i],&buf))
         {
          int socket;
	  char *refresh=NULL;
          char buffer[IO_BUFFER_SIZE];

          Url=SplitURL(url_file_list[i]);

          if(!IsProtocolHandled(Url))
            {
             PrintMessage(Warning,"Cannot request protocol '%s'.",Url->proto);
             continue;
            }

          socket=OpenClientSocket(host,port);

          if(socket==-1)
             PrintMessage(Fatal,"Cannot open connection to wwwoffle server %s port %d.",host,port);

          init_io(socket);

          if(recursive_mode || depth || force || stylesheets || images || frames || scripts || objects || nothing)
            {
             char *limit=NULL;

             if(recursive_mode==3)
                limit=strdup("");
             else if(recursive_mode==2)
               {
		limit=(char*)malloc((Url->pathp-Url->name)+2);
		{char *p=mempcpy(limit,Url->name,Url->pathp-Url->name); *p++='/'; *p=0; }
               }
             else if(recursive_mode==1)
               {
		char *p=Url->pathendp;
		while(--p>=Url->pathp && *p!='/');
		++p;
		limit=strndup(Url->name,p-Url->name);
               }
             else
                depth=0;

             refresh=CreateRefreshPath(Url,limit,depth,force,stylesheets,images,frames,iframes,scripts,objects);

             if(limit)
                free(limit);

             printf("Requesting: %s (with recursive options).\n",Url->name);
            }
          else
             printf("Requesting: %s\n",Url->name);

          if(Url->user)
            {
	     size_t userpasslen=strlen(Url->user)+(Url->pass?strlen(Url->pass):0)+1;
	     size_t encuserpasslen=base64enclen(userpasslen);
             char userpass1[userpasslen+1],userpass2[encuserpasslen+1];

             sprintf(userpass1,"%s:%s",Url->user,Url->pass?Url->pass:"");
             Base64Encode((unsigned char *)userpass1,userpasslen,(unsigned char *)userpass2,encuserpasslen+1);

             if(refresh)
                write_formatted(socket,"HEAD %s HTTP/1.0\r\n"
                                       "Authorization: Basic %s\r\n"
                                       "Pragma: wwwoffle-client\r\n"
                                       "Accept: */*\r\n"
                                       "\r\n",
                                refresh,userpass2);
             else
	       {
		char *urlenc=URLEncodeFormArgs(Url->file);
                write_formatted(socket,"HEAD /refresh/?%s HTTP/1.0\r\n"
                                       "Pragma: wwwoffle-client\r\n"
                                       "Accept: */*\r\n"
                                       "\r\n",
                                urlenc);
		free(urlenc);
	       }
            }
          else
            {
             if(refresh)
                write_formatted(socket,"HEAD %s HTTP/1.0\r\n"
                                       "Pragma: wwwoffle-client\r\n"
                                       "Accept: */*\r\n"
                                       "\r\n",
                                refresh);
             else
	       {
		char *urlenc=URLEncodeFormArgs(Url->name);
                write_formatted(socket,"HEAD /refresh/?%s HTTP/1.0\r\n"
                                       "Pragma: wwwoffle-client\r\n"
                                       "Accept: */*\r\n"
                                       "\r\n",
                                urlenc);
		free(urlenc);
	       }
            }

          while(read_data(socket,buffer,IO_BUFFER_SIZE)>0)
             ;

          finish_io(socket);
          CloseSocket(socket);

          free(refresh);
          FreeURL(Url);
         }
       else if(!strcmp(url_file_list[i],"-") || S_ISREG(buf.st_mode))
         {
          int file;
          URL **links;

          if(strcmp(url_file_list[i],"-"))
            {
             file=open(url_file_list[i],O_RDONLY);

             if(file==-1)
               {PrintMessage(Warning,"Cannot open file '%s' for reading.",url_file_list[i]);continue;}

             printf("Reading: %s\n",url_file_list[i]);
            }
          else
            {
             file=0;

             printf("Reading: <stdin>\n");
            }

          init_io(file);

          if(*url_file_list[i]=='/')
             Url=CreateURL("file","localhost",url_file_list[i],NULL,NULL,NULL);
          else
            {
	      char *cwd;

	      /* needs glibc to work */
	      if(!(cwd=getcwd(NULL,0)))
		PrintMessage(Fatal,"Cannot get value of current working directory [%!s].");

	      {
		char *p; char fullpath[strlen(cwd)+strlen(url_file_list[i])+2]; 

		p=stpcpy(fullpath,cwd);
		*p++='/';
		stpcpy(p,url_file_list[i]);
		Url=CreateURL("file","localhost",fullpath,NULL,NULL,NULL);
	      }
	      free(cwd);
	    }

          ParseHTML(file,Url);

          if(stylesheets && (links=GetReferences(RefStyleSheet)))
             add_url_list(links);

          if(images && (links=GetReferences(RefImage)))
             add_url_list(links);

          if(frames && (links=GetReferences(RefFrame)))
             add_url_list(links);

          if(iframes && (links=GetReferences(RefIFrame)))
             add_url_list(links);

          if(scripts && (links=GetReferences(RefScript)))
             add_url_list(links);

          if(objects && (links=GetReferences(RefObject)))
             add_url_list(links);

          if(objects && (links=GetReferences(RefInlineObject)))
             add_url_list(links);

          if((links=GetReferences(RefLink)))
             add_url_list(links);

          FreeURL(Url);
          finish_io(file);
          if(file!=0)
             close(file);
         }
       else
          PrintMessage(Warning,"The file '%s' is not a regular file.",url_file_list[i]);
   }
 else if(action==Post || action==Put)
   {
    URL *Url;
    int socket,n,length=0;
    char buffer[IO_BUFFER_SIZE];
    char *data=(char*)malloc((size_t)(IO_BUFFER_SIZE+1));

    Url=SplitURL(url_file_list[0]);

    if(!IsProtocolHandled(Url))
       PrintMessage(Fatal,"Cannot post or put protocol '%s'.",Url->proto);

    socket=OpenClientSocket(host,port);

    if(socket==-1)
       PrintMessage(Fatal,"Cannot open connection to wwwoffle server %s port %d.",host,port);

    init_io(socket);

    if(action==Post)
       printf("Posting: %s\n",Url->name);
    else
       printf("Putting: %s\n",Url->name);

    init_io(0);

    while((n=read_data(0,data+length,IO_BUFFER_SIZE))>0)
      {
       length+=n;
       data=(char*)realloc((void*)data,length+IO_BUFFER_SIZE+1);
      }

    if(action==Post)
      {
       data[length]=0;
       data=URLRecodeFormArgs(data);
       length=strlen(data);
      }

    if(Url->user)
      {
       size_t userpasslen=strlen(Url->user)+(Url->pass?strlen(Url->pass):0)+1;
       size_t encuserpasslen=base64enclen(userpasslen);
       char userpass1[userpasslen+1],userpass2[encuserpasslen+1];

       sprintf(userpass1,"%s:%s",Url->user,Url->pass?Url->pass:"");
       Base64Encode((unsigned char *)userpass1,userpasslen,(unsigned char *)userpass2,encuserpasslen+1);

       if(action==Post)
          write_formatted(socket,"POST %s HTTP/1.0\r\n"
                                 "Authorization: Basic %s\r\n"
                                 "Content-Type: application/x-www-form-urlencoded\r\n"
                                 "Content-Length: %d\r\n"
                                 "Pragma: wwwoffle-client\r\n"
                                 "Accept: */*\r\n"
                                 "\r\n",
                          Url->name,userpass2,length);
       else
          write_formatted(socket,"PUT %s HTTP/1.0\r\n"
                                 "Authorization: Basic %s\r\n"
                                 "Content-Length: %d\r\n"
                                 "Pragma: wwwoffle-client\r\n"
                                 "Accept: */*\r\n"
                                 "\r\n",
                          Url->name,userpass2,length);
      }
    else
      {
       if(action==Post)
          write_formatted(socket,"POST %s HTTP/1.0\r\n"
                                 "Content-Type: application/x-www-form-urlencoded\r\n"
                                 "Content-Length: %d\r\n"
                                 "Pragma: wwwoffle-client\r\n"
                                 "Accept: */*\r\n"
                                 "\r\n",
                          Url->name,length);
       else
          write_formatted(socket,"PUT %s HTTP/1.0\r\n"
                                 "Content-Length: %d\r\n"
                                 "Pragma: wwwoffle-client\r\n"
                                 "Accept: */*\r\n"
                                 "\r\n",
                          Url->name,length);
      }

    write_data(socket,data,length);

    write_data(socket,"\r\n",2);

    while(read_data(socket,buffer,IO_BUFFER_SIZE)>0)
       ;

    finish_io(socket);
    CloseSocket(socket);

    FreeURL(Url);
    free(data);
   }
 else /* action==Output or action==OutputWithHeader */
   {
    URL *Url;
    int socket;
    char *line=NULL;

    Url=SplitURL(url_file_list[0]);

    if(!IsProtocolHandled(Url))
       PrintMessage(Fatal,"Cannot fetch data from protocol '%s'.",Url->proto);

    socket=OpenClientSocket(host,port);

    if(socket==-1)
       PrintMessage(Fatal,"Cannot open connection to wwwoffle server %s port %d.",host,port);

    init_io(socket);

    if(action!=Output && action!=OutputWithHeader)
       fprintf(stderr,"Requesting: %s\n",Url->name);

    if(Url->user)
      {
       size_t userpasslen=strlen(Url->user)+(Url->pass?strlen(Url->pass):0)+1;
       size_t encuserpasslen=base64enclen(userpasslen);
       char userpass1[userpasslen+1],userpass2[encuserpasslen+1];

       sprintf(userpass1,"%s:%s",Url->user,Url->pass?Url->pass:"");
       Base64Encode((unsigned char *)userpass1,userpasslen,(unsigned char *)userpass2,encuserpasslen+1);

       write_formatted(socket,"GET %s HTTP/1.0\r\n"
                              "Authorization: Basic %s\r\n"
                              "Pragma: wwwoffle-client\r\n"
                              "Accept: */*\r\n"
                              "\r\n",
                       Url->name,userpass2);
      }
    else
       write_formatted(socket,"GET %s HTTP/1.0\r\n"
                              "Pragma: wwwoffle-client\r\n"
                              "Accept: */*\r\n"
                              "\r\n",
                       Url->name);

    if((line=read_line(socket,line)))
      {
       char *willget="HTTP/1.0 404 WWWOFFLE Will Get\r\n";
       int status;

       sscanf(line,"%*s %d",&status);

       if(!strcmp(willget,line))
         {
          fprintf(stderr,"The URL is not in the cache but has been requested.\n");
          exitval=10;
         }
       else if((status>=300 && status<400) && action!=OutputWithHeader)
         {
          fprintf(stderr,"The URL has been moved, check with a browser.\n");
          exitval=11;
         }
       else if(status!=200 && action!=OutputWithHeader)
         {
          fprintf(stderr,"The URL returns an error message, check with a browser.\n");
          exitval=12;
         }
       else
         {
	  ssize_t nbytes;
	  char buffer[IO_BUFFER_SIZE];

          if(action==OutputWithHeader)
             fputs(line,stdout);

          while((line=read_line(socket,line)))
            {
             if(action==OutputWithHeader)
                fputs(line,stdout);
             if(line[0]=='\r' || line[0]=='\n')
                break;
            }

          while((nbytes=read_data(socket,buffer,IO_BUFFER_SIZE))>0)
             fwrite(buffer,1,nbytes,stdout);
         }
      }
    else
       PrintMessage(Fatal,"Cannot read from wwwoffle server.");

    finish_io(socket);
    CloseSocket(socket);

    FreeURL(Url);
   }

 /* exit. */

 return(exitval);
}


/*++++++++++++++++++++++++++++++++++++++
  Print the program usage in long or short format.

  int verbose Set to 0 for short format, 1 for long format, 2 for version.
  ++++++++++++++++++++++++++++++++++++++*/

static void usage(int verbose)
{
 if(verbose==2)
   {
    fprintf(stderr,
            "wwwoffle version %s\n",
            WWWOFFLE_VERSION);
    exit(0);
   }

 fprintf(stderr,
         "\n"
         "WWWOFFLE - World Wide Web Offline Explorer - Version %s\n"
         "\n",WWWOFFLE_VERSION);

 if(verbose)
    fprintf(stderr,
            "(c) Andrew M. Bishop 1996,97,98,99,2000,01 [       amb@gedanken.demon.co.uk ]\n"
            "                                           [http://www.gedanken.demon.co.uk/]\n"
            "\n");

 fprintf(stderr,
         "Usage: wwwoffle -h | --help | --version\n"
         "       wwwoffle -online | -autodial | -offline | -fetch\n"
         "       wwwoffle -config | -dump | -cyclelog | -purge | -status | -kill\n"
         "       wwwoffle [-o|-O] <url>\n"
         "       wwwoffle [-post|-put] <url>\n"
         "       wwwoffle [-g[Sisfo]] [-F] [-(d|r|R)[<depth>]] <url> ...\n"
         "       wwwoffle [-g[Sisfo]] [-F] [-(d|r|R)[<depth>]] [<file>|-] ...\n"
         "\n"
         "Any of these can also take:  [-p <host>[:<port>] | -c <config-file>]\n"
         "The environment variable WWWOFFLE_PROXY can be set instead of -p or -c options.\n"
         "\n");

 if(verbose)
    fprintf(stderr,
            "wwwoffle -h | --help : Display this help.\n"
            "\n"
            "wwwoffle --version   : Display the version number of wwwoffle.\n"
            "\n"
            "wwwoffle -on[line]   : Indicate to the server that the network is active.\n"
            "                       (Proxy requests will be fetched from remote hosts.)\n"
            "\n"
            "wwwoffle -auto[dial] : Indicate to the server that the network is automatic.\n"
            "                       (Proxy requests will be fetched from remote hosts\n"
            "                        ONLY if they are not already cached.)\n"
            "\n"
            "wwwoffle -off[line]  : Indicate to the server that the network is inactive.\n"
            "                       (Proxy requests will be fetched from cache or recorded.)\n"
            "\n"
            "wwwoffle -fetch      : Force the server to fetch the pages that are recorded.\n"
            "\n"
            "wwwoffle -config     : Force the server to re-read the configuration file.\n"
            "\n"
            "wwwoffle -dump       : Force the server to dump the current configuration.\n"
            "\n"
            "wwwoffle -cyclelog   : Force the server to close and re-open the log file.\n"
            "\n"
            "wwwoffle -status     : Query the server about its current status.\n"
            "\n"
            "wwwoffle -purge      : Force the server to purge pages from the cache.\n"
            "\n"
            "wwwoffle -kill       : Force the server to exit cleanly.\n"
            "\n"
            "wwwoffle <url> ...   : Fetch the specified URLs.\n"
            "\n"
            "wwwoffle <file> ...  : Fetch the URLs that are links in the specified file.\n"
            "\n"
            " -o                  : Fetch the URL and output it on the standard output.\n"
            " -O                  : As above but include the HTTP header.\n"
            "\n"
            " -g[Sisfo]           : Fetch the items included in the specified URLs:\n"
            "                       (S=stylesheets, i=images, f=frames, s=scripts, o=objects)\n"
            "                       When used without any options fetches none of these.\n"
            " -F                  : Force the url to be refreshed even if already cached.\n"
            " -(d|r|R)[<depth>]   : Fetch pages linked to the URLs and their links,\n"
            "                       going no more than <depth> steps (default 1).\n"
            "                        (-d => URLs in the same directory or sub-directory)\n"
            "                        (-r => URLs on the same host)\n"
            "                        (-R => URLs on any host)\n"
            "\n"
            "                      (If the -F, -(d|r|R) or -g[Sisfo] options are set they\n"
            "                       override the options in the FetchOptions section of the\n"
            "                       config file and only the -g[Sisfo] options are fetched.)\n"
            "\n"
            "wwwoffle -post <url> : Create a request using the POST method, the data is read\n"
            "                       from stdin and appended to the request.  The user should\n"
            "                       ensure that the data is correctly url-encoded.\n"
            "wwwoffle -put <url>  : Create a request using the PUT method, the data is read\n"
            "                       from stdin and appended to the request.\n"
            "\n"
            " -p <host>[:<port>]  : The host name and port number to talk to the demon on.\n"
            "                       (Defaults to %s for the server and\n"
            "                        %d for control port, %d for http proxy port).\n"
            "\n"
            " -c <config-file>    : The name of the configuration file with the hostname,\n"
            "                       port number and the password (if any).\n"
            "\n"
            "WWWOFFLE_PROXY       : An environment variable that can be set to either the\n"
            "                       name of the config file (absolute path) or the hostname\n"
            "                       and port number (both proxy and control) for the proxy.\n"
            "                       e.g. \"/var/spool/wwwoffle/wwwoffle.conf\",\n"
            "                       \"localhost:8080:8081\" or \"localhost:8080\" are valid.\n"
            "\n",DEF_LOCALHOST,DEF_WWWOFFLE_PORT,DEF_HTTP_PORT);

 if(verbose)
    exit(0);
 else
    exit(1);
}


/*++++++++++++++++++++++++++++++++++++++
  Add a list of URLs to the list.

  URL **links The list of URLs to add.
  ++++++++++++++++++++++++++++++++++++++*/

static void add_url_list(URL **links)
{
 int i;

 for(i=0;links[i];i++)
    if(strcmp(links[i]->proto,"file"))
       add_url_file(links[i]->name);
}


/*++++++++++++++++++++++++++++++++++++++
  Add a URL or a file to the list.

  char *url_file The URL or file to add.
  ++++++++++++++++++++++++++++++++++++++*/

static void add_url_file(char *url_file)
{
 if(!(n_url_file_list%16))
   {
     url_file_list=(char**)realloc(url_file_list,(n_url_file_list+16)*sizeof(char*));
   }

 url_file_list[n_url_file_list]=strdup(url_file);

 n_url_file_list++;
}
