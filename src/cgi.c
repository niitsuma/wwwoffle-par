/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/cgi.c 1.27 2007/04/18 18:59:20 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  CGI Execution functions.
  ******************/ /******************
  Written by Paul A. Rombouts
  Modified by Andrew M. Bishop

  This file Copyright 2002,03,04,05,07 Paul A. Rombouts & Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "headbody.h"
#include "sockets.h"
#include "version.h"


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

--------------------------------------------------------------------------------*/


/* Some global variables used to pass on information about remote client */
extern char *client_hostname, *client_ip;
extern char *proxy_user;
extern int online;          /*+ The online / offline / autodial status. +*/


/* Some macro definitions that makes environment variable setting a little easier. */
#define putenv_string(str)			\
{						\
  if(putenv(str) == -1) {			\
    CLEANUP_HANDLER;				\
    return -1;					\
  }						\
}
#define putenv_var_val(varname,value)					\
{									\
  char *envstr = (char *)malloc(sizeof(varname "=")+strlen(value));	\
  stpcpy(stpcpy(envstr,varname "="),value);				\
  putenv_string(envstr);						\
}
#define CLEANUP_HANDLER

/*++++++++++++++++++++++++++++++++++++++
  Put information of request headers into environment according to CGI-specification.

  int putenv_request Returns 0 on success or -1 on error.

  char *file: Filename of script.
  URL *Url: The URL referring to the CGI script.

  Header *request_head The head of the request that was made for this page.

  Body *request_body The body of the request (only if POST or PUT).
  ++++++++++++++++++++++++++++++++++++++*/

