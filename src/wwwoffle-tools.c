/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/wwwoffle-tools.c 1.37 2002/07/19 16:30:01 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7d.
  Tools for use in the cache for version 2.x.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02 Andrew M. Bishop
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
#include <errno.h>
#include <grp.h>

#include "wwwoffle.h"
#include "version.h"
#include "misc.h"
#include "errors.h"
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

static void wwwoffle_ls(URL *Url);
static void wwwoffle_ls_special(char *name);
static void wwwoffle_mv(URL *Url1,URL *Url2);
static void wwwoffle_rm(URL *Url);
static void wwwoffle_read(URL *Url);
static void wwwoffle_write(URL *Url);
static void wwwoffle_hash(URL *Url);

static void ls(char *file);

/*+ A file descriptor for the spool directory. +*/
int fSpoolDir=-1;


/*++++++++++++++++++++++++++++++++++++++
  The main program
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char **argv)
{
 char cwd[PATH_MAX+1];
 struct stat buf;
 URL **Url=NULL;
 int mode=0;
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
          !strcmp(argv[i],"lasttime") || (!strncmp(argv[i],"prevtime",8) && isdigit(argv[i][8])) || 
          !strcmp(argv[i],"lastout")  || (!strncmp(argv[i],"prevout",7)  && isdigit(argv[i][7])))
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

       argc-=2;
      }

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
   {fprintf(stderr,"Usage: [-c <config-file>] wwwoffle-hash <URL>\n");exit(0);}

 /* Initialise */

 InitErrorHandler(argv0,0,1);

 InitConfigurationFile(config_file);

 if(config_file)
   {
    init_buffer(2);

    if(ReadConfigurationFile(2))
       PrintMessage(Fatal,"Error in configuration file '%s'.",config_file);
   }

 umask(0);

 if(mode!=HASH && (stat("outgoing",&buf) || !S_ISDIR(buf.st_mode)))
   {
    if(chdir(ConfigString(SpoolDir)))
      {fprintf(stderr,"The %s program must be started from the spool directory\n"
                      "Cannot change to the '%s' directory.\n",argv0,ConfigString(SpoolDir));exit(1);}
    if(stat("outgoing",&buf) || !S_ISDIR(buf.st_mode))
      {fprintf(stderr,"The %s program must be started from the spool directory\n"
                      "There is no accessible 'outgoing' directory here so it can't be right.\n",argv0);exit(1);}
   }

#if !defined(__CYGWIN__)
 fSpoolDir=open(".",O_RDONLY);
 if(fSpoolDir==-1)
    PrintMessage(Fatal,"Cannot open the spool directory '%s' [%!s].",getcwd(cwd,PATH_MAX));
