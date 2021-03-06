/***************************************
  Make a url hash file from the conventional U* files in the WWWOFFLE cache.

  Written by Paul A. Rombouts, based on convert-cache.c by Andrew M. Bishop.

  This file Copyright 2005 Paul A. Rombouts.
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
#include <grp.h>

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

#if HAVE_DIRENT_H
# include <dirent.h>
#else
# define dirent direct
# if HAVE_SYS_NDIR_H
#  include <sys/ndir.h>
# endif
# if HAVE_SYS_DIR_H
#  include <sys/dir.h>
# endif
# if HAVE_NDIR_H
#  include <ndir.h>
# endif
#endif

#include <sys/stat.h>
#include <fcntl.h>

#include "wwwoffle.h"
#include "urlhash.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "proto.h"
#include "version.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif


static void ConvertProto(char *proto);
static void ConvertHost(char *proto,char *host);
static void ConvertDir(char *dirname);
static char *readurlfile(char *file,size_t len);

static void print_usage_statement(FILE *f)
{
  fprintf(f,"Usage: make_urlhash_file  [-c <config-file>] [-d <full-path-to-spool-dir>] [<dir>]\n\n"
	  "Note: The optional <dir> arguments are, unless they begin with a \"/\",\n"
	  "      interpreted relative to the spool directory.\n"
	  "      These directories are scanned for U* files non-recursively.\n"
	  "      If no <dir> arguments are specified all the U* files in the cache are\n"
	  "      added to the hash table.\n");
}

static int exitval=0;

/*++++++++++++++++++++++++++++++++++++++
  The main program
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char** argv)
{
 int i;
 struct stat buf;
 char *config_file=NULL,*spool_dir=NULL,**rdirs=NULL;
 int nrdirs=0;

 /* Parse the options */

 while(argc>1 && argv[1][0]=='-') {
   if(!strcmp(argv[1],"--help"))
     {print_usage_statement(stdout);exit(0);}
   else if(!strcmp(argv[1],"--version"))
     {fprintf(stdout,"WWWOFFLE make_urlhash_file version %s\n",WWWOFFLE_VERSION);exit(0);}
   else if(!strcmp(argv[1],"-c")) {
     if(argc>2) {
       config_file=argv[2];
       argv += 2;
       argc -= 2;
     }
     else
       {fprintf(stderr,"make_urlhash_file: The '-c' argument requires a configuration file name.\n"); exit(1);}
   }
   else if(!strcmp(argv[1],"-d")) {
     if(argc>2 && argv[2][0]=='/') {
       spool_dir=argv[2];
       argv += 2;
       argc -= 2;
     }
     else
       {fprintf(stderr,"make_urlhash_file: The '-d' argument requires the full path to the spool directory.\n"); exit(1);}
   }
   else {
     fprintf(stderr,"unknown option '%s'\n",argv[1]);
     print_usage_statement(stderr);
     exit(1);
   }
 }
 if(argc>1) {
   rdirs=argv+1;
   nrdirs=argc-1;
#if 0
   for(i=0;i<nrdirs;++i)
     if (rdirs[i][0]=='/')
       {fprintf(stderr,"argument '%s' must be relative to the spool directory.\n",rdirs[i]);
        print_usage_statement(stderr);exit(1);}
#endif
 }

 if(!config_file)
   {
    char *env=getenv("WWWOFFLE_PROXY");

    if(env && *env=='/')
       config_file=env;
   }

 /* Initialise */

 InitErrorHandler("make_urlhash_file",0,1);

 InitConfigurationFile(config_file);

 init_io(STDERR_FILENO);

 if(ReadConfigurationFile(STDERR_FILENO))
   PrintMessage(Fatal,"Error in configuration file '%s'.",config_file);

 finish_io(STDERR_FILENO);


 /* Change the user and group. */
 {
   uid_t uid=ConfigInteger(WWWOFFLE_Uid);
   gid_t gid=ConfigInteger(WWWOFFLE_Gid);

   /* gain superuser privileges if possible */
   if(geteuid()!=0 && uid!=-1) seteuid(0);

   if(gid != -1)
     {
#if HAVE_SETGROUPS
       if(geteuid()==0 || getuid()==0)
	 if(setgroups(0, NULL)<0)
	   PrintMessage(Fatal,"Cannot clear supplementary group list [%!s].");
#endif

#if HAVE_SETRESGID
       if(setresgid(gid,gid,gid)<0)
	 PrintMessage(Fatal,"Cannot set real/effective/saved group id to %d [%!s].",gid);
#else
       if(geteuid()==0)
	 {
	   if(setgid(gid)<0)
	     PrintMessage(Fatal,"Cannot set group id to %d [%!s].",gid);
	 }
       else
	 {
#if HAVE_SETREGID
	   if(setregid(getegid(),gid)<0)
	     PrintMessage(Fatal,"Cannot set effective group id to %d [%!s].",gid);
	   if(setregid(gid,-1)<0)
	     PrintMessage(Fatal,"Cannot set real group id to %d [%!s].",gid);
#else
	   PrintMessage(Fatal,"Must be root to totally change group id.");
#endif
	 }
#endif
     }

   if(uid!=-1)
     {
#if HAVE_SETRESUID
       if(setresuid(uid,uid,uid)<0)
	 PrintMessage(Fatal,"Cannot set real/effective/saved user id to %d [%!s].",uid);
#else
       if(geteuid()==0)
	 {
	   if(setuid(uid)<0)
	     PrintMessage(Fatal,"Cannot set user id to %d [%!s].",uid);
	 }
       else
	 {
#if HAVE_SETREUID
	   if(setreuid(geteuid(),uid)<0)
	     PrintMessage(Fatal,"Cannot set effective user id to %d [%!s].",uid);
	   if(setreuid(uid,-1)<0)
	     PrintMessage(Fatal,"Cannot set real user id to %d [%!s].",uid);
#else
	   PrintMessage(Fatal,"Must be root to totally change user id.");
#endif
	 }
#endif
     }

   if(uid!=-1 || gid!=-1)
     PrintMessage(Inform,"Running with uid=%d, gid=%d.",geteuid(),getegid());
 }


 umask(0);

 /* Change to the spool directory. */

 if(!spool_dir) spool_dir=ConfigString(SpoolDir);

 if(ChangeToSpoolDir(spool_dir))
    PrintMessage(Fatal,"Cannot change to the spool directory '%s' [%!s]; conversion failed.",spool_dir);

 /* Open url hash table */

 if(!urlhash_open(urlhash_filename))
   PrintMessage(Fatal,"Opening url hash file '%s' failed.\n",urlhash_filename);
   

 printf("WWWOFFLE Spool directory is '%s'\n",spool_dir);

 /* Process the directories containing U* files. */

 if(rdirs) {
   for(i=0;i<nrdirs;++i) {
     char *rdir=rdirs[i];
     if(stat(rdir,&buf)) {
       PrintMessage(Warning,"Cannot stat the '%s' directory [%!s]; not converted.",rdir);
       exitval=1;
     }
     else
       {
	 printf("Converting %s\n",rdir);

	 if(chdir(rdir)) {
	   PrintMessage(Warning,"Cannot change to the '%s' directory [%!s]; conversion failed.",rdir);
	   exitval=1;
	 }
	 else
	   {
	     ConvertDir(rdir);

	     ChangeBackToSpoolDir();
	   }
       }
   }
 }
 else {
   for(i=0;i<NProtocols;i++)
     {
       char *proto=Protocols[i].name;

       if(stat(proto,&buf))
	 PrintMessage(Inform,"Cannot stat the '%s' directory [%!s]; not converted.",proto);
       else
	 {
	   printf("Converting %s\n",proto);

	   if(chdir(proto)) {
	     PrintMessage(Warning,"Cannot change to the '%s' directory [%!s]; conversion failed.",proto);
	     exitval=1;
	   }
	   else
	     {
	       ConvertProto(proto);

	       ChangeBackToSpoolDir();
	     }
	 }
     }

#define nspecial 6
   for(i=0;i<nspecial;i++)
     {
       static char *special[nspecial]={"monitor","outgoing","lastout","prevout","lasttime","prevtime"};
       char *dirname=special[i];
       int j,n=0;

       if(!strcmp_litbeg(special[i],"prev")) n=NUM_PREVTIME_DIR;

       for(j=1; j<=(n?n:1); ++j) {
	 char dbuf[sizeof("prevtime")+MAX_INT_STR];

	 if(n) {
	   sprintf(dbuf,"%s%u",special[i],(unsigned)j);
	   dirname=dbuf;
	 }

	 if(stat(dirname,&buf))
	   PrintMessage(Inform,"Cannot stat the '%s' directory [%!s]; not converted.",dirname);
	 else
	   {
	     printf("Converting %s\n",dirname);

	     if(chdir(dirname)) {
	       PrintMessage(Warning,"Cannot change to the '%s' directory [%!s]; conversion failed.",dirname);
	       exitval=1;
	     }
	     else
	       {
		 ConvertDir(dirname);

		 ChangeBackToSpoolDir();
	       }
	   }
       }
     }
 }
 /* urlhash_close will ensure url hash table is written to disk. */

 urlhash_close();

 printf("Done.\n");

 return(exitval);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a complete protocol directory.

  char *proto The protocol of the spool directory we are in.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConvertProto(char *proto)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;

 /* Open the spool directory. */

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open spool directory '%s' [%!s]; conversion failed.",proto);exitval=1;return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read spool directory '%s' [%!s]; conversion failed.",proto);exitval=1;goto close_return;}

 /* Go through each entry. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(stat(ent->d_name,&buf)) {
       PrintMessage(Warning,"Cannot stat file '%s/%s' [%!s] not converted.",proto,ent->d_name);
       exitval=1;
    }
    else if(S_ISDIR(buf.st_mode))
      {
       if(chdir(ent->d_name)) {
         PrintMessage(Warning,"Cannot change to the '%s/%s' directory [%!s]; conversion failed.",proto,ent->d_name);
	 exitval=1;
	}
       else
         {
          printf("  Converting %s\n",ent->d_name);

          ConvertHost(proto,ent->d_name);

          if(fchdir(dirfd(dir))) {
	    PrintMessage(Warning,"Cannot change back to protocol directory '%s' [%!s].",proto);
	    exitval=1;
	    break;
	  }
         }
      }
   }
 while((ent=readdir(dir)));

close_return:
 closedir(dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a complete host directory.

  char *proto The protocol of the spool directory we are in.

  char *host The hostname of the spool directory we are in.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConvertHost(char *proto,char *host)
{
  char dir[strlen(proto)+strlen(host)+sizeof("/")];

  sprintf(dir,"%s/%s",proto,host);
  ConvertDir(dir);
}

/*++++++++++++++++++++++++++++++++++++++
  Convert the current directory.

  char *dirname The name of the directory we are in..
  ++++++++++++++++++++++++++++++++++++++*/

