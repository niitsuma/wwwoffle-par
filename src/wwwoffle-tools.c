/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/wwwoffle-tools.c 1.47 2005/01/26 18:57:30 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8e.
  Tools for use in the cache for version 2.x.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <unistd.h>
#if HAVE_SETRESUID
int setresuid(uid_t ruid, uid_t euid, uid_t suid);
#endif
#if HAVE_SETRESGID
int setresgid(gid_t rgid, gid_t egid, gid_t sgid);
#endif

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

#include <utime.h>
#include <sys/stat.h>

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

#include <fcntl.h>
#include <grp.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "version.h"
#include "config.h"


#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

#define LS         1
#define LS_SPECIAL 2
#define MV         3
#define RM         4
#define READ       5
#define WRITE      6
#define HASH       7

static int wwwoffle_ls(URL *Url);
static int wwwoffle_ls_special(char *name);
static int wwwoffle_mv(URL *Url1,URL *Url2);
static int wwwoffle_rm(URL *Url);
static int wwwoffle_read(URL *Url);
static int wwwoffle_write(URL *Url);
static int wwwoffle_hash(URL *Url);

static int ls(char *file);


/*++++++++++++++++++++++++++++++++++++++
  The main program
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char **argv)
{
 struct stat buf;
 URL **Url=NULL;
 int mode=0,exitval=0;
 int i;
 char *argv0,*config_file=NULL;

 /* Check which program we are running */

 argv0=argv[0]+strlen(argv[0])-1;
 while(argv0>=argv[0] && *argv0!='/')
    argv0--;

 argv0++;

 if(!strcmp(argv0,"wwwoffle-ls"))
    mode=LS;
 else if(!strcmp(argv0,"wwwoffle-mv"))
    mode=MV;
 else if(!strcmp(argv0,"wwwoffle-rm"))
    mode=RM;
 else if(!strcmp(argv0,"wwwoffle-read"))
    mode=READ;
 else if(!strcmp(argv0,"wwwoffle-write"))
    mode=WRITE;
 else if(!strcmp(argv0,"wwwoffle-hash"))
    mode=HASH;
 else if(!strcmp(argv0,"wwwoffle-tools"))
   {
    if(argc>1 && !strcmp(argv[1],"-ls"))
      {mode=LS; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-mv"))
      {mode=MV; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-rm"))
      {mode=RM; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-read"))
      {mode=READ; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-write"))
      {mode=WRITE; argv++; argc--;}
    else if(argc>1 && !strcmp(argv[1],"-hash"))
      {mode=HASH; argv++; argc--;}
   }

 if(mode==0)
   {
    fprintf(stderr,"wwwoffle-tools version %s\n"
                   "To select the mode of operation choose:\n"
                   "        wwwoffle-ls    ( = wwwoffle-tools -ls )\n"
                   "        wwwoffle-mv    ( = wwwoffle-tools -mv )\n"
                   "        wwwoffle-rm    ( = wwwoffle-tools -rm )\n"
                   "        wwwoffle-read  ( = wwwoffle-tools -read )\n"
                   "        wwwoffle-write ( = wwwoffle-tools -write )\n"
                   "        wwwoffle-hash  ( = wwwoffle-tools -hash )\n",
                   WWWOFFLE_VERSION);
    exit(1);
   }

 if(mode==LS)
    for(i=1;i<argc;i++)
       if(!strcmp(argv[i],"outgoing") || !strcmp(argv[i],"monitor") ||
          !strcmp(argv[i],"lasttime") || (!strcmp_litbeg(argv[i],"prevtime") && isdigit(argv[i][strlitlen("prevtime")])) || 
          !strcmp(argv[i],"lastout")  || (!strcmp_litbeg(argv[i],"prevout")  && isdigit(argv[i][strlitlen("prevout")])))
          mode=LS_SPECIAL;

 /* Find the configuration file */

 for(i=1;i<argc;i++)
    if(config_file)
      {
       argv[i-2]=argv[i];
      }
    else if(!strcmp(argv[i],"-c"))
      {
       if(++i>=argc)
         {fprintf(stderr,"%s: The '-c' argument requires a configuration file name.\n",argv0); exit(1);}

       config_file=argv[i];
      }

 if(config_file)
    argc-=2;

 if(!config_file)
   {
    char *env=getenv("WWWOFFLE_PROXY");

    if(env && *env=='/')
       config_file=env;
   }

 /* Print the usage instructions */

 if(argc>1 && !strcmp(argv[1],"--version"))
   {fprintf(stderr,"%s version %s\n",argv0,WWWOFFLE_VERSION);exit(0);}

 else if((mode==LS && (argc<2 || (argc>1 && !strcmp(argv[1],"--help")))) ||
         (mode==LS_SPECIAL && (argc!=2 || (argc>1 && !strcmp(argv[1],"--help")))))
   {fprintf(stderr,"Usage: wwwoffle-ls [-c <config-file>]\n"
                   "                   ( <dir>/<subdir> | <protocol>://<host> | <URL> ) ...\n"
                   "       wwwoffle-ls [-c <config-file>]\n"
                   "                   ( outgoing | lastout | prevout[0-9] |\n"
                   "                     monitor | lasttime | prevtime[0-9] )\n");exit(0);}
 else if(mode==MV && (argc!=3 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-mv [-c <config-file>]\n"
                   "                   (<dir1>/<subdir1> | <protocol1>://<host1>)\n"
                   "                   (<dir2>/<subdir2> | <protocol2>://<host2>)\n");exit(0);}
 else if(mode==RM && (argc<2 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-rm [-c <config-file>] <URL> ...\n");exit(0);}
 else if(mode==READ && (argc!=2 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-read [-c <config-file>] <URL>\n");exit(0);}
 else if(mode==WRITE && (argc!=2 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-write [-c <config-file>] <URL>\n");exit(0);}
 else if(mode==HASH && (argc!=2 || (argc>1 && !strcmp(argv[1],"--help"))))
   {fprintf(stderr,"Usage: wwwoffle-hash [-c <config-file>] <URL>\n");exit(0);}

 /* Initialise */

 InitErrorHandler(argv0,0,1);

 InitConfigurationFile(config_file);

 if(config_file)
   {
    init_io(STDERR_FILENO);

    if(ReadConfigurationFile(STDERR_FILENO))
       PrintMessage(Fatal,"Error in configuration file '%s'.",config_file);

    finish_io(STDERR_FILENO);
   }

 umask(0);

 if(mode==HASH)
    ;
 else if((stat("outgoing",&buf) || !S_ISDIR(buf.st_mode)) ||
         (stat("http",&buf) || !S_ISDIR(buf.st_mode)))
   {
    if(ChangeToSpoolDir(ConfigString(SpoolDir)))
      {fprintf(stderr,"The %s program must be started from the spool directory\n"
                      "Cannot change to the '%s' directory.\n",argv0,ConfigString(SpoolDir));exit(1);}
    if((stat("outgoing",&buf) || !S_ISDIR(buf.st_mode)) ||
       (stat("http",&buf) || !S_ISDIR(buf.st_mode)))
      {fprintf(stderr,"The %s program must be started from the spool directory\n"
                      "There is no accessible 'outgoing' directory here so it can't be right.\n",argv0);exit(1);}
   }
 else
   {
    /* needs glibc to work */
    char *cwd=getcwd(NULL,0);

    if(!cwd)
      PrintMessage(Fatal,"Cannot get value of current working directory [%!s].");

    ChangeToSpoolDir(cwd);
    free(cwd);
   }

 /* Get the arguments */

 if(mode!=LS_SPECIAL)
   {
    Url=(URL**)malloc(argc*sizeof(URL*));

    for(i=1;i<argc;i++)
      {
       char *arg;
       {
	 char *spooldir=ConfigString(SpoolDir); int strlen_spooldir=strlen(spooldir);
	 if(!strncmp(argv[i],spooldir,strlen_spooldir) &&
	    argv[i][strlen_spooldir]=='/')
	   arg=argv[i]+strlen_spooldir+1;
	 else
	   arg=argv[i];
       }
       {
	 char *colon=strchr(arg,':');
	 char *slash=strchr(arg,'/');

	 if((colon && slash && colon<slash) ||
	    !slash)
	   {
	     Url[i]=SplitURL(arg);
	   }
	 else
	   {
	     char *url;

	     *slash=0;
	     url=(char*)malloc(strlen(slash+1)+strlen(arg)+8);
	     sprintf(url,"%s://%s",arg,slash+1);

	     Url[i]=SplitURL(url);
	     free(url);
	   }
       }
      }
   }

 if(mode==LS)
    for(i=1;i<argc;i++)
       exitval+=wwwoffle_ls(Url[i]);
 else if(mode==LS_SPECIAL)
    exitval=wwwoffle_ls_special(argv[1]);
 else if(mode==MV)
    exitval=wwwoffle_mv(Url[1],Url[2]);
 else if(mode==RM)
    for(i=1;i<argc;i++)
       exitval+=wwwoffle_rm(Url[i]);
 else if(mode==READ)
    exitval=wwwoffle_read(Url[1]);
 else if(mode==WRITE)
   {
    if(config_file)
      {
	/* Change the user and group. */

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

    exitval=wwwoffle_write(Url[1]);
   }
 else if(mode==HASH)
    exitval=wwwoffle_hash(Url[1]);

 exit(exitval);
}


/*++++++++++++++++++++++++++++++++++++++
  List the URLs within a directory of the cache.

  int wwwoffle_ls Return 1 in case of error or 0 if OK.

  URL *Url The URL to list.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_ls(URL *Url)
{
 int retval=0;

 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return(1);}

 if(chdir(Url->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,Url->dir);ChangeBackToSpoolDir();return(1);}

 if(strcmp(Url->path,"/"))
   {
    local_URLToFileName(Url,'D',name)

    retval+=ls(name);
   }
 else
   {
    struct dirent* ent;
    DIR *dir=opendir(".");

    if(!dir)
      {PrintMessage(Warning,"Cannot open current directory '%s/%s' [%!s].",Url->proto,Url->dir);ChangeBackToSpoolDir();return(1);}

    ent=readdir(dir);
    if(!ent)
      {PrintMessage(Warning,"Cannot read current directory '%s/%s' [%!s].",Url->proto,Url->dir);closedir(dir);ChangeBackToSpoolDir();return(1);}

    do
      {
       if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
          continue; /* skip . & .. */

       if(*ent->d_name=='D' && ent->d_name[strlen(ent->d_name)-1]!='~')
          retval+=ls(ent->d_name);
      }
    while((ent=readdir(dir)));

    closedir(dir);
   }

 ChangeBackToSpoolDir();

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  List the URLs within the outgoing, monitor or lasttime/prevtime special directory of the cache.

  int wwwoffle_ls_special Return 1 in case of error or 0 if OK.

  char *name The name of the directory to list.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_ls_special(char *name)
{
 struct dirent* ent;
 DIR *dir;
 int retval=0;

 if(chdir(name))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",name);return(1);}

 dir=opendir(".");

 if(!dir)
   {PrintMessage(Warning,"Cannot open current directory '%s' [%!s].",name);ChangeBackToSpoolDir();return(1);}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read current directory '%s' [%!s].",name);closedir(dir);ChangeBackToSpoolDir();return(1);}

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if((*ent->d_name=='D' || *ent->d_name=='O') && ent->d_name[strlen(ent->d_name)-1]!='~')
       retval+=ls(ent->d_name);
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  List one file.

  int ls Return 1 in case of error or 0 if OK.

  char *file The name of the file to ls.
  ++++++++++++++++++++++++++++++++++++++*/

static int ls(char *file)
{
 struct stat buf;
 time_t now=-1;

 if(now==-1)
    now=time(NULL);

 if(stat(file,&buf))
   {PrintMessage(Warning,"Cannot stat the file '%s' [%!s].",file);return(1);}
 else
   {
    char *url=FileNameToURL(file);

    if(url)
      {
       char month[4];
       struct tm *tim=localtime(&buf.st_mtime);

       strftime(month,4,"%b",tim);

       if(buf.st_mtime<(now-(180*24*3600)))
          printf("%s %7ld %3s %2d %5d %s\n",file,(long)buf.st_size,month,tim->tm_mday,tim->tm_year+1900,url);
       else
          printf("%s %7ld %3s %2d %2d:%02d %s\n",file,(long)buf.st_size,month,tim->tm_mday,tim->tm_hour,tim->tm_min,url);

       /* free(url); */
      }
   }

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Move one URL or host to another.

  int wwwoffle_mv Return 1 in case of error or 0 if OK.

  URL *Url1 The source URL.

  URL *Url2 The destination URL.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_mv(URL *Url1,URL *Url2)
{
 struct dirent* ent;
 DIR *dir;

 if(chdir(Url1->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url1->proto);return(1);}

 if(chdir(Url1->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url1->proto,Url1->dir);ChangeBackToSpoolDir();return(1);}

 dir=opendir(".");

 if(!dir)
   {PrintMessage(Warning,"Cannot open current directory '%s/%s' [%!s].",Url1->proto,Url1->dir);ChangeBackToSpoolDir();return(1);}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read current directory '%s/%s' [%!s].",Url1->proto,Url1->dir);closedir(dir);ChangeBackToSpoolDir();return(1);}

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(*ent->d_name=='D')
      {
       char *url1=FileNameToURL(ent->d_name);

       if(url1 && !strncmp(Url1->name,url1,strlen(Url1->name)))
         {
          char *url2;
          URL *Url;
          char *path2,*name1;
          int fd2;

          Url=SplitURL(url1);
          url2=(char*)malloc(strlen(Url2->proto)+strlen(Url2->host)+strlen(Url->pathp)+strlen(Url2->pathp)+8);
          sprintf(url2,"%s://%s%s%s",Url2->proto,Url2->host,Url2->path,Url->pathp+strlen(Url1->path));
          FreeURL(Url);

          name1=ent->d_name;

          Url=SplitURL(url2);
          {
	    local_URLToFileName(Url,'D',name2)

	    path2=(char*)malloc(strlen(Url2->proto)+strlen(Url2->dir)+strlen(name2)+16);

	    sprintf(path2,"../../%s",Url2->proto);
	    mkdir(path2,DEF_DIR_PERM);

	    sprintf(path2,"../../%s/%s",Url2->proto,Url2->dir);
	    mkdir(path2,DEF_DIR_PERM);

	    *name1=*name2='D';
	    sprintf(path2,"../../%s/%s/%s",Url2->proto,Url2->dir,name2);
	    rename(name1,path2);

	    *name1=*name2='U';
	    sprintf(path2,"../../%s/%s/%s",Url2->proto,Url2->dir,name2);
	    fd2=open(path2,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,DEF_FILE_PERM);
	    if(fd2!=-1)
	      {
		struct stat buf;
		struct utimbuf utbuf;

		init_io(fd2);
		write_string(fd2,Url->name);
		finish_io(fd2);
		close(fd2);

		if(!stat(name1,&buf))
		  {
		    utbuf.actime=time(NULL);
		    utbuf.modtime=buf.st_mtime;
		    utime(path2,&utbuf);
		  }

		unlink(name1);
	      }
	  }
          /* free(url1); */
          free(url2);
          free(path2);
          FreeURL(Url);
         }
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);

 ChangeBackToSpoolDir();

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a URL.

  int wwwoffle_rm Return 1 in case of error or 0 if OK.

  URL *Url The URL to delete.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_rm(URL *Url)
{
 char *error=DeleteWebpageSpoolFile(Url,0);
 free(error);
 return(error!=NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a URL and output on stdout.

  int wwwoffle_read Return 1 in case of error or 0 if OK.

  URL *Url The URL to read.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_read(URL *Url)
{
 char *line=NULL,buffer[READ_BUFFER_SIZE];
 int n,spool=OpenWebpageSpoolFile(1,Url);
#if USE_ZLIB
 int compression=0;
#endif

 if(spool==-1)
    return(1);

 init_io(1);
 init_io(spool);

 while((line=read_line(spool,line)))
   {
#if USE_ZLIB
    if(!strcmp_litbeg(line,"Pragma: wwwoffle-compressed"))
       compression=2;
    else
#endif
       write_string(1,line);

    if(*line=='\r' || *line=='\n')
       break;
   }

#if USE_ZLIB
 if(compression)
    configure_io_read(spool,-1,compression,0);
#endif

 while((n=read_data(spool,buffer,READ_BUFFER_SIZE))>0)
    write_data(1,buffer,n);

 finish_io(1);
 finish_io(spool);
 close(spool);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Write a URL from the input on stdin.

  int wwwoffle_write Return 1 in case of error or 0 if OK.

  URL *Url The URL to write.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_write(URL *Url)
{
 char buffer[READ_BUFFER_SIZE];
 int n,spool=OpenWebpageSpoolFile(0,Url);

 if(spool==-1)
    return(1);

 init_io(0);
 init_io(spool);

 while((n=read_data(0,buffer,READ_BUFFER_SIZE))>0)
    write_data(spool,buffer,n);

 finish_io(0);
 finish_io(spool);
 close(spool);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Print the hash pattern for an URL
  
  int wwwoffle_hash Return 1 in case of error or 0 if OK.

  URL *Url The URL to be hashed.
  ++++++++++++++++++++++++++++++++++++++*/

static int wwwoffle_hash(URL *Url)
{
 local_URLToFileName(Url,'D',name)

 printf("%s\n",name+1);

 return(0);
}
