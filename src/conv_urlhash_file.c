/***************************************
  Convert a url hash file using a different number of hash buckets or a
  different hash index function to a new one.

  Written by Paul A. Rombouts.

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
#include <stddef.h>             /* for offsetof*/

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


static void print_usage_statement(FILE *f)
{
  fprintf(f,"Usage: conv_urlhash_file  [-c <config-file>] [-d <spool-dir>] <old-url-hash-file>\n");
}

static int exitval=0;
static unsigned oldpos,oldsize;
static int ibuf,nbuf;
unsigned char *hnodep=NULL;
unsigned hnallocsize=0;
static unsigned char readbuf[4*1024];

static int init_hfile(int fd);
static urlhash_node *get_hn(int fd);

/*++++++++++++++++++++++++++++++++++++++
  The main program
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char** argv)
{
 char *config_file=NULL,*spool_dir=NULL,*oldfilename=NULL;
 int ofd;
 urlhash_node *hp;

 /* Parse the options */

 while(argc>1 && argv[1][0]=='-') {
   if(!strcmp(argv[1],"--help"))
     {print_usage_statement(stdout);exit(0);}
   else if(!strcmp(argv[1],"--version"))
     {fprintf(stdout,"WWWOFFLE conv_urlhash_file version %s\n",WWWOFFLE_VERSION);exit(0);}
   else if(!strcmp(argv[1],"-c")) {
     if(argc>2) {
       config_file=argv[2];
       argv += 2;
       argc -= 2;
     }
     else
       {fprintf(stderr,"conv_urlhash_file: The '-c' argument requires a configuration file name.\n"); exit(1);}
   }
   else if(!strcmp(argv[1],"-d")) {
     if(argc>2) {
       spool_dir=argv[2];
       argv += 2;
       argc -= 2;
     }
     else
       {fprintf(stderr,"conv_urlhash_file: The '-d' argument requires a spool directory name.\n"); exit(1);}
   }
   else {
     fprintf(stderr,"unknown option '%s'\n",argv[1]);
     print_usage_statement(stderr);
     exit(1);
   }
 }
 if(argc>1) {
   oldfilename=argv[1];
   if(argc>2)
     {fprintf(stderr,"too many arguments.\n");print_usage_statement(stderr);exit(1);}
 }
 else
   {fprintf(stderr,"old url hash file name missing.\n");print_usage_statement(stderr);exit(1);}

 if(!config_file)
   {
    char *env=getenv("WWWOFFLE_PROXY");

    if(env && *env=='/')
       config_file=env;
   }

 /* Initialise */

 InitErrorHandler("conv_urlhash_file",0,1);

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
	   PrintMessage(Warning,"Cannot clear supplementary group list [%!s].");
#endif

#if HAVE_SETRESGID
       if(setresgid(gid,gid,gid)<0)
	 PrintMessage(Warning,"Cannot set real/effective/saved group id to %d [%!s].",gid);
#else
       if(geteuid()==0)
	 {
	   if(setgid(gid)<0)
	     PrintMessage(Warning,"Cannot set group id to %d [%!s].",gid);
	 }
       else
	 {
#if HAVE_SETREGID
	   if(setregid(getegid(),gid)<0)
	     PrintMessage(Warning,"Cannot set effective group id to %d [%!s].",gid);
	   if(setregid(gid,-1)<0)
	     PrintMessage(Warning,"Cannot set real group id to %d [%!s].",gid);
#else
	   PrintMessage(Warning,"Must be root to totally change group id.");
#endif
	 }
#endif
     }

   if(uid!=-1)
     {
#if HAVE_SETRESUID
       if(setresuid(uid,uid,uid)<0)
	 PrintMessage(Warning,"Cannot set real/effective/saved user id to %d [%!s].",uid);
#else
       if(geteuid()==0)
	 {
	   if(setuid(uid)<0)
	     PrintMessage(Warning,"Cannot set user id to %d [%!s].",uid);
	 }
       else
	 {
#if HAVE_SETREUID
	   if(setreuid(geteuid(),uid)<0)
	     PrintMessage(Warning,"Cannot set effective user id to %d [%!s].",uid);
	   if(setreuid(uid,-1)<0)
	     PrintMessage(Warning,"Cannot set real user id to %d [%!s].",uid);
#else
	   PrintMessage(Warning,"Must be root to totally change user id.");
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
    PrintMessage(Fatal,"Cannot change to the spool directory '%s' [%!s].",spool_dir);

 /* Open url hash table */

 if(!urlhash_open(urlhash_filename))
   PrintMessage(Fatal,"Opening url hash file '%s' failed.\n",urlhash_filename);
   

 ofd=open(oldfilename,O_RDONLY|O_BINARY);
 if(ofd == -1)
   PrintMessage(Fatal,"Opening old url hash file '%s' failed [%!s].\n",oldfilename);
 
 if(!init_hfile(ofd))
    PrintMessage(Fatal,"Cannot read old url hash file [%!s].");


 while((hp=get_hn(ofd))) {
   urlhash_add(hp->url,&hp->h);
 }

 if(close(ofd))
   PrintMessage(Warning,"closing old url hash file '%s' failed [%!s].\n",oldfilename);

 urlhash_close();


 return(exitval);
}


static int init_hfile(int fd)
{
  unsigned u;

  if(read(fd,&u, sizeof(u))!=sizeof(u))
     return 0;

  oldsize=u;

  if(read(fd,&u, sizeof(u))!=sizeof(u))
     return 0;

  oldpos=sizeof(urlhash_info)+u*sizeof(unsigned);

  if(lseek(fd,oldpos,SEEK_SET)==(off_t)-1)
    return 0;

  ibuf=nbuf=0;
  return 1;
}


static urlhash_node *get_hn(int fd)
{
  int n,nsize;

  if(oldpos>=oldsize) return NULL;

  for(n=0; ; ++n) {
    if(ibuf>=nbuf) {
      ibuf=0;
      nbuf=read(fd,readbuf,sizeof(readbuf));
      if(nbuf==-1) {
	PrintMessage(Warning,"Cannot read from the old url hash file [%!s].");
	return NULL;
      }
      if(!nbuf) {
	if(n)
	  PrintMessage(Warning,"Premature EOF on old url hash file; incomplete urlhash_node.");
	return NULL;
      }
    }
    if(n>=hnallocsize) {
      hnallocsize = n + 256;
      hnodep=realloc(hnodep,hnallocsize);
    }
    hnodep[n] = readbuf[ibuf++];

    if(n>=offsetof(urlhash_node,url) && !hnodep[n])
      break;
  }

  ++n;
  nsize = urlhash_align(n);
  oldpos += nsize;
  ibuf += (nsize - n);

  if(ibuf>nbuf) {
    if(lseek(fd,oldpos,SEEK_SET)==(off_t)-1) {
      PrintMessage(Warning,"Cannot lseek the old url hash file to position %u [%!s].",oldpos);
      return NULL;
    }
    ibuf=nbuf=0;
  }

  return (urlhash_node *)hnodep;
}
