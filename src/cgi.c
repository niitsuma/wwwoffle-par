/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/cgi.c 1.5 2002/08/04 10:24:43 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7e.
  CGI Execution functions.
  ******************/ /******************
  Written by Paul A. Rombouts
  Modified by Andrew M. Bishop

  This file Copyright 2002 Paul A. Rombouts & Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "version.h"
#include "wwwoffle.h"
#include "errors.h"
#include "config.h"
#include "misc.h"


/*--------------------------------------------------------------------------------

 With the patch from Paul A. Rombouts <p.a.rombouts@home.nl> came the following
 explanation:

   I've completely implemented the CGI specification defined at
   http://hoohoo.ncsa.uiuc.edu/cgi/interface.html, with the following exceptions:

   - the REMOTE_IDENT environment variable. The specification says that this
     variable only has to be set if the server supports RFC 931 identification, which
     WWWOFFLE doesn't, so I guess I'm not really in violation here.

   - the CGI program is never passed any arguments, even if the query string
     contains no unencoded "=". So my implementation cannot be used for ISINDEX type
     queries, which I see no use for anyway.

   - The CGI specification says that if the script name begins with nph- the
     server should not parse its header. In my implementation WWWOFFLE always parses
     the header of the output of the CGI program. Strict compliance with the CGI
     specification on this point is more trouble than it's worth, as I see it.

   - The CGI specification was originally intended for web servers, not proxy
     servers, so sometimes it's sometimes a bit dubious how to interpret the
     specification when applied to WWWOFFLE.

     For instance, I use the PATH_INFO variable to contain the full path information
     in the original request (before replacements). This is not in strict compliance
     with the specification, but its the most useful interpretation I can think of
     for this variable.

 In the final implementation of the code I have made the following additional
 change:

  - The Location: header returned from the CGI is not used to send the specified
    URL to the browser in place of the CGI output but is sent to the browser to
    handle.  This removes the possibility of an infinite recursion in the WWWOFFLE
    server.

--------------------------------------------------------------------------------*/

/* Local functions */

static int putenv_request(char *file, Header *request_head, /*@null@*/ Body *request_body);
static int exec_cgi(int fd, char *file, Header *request_head, /*@null@*/ Body *request_body);
static void handle_cgi_headers(int fd_in,int fd_out);


/* in case the function stpcpy is not available on this platform */
#define stpcpy(dst,src) strchr(strcpy(dst,src),0)


/* Some global variables used to pass on information about remote client */
char *client_hostname=NULL, *client_ip=NULL;


/*++++++++++++++++++++++++++++++++++++++
  Output the results of a local CGI script.

  int fd The file descriptor to write to.

  URL *Url The URL containing the CGI.

  char *file The name of the file.

  Header *request_head The request that was made for this page.

  Body *request_body The body of the request that was made for this page.
  ++++++++++++++++++++++++++++++++++++++*/

void LocalCGI(int fd,URL *Url,char *file,Header *request_head,Body *request_body)
{
 int exec_success=0;
#if HAVE_GETRESUID
 uid_t ruid,euid,suid;
#endif
#if HAVE_GETRESGID
 gid_t rgid,egid,sgid;
#endif

 if(
#if HAVE_GETRESUID
    !getresuid(&ruid,&euid,&suid) && ruid && euid && suid && 
#else
    getuid() && geteuid() && 
#endif
#if HAVE_GETRESGID
    !getresgid(&rgid,&egid,&sgid) && rgid && egid && sgid
#else
    getgid() && getegid()
#endif
    )
   {
    PrintMessage(Debug,"Using the local CGI '%s'.",file);

    exec_success=exec_cgi(fd,file,request_head,request_body);
   }
 else
    PrintMessage(Warning,"Refuse to run local CGI program file '%s' with root privileges.",file);

 if(!exec_success)
   {
    lseek(fd,0,SEEK_SET);

    HTMLMessage(fd,500,"WWWOFFLE Local Program Execution Error",NULL,"ExecCGIFailed",
                "url",Url->path,
                NULL);
   }
}


/* First a macro definition that makes life a little easier. */
#define putenv_var_val(varname,value,failaction) \
{ \
  char *envstr = (char *)malloc(sizeof(varname "=")+strlen(value)); \
  strcpy(stpcpy(envstr,varname "="),value); \
  if(putenv(envstr) == -1) failaction; \
}


