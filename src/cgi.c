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
/* The following declarations should be contained in unistd.h, but for some reason there are not on my system.
   I list them here to avoid distracting warning messages when compiling with the -Wall option. */
#if HAVE_GETRESUID
int getresuid(uid_t *ruid, uid_t *euid, uid_t *suid);
#endif
#if HAVE_GETRESGID
int getresgid(gid_t *rgid, gid_t *egid, gid_t *sgid);
#endif

#include <sys/wait.h>
#include <sys/stat.h>

#include "version.h"
#include "wwwoffle.h"
#include "errors.h"
#include "config.h"
#include "misc.h"
#include "headbody.h"


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
char *client_hostname=NULL, *client_ip=NULL;
char *proxy_user=NULL;


/* First a macro definition that makes life a little easier. */
#define putenv_var_val(varname,value,failaction) \
{ \
  char *envstr = (char *)malloc(sizeof(varname "=")+strlen(value)); \
  stpcpy(stpcpy(envstr,varname "="),value); \
  if(putenv(envstr) == -1) failaction; \
}


/*++++++++++++++++++++++++++++++++++++++
  Put information of request headers into environment according to CGI-specification.

  int putenv_request Returns 0 on success or -1 on error.

  char *file: Filename of script.
  URL *Url: The URL referring to the CGI script.

  Header *request_head The head of the request that was made for this page.

  Body *request_body The body of the request (only if POST or PUT).
  ++++++++++++++++++++++++++++++++++++++*/

/*inline*/ static int putenv_request(char *file, URL *Url, Header *request_head, /*@null@*/ Body *request_body)
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
    URL *request_Url=SplitURL(request_head->url);

    if(!GetHeader(request_head,"Host"))
      putenv_var_val("HTTP_HOST",request_Url->hostport, {retval=-1; goto cleanupURL;});

    putenv_var_val("PATH_INFO",request_Url->path, {retval=-1; goto cleanupURL;});

    if (request_Url->args)
      putenv_var_val("QUERY_STRING",request_Url->args, retval=-1);

  cleanupURL:
    FreeURL(request_Url);
    if(retval) return retval;
  }

  putenv_var_val("SCRIPT_NAME",Url->path, return -1);

  putenv_var_val("PATH_TRANSLATED",file, return -1);

  if(client_hostname)
    putenv_var_val("REMOTE_HOST",client_hostname, return -1);

  if(client_ip)
    putenv_var_val("REMOTE_ADDR",client_ip, return -1);

  if(proxy_user) {
    if(putenv("AUTH_TYPE=Basic") == -1) return -1;
    putenv_var_val("REMOTE_USER",proxy_user, return -1);
  }

  {
    int i=0, n = request_head->n; KeyValuePair *line = request_head->line;

    while(i<n) {
      int j=i+1;

      if(line[i].key && strcasecmp(line[i].key,"Content-Length") && strcasecmp(line[i].key,"Proxy-Authorization")) {
	char *envstr, *p,*q;
	int key_val_len = strlen(line[i].key)+strlen(line[i].val);

	/* first look for a run of identical keys */
	for (; j<n; ++j) {
	  if(strcasecmp(line[j].key,line[i].key)) break;
	  key_val_len += strlen(line[j].val)+2;  /* extra room for ", " */
	}
    
 	if(!strcasecmp(line[i].key,"Content-Type")) {
	  envstr = (char *)malloc(key_val_len+sizeof("="));
	  p = envstr;
	}
	else {
	  envstr = (char *)malloc(key_val_len+sizeof("HTTP_="));
	  p = stpcpy(envstr, "HTTP_");
	}

	for(q = line[i].key; *q; ++p,++q) {
	  if(isupper(*q) || isdigit(*q) || *q == '_') *p = *q;
	  else if (islower(*q)) *p = toupper(*q);
	  else if(*q == '-') *p = '_';
	  else {
	    /* key contains illegal character, ignore key */
	    free(envstr);
	    goto nexti;
	  }
	}

	*p++ = '=';
	p = stpcpy(p, line[i].val);

	{
	  int l;
	  for(l=i+1; l<j; ++l)
	    p = stpcpy(stpcpy(p, ", "), line[l].val);
	}

 	if(putenv(envstr) == -1) return -1;
      }
    nexti:
      i=j;
    }
  }

  if(request_body)
    {
      char length[12];
      sprintf(length,"%d",request_body->length);
      putenv_var_val("CONTENT_LENGTH",length, return -1);
    }

  return retval;
}


/*++++++++++++++++++++++++++++++++++++++
  Execute a local file as CGI program.

  int ExecLocalCGI returns 1 on success, 0 on failure.

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
      if(request_body)
	{
	  if(fdpipe[0]!=0)
	    {
	      if(dup2(fdpipe[0],0) == -1)
		{
		  PrintMessage(Warning, "Cannnot dup standard input for local CGI program '%s' [%!s].", file);
		  exit(2);
		}
	      close(fdpipe[0]);
	    }
	  close(fdpipe[1]);
	}
      if(fd!=1)
	{
	  if(dup2(fd,1) == -1)
	    {
	      PrintMessage(Warning, "Cannnot dup standard output for local CGI program '%s' [%!s].", file);
	      exit(2);
	    }
	  close(fd);
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
  LocalCGI outputs the results of a local CGI script.

  int fd The file descriptor to write to.

  char *file: The name of the file.

  URL *Url The URL refering to the CGI.

  Header *request_head The request that was made for this page.

  Body *request_body The body of the request that was made for this page.
  ++++++++++++++++++++++++++++++++++++++*/

void LocalCGI(int fd, char *file, URL *Url, Header *request_head, Body *request_body)
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

 if(!exec_success)
   {
    lseek(fd,0,SEEK_SET);
    ftruncate(fd,0);

    HTMLMessage(fd,500,"WWWOFFLE Local Program Execution Error",NULL,"ExecCGIFailed",
                "url",Url->path,
                NULL);
   }
}
