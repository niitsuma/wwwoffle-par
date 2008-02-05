/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/connect.c 2.52 2007/04/20 16:04:29 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9c.
  Handle WWWOFFLE connections received by the demon.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1996,97,98,99,2000,01,02,03,05,07 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2004,2005,2006,2007,2008 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

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

#include <signal.h>

#include "wwwoffle.h"
#include "urlhash.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "sockets.h"
#include "version.h"


/*+ The time that the program went online. +*/
extern time_t OnlineTime;

/*+ The time that the program went offline. +*/
extern time_t OfflineTime;

/*+ The server sockets that we listen on +*/
extern int socks_fd[numsocktype][NUMIPPROT];

/*+ The online / offline / autodial status. +*/
extern int online;

/*+ The current number of active servers +*/
extern int n_servers,           /*+ in total. +*/
           n_fetch_servers;     /*+ fetching a previously requested page. +*/

/*+ The maximum number of servers +*/
extern int max_servers,                /*+ in total. +*/
           max_fetch_servers;          /*+ fetching a page. +*/

/*+ The wwwoffle client file descriptor when fetching. +*/
extern int fetch_fd;

/*+ The pids of the servers. +*/
extern int server_pids[MAX_SERVERS];

/*+ The pids of the servers that are fetching. +*/
extern int fetch_pids[MAX_FETCH_SERVERS];

/*+ The purge status. +*/
extern int purging;

/*+ The pid of the purge process. +*/
extern int purge_pid;

/*+ The current status, fetching or not. +*/
extern int fetching;

/*+ True if the -f option was passed on the command line. +*/
extern int nofork;


/*++++++++++++++++++++++++++++++++++++++
  Parse a request that comes from wwwoffle.

  int client The file descriptor that corresponds to the wwwoffle connection.
  ++++++++++++++++++++++++++++++++++++++*/

