/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/errors.h 2.8 2002/07/28 10:07:47 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7d.
  Error logging header file.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef ERRORS_H
#define ERRORS_H    /*+ To stop multiple inclusions. +*/

/* Definitions that cause errno not to be used. */

#define ERRNO_USE_H_ERRNO   -1
#define ERRNO_USE_Z_ERRNO   -2
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
extern ErrorLevel LoggingLevel,     /*+ in the config file for syslog and stderr. +*/
                  DebuggingLevel;   /*+ on the command line for stderr. +*/


void  InitErrorHandler(char *name,int syslogable,int stderrable);
char /*@observer@*/ *PrintMessage(ErrorLevel errlev,const char* fmt, ...);

#endif /* ERRORS_H */
