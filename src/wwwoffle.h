/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9d.
  A header file for all of the programs wwwoffle, wwwoffled.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1996-2007 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2005,2006,2007,2008,2011 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef WWWOFFLE_H
#define WWWOFFLE_H    /*+ To stop multiple inclusions. +*/

#include "autoconfig.h"

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

#ifndef CLIENT_ONLY
#include <signal.h>
#endif
#include "misc.h"

#if USE_IPV6
#define NUMIPPROT 2
#else
#define NUMIPPROT 1
#endif

typedef enum _socktype {
  wwwoffle_sock
 ,http_sock
#if USE_GNUTLS
 ,https_sock
#endif
 ,numsocktype
}
socktype;


/* In connect.c */

int CommandConnect(int client, char *log_file);
void ForkRunModeScript(/*@null@*/ char *filename,char *mode,/*@null@*/ char *arg,int client);
void ForkServer(int fd);


/* In purge.c */

int PurgeCache(int fd);


/* In spool.c */

int OpenNewOutgoingSpoolFile(void);
void CloseNewOutgoingSpoolFile(int fd,URL *Url);
int OpenExistingOutgoingSpoolFile(/*@out@*/ URL **Url);

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
char /*@only@*/ /*@null@*/ *DeleteLastTimeSpoolFile(const char *name);
void CycleLastTimeSpoolFile(void);
void CycleLastOutSpoolFile(void);

int CreateMonitorSpoolFile(URL *Url,char MofY[13],char DofM[32],char DofW[8],char HofD[25]);
long ReadMonitorTimesSpoolFile(URL *Url,/*@out@*/ char MofY[13],/*@out@*/ char DofM[32],/*@out@*/ char DofW[8],/*@out@*/ char HofD[25]);
char /*@only@*/ /*@null@*/ *DeleteMonitorSpoolFile(/*@null@*/ URL *Url);

int CreateTempSpoolFile(void);
void CloseTempSpoolFile(int fd);

char *FileNameToUrl(const char *file);
inline static URL *FileNameToURL(const char *file) {char *url=FileNameToUrl(file); return url?SplitURL(url):NULL;}

int FileMarkHash(const char *file);

/*++++++++++++++++++++++++++++++++++++++
  A macro to Convert a URL to a filename.
  local_URLToFileName declares filename as an array of char
  and fills it with the file name corresponding to Url.
  Some extra space is reserved to add a possible ~ suffix.
  Written by Paul Rombouts as a replacement for the function URLToFileName().
  ++++++++++++++++++++++++++++++++++++++*/

#define local_URLToFileName(Url,c,p,filename)		\
 char filename[base64enclen(sizeof(md5hash_t))+3];	\
 *(filename)=(c);					\
 GetHash(Url,(filename)+1,sizeof(filename)-2);		\
 if(p) {						\
   filename[CACHE_HASHED_NAME_LEN+1]='~';		\
   filename[CACHE_HASHED_NAME_LEN+2]=0;			\
 }

#ifdef __CYGWIN__
char /*@observer@*/ *URLToDirName(URL *Url);
#else
inline static char *URLToDirName(URL *Url) {return Url->hostport;}
#endif

int ChangeToSpoolDir(const char *dir);
void ChangeBackToSpoolDir(void);
int CloseSpoolDir();

int SetCurrentOnlineStatus(int online);
int GetCurrentOnlineStatus(int *online);
int CleanupCurrentOnlineStatus();


/* In parse.c */

extern time_t OnlineTime;     /*+ The time that the program went online. +*/
extern time_t OfflineTime;    /*+ The time that the program went offline. +*/

URL /*@null@*/ *ParseRequest(int fd,/*@out@*/ Header **request_head,/*@out@*/ Body **request_body);

int RequireForced(const Header *request_head,const URL *Url,int online);
int RequireChanges(int fd,const Header *spooled_head,const URL *Url,char **ims,char **inm);
int IsModified(int fd,const Header *spooled_head,const Header *request_head);
URL /*@null@*/ *MovedLocation(const URL *Url,const Header *reply_head);
Header *RequestURL(const URL *Url,/*@null@*/ const URL *refererUrl);

