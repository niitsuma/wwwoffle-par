/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/errors.h 2.13 2004/01/17 16:22:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Error logging header file.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef ERRORS_H
#define ERRORS_H    /*+ To stop multiple inclusions. +*/

/* Definitions that cause errno not to be used. */

#define ERRNO_USE_H_ERRNO   -1
#define ERRNO_USE_IO_ERRNO  -2
#define ERRNO_USE_GAI_ERRNO -3

/*+ The different error levels. +*/
typedef enum _ErrorLevel
{
 ExtraDebug,                    /*+ For extra debugging (not in syslog). +*/
 Debug,                         /*+ For debugging (debug in syslog). +*/
 Inform,                        /*+ General information (info in syslog). +*/
 Important,                     /*+ Important information (notice in syslog). +*/
 Warning,                       /*+ A warning (warning in syslog). +*/
 Fatal                          /*+ A fatal error (err in syslog). +*/
}
ErrorLevel;

/* In errors.c */

/*+ The level of error logging +*/
extern ErrorLevel SyslogLevel,  /*+ in the config file for syslog and stderr. +*/
                  StderrLevel;  /*+ on the command line for stderr. +*/


void InitErrorHandler(char *name,int syslogable,int stderrable);
void OpenErrorLog(char *name);
void PrintMessage(ErrorLevel errlev,const char* fmt, ...);
char /*@special@*/ *GetPrintMessage(ErrorLevel errlev,const char* fmt, ...) /*@defines result@*/;

#endif /* ERRORS_H */
