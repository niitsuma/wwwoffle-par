/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/errors.c 2.32 2002/08/04 10:27:34 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7e.
  Generate error messages in a standard format optionally to syslog and stderr.
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

#include <sys/fcntl.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

/* syslog definitions */

#if !defined(__CYGWIN__)
#include <syslog.h>

#if defined (hpux)
/* HP/UX does not declare these in <syslog.h> */
extern int syslog (int pri, const char *message, ...);
extern int openlog (const char *ident, int logopt, int facility);
extern int closelog (void);
#endif

#else
/* I prefer not to use the Cygwin syslog functions here. */
static void openlog(char *facility);
static void closelog(void);
static void syslog(int level,const char* format,char* string);
#endif /* __CYGWIN__ */

/* errno and str_error() definitions */

#include <errno.h>

#if defined(__sun__) && !defined(__svr4__)
/* SunOS 4.x does not have strerror(). */
char* strerror(int err);

extern int sys_nerr;
extern char *sys_errlist[];

char* strerror(int err)
{
 if(err>0 && err<sys_nerr)
    return(sys_errlist[err]);
 else
    return("Unknown error");
}
#endif /* defined(__sun__) && !defined(__svr4__) */

/* h_errno and h_strerror() definitions */

#include <netdb.h>

#if defined (hpux)
/* HP/UX has h_errno but does not declare it in any header file */
extern int h_errno;
#endif

#include "errors.h"
#include "config.h"
#include "misc.h"


/* A function to get an error message for h_errno. */

static /*@observer@*/ char* h_strerror(int err);

static char* h_strerror(int err)
{
#ifdef NETDB_INTERNAL
 if(err==NETDB_INTERNAL)
    return("Name Lookup Internal error");
 else
#endif
#ifdef NETDB_SUCCESS
 if(err==NETDB_SUCCESS)
    return("Name Lookup Success");
 else
#endif
#ifdef HOST_NOT_FOUND
 if(err==HOST_NOT_FOUND)
    return("Name Lookup Authoritative Answer Host not found");
 else
#endif
#ifdef TRY_AGAIN
 if(err==TRY_AGAIN)
    return("Name Lookup Non-Authoritative Answer Host not found");
 else
#endif
#ifdef NO_RECOVERY
 if(err==NO_RECOVERY)
    return("Name Lookup Non recoverable error");
 else
#endif
#ifdef NO_DATA
 if(err==NO_DATA)
    return("Name Lookup Valid name, no data record of requested type");
 else
#endif
#ifdef NO_ADDRESS
 if(err==NO_ADDRESS)
    return("Name Lookup Valid name, no data record of requested type");
 else
#endif
 return("Unknown error");
}

/* z_errno and z_errstr definitions */

#if USE_ZLIB
/*+ The compression error number. +*/
extern int z_errno;

/*+ The compression error message string. +*/
extern char *z_strerror;
#endif

/* gai_errno and gai_strerror() definitions */

#if USE_IPV6
/*+ The IPv6 functions error number. +*/
extern int gai_errno;
#endif

static char unknown_program[]= "?";
/*+ The name of the program. +*/
static char *program=unknown_program;

/*+ The process id of the program. +*/
static pid_t pid;

/*+ The last time that a message was printed. +*/
static time_t last_time=0;

/*+ The error messages. +*/
static char *ErrorString[]={"ExtraDebug","Debug"  ,"Information","Important","Warning"  ,"Fatal"};

/*+ The priority to apply to syslog messages. +*/
#if defined(__CYGWIN__)
static int ErrorPriority[]={-1          ,Debug    ,Inform       ,Important  ,Warning    ,Fatal};
#else
static int ErrorPriority[]={-1          ,LOG_DEBUG,LOG_INFO     ,LOG_NOTICE ,LOG_WARNING,LOG_ERR};
#endif

/*+ The error facility to use, +*/
static int use_syslog=0,        /*+ use syslog. +*/
           use_stderr=1;        /*+ use stderr. +*/