static void ConvertDir(char *dirname)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;

 /* Open the spool directory. */

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open spool directory '%s' [%!s]; conversion failed.",dirname);exitval=1;return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read spool directory '%s' [%!s]; conversion failed.",dirname);exitval=1;goto close_return;}

 /* Go through each entry. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(!stat(ent->d_name,&buf) && S_ISREG(buf.st_mode) && *ent->d_name=='U')
      {
       int tpos;
       md5hash_t *md5hash;
       size_t hlen;
       unsigned char hbuf[sizeof(md5hash_t)+1];

       tpos=strlen(ent->d_name)-1;
       if(tpos>=0 && ent->d_name[tpos]=='~')
	 ent->d_name[tpos]=0;
       else
	 tpos=-1;

       md5hash=(md5hash_t *)Base64Decode((unsigned char *)(ent->d_name+1),&hlen,hbuf,sizeof(hbuf));

       if(tpos>=0) ent->d_name[tpos]='~';

       if(md5hash && hlen==sizeof(md5hash_t)) {
	 char *url=readurlfile(ent->d_name,buf.st_size);
	 if(url)
	   {
	     urlhash_add(url,md5hash);
	     free(url);
	   }
	 else {
	   PrintMessage(Warning,"Cannot read file '%s/%s' [%!s].",dirname,ent->d_name);
	   exitval=1;
	 }
       }
       else
	 PrintMessage(Warning,"The file name '%s/%s' does not respresent a valid hash value.",dirname,ent->d_name);
      }
   }
 while((ent=readdir(dir)));

close_return:
 closedir(dir);
}


static char *readurlfile(char *file,size_t len)
{
 char *path=NULL;
 int ufd;

 ufd=open(file,O_RDONLY|O_BINARY);

 if(ufd==-1)
   return(NULL);

 path=malloc(len+1);

 if(path) {
   if(read_all(ufd,path,len)==len)
     path[len]=0;
   else {
     free(path); path=NULL;
   }
 }

 close(ufd);

 return(path);
}