/*++++++++++++++++++++++++++++++++++++++
  Put information of request headers into environment according to CGI-specification.

  int putenv_request Returns 0 on success or -1 on error.

  char *file Filename of script.

  Header *request_head The head of the request that was made for this page.

  Body *request_body The body of the request (only if POST or PUT).
  ++++++++++++++++++++++++++++++++++++++*/

static int putenv_request(char *file, Header *request_head, Body *request_body)
{
 int retval=0;

 if(putenv("SERVER_SOFTWARE=WWWOFFLE/" WWWOFFLE_VERSION) == -1) return -1;

    {
     char *localhost=GetLocalHost(0);
     putenv_var_val("SERVER_NAME",localhost, retval=-1);
     free(localhost);
     if(retval) return retval;
    }

 if(putenv("GATEWAY_INTERFACE=CGI/1.1") == -1) return -1;

 putenv_var_val("SERVER_PROTOCOL",request_head->version, return -1);

    {
     char portstr[12];
     sprintf(portstr,"%d",ConfigInteger(HTTP_Port));
     putenv_var_val("SERVER_PORT",portstr, return -1);
    }

 putenv_var_val("REQUEST_METHOD",request_head->method, return -1);

 putenv_var_val("REQUEST_URI",request_head->url, return -1);

    {    
     URL *Url=SplitURL(request_head->url);

     if(!GetHeader(request_head,"Host"))
        putenv_var_val("HTTP_HOST",Url->host, {retval=-1; goto cleanupURL;});

     putenv_var_val("PATH_INFO",Url->path, {retval=-1; goto cleanupURL;});

     if (Url->args)
        putenv_var_val("QUERY_STRING",Url->args, retval=-1);

     putenv_var_val("SCRIPT_NAME",Url->path, return -1);

    cleanupURL:
     FreeURL(Url);
     if(retval) return retval;
    }

 putenv_var_val("PATH_TRANSLATED",file, return -1);

 if(client_hostname && client_ip && strcmp(client_ip,client_hostname))
    putenv_var_val("REMOTE_HOST",client_hostname, return -1);

 if(client_ip)
    putenv_var_val("REMOTE_ADDR",client_ip, return -1);

    {
     int i, n = request_head->n; char **key = request_head->key, **val = request_head->val;

     for(i=0;i<n;++i)
       {
        if(key[i] && strcasecmp(key[i],"Content-Length") && strcasecmp(key[i],"Proxy-Authorization"))
          {
           char *envstr, *p,*q;

           if(!strcasecmp(key[i],"Content-Type"))
             {
              envstr = (char *)malloc(strlen(key[i])+strlen(val[i])+sizeof("=")+1);
              p = envstr;
             }
           else
             {
              envstr = (char *)malloc(strlen(key[i])+strlen(val[i])+sizeof("HTTP_=")+1);
              p = stpcpy(envstr, "HTTP_");
             }

           for(q = key[i]; *q; ++p,++q)
             {
              if(isupper(*q) || isdigit(*q) || *q == '_')
                 *p = *q;
              else if (islower(*q))
                 *p = toupper(*q);
              else if(*q == '-')
                 *p = '_';
              else
                {
                 /* key contains illegal character, ignore key */
                 free(envstr);
                 goto nexti;
                }
             }

           *p++ = '=';
           strcpy(p, val[i]);

           if(putenv(envstr) == -1) return -1;
          }
       nexti:
        ;
       }
    }

 if(request_body)
   {
    char length[12];
    sprintf(length,"%d",request_body->length);
    putenv_var_val("CONTENT_LENGTH",length, return -1);
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Execute a local file as CGI program.

  int ExecLocalCGI returns 1 on success, 0 on failure.

  int fd The file descriptor to write the output to.

  char *file The name of the file to execute.

  Header *request_head The head of the request.

  Body *request_body The body of the request.
  ++++++++++++++++++++++++++++++++++++++*/

static int exec_cgi(int fd, char *file, Header *request_head, Body *request_body)
{
 int cgi_out[2];
 pid_t childpid;

 if(pipe(cgi_out))
   {
    PrintMessage(Warning, "Cannnot create pipe for local CGI program '%s' output [%!s].", file);
    return 0;
   }

 if((childpid = fork()) == -1)
    PrintMessage(Warning, "Cannnot fork to execute local CGI program '%s' [%!s].", file);
 else if(childpid == 0)   /* The child */
   {
    if(!strcmp(request_head->method,"POST") || !strcmp(request_head->method,"PUT"))
      {
       int cgi_in=CreateTempSpoolFile();

       if(cgi_in==-1)
         {
          PrintMessage(Warning, "Cannnot create temporary file for local CGI program '%s' input [%!s].", file);
          return 0;
         }

       if(write_data(cgi_in,request_body->content,request_body->length)<0)
          PrintMessage(Warning, "Failed to write data to local CGI program '%s' [%!s].", file);

       lseek(cgi_in,0,SEEK_SET);

       if(cgi_in!=0)
         {
          if(dup2(cgi_in,0) == -1)
             PrintMessage(Fatal,"Cannnot create standard input for local CGI program '%s' [%!s].", file);
          close(cgi_in);
         }

       if(putenv_request(file,request_head,request_body) == -1)
          PrintMessage(Warning, "Failed to create environment for local CGI program '%s' [%!s].", file);
      }
    else
       if(putenv_request(file,request_head,NULL) == -1)
          PrintMessage(Warning, "Failed to create environment for local CGI program '%s' [%!s].", file);

    if(cgi_out[1]!=1)
      {
       if(dup2(cgi_out[1],1) == -1)
          PrintMessage(Fatal,"Cannnot create standard output for local CGI program '%s' [%!s].", file);
       close(cgi_out[1]);
      }
    close(cgi_out[0]);

    execl(file, file, NULL);

    PrintMessage(Fatal, "Failed to execute local CGI program '%s' [%!s].", file);
   }
 else   /* The parent */
   {
    int statval;

    PrintMessage(Inform, "Forked local CGI program '%s' (pid=%d).", file,childpid);

    close(cgi_out[1]);
    init_buffer(cgi_out[0]);

    handle_cgi_headers(cgi_out[0],fd);

    close(cgi_out[0]);

    if(waitpid(childpid, &statval, 0) != -1)
      {
       if(WIFEXITED(statval)) 
         {
          int exitstatus = WEXITSTATUS(statval);
          PrintMessage(exitstatus?Warning:Inform,
                       "Local CGI program '%s' (pid=%d) exited with status %d.", file,childpid,exitstatus);
          if(exitstatus <=1)
             return 1;
         }
       else 
          PrintMessage(Warning, "Local CGI program '%s' (pid=%d) terminated by signal %d.", file,childpid,WTERMSIG(statval));
      }
    else
       PrintMessage(Warning, "wait failed for local CGI program '%s' (pid=%d) [%!s].", file,childpid);
   }

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Handle the CGI headers and create proper headers.

  int fd_in The input data from the CGI.

  int fd_out The output data to the client.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_cgi_headers(int fd_in,int fd_out)
{
 Header *cgi_head=NULL;
 char *line=NULL;
 char *val,*head;
 char buffer[READ_BUFFER_SIZE];
 int n;

 cgi_head=CreateHeader("HTTP/1.0 200 WWWOFFLE CGI output",0);

 while((line=read_line(fd_in,line)))
    if(!AddToHeaderRaw(cgi_head,line))
       break;

 if((val=GetHeader(cgi_head,"Status")))
   {
    int status=atoi(val);

    cgi_head->status=(status>0)?(status%1000):0;
    while(*val && !isspace(*val)) ++val;  /* skip status code */
    while(*val && isspace(*val)) ++val;   /* skip whitespace */

    ChangeNoteInHeader(cgi_head,val);

    RemoveFromHeader(cgi_head,"Status");
   }

 if((val=GetHeader(cgi_head,"Location")))
   {
    URL *Url=SplitURL(val);

    cgi_head->status=302;

    RemoveFromHeader(cgi_head,"Location");
    AddToHeader(cgi_head,"Location",Url->name);

    FreeURL(Url);
   }

 if(!GetHeader(cgi_head,"Server"))
    AddToHeader(cgi_head,"Server","WWWOFFLE/" WWWOFFLE_VERSION);

 if(!GetHeader(cgi_head,"Date"))
    AddToHeader(cgi_head,"Date",RFC822Date(time(NULL),1));

 head=HeaderString(cgi_head);

 write_string(fd_out,head);

 FreeHeader(cgi_head);
 free(head);

 while((n=read_data(fd_in,buffer,READ_BUFFER_SIZE))>0)
    write_data(fd_out,buffer,n);
}
