/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/wwwoffle.c 2.51 2002/05/26 10:50:42 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7c.
  A user level program to interact with the server.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02 Andrew M. Bishop
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

#include "version.h"
#include "wwwoffle.h"
#include "document.h"
#include "misc.h"
#include "config.h"
#include "sockets.h"
#include "errors.h"

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif


static void usage(int verbose);


/*+ The action to perform. +*/
typedef enum _Action
{
 None,                          /*+ Undecided. +*/

 Online,                        /*+ Tell the server that we are online. +*/
 Autodial,                      /*+ Tell the server that we are in autodial mode. +*/
 Offline,                       /*+ Tell the server that we are offline. +*/

 Fetch,                         /*+ Tell the server to fetch the requested pages. +*/

 Config,                        /*+ Tell the server to re-read the configuration file. +*/

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


static void add_url_list(char **links);
static void add_url_file(char *url_file);

/*+ The list of URLs or files. +*/
static char **url_file_list=NULL;

/*+ The number of URLs or files. +*/
static int n_url_file_list=0;

/*+ A file descriptor for the spool directory; not used anywhere, but required for linking with spool.o. +*/
int fSpoolDir=-1;


/*++++++++++++++++++++++++++++++++++++++
  The main program.
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc, char** argv)
{
 int i;
 int recursive_mode=0,depth=0;
 int force=0;
 int stylesheets=0,images=0,frames=0,scripts=0,objects=0;
 char *config_file=NULL;
 int exitval=0;

 Action action=None;

 char *env=NULL;
 char *host=NULL;
 int port=0,http_port,wwwoffle_port;

 /* Parse the command line options */

 if(argc==1)
    usage(0);

 for(i=1;i<argc;i++)
   {
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
       if(strchr(argv[i]+2,'i'))
         {
          if(images)
            {fprintf(stderr,"wwwoffle: Only one '-gi' argument may be given.\n"); exit(1);}
          images=1;
         }
       if(strchr(argv[i]+2,'f'))
         {
          if(frames)
            {fprintf(stderr,"wwwoffle: Only one '-gf' argument may be given.\n"); exit(1);}
          frames=1;
         }
       if(strchr(argv[i]+2,'s'))
         {
          if(scripts)
            {fprintf(stderr,"wwwoffle: Only one '-gs' argument may be given.\n"); exit(1);}
          scripts=1;
         }
       if(strchr(argv[i]+2,'o'))
         {
          if(objects)
            {fprintf(stderr,"wwwoffle: Only one '-go' argument may be given.\n"); exit(1);}
          objects=1;
         }
       if(argv[i][2]==0)
          fprintf(stderr,"wwwoffle: The '-g' option does nothing on its own.\n");

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
       else
          depth=1;
       if(depth<=0)
         {fprintf(stderr,"wwwoffle: The '-%c' argument may only be followed by a positive integer.\n",argv[i][1]); exit(1);}

       argv[i]=NULL;
       continue;
      }

    if(!strcmp(argv[i],"-p"))
      {
       char *hoststr,*portstr; int hostlen;

       if(++i>=argc)
         {fprintf(stderr,"wwwoffle: The '-p' argument requires a hostname and optionally a port number.\n"); exit(1);}

       if(config_file)
         {fprintf(stderr,"wwwoffle: The '-p' and '-c' options cannot be used together.\n"); exit(1);}

       SplitHostPort(argv[i],&hoststr,&hostlen,&portstr);

       if(portstr)
         {
          port=atoi(portstr);

          if(port<=0 || port>=65536)
            {fprintf(stderr,"wwwoffle: The port number %d '%s' is invalid.\n",port,argv[i]); exit(1);}
         }

       host=strndup(hoststr,hostlen);

       /* Don't need to use RejoinHostPort(argv[i],hoststr,portstr) here. */

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

    if(argv[i][0]=='-' && argv[i][1])
      {
       fprintf(stderr,"wwwoffle: Unknown option '%s'.\n\n",argv[i]);
       usage(0);
      }

    add_url_file(argv[i]);
   }

 if(action==None)
    action=Get;

 if((action==Post || action==Put) && n_url_file_list!=1)
   {
    fprintf(stderr,"wwwoffle: The -post and -put options require exactly one URL.\n\n");
    usage(0);
   }

 if((action==Output || action==OutputWithHeader) && n_url_file_list!=1)
   {
    fprintf(stderr,"wwwoffle: The -o and -O options require exactly one URL.\n\n");
    usage(0);
   }

 /* Initialise things. */

 if(!config_file && !host && (env=getenv("WWWOFFLE_PROXY")))
   {
    if(*env=='/')
       config_file=env;
    else
      {
       char *hoststr,*portstr; int hostlen;

       SplitHostPort(env,&hoststr,&hostlen,&portstr);
       host=strndup(hoststr,hostlen);

       if(portstr)
         {
	  char *colon=strchr(portstr,':');

	  if(!colon || action==Get || action==Output || action==OutputWithHeader)
	    port=atoi(portstr);
	  else
	    port=atoi(colon+1);

          if(port<=0 || port>=65536)
            {fprintf(stderr,"wwwoffle: The port number %d '%s' is invalid.\n",port,env); exit(1);}
         }
      }
   }

 InitErrorHandler("wwwoffle",0,1);

 InitConfigurationFile(config_file);

 if(config_file)
   {
    init_buffer(2);

    if(ReadConfigurationFile(2))
       PrintMessage(Fatal,"Error in configuration file '%s'.",config_file);
   }

 if(!host)
    host=GetLocalHost(0);

 wwwoffle_port=ConfigInteger(WWWOFFLE_Port);
 http_port=ConfigInteger(HTTP_Port);

 /* The connections to the WWWOFFLE server. */

 if(action!=Get && action!=Post && action!=Put && action!=Output && action!=OutputWithHeader)
   {
    int socket;
    char *line=NULL;

    socket=OpenClientSocket(host,port?port:wwwoffle_port);
    init_buffer(socket);

    if(socket==-1)
       PrintMessage(Fatal,"Cannot open connection to wwwoffle server %s port %d.",host,port?port:wwwoffle_port);

    /* Send the message. */

    if(ConfigString(PassWord))
       write_formatted(socket,"WWWOFFLE PASSWORD %s\r\n",ConfigString(PassWord));

    write_string(socket,(action==Online)?  "WWWOFFLE ONLINE\r\n":
			(action==Autodial)?"WWWOFFLE AUTODIAL\r\n":
			(action==Offline)? "WWWOFFLE OFFLINE\r\n":
			(action==Fetch)?   "WWWOFFLE FETCH\r\n":
			(action==Config)?  "WWWOFFLE CONFIG\r\n":
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
      }

    CloseSocket(socket);
   }

 /* The connections to the http proxy. */

 else if(action==Get)
   {
    URL *Url;
    char *refresh=NULL;
    struct stat buf;

    for(i=0;i<n_url_file_list;i++)
       if(strcmp(url_file_list[i],"-") && stat(url_file_list[i],&buf))
         {
          int socket;
          char buffer[READ_BUFFER_SIZE];

          Url=SplitURL(url_file_list[i]);

          if(!Url->Protocol)
            {
             PrintMessage(Warning,"Cannot request protocol '%s'.",Url->proto);
             continue;
            }

          socket=OpenClientSocket(host,port?port:http_port);
          init_buffer(socket);

          if(socket==-1)
             PrintMessage(Fatal,"Cannot open connection to wwwoffle server %s port %d.",host,port?port:http_port);

          if(recursive_mode || depth || force || stylesheets || images || frames || scripts || objects)
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
		limit=strndup(Url->name,p+1-Url->name);
               }
             else
                depth=0;

             refresh=CreateRefreshPath(Url,limit,depth,
                                       force,
                                       stylesheets,images,frames,scripts,objects);

             if(limit)
                free(limit);

             printf("Getting: %s (with recursive options).\n",Url->name);
            }
          else
             printf("Getting: %s\n",Url->name);

          if(Url->user)
            {
             char *userpass1=(char*)(malloc(strlen(Url->user)+(Url->pass?strlen(Url->pass):1)+3)),*userpass2;

             sprintf(userpass1,"%s:%s",Url->user,Url->pass?Url->pass:"");
             userpass2=Base64Encode(userpass1,strlen(userpass1));

             if(refresh)
                write_formatted(socket,"GET %s HTTP/1.0\r\n"
                                       "Authorization: Basic %s\r\n"
                                       "Pragma: wwwoffle\r\n"
                                       "Accept: */*\r\n"
                                       "\r\n",
                                refresh,userpass2);
             else
                write_formatted(socket,"GET /refresh/?%s HTTP/1.0\r\n"
                                       "Authorization: Basic %s\r\n"
                                       "Pragma: wwwoffle\r\n"
                                       "Accept: */*\r\n"
                                       "\r\n",
                                URLEncodeFormArgs(Url->name),userpass2);

             free(userpass1);
             free(userpass2);
            }
          else
            {
             if(refresh)
                write_formatted(socket,"GET %s HTTP/1.0\r\n"
                                       "Pragma: wwwoffle\r\n"
                                       "Accept: */*\r\n"
                                       "\r\n",
                                refresh);
             else
                write_formatted(socket,"GET /refresh/?%s HTTP/1.0\r\n"
                                       "Pragma: wwwoffle\r\n"
                                       "Accept: */*\r\n"
                                       "\r\n",
                                URLEncodeFormArgs(Url->name));
            }

          while(read_data(socket,buffer,READ_BUFFER_SIZE)>0)
             ;

          CloseSocket(socket);

          free(refresh);
          FreeURL(Url);
         }
       else if(!strcmp(url_file_list[i],"-") || S_ISREG(buf.st_mode))
         {
          int file;
          char **links;

          if(strcmp(url_file_list[i],"-"))
            {
             file=open(url_file_list[i],O_RDONLY);
             init_buffer(file);

             if(file==-1)
               {PrintMessage(Warning,"Cannot open file '%s' for reading.",url_file_list[i]);continue;}

             printf("Reading: %s\n",url_file_list[i]);
            }
          else
            {
             file=fileno(stdin);
             init_buffer(file);

             printf("Reading: <stdin>\n");
            }

	  {
	    int len=strlitlen("file://localhost")+strlen(url_file_list[i]);
	    char *cwd=NULL;
	      
	    if(*url_file_list[i]!='/')
	      { /* needs glibc to work */
		if(!(cwd=getcwd(NULL,0)))
		  PrintMessage(Fatal,"Cannot get value of current working directory [%!s].");
		len+=strlen(cwd)+1;
	      }

	    {
	      char *p; char buffer[len+1]; 

	      p=stpcpy(buffer,"file://localhost");

	      if(cwd)
		{
		  p=stpcpy(p,cwd);
		  *p++='/';
		  free(cwd);
		}
	      stpcpy(p,url_file_list[i]);
	      Url=SplitURL(buffer);
	    }
	  }

          ParseHTML(file,Url);

          if(stylesheets && (links=GetReferences(RefStyleSheet)))
             add_url_list(links);

          if(images && (links=GetReferences(RefImage)))
             add_url_list(links);

          if(frames && (links=GetReferences(RefFrame)))
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
    char buffer[READ_BUFFER_SIZE];
    char *data=(char*)malloc(READ_BUFFER_SIZE+1);

    Url=SplitURL(url_file_list[0]);

    if(!Url->Protocol)
       PrintMessage(Fatal,"Cannot post or put protocol '%s'.",Url->proto);

    socket=OpenClientSocket(host,port?port:http_port);
    init_buffer(socket);

    if(socket==-1)
       PrintMessage(Fatal,"Cannot open connection to wwwoffle server %s port %d.",host,port?port:http_port);

    if(action==Post)
       printf("Posting: %s\n",Url->name);
    else
       printf("Putting: %s\n",Url->name);

    while((n=read_data(0,data+length,READ_BUFFER_SIZE))>0)
      {
       length+=n;
       data=(char*)realloc((void*)data,length+READ_BUFFER_SIZE+1);
      }

    if(action==Post)
      {
       data[length]=0;
       data=URLRecodeFormArgs(data);
       length=strlen(data);
      }

    if(Url->user)
      {
       char *userpass1=(char*)(malloc(strlen(Url->user)+(Url->pass?strlen(Url->pass):1)+3)),*userpass2;

       sprintf(userpass1,"%s:%s",Url->user,Url->pass?Url->pass:"");
       userpass2=Base64Encode(userpass1,strlen(userpass1));

       if(action==Post)
          write_formatted(socket,"POST %s HTTP/1.0\r\n"
                                 "Authorization: Basic %s\r\n"
                                 "Content-Type: application/x-www-form-urlencoded\r\n"
                                 "Content-Length: %d\r\n"
                                 "Pragma: wwwoffle\r\n"
                                 "Accept: */*\r\n"
                                 "\r\n",
                          Url->name,userpass2,length);
       else
          write_formatted(socket,"PUT %s HTTP/1.0\r\n"
                                 "Authorization: Basic %s\r\n"
                                 "Content-Length: %d\r\n"
                                 "Pragma: wwwoffle\r\n"
                                 "Accept: */*\r\n"
                                 "\r\n",
                          Url->name,userpass2,length);

       free(userpass1);
       free(userpass2);
      }
    else
      {
       if(action==Post)
          write_formatted(socket,"POST %s HTTP/1.0\r\n"
                                 "Content-Type: application/x-www-form-urlencoded\r\n"
                                 "Content-Length: %d\r\n"
                                 "Pragma: wwwoffle\r\n"
                                 "Accept: */*\r\n"
                                 "\r\n",
                          Url->name,length);
       else
          write_formatted(socket,"PUT %s HTTP/1.0\r\n"
                                 "Content-Length: %d\r\n"
                                 "Pragma: wwwoffle\r\n"
                                 "Accept: */*\r\n"
                                 "\r\n",
                          Url->name,length);
      }

    write_data(socket,data,length);

    write_data(socket,"\r\n",2);

    while(read_data(socket,buffer,READ_BUFFER_SIZE)>0)
       ;

    CloseSocket(socket);

    FreeURL(Url);
    free(data);
   }
 else /* action==Output or action==OutputWithHeader */
   {
    URL *Url;
    int socket;
    char *line=NULL,buffer[READ_BUFFER_SIZE];
    int nbytes;

    if(!n_url_file_list)
       PrintMessage(Fatal,"No URL specified to output.");

    Url=SplitURL(url_file_list[0]);

    if(!Url->Protocol)
       PrintMessage(Fatal,"Cannot fetch data from protocol '%s'.",Url->proto);

    socket=OpenClientSocket(host,port?port:http_port);
    init_buffer(socket);

    if(socket==-1)
       PrintMessage(Fatal,"Cannot open connection to wwwoffle server %s port %d.",host,port?port:http_port);

    if(action!=Output && action!=OutputWithHeader)
       fprintf(stderr,"Getting: %s\n",Url->name);

    if(Url->user)
      {
       char *userpass1=(char*)(malloc(strlen(Url->user)+(Url->pass?strlen(Url->pass):1)+3)),*userpass2;

       sprintf(userpass1,"%s:%s",Url->user,Url->pass?Url->pass:"");
       userpass2=Base64Encode(userpass1,strlen(userpass1));

       write_formatted(socket,"GET %s HTTP/1.0\r\n"
                              "Authorization: Basic %s\r\n"
                              "Pragma: wwwoffle\r\n"
                              "Accept: */*\r\n"
                              "\r\n",
                       Url->name,userpass2);

       free(userpass1);
       free(userpass2);
      }
    else
       write_formatted(socket,"GET %s HTTP/1.0\r\n"
                              "Pragma: wwwoffle\r\n"
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
          if(action==OutputWithHeader)
             fputs(line,stdout);

          while((line=read_line(socket,line)))
            {
             if(action==OutputWithHeader)
                fputs(line,stdout);
             if(line[0]=='\r' || line[0]=='\n')
                break;
            }

          while((nbytes=read_data(socket,buffer,READ_BUFFER_SIZE))>0)
             fwrite(buffer,1,nbytes,stdout);
         }
      }
    else
       PrintMessage(Fatal,"Cannot read from wwwoffle server.");

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
         "       wwwoffle -config | -purge | -status | -kill\n"
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
            " -g[Sisfo]           : Fetch the items included in the specified URLs.\n"
            "                       (S=stylesheets, i=images, f=frames, s=scripts, o=objects)\n"
            " -F                  : Force the url to be refreshed even if already cached.\n"
            " -(d|r|R)[<depth>]   : Fetch pages linked to the URLs and their links,\n"
            "                       going no more than <depth> steps (default 1).\n"
            "                        (-d => URLs in the same directory or sub-directory)\n"
            "                        (-r => URLs on the same host)\n"
            "                        (-R => URLs on any host)\n"
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

  char **links The list of URLs to add.
  ++++++++++++++++++++++++++++++++++++++*/

static void add_url_list(char **links)
{
 int i;

 for(i=0;links[i];i++)
   {
    URL *linkUrl=SplitURL(links[i]);

    if(strcmp(linkUrl->proto,"file"))
       add_url_file(linkUrl->name);

    FreeURL(linkUrl);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Add a URL or a file to the list.

  char *url_file The URL or file to add.
  ++++++++++++++++++++++++++++++++++++++*/

static void add_url_file(char *url_file)
{
 if(!(n_url_file_list%16))
   {
    if(n_url_file_list)
       url_file_list=(char**)realloc(url_file_list,(n_url_file_list+16)*sizeof(char*));
    else
       url_file_list=(char**)malloc(16*sizeof(char*));
   }

 url_file_list[n_url_file_list]=strdup(url_file);

 n_url_file_list++;
}