/*+ The level of error logging +*/
ErrorLevel LoggingLevel=Important,  /*+ in the config file for syslog and stderr. +*/
           DebuggingLevel=-1;       /*+ on the command line for stderr. +*/


/*++++++++++++++++++++++++++++++++++++++
  Initialise the error handler, get the program name and pid.

  char *name The name of the program.

  int syslogable Set to true if the errors are allowed to go to syslog (or -1 to remain unchanged).

  int stderrable Set to true if the errors are allowed to go to stderr (or -1 to remain unchanged).
  ++++++++++++++++++++++++++++++++++++++*/

void InitErrorHandler(char *name,int syslogable,int stderrable)
{
 if(use_syslog && program!=unknown_program)
    closelog();

 program=name;
 if(syslogable!=-1)
    use_syslog=syslogable;
 if(stderrable!=-1)
    use_stderr=stderrable;

 if(use_syslog)
   {
#if defined(__CYGWIN__)
    openlog(program);
#elif defined(__ultrix__)
    openlog(program,LOG_PID);
#else
    openlog(program,LOG_CONS|LOG_PID,LOG_DAEMON);
#endif
    atexit(closelog);
   }

 pid=getpid();

 last_time=time(NULL)-3300;
}


/*++++++++++++++++++++++++++++++++++++++
  Print an error message.

  char *PrintMessage Return the error message (except the pid etc.).

  ErrorLevel errlev Which error level.

  const char* fmt The format of the message.

  ... The rest of the arguments (printf style).
  ++++++++++++++++++++++++++++++++++++++*/