static int putenv_request(char *file, URL *Url, Header *request_head, /*@null@*/ Body *request_body)
{

  putenv_string("SERVER_SOFTWARE=WWWOFFLE/" WWWOFFLE_VERSION);

  {
    char *localhost=GetLocalHost();
#   undef  CLEANUP_HANDLER
#   define CLEANUP_HANDLER free(localhost)
    putenv_var_val("SERVER_NAME",localhost);
    CLEANUP_HANDLER;
#   undef  CLEANUP_HANDLER
#   define CLEANUP_HANDLER
  }

  putenv_string("GATEWAY_INTERFACE=CGI/1.1");

  putenv_var_val("SERVER_PROTOCOL",request_head->version);

  {
    char portstr[MAX_INT_STR+1];
    sprintf(portstr,"%d",ConfigInteger(HTTP_Port));
    putenv_var_val("SERVER_PORT",portstr);
  }

  putenv_var_val("REQUEST_METHOD",request_head->method);

  putenv_var_val("REQUEST_URI",request_head->url);

  {
    URL *request_Url=SplitURL(request_head->url);
#   undef  CLEANUP_HANDLER
#   define CLEANUP_HANDLER FreeURL(request_Url)

    if(!GetHeader(request_head,"Host"))
      putenv_var_val("HTTP_HOST",request_Url->hostport);

    putenv_var_val("PATH_INFO",request_Url->path);

    if (request_Url->args)
      putenv_var_val("QUERY_STRING",request_Url->args);

    CLEANUP_HANDLER;
#   undef  CLEANUP_HANDLER
#   define CLEANUP_HANDLER
  }

  putenv_var_val("SCRIPT_NAME",Url->path);

  putenv_var_val("PATH_TRANSLATED",file);

  if(client_hostname)
    putenv_var_val("REMOTE_HOST",client_hostname);

  if(client_ip)
    putenv_var_val("REMOTE_ADDR",client_ip);

  if(proxy_user) {
    putenv_string("AUTH_TYPE=Basic");
    putenv_var_val("REMOTE_USER",proxy_user);
  }

  {
    KeyValueNode *line = request_head->line, *next;

    while(line) {
      next=line->next;

      if(strcasecmp(line->key,"Content-Length") && strcasecmp(line->key,"Proxy-Authorization")) {
	char *envstr, *p,*q;
	size_t key_val_len = strlen(line->key)+strlen(line->val);

	/* first look for a run of identical keys */
	for (; next; next=next->next) {
	  if(strcasecmp(next->key,line->key)) break;
	  key_val_len += strlen(next->val)+2;  /* extra room for ", " */
	}

 	if(!strcasecmp(line->key,"Content-Type")) {
	  envstr = (char *)malloc(key_val_len+sizeof("="));
	  p = envstr;
	}
	else {
	  envstr = (char *)malloc(key_val_len+sizeof("HTTP_="));
	  p = stpcpy(envstr, "HTTP_");
	}

	for(q = line->key; *q; ++p,++q) {
	  if(isupper(*q) || isdigit(*q) || *q == '_') *p = *q;
	  else if (islower(*q)) *p = toupper(*q);
	  else if(*q == '-') *p = '_';
	  else {
	    /* key contains illegal character, ignore key */
	    free(envstr);
	    goto nextline;
	  }
	}

	*p++ = '=';
	p = stpcpy(p, line->val);

	{
	  KeyValueNode *l;
	  for(l=line->next; l!=next; l=l->next)
	    p = stpcpy(stpcpy(p, ", "), l->val);
	}

 	putenv_string(envstr);
      }
    nextline:
      line=next;
    }
  }

  if(request_body)
    {
      char length[MAX_INT_STR+1];
      sprintf(length,"%lu",(unsigned long)request_body->length);
      putenv_var_val("CONTENT_LENGTH",length);
    }

  {
    char onlinestr[MAX_INT_STR+1];
    sprintf(onlinestr,"%d",online);
    putenv_var_val("ONLINE",onlinestr);
  }

  return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Execute a local file as CGI program.

  int exec_cgi returns 1 on success, 0 on failure.

  int fd The file descriptor to write the output to.

  char *file: The name of the file to execute.

  URL *Url: The URL referring to the CGI script to execute.

  Header *request_head The head of the request.

  Body *request_body The body of the request.
  ++++++++++++++++++++++++++++++++++++++*/

inline static int exec_cgi(int fd, char *file, URL *Url, Header *request_head, /*@null@*/ Body *request_body)
{
  int fdpipe[2];
  pid_t childpid;

  if(request_body)
    {
      if(pipe(fdpipe))
	{
	  PrintMessage(Warning, "Cannnot create pipe for local CGI program '%s' [%!s].", file);
	  return 0;
	}
    }

  if((childpid = fork()) == -1)
    PrintMessage(Warning, "Cannnot fork to execute local CGI program '%s' [%!s].", file);
  else if(childpid == 0)   /* The child */
    {
      int cgi_in;
      if(request_body) {
	cgi_in=fdpipe[0];
	close(fdpipe[1]);
      }
      else {
	cgi_in=open("/dev/null",O_RDONLY);
	if(cgi_in==-1) {
	      PrintMessage(Warning, "Cannnot open /dev/null for local CGI program '%s' [%!s].", file);
	      exit(2);
	}
      }

      if(cgi_in!=STDIN_FILENO)
	{
	  if(dup2(cgi_in,STDIN_FILENO) == -1)
	    {
	      PrintMessage(Warning, "Cannnot dup standard input for local CGI program '%s' [%!s].", file);
	      exit(2);
	    }
	  close(cgi_in);
	}

      if(fd!=STDOUT_FILENO)
	{
	  if(dup2(fd,STDOUT_FILENO) == -1)
	    {
	      PrintMessage(Warning, "Cannnot dup standard output for local CGI program '%s' [%!s].", file);
	      exit(2);
	    }
	  close(fd);
	}

    if(fcntl(STDERR_FILENO,F_GETFL,0)==-1 && errno==EBADF) /* stderr is not open */
      {
       int cgi_err=open("/dev/null",O_WRONLY);

       if(cgi_err!=STDERR_FILENO)
         {
          if(dup2(cgi_err,STDERR_FILENO)==-1)
	    {
	      PrintMessage(Warning, "Cannnot create standard error for local CGI program '%s' [%!s].",file);
	      exit(2);
	    }
          close(cgi_err);
         }
      }

      if(putenv_request(file,Url,request_head,request_body) == -1)
	PrintMessage(Warning, "Failed to create environment for local CGI program '%s' [%!s].", file);
      execl(file, file, NULL);
      PrintMessage(Warning, "Failed to execute local CGI program '%s' [%!s].", file);
      exit(2);
    }
  else   /* The parent */
    {
      int statval;

      PrintMessage(Inform, "Forked local CGI program '%s' (pid=%d).", file,childpid);

      if(request_body)
	{
	  close(fdpipe[0]);

	  if(write_all(fdpipe[1],request_body->content,request_body->length)<0)
	    PrintMessage(Warning, "Failed to write data to local CGI program '%s' (pid=%d) [%!s].", file,childpid);

	  close(fdpipe[1]);
	}

      if(waitpid(childpid, &statval, 0) != -1)
	{
	  if(WIFEXITED(statval))
	    {
	      int exitstatus = WEXITSTATUS(statval);
	      PrintMessage(exitstatus?Warning:Inform,
			   "Local CGI program '%s' (pid=%d) exited with status %d.", file,childpid,exitstatus);
	      if(exitstatus <=1) return 1;
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
  LocalCGI writes the results of a local CGI script,
  and returns 1 if it succeeds otherwise 0.

  int fd The file descriptor to write to.

  char *file: The name of the file.

  URL *Url The URL refering to the CGI.

  Header *request_head The request that was made for this page.

  Body *request_body The body of the request that was made for this page.
  ++++++++++++++++++++++++++++++++++++++*/

int LocalCGI(int fd, char *file, URL *Url, Header *request_head, Body *request_body)
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
    PrintMessage(Debug,"Running the local CGI program '%s'.",file);

    exec_success=exec_cgi(fd,file,Url,request_head,request_body);
   }
 else
    PrintMessage(Warning,"Refuse to run local CGI program file '%s' with root privileges.",file);

 return exec_success;
}