#endif

 /* Get the arguments */

 if(mode!=LS_SPECIAL)
   {
    Url=(URL**)malloc(argc*sizeof(URL*));

    for(i=1;i<argc;i++)
      {
       char *arg,*colon,*slash;

       if(!strncmp(ConfigString(SpoolDir),argv[i],strlen(ConfigString(SpoolDir))) &&
          argv[i][strlen(ConfigString(SpoolDir))]=='/')
          arg=argv[i]+strlen(ConfigString(SpoolDir))+1;
       else
          arg=argv[i];

       colon=strchr(arg,':');
       slash=strchr(arg,'/');

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

 if(mode==LS)
    for(i=1;i<argc;i++)
       wwwoffle_ls(Url[i]);
 else if(mode==LS_SPECIAL)
    wwwoffle_ls_special(argv[1]);
 else if(mode==MV)
    wwwoffle_mv(Url[1],Url[2]);
 else if(mode==RM)
    for(i=1;i<argc;i++)
       wwwoffle_rm(Url[i]);
 else if(mode==READ)
    wwwoffle_read(Url[1]);
 else if(mode==WRITE)
   {
    if(config_file)
      {
       /* Change the user and group. */

       int gid=ConfigInteger(WWWOFFLE_Gid);
       int uid=ConfigInteger(WWWOFFLE_Uid);

       if(uid!=-1)
          seteuid(0);

       if(gid!=-1)
         {
#if HAVE_SETGROUPS
          if(getuid()==0 || geteuid()==0)
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

    wwwoffle_write(Url[1]);
   }
 else if(mode==HASH)
    wwwoffle_hash(Url[1]);

#if !defined(__CYGWIN__)
 close(fSpoolDir);
#endif

 exit(0);
}


/*++++++++++++++++++++++++++++++++++++++
  List the URLs within a directory of the cache.

  URL *Url The URL to list.
  ++++++++++++++++++++++++++++++++++++++*/

static void wwwoffle_ls(URL *Url)
{
 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return;}

 if(chdir(Url->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,Url->dir);fchdir(fSpoolDir);return;}

 if(strcmp(Url->path,"/"))
   {
    char *name=URLToFileName(Url);

    *name='D';
    ls(name);

    free(name);
   }
 else
   {
    struct dirent* ent;
    DIR *dir=opendir(".");

    if(!dir)
      {PrintMessage(Warning,"Cannot open current directory '%s/%s' [%!s].",Url->proto,Url->dir);fchdir(fSpoolDir);return;}

    ent=readdir(dir);  /* skip .  */
    if(!ent)
      {PrintMessage(Warning,"Cannot read current directory '%s/%s' [%!s].",Url->proto,Url->dir);closedir(dir);fchdir(fSpoolDir);return;}
    ent=readdir(dir);  /* skip .. */

    while((ent=readdir(dir)))
      {
       if(*ent->d_name=='D' && ent->d_name[strlen(ent->d_name)-1]!='~')
          ls(ent->d_name);
      }

    closedir(dir);
   }

 fchdir(fSpoolDir);
}


/*++++++++++++++++++++++++++++++++++++++
  List the URLs within the outgoing, monitor or lasttime/prevtime special directory of the cache.

  char *name The name of the directory to list.
  ++++++++++++++++++++++++++++++++++++++*/

static void wwwoffle_ls_special(char *name)
{
 struct dirent* ent;
 DIR *dir;

 if(chdir(name))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",name);return;}

 dir=opendir(".");

 if(!dir)
   {PrintMessage(Warning,"Cannot open current directory '%s' [%!s].",name);fchdir(fSpoolDir);return;}

 ent=readdir(dir);  /* skip .  */
 if(!ent)
   {PrintMessage(Warning,"Cannot read current directory '%s' [%!s].",name);closedir(dir);fchdir(fSpoolDir);return;}
 ent=readdir(dir);  /* skip .. */

 while((ent=readdir(dir)))
   {
    if((*ent->d_name=='D' || *ent->d_name=='O') && ent->d_name[strlen(ent->d_name)-1]!='~')
       ls(ent->d_name);
   }

 closedir(dir);

 fchdir(fSpoolDir);
}


/*++++++++++++++++++++++++++++++++++++++
  List one file.

  char *file The name of the file to ls.
  ++++++++++++++++++++++++++++++++++++++*/

static void ls(char *file)
{
 struct stat buf;
 time_t now=-1;

 if(now==-1)
    now=time(NULL);

 if(stat(file,&buf))
   {PrintMessage(Warning,"Cannot stat the file '%s' [%!s].",file);return;}
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

       free(url);
      }
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Move one URL or host to another.

  URL *Url1 The source URL.

  URL *Url2 The destination URL.
  ++++++++++++++++++++++++++++++++++++++*/

static void wwwoffle_mv(URL *Url1,URL *Url2)
{
 struct dirent* ent;
 DIR *dir;

 if(chdir(Url1->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url1->proto);return;}

 if(chdir(Url1->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url1->proto,Url1->dir);fchdir(fSpoolDir);return;}

 dir=opendir(".");

 if(!dir)
   {PrintMessage(Warning,"Cannot open current directory '%s/%s' [%!s].",Url1->proto,Url1->dir);fchdir(fSpoolDir);return;}

 ent=readdir(dir);  /* skip .  */
 if(!ent)
   {PrintMessage(Warning,"Cannot read current directory '%s/%s' [%!s].",Url1->proto,Url1->dir);closedir(dir);fchdir(fSpoolDir);return;}
 ent=readdir(dir);  /* skip .. */

 while((ent=readdir(dir)))
   {
    if(*ent->d_name=='D')
      {
       char *url1=FileNameToURL(ent->d_name);

       if(url1)
         {
          char *url2;
          URL *Url;
          char *path2,*name1,*name2;
          int fd2;

          Url=SplitURL(url1);
          url2=(char*)malloc(strlen(Url->pathp)+strlen(Url2->dir)+strlen(Url2->proto)+8);
          sprintf(url2,"%s://%s%s",Url2->proto,Url2->dir,Url->pathp);
          FreeURL(Url);

          name1=ent->d_name;

          Url=SplitURL(url2);
          name2=URLToFileName(Url);

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
          init_buffer(fd2);
          write_string(fd2,Url->name);
          close(fd2);
          unlink(name2);

          free(url1);
          free(url2);
          free(name2);
          free(path2);
          FreeURL(Url);
         }
      }
   }

 closedir(dir);

 fchdir(fSpoolDir);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a URL.

  URL *Url The URL to delete.
  ++++++++++++++++++++++++++++++++++++++*/

static void wwwoffle_rm(URL *Url)
{
 DeleteWebpageSpoolFile(Url,0);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a URL and output on stdout.

  URL *Url The URL to read.
  ++++++++++++++++++++++++++++++++++++++*/

static void wwwoffle_read(URL *Url)
{
 char *line=NULL,buffer[READ_BUFFER_SIZE];
 int n,spool=OpenWebpageSpoolFile(1,Url);
#if USE_ZLIB
 int compression=0;
#endif

 if(spool==-1)
    return;

 init_buffer(1);
 init_buffer(spool);

 while((line=read_line(spool,line)))
   {
#if USE_ZLIB
    if(!strncmp(line,"Pragma: wwwoffle-compressed",27))
       compression=2;
    else
#endif
       write_string(1,line);

    if(*line=='\r' || *line=='\n')
       break;
   }

#if USE_ZLIB
 if(compression)
    init_zlib_buffer(spool,compression);
#endif

 while((n=read_data(spool,buffer,READ_BUFFER_SIZE))>0)
    write_data(1,buffer,n);

#if USE_ZLIB
 if(compression)
    finish_zlib_buffer(spool);
#endif

 close(spool);
}


/*++++++++++++++++++++++++++++++++++++++
  Write a URL from the input on stdin.

  URL *Url The URL to write.
  ++++++++++++++++++++++++++++++++++++++*/

static void wwwoffle_write(URL *Url)
{
 char buffer[READ_BUFFER_SIZE];
 int n,spool=OpenWebpageSpoolFile(0,Url);

 if(spool==-1)
    return;

 init_buffer(0);
 init_buffer(spool);

 while((n=read_data(0,buffer,READ_BUFFER_SIZE))>0)
    write_data(spool,buffer,n);

 close(spool);
}


/*++++++++++++++++++++++++++++++++++++++
  Print the hash pattern for an URL
  
  URL *Url The URL to be hashed.
  ++++++++++++++++++++++++++++++++++++++*/

static void wwwoffle_hash(URL *Url)
{
 char *name=URLToFileName(Url);

 printf("%s\n",name+1);
 free(name);
}