char *PrintMessage(ErrorLevel errlev,const char* fmt, ...)
{
 static char* /*@only@*/ string=NULL;
 const char *new_fmt=fmt;
 time_t this_time;

 /* Shortcut (bypass if debug) */

 if(errlev<=Debug && (errlev<LoggingLevel && errlev<DebuggingLevel))
    return(NULL);

 /* Periodic timestamp */

 this_time=time(NULL);
 if(last_time && (this_time-last_time)>3600)
   {
    last_time=this_time;
    if(use_stderr)
       fprintf(stderr,"%s[%ld] Timestamp: %s",program,(long)pid,ctime(&this_time)); /* Used in audit-usage.pl */
   }

 if(string)
    free(string);

 /* Parsing of special conversion specifiers %!s and %!d. */
 {
   int fmt_len=0,strerrlen=0,strerrnolen=0;
   char *strerr=NULL, *strerrno=NULL, *p; const char *q;

   for(q=fmt; *q; ++q) {
     if(*q=='%') {
       if(*++q=='!') {
	 if(*++q=='s') {
	   if(!strerr) {
	     char *r; const char *str,*s;
	     if(errno==ERRNO_USE_H_ERRNO)
	       str=h_strerror(h_errno);
#if USE_ZLIB
	     else if(errno==ERRNO_USE_Z_ERRNO)
	       str=z_strerror;
#endif
#if USE_IPV6
	     else if(errno==ERRNO_USE_GAI_ERRNO)
	       str=(char*)gai_strerror(gai_errno);
#endif
	     else
	       str=strerror(errno);

	     /* We have to double any possible % */
	     strerrlen=0;
	     for(s=str; *s; ++s) {
	       ++strerrlen;
	       if(*s=='%') ++strerrlen;
	     }
	   
	     strerr=r=(char *)alloca(strerrlen+1);
	     for(s=str; *s; ++s) {
	       *r++=*s;
	       if(*s=='%') *r++=*s;
	     }
	     *r=0;
	   }
	   fmt_len+=strerrlen;
	 }
	 else {
	   if(!strerrno) {
	     strerrno=(char *)alloca(24);
	     if(errno==ERRNO_USE_H_ERRNO)
	       strerrnolen=sprintf(strerrno,"%d (h_errno)",h_errno);
#if USE_ZLIB
	     else if(errno==ERRNO_USE_Z_ERRNO)
	       strerrnolen=sprintf(strerrno,"%d (z_errno)",z_errno);
#endif
#if USE_IPV6
	     else if(errno==ERRNO_USE_GAI_ERRNO)
	       strerrnolen=sprintf(strerrno,"%d (gai_errno)",gai_errno);
#endif
	     else
	       strerrnolen=sprintf(strerrno,"%d",errno);
	   }
	   fmt_len+=strerrnolen;
	   if(!*q) break;
	 }
	 continue;
       }
       else {
	 fmt_len+=1;
	 if(!*q) break;
       }
     }

     fmt_len+=1;
   }

   if(q==fmt || *(q-1)!='\n') {
     fmt_len+=1;
     goto make_new_fmt;
   }

   if(strerr || strerrno)
   make_new_fmt:
   {
     new_fmt= p= (char*)alloca(fmt_len+1);

     for(q=fmt; *q; ++q) {
       if(*q=='%') {
	 if(*++q=='!') {
	   if(*++q=='s')
	     p=stpcpy(p,strerr);
	   else {
	     p=stpcpy(p,strerrno);
	     if(!*q) break;
	   }
	   continue;
	 }
	 else {
	   *p++=*(q-1);
	   if(!*q) break;
	 }
       }

       *p++=*q;
     }

     if(q==fmt || *(q-1)!='\n') *p++='\n';

     *p=0;
   }
 }

 {
   va_list ap;

   va_start(ap,fmt);
   if(vasprintf(&string,new_fmt,ap)<0) string=NULL;
   va_end(ap);
 }

 /* Output the result. */

 if(use_syslog && errlev>=LoggingLevel && ErrorPriority[errlev]!=-1)
    syslog(ErrorPriority[errlev],"%s",string);

 if(use_stderr && ((DebuggingLevel==-1 && errlev>=LoggingLevel) || (DebuggingLevel!=-1 && errlev>=DebuggingLevel)))
   {
    if(LoggingLevel==ExtraDebug || DebuggingLevel==ExtraDebug)
       fprintf(stderr,"%s[%ld] %10ld: %s: %s",program,(long)pid,this_time,ErrorString[errlev],string);
    else
       fprintf(stderr,"%s[%ld] %s: %s",program,(long)pid,ErrorString[errlev],string);
   }

 if(errlev==Fatal)
    exit(2);

 return(string);
}

#if defined(__CYGWIN__)

static int syslog_file=-1;
static char *syslog_facility=NULL;

/*++++++++++++++++++++++++++++++++++++++
  Open the syslog file.
  ++++++++++++++++++++++++++++++++++++++*/

static void openlog(char *facility)
{
 char *config_file=ConfigurationFileName();
 char *p=strchrnul(config_file,0);
 char *syslogfile;
 int pathlen;

 while(--p>=config_file && *p!='/');
 pathlen=p+1-config_file;
 syslogfile=(char *)alloca(pathlen+sizeof("wwwoffle.syslog"));
 stpcpy(mempcpy(syslogfile,config_file,pathlen),"wwwoffle.syslog");

 syslog_file=open(syslogfile,O_WRONLY|O_CREAT|O_APPEND);
 init_buffer(syslog_file);

 syslog_facility=facility;
}


/*++++++++++++++++++++++++++++++++++++++
  Close the syslog file.
  ++++++++++++++++++++++++++++++++++++++*/

static void closelog(void)
{
 syslog_file=-1;
 syslog_facility=NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Write to the syslog file.
  ++++++++++++++++++++++++++++++++++++++*/

static void syslog(int level,const char* format,char* string)
{
 if(syslog_file!=-1 && syslog_facility)
    write_formatted(syslog_file,"%s[%ld](%s): %s",syslog_facility,(long)pid,ErrorString[level],string);
}

#endif /* __CYGWIN__ */
