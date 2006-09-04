/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/wwwoffle.h 2.103 2004/01/17 16:22:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  A header file for all of the programs wwwoffle, wwwoffled.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04 Andrew M. Bishop
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
void ForkServer(int fd);


/* In purge.c */

int PurgeCache(int fd);


/* In spool.c */

int OpenOutgoingSpoolFile(int rw);
void CloseOutgoingSpoolFile(int fd,URL *Url);
int ExistsOutgoingSpoolFile(URL *Url);
char /*@null@*/ *HashOutgoingSpoolFile(URL *Url);
char /*@only@*/ /*@null@*/ *DeleteOutgoingSpoolFile(/*@null@*/ URL *Url);

int OpenWebpageSpoolFile(int rw,URL *Url);
char /*@only@*/ /*@null@*/ *DeleteWebpageSpoolFile(URL *Url,int all);
void TouchWebpageSpoolFile(URL *Url,time_t when);
time_t ExistsWebpageSpoolFile(URL *Url,int backup);

void CreateBackupWebpageSpoolFile(URL *Url, int overwrite);
void RestoreBackupWebpageSpoolFile(URL *Url);
void DeleteBackupWebpageSpoolFile(URL *Url);
int OpenBackupWebpageSpoolFile(URL *Url);

int CreateLockWebpageSpoolFile(URL *Url);
void DeleteLockWebpageSpoolFile(URL *Url);
int ExistsLockWebpageSpoolFile(URL *Url);

int CreateLastTimeSpoolFile(URL *Url);
char /*@only@*/ /*@null@*/ *DeleteLastTimeSpoolFile(char *name);
void CycleLastTimeSpoolFile(void);
void CycleLastOutSpoolFile(void);

int CreateMonitorSpoolFile(URL *Url,char MofY[13],char DofM[32],char DofW[8],char HofD[25]);
long ReadMonitorTimesSpoolFile(URL *Url,/*@out@*/ char MofY[13],/*@out@*/ char DofM[32],/*@out@*/ char DofW[8],/*@out@*/ char HofD[25]);
char /*@only@*/ /*@null@*/ *DeleteMonitorSpoolFile(/*@null@*/ URL *Url);

int CreateTempSpoolFile(void);
void CloseTempSpoolFile(int fd);

char *FileNameToURL(const char *file);
int FileMarkHash(const char *file);

/*++++++++++++++++++++++++++++++++++++++
  A macro to Convert a URL to a filename.
  local_URLToFileName declares filename as an array of char
  and fills it with the file name corresponding to Url.
  Some extra space is reserved to add a possible ~ suffix.
  Written by Paul Rombouts as a replacement for the function URLToFileName().
  ++++++++++++++++++++++++++++++++++++++*/

#define local_URLToFileName(Url,c,filename)		\
 char filename[base64enclen(sizeof(md5hash_t))+3];	\
 *(filename)=(c);					\
 GetHash(Url,(filename)+1,sizeof(filename)-2);


int ChangeToSpoolDir(char *dir);
int /*@alt void@*/ ChangeBackToSpoolDir(void);
int CloseSpoolDir();


/* In parse.c */

char /*@null@*/ *ParseRequest(int fd,/*@out@*/ Header **request_head,/*@out@*/ Body **request_body);

int RequireChanges(int fd,URL *Url,char **ims,char **inm);
int IsModified(int fd,Header *request_head);
char *MovedLocation(URL *Url,Header *reply_head);
Header *RequestURL(URL *Url,/*@null@*/ char *referer);

void FinishParse(void);

void ModifyRequest(URL *Url,Header *request_head);

void MakeRequestProxyAuthorised(char *proxy,Header *request_head);
void MakeRequestNonProxy(URL *Url,Header *request_head);

int ParseReply(int fd,/*@out@*/ Header **reply_head);

int SpooledPageStatus(URL *Url,int backup);
Header *SpooledPageHeader(URL *Url,int backup);

int WhichCompression(char *content_encoding);
int AcceptWhichCompression(HeaderList *list);

void ModifyReply(URL *Url,Header *reply_head);


/* In messages.c (messages.l) */

extern int out_err;
extern int head_only;
void SetMessageOptions(int compressed,int chunked);
void HTMLMessage(int fd,int status_val,char *status_str,/*@null@*/ char *location,char *template, ...);
void HTMLMessageHead(int fd,int status_val,char *status_str, ...);
void HTMLMessageBody(int fd,char *template, ...);
char /*@observer@*/ *HTMLMessageString(char *template, ...);
void FinishMessages(void);


/* In local.c */

int LocalPage(int fd,URL *Url,Header *request_head,/*@null@*/ Body *request_body);
int OpenLocalFile(char *path);
int OpenLanguageFile(char* search);
void SetLanguage(Header *head);


/* In cgi.c */

int LocalCGI(int fd,char *file,URL *Url,Header *request_head,/*@null@*/ Body *request_body);


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
void DefaultRecurseOptions(URL *Url,Header *head);
int RecurseFetch(URL *Url);
int RecurseFetchRelocation(URL *Url,char *location);
char /*@only@*/ *CreateRefreshPath(URL *Url,char *limit,int depth,
                                   int force,
                                   int stylesheets,int images,int frames,int scripts,int objects);
int RefreshForced(void);


/* In monitor.c */

void MonitorPage(int fd,URL *Url,/*@null@*/ Body *request_body);
void RequestMonitoredPages(void);
void MonitorTimes(URL *Url,/*@out@*/ int *last,/*@out@*/ int *next);


/* In wwwoffles.c */

int wwwoffles(int online,int fetching,int client);

int wwwoffles_read_data(/*@out@*/ char *data,int len);
int wwwoffles_write_data(/*@in@*/ char *data,int len);


/* In search.c */

int SearchPage(int fd,URL *Url,Header *request_head,/*@null@*/ Body *request_body);


#endif /* WWWOFFLE_H */