void CommandConnect(int client)
{
 char *line=NULL;

 if(!(line=read_line(client,line)))
   {PrintMessage(Warning,"Nothing to read from the wwwoffle control socket [%!s]."); return;}

 if(strcmp_litbeg(line,"WWWOFFLE "))
   {
    PrintMessage(Warning,"WWWOFFLE Not a command."); /* Used in audit-usage.pl */
    goto cleanup_return;
   }

 if(ConfigString(PassWord) || !strcmp_litbeg(&line[9],"PASSWORD "))
   {
    char *password=&line[18];

    chomp_str(password);

    if(!ConfigString(PassWord) || strcmp(password,ConfigString(PassWord)))
      {
       write_string(client,"WWWOFFLE Incorrect Password\n"); /* Used in wwwoffle.c */
       PrintMessage(Warning,"WWWOFFLE Incorrect Password."); /* Used in audit-usage.pl */
       goto cleanup_return;
      }

    if(!(line=read_line(client,line)))
      {PrintMessage(Warning,"Unexpected end of wwwoffle control command [%!s]."); return;}

    if(strcmp_litbeg(line,"WWWOFFLE "))
      {
       PrintMessage(Warning,"WWWOFFLE Not a command."); /* Used in audit-usage.pl */
       goto cleanup_return;
      }
   }

 if(!strcmp_litbeg(&line[9],"ONLINE"))
   {
    if(online==1)
       write_string(client,"WWWOFFLE Already Online\n"); /* Used in wwwoffle.c */
    else
      {
       write_string(client,"WWWOFFLE Now Online\n");
       PrintMessage(Important,"WWWOFFLE Online."); /* Used in audit-usage.pl */

       CycleLastTimeSpoolFile();

       ForkRunModeScript(ConfigString(RunOnline),"online",NULL,client);
      }

    OnlineTime=time(NULL);
    online=1;
    SetCurrentOnlineStatus(online);

    if(fetch_fd!=-1)
       fetching=1;
   }
 else if(!strcmp_litbeg(&line[9],"AUTODIAL"))
   {
    if(online==-1)
       write_string(client,"WWWOFFLE Already in Autodial Mode\n"); /* Used in wwwoffle.c */
    else
      {
       write_string(client,"WWWOFFLE Now In Autodial Mode\n");
       PrintMessage(Important,"WWWOFFLE In Autodial Mode."); /* Used in audit-usage.pl */

       ForkRunModeScript(ConfigString(RunAutodial),"autodial",NULL,client);
      }

    OnlineTime=time(NULL);
    online=-1;
    SetCurrentOnlineStatus(online);
   }
 else if(!strcmp_litbeg(&line[9],"OFFLINE"))
   {
    if(!online)
       write_string(client,"WWWOFFLE Already Offline\n"); /* Used in wwwoffle.c */
    else
      {
       write_string(client,"WWWOFFLE Now Offline\n");
       PrintMessage(Important,"WWWOFFLE Offline."); /* Used in audit-usage.pl */

       ForkRunModeScript(ConfigString(RunOffline),"offline",NULL,client);
      }

    OfflineTime=time(NULL);
    online=0;
    SetCurrentOnlineStatus(online);
   }
 else if(!strcmp_litbeg(&line[9],"FETCH"))
   {
    if(fetch_fd!=-1)
      {
       fetching=1;
       write_string(client,"WWWOFFLE Already Fetching.\n"); /* Used in wwwoffle.c */
      }
    else if(online==0)
       write_string(client,"WWWOFFLE Must be online or autodial to fetch.\n"); /* Used in wwwoffle.c */
    else
      {
       write_string(client,"WWWOFFLE Now Fetching.\n");
       PrintMessage(Important,"WWWOFFLE Fetch."); /* Used in audit-usage.pl */

       RequestMonitoredPages();

       CycleLastOutSpoolFile();

       fetch_fd=client;
       fetching=1;

       ForkRunModeScript(ConfigString(RunFetch),"fetch","start",fetch_fd);
      }
   }
 else if(!strcmp_litbeg(&line[9],"CONFIG"))
   {
    write_string(client,"WWWOFFLE Reading Configuration File.\n");
    PrintMessage(Important,"WWWOFFLE Re-reading Configuration File."); /* Used in audit-usage.pl */

    if(ReadConfigurationFile(client))
      {
       PrintMessage(Warning,"Error in configuration file; keeping old values.");
       write_string(client,"WWWOFFLE Error Reading Configuration File.\n");
      }
    else
       write_string(client,"WWWOFFLE Read Configuration File.\n");

    PrintMessage(Important,"WWWOFFLE Finished Re-reading Configuration File.");
   }
 else if(!strcmp_litbeg(&line[9],"DUMP"))
   {
    PrintMessage(Important,"WWWOFFLE Dumping Configuration File."); /* Used in audit-usage.pl */

    DumpConfigFile(client);

    PrintMessage(Important,"WWWOFFLE Finished Dumping Configuration File.");
   }
 else if(!strcmp_litbeg(&line[9],"PURGE"))
   {
    pid_t pid;

    if(nofork)
      {
       write_string(client,"WWWOFFLE Purge Starting.\n");
       PrintMessage(Important,"WWWOFFLE Purge."); /* Used in audit-usage.pl */

       if(PurgeCache(client)) {
	 PrintMessage(Important,"WWWOFFLE Purge finished.");

	 /* We haven't forked any server child processes, so it should be safe to compact the url hash table. */
	 if(urlhash_copycompact()) {
	   PrintMessage(Important,"Copy compacting of url hash table succeeded.");
	   write_string(client,"WWWOFFLE Purge Finished.\n");
	 }
	 else {
	   PrintMessage(Warning,"Copy compacting of url hash table failed.");
	   write_string(client,"WWWOFFLE Purge Finished, but compaction of url hash table failed.\n");
	 }
       }
       else {
	 PrintMessage(Important,"WWWOFFLE Purge prematurely aborted.");
	 write_string(client,"WWWOFFLE Purge prematurely aborted.\n");
       }
      }
    else if(purging)
      {write_string(client,"WWWOFFLE Already Purging.\n");}
    else if((pid=fork())==-1)
       PrintMessage(Warning,"Cannot fork to do a purge [%!s].");
    else if(pid)
      {
       purging=1;
       purge_pid=pid;
      }
    else
      {
       int purge_ok,i,*fdp;

       InitErrorHandler(NULL,-1,-1);  /* Change nothing except pid */

       if(fetch_fd!=-1)
         {
          finish_io(fetch_fd);
          CloseSocket(fetch_fd);
         }

       /* These six sockets don't need finish_io() calling because they never
          had init_io() called, they are just bound to a port listening. */

       for(i=0,fdp=socks_fd[0]; i<numsocktype*NUMIPPROT; ++i) {
	 int fd= *fdp++;
	 if(fd!=-1) CloseSocket(fd);
       }

       write_string(client,"WWWOFFLE Purge Starting.\n");
       PrintMessage(Important,"WWWOFFLE Purge."); /* Used in audit-usage.pl */

       purge_ok=PurgeCache(client);

       if(purge_ok) {
	 PrintMessage(Important,"WWWOFFLE Purge finished.");
	 write_string(client,"WWWOFFLE Purge Finished.\n");
       }
       else {
	 PrintMessage(Important,"WWWOFFLE Purge prematurely aborted.");
	 write_string(client,"WWWOFFLE Purge prematurely aborted.\n");
       }
       finish_io(client);
       CloseSocket(client);

       exit(!purge_ok);
      }
   }
 else if(!strcmp_litbeg(&line[9],"STATUS"))
   {
    int i;

    PrintMessage(Important,"WWWOFFLE Status."); /* Used in audit-usage.pl */

    write_string(client,"WWWOFFLE Server Status\n");
    write_string(client,"----------------------\n");

    write_formatted(client,"Version      : %s\n",WWWOFFLE_VERSION);

    write_string(client,(online==1)?  "State        : online\n":
		        (online==-1)? "State        : autodial\n":
		                      "State        : offline\n");

    write_string(client,(fetch_fd!=-1)? "Fetch        : active (wwwoffle -fetch)\n":
		        (fetching==1)?  "Fetch        : active (recursive request)\n":
		                        "Fetch        : inactive\n");

    write_string(client,purging?"Purge        : active\n":"Purge        : inactive\n");

    write_formatted(client,"Last-Online  : %s\n",OnlineTime?RFC822Date(OnlineTime,0):"unknown");

    write_formatted(client,"Last-Offline : %s\n",OfflineTime?RFC822Date(OfflineTime,0):"unknown");

    write_formatted(client,"Total-Servers: %d\n",n_servers);
    write_formatted(client,"Fetch-Servers: %d\n",n_fetch_servers);

    if(n_servers)
      {
       write_string(client,"Server-PIDs  : ");
       for(i=0;i<max_servers;i++)
          if(server_pids[i])
             write_formatted(client," %d",server_pids[i]);
       write_string(client,"\n");
      }
    if(n_fetch_servers)
      {
       write_string(client,"Fetch-PIDs   : ");
       for(i=0;i<max_fetch_servers;i++)
          if(fetch_pids[i])
             write_formatted(client," %d",fetch_pids[i]);
       write_string(client,"\n");
      }
    if(purging)
       write_formatted(client,"Purge-PID    : %d\n",purge_pid);
   }
 else if(!strcmp_litbeg(&line[9],"KILL"))
   {
    write_string(client,"WWWOFFLE Kill Signalled.\n");
    PrintMessage(Important,"WWWOFFLE Kill."); /* Used in audit-usage.pl */

    raise(SIGINT);
   }
 else
   {
    chomp_str(line);

    write_formatted(client,"WWWOFFLE Unknown Command '%s'.",line);
    PrintMessage(Warning,"WWWOFFLE Unknown control command '%s'.",line); /* Used in audit-usage.pl */
   }

cleanup_return:
 if(line)
    free(line);
}


