/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/wwwoffle.h 2.92 2002/10/27 13:33:51 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7g.
  A header file for all of the programs wwwoffle, wwwoffled.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef WWWOFFLE_H
#define WWWOFFLE_H    /*+ To stop multiple inclusions. +*/

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

#include "misc.h"

/* In connect.c */

void CommandConnect(int client);
void ForkRunModeScript(/*@null@*/ char *filename,char *mode,/*@null@*/ char *arg,int client);
void ForkServer(int client,int browser);

/* In purge.c */

void PurgeCache(int fd);

/* In spool.c */

int OpenOutgoingSpoolFile(int rw);
void CloseOutgoingSpoolFile(int fd,URL *Url);
int ExistsOutgoingSpoolFile(URL *Url);
char /*@null@*/ *HashOutgoingSpoolFile(URL *Url);
char /*@observer@*/ *DeleteOutgoingSpoolFile(/*@null@*/ URL *Url);

int OpenWebpageSpoolFile(int rw,URL *Url);
char /*@observer@*/ *DeleteWebpageSpoolFile(URL *Url,int all);
void TouchWebpageSpoolFile(URL *Url,time_t when);
time_t ExistsWebpageSpoolFile(URL *Url);

void CreateBackupWebpageSpoolFile(URL *Url);
void RestoreBackupWebpageSpoolFile(URL *Url);
void DeleteBackupWebpageSpoolFile(URL *Url);

void CreateLockWebpageSpoolFile(URL *Url);
void DeleteLockWebpageSpoolFile(URL *Url);
int ExistsLockWebpageSpoolFile(URL *Url);

int CreateLastTimeSpoolFile(URL *Url);
char /*@observer@*/ *DeleteLastTimeSpoolFile(URL *Url);
void CycleLastTimeSpoolFile(void);
void CycleLastOutSpoolFile(void);

int CreateMonitorSpoolFile(URL *Url,char MofY[13],char DofM[32],char DofW[8],char HofD[25]);
long ReadMonitorTimesSpoolFile(URL *Url,/*@out@*/ char MofY[13],/*@out@*/ char DofM[32],/*@out@*/ char DofW[8],/*@out@*/ char HofD[25]);
char /*@observer@*/ *DeleteMonitorSpoolFile(/*@null@*/ URL *Url);

int CreateTempSpoolFile(void);
void CloseTempSpoolFile(int fd);

char *FileNameToURL(char *file);
char *URLToFileName(URL *Url);

#if defined(__CYGWIN__)
int fchdir(int fd);
#endif

/* In parse.c */

char /*@null@*/ *ParseRequest(int fd,/*@out@*/ Header **request_head,/*@out@*/ Body **request_body);

int RequireChanges(int fd,Header *request_head,URL *Url);
int IsModified(int fd,Header *request_head);
char *MovedLocation(URL *Url,Header *reply_head);
Header *RequestURL(URL *Url,/*@null@*/ char *referer);

void ModifyRequest(URL *Url,Header *request_head);

void MakeRequestProxyAuthorised(char *proxy,Header *request_head);
void MakeRequestNonProxy(URL *Url,Header *request_head);

int ParseReply(int fd,/*@out@*/ Header **reply_head);

int SpooledPageStatus(URL *Url);

int WhichCompression(char *content_encoding);

void ModifyReply(URL *Url,Header *reply_head);

/* In messages.c (messages.l) */

char /*@observer@*/ *HTMLMessage(int fd,int status_val,char *status_str,/*@null@*/ char *location,char *template, ...);
char /*@observer@*/ *HTMLMessageHead(int fd,int status_val,char *status_str, ...);
char /*@observer@*/ *HTMLMessageBody(int fd,char *template, ...);

/* In local.c */

void LocalPage(int fd,URL *Url,Header *request_head,/*@null@*/ Body *request_body);

char /*@null@*/ *FindLanguageFile(char* search);
int OpenLanguageFile(char* search);
void SetLanguage(char *accept);

/* In cgi.c */

void LocalCGI(int fd,URL *Url,char *file,Header *request_head,/*@null@*/ Body *request_body);

/* In index.c */

void IndexPage(int fd,URL *Url);

/* In info.c */

void InfoPage(int fd,URL *Url,Header *request_head,/*@null@*/ Body *request_body);

/* In control.c */

void ControlPage(int fd,URL *Url,/*@null@*/ Body *request_body);

/* In controledit.c */

void ControlEditPage(int fd,char *request_args,/*@null@*/ Body *request_body);

/* In configuration.h */

void ConfigurationPage(int fd,URL *Url,/*@null@*/ Body *request_body);

/* In refresh.c */

char /*@null@*/ *RefreshPage(int fd,URL *Url,/*@null@*/ Body *request_body,int *recurse);
void DefaultRecurseOptions(URL *Url);
int RecurseFetch(URL *Url,int new);
int RecurseFetchRelocation(URL *Url,char *location);
char *CreateRefreshPath(URL *Url,char *limit,int depth,
                        int force,
                        int stylesheets,int images,int frames,int scripts,int objects);
int RefreshForced(void);

/* In monitor.c */

void MonitorPage(int fd,URL *Url,/*@null@*/ Body *request_body);
void RequestMonitoredPages(void);
void MonitorTimes(URL *Url,/*@out@*/ int *last,/*@out@*/ int *next);

/* In wwwoffles.c */

int wwwoffles(int online,int browser,int client);

/* In search.c */

void SearchPage(int fd,URL *Url,Header *request_head,/*@null@*/ Body *request_body);

#endif /* WWWOFFLE_H */