void FinishParse(void);

void ModifyRequest(const URL *Url,Header *request_head);

void MakeRequestProxyAuthorised(const URL *proxyUrl,Header *request_head);
void MakeRequestNonProxy(const URL *Url,Header *request_head);

int ParseReply(int fd,/*@out@*/ Header **reply_head);

int SpooledPageStatus(URL *Url,int backup);
Header *SpooledPageHeader(URL *Url,int backup);

int WhichCompression(char *content_encoding);
int AcceptWhichCompression(HeaderList *list);

void ModifyReply(const URL *Url,Header *reply_head);


/* In messages.c (messages.l) */

extern ssize_t out_err;
extern int head_only;
extern int client_keep_connection;
void SetMessageOptions(int compressed,int chunked);
void HTMLMessage(int fd,int status_val,const char *status_str,/*@null@*/ const char *location,const char *template, ...);
int  HTMLMessageHead(int fd,int status_val,const char *status_str, ...);
void HTMLMessageBody(int fd,const char *template, ...);
char /*@observer@*/ *HTMLMessageString(const char *template, ...);
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


#if USE_GNUTLS

/* In certinfo.c */

void CertificatesPage(int fd,URL *Url,Header *request_head);

#endif


/* In control.c */

void ControlPage(int fd,URL *Url,/*@null@*/ Body *request_body);


/* In controledit.c */

void ControlEditPage(int fd,char *request_args,/*@null@*/ Body *request_body);


/* In configuration.h */

void ConfigurationPage(int fd,URL *Url,/*@null@*/ Body *request_body);


/* In refresh.c */

URL /*@null@*/ *RefreshPage(int fd,URL *Url,/*@null@*/ Body *request_body,int *recurse);
void DefaultRecurseOptions(URL *Url,Header *head);
int RecurseFetch(const URL *Url);
int RecurseFetchRelocation(const URL *Url,URL *locationUrl);
char /*@only@*/ *CreateRefreshPath(const URL *Url,char *limit,int depth,
                                   int force,
                                   int stylesheets,int images,int frames,int iframes,int scripts,int objects);
int RefreshForced(void);


/* In monitor.c */

void MonitorPage(int fd,URL *Url,/*@null@*/ Body *request_body);
void RequestMonitoredPages(void);
void MonitorTimes(URL *Url,/*@out@*/ int *last,/*@out@*/ int *next);


/* In wwwoffled.c */
#ifndef CLIENT_ONLY
extern int socks_fd[numsocktype][NUMIPPROT]; /*+ The server sockets that we listen on +*/
extern int online;            /*+ The online / offline / autodial status. +*/
extern int n_servers,         /*+ The number of active servers in total. +*/
           n_fetch_servers;   /*+ The number of active servers fetching a page. +*/
extern int max_servers,       /*+ The maximum number of servers in total. +*/
           max_fetch_servers; /*+ The maximum number of servers fetching a page. +*/
extern int fetch_fd;          /*+ The wwwoffle client file descriptor when fetching. +*/
extern int server_pids[MAX_SERVERS]; /*+ The pids of the servers. +*/
extern unsigned char server_fetching[MAX_SERVERS]; /*+ The flags indicating which of the servers are fetching. +*/
extern int purging;           /*+ The current purge status. +*/
extern int purge_pid;         /*+ The pid of the purge process. +*/
extern short int fetching;    /*+ The current status, fetching or not. +*/
extern short int nofork;      /*+ True if the -f option was passed on the command line. +*/
extern volatile sig_atomic_t got_sigexit; /*+ Set when the process received a signal requesting termination. +*/
extern char *client_hostname, *client_ip;

/* In wwwoffles.c */

extern char* proxy_user;      /* pass on name of proxy user via global variable */
#endif

int wwwoffles(int online,int fetching,int client);

ssize_t wwwoffles_read_data(/*@out@*/ char *data,size_t len);
ssize_t wwwoffles_write_data(/*@in@*/ const char *data,size_t len);


/* In search.c */

int SearchPage(int fd,URL *Url,Header *request_head,/*@null@*/ Body *request_body);


#endif /* WWWOFFLE_H */