/*++++++++++++++++++++++++++++++++++++++
  Run the associated program when changing mode.

  char *filename The name of the program to run or NULL if none.

  char *mode The new mode to use as the argument.

  char *arg An additional argument (used for fetch mode).

  int client The current client socket to be closed.
  ++++++++++++++++++++++++++++++++++++++*/

void ForkRunModeScript(char *filename,char *mode,char *arg,int client)
{
 pid_t pid;

 if(!filename)
    return;

 if((pid=fork())==-1)
   {PrintMessage(Warning,"Cannot fork to run the run-%s program [%!s].",mode);return;}
 else if(!pid) /* The child */
   {
    int i,*fdp;

    if(fetch_fd!=-1)
      {
       finish_io(fetch_fd);
       CloseSocket(fetch_fd);
      }

    /* These six sockets don't need finish_io() calling because they never
       had init_io() called, they are just bound to a port listening. */

    for(i=0,fdp=socks_fd[0]; i<numsocktype*NUMIPPROT; ++i) {
      int fd= *fdp++;
      if(fd!=-1) CloseSocket(fd);
    }

    if(client!=fetch_fd)
      {
       finish_io(client);
       CloseSocket(client);
      }

    if(arg)
       execl(filename,filename,mode,arg,NULL);
    else
       execl(filename,filename,mode,NULL);

    PrintMessage(Warning,"Cannot exec the run-%s program '%s' [%!s].",mode,filename);
    exit(1);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Fork a wwwoffles server.

  int fd The file descriptor that the data is transfered on.
  ++++++++++++++++++++++++++++++++++++++*/

void ForkServer(int fd)
{
 pid_t pid;
 int i;
 int fetcher=0;

 if(fetching && fetch_fd==fd)
    fetcher=1;

 if(nofork)
    wwwoffles(online,fetcher,fd);
 else if((pid=fork())==-1)
    PrintMessage(Warning,"Cannot fork a server [%!s].");
 else if(pid) /* The parent */
   {
    for(i=0;i<max_servers;i++)
       if(server_pids[i]==0)
         {server_pids[i]=pid;break;}

    n_servers++;

    if(online!=0 && fetcher)
      {
       for(i=0;i<max_fetch_servers;i++)
          if(fetch_pids[i]==0)
            {fetch_pids[i]=pid;break;}

       n_fetch_servers++;
      }

    /* Used in audit-usage.pl */
    PrintMessage(Inform,"Forked wwwoffles -%s (pid=%d).",online==1?(fetcher?"fetch":"real"):(online==-1?"autodial":"spool"),pid);
   }
 else /* The child */
   {
    int status,i,*fdp;

    if(fetch_fd!=-1 && fetch_fd!=fd)
      {
       finish_io(fetch_fd);
       CloseSocket(fetch_fd);
      }

    /* These six sockets don't need finish_io() calling because they never
       had init_io() called, they are just bound to a port listening. */

    for(i=0,fdp=socks_fd[0]; i<numsocktype*NUMIPPROT; ++i) {
      int fd= *fdp++;
      if(fd!=-1) CloseSocket(fd);
    }

    status=wwwoffles(online,fetcher,fd);

    exit(status);
   }
}
