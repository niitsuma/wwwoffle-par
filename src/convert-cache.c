/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/convert-cache.c 1.9 2001/12/30 10:21:40 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7.
  Convert the cache to handle the changed URL Decoding/Encoding.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01 Andrew M. Bishop
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
#include <errno.h>

#include "wwwoffle.h"
#include "version.h"
#include "misc.h"
#include "errors.h"
#include "config.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

/*+ Set this to 0 for debugging so that nothing is changed. +*/
#define DO_RENAME 1

static void ConvertProto(char *proto);
static void ConvertHost(char *proto,char *host);
static void ConvertSpecial(char *dirname);


/*+ A file descriptor for the spool directory. +*/
int fSpoolDir=-1;


/*++++++++++++++++++++++++++++++++++++++
  The main program
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char** argv)
{
 int i;
 struct stat buf;
 char *config_file=NULL,*spool_dir=NULL;

 /* Find the configuration file */

 for(i=1;i<argc;i++)
    if(config_file)
      {
       argv[i-2]=argv[i];
      }
    else if(!strcmp(argv[i],"-c"))
      {
       if(++i>=argc)
         {fprintf(stderr,"convert-cache: The '-c' argument requires a configuration file name.\n"); exit(1);}

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

 if(argc>1 && !strcmp(argv[1],"--help"))
   {fprintf(stderr,"Usage: convert-cache <full-path-to-spool-dir> | -c <config-file>\n");exit(0);}
 else if(argc>1 && !strcmp(argv[1],"--version"))
   {fprintf(stderr,"WWWOFFLE convert-cache version %s\n",WWWOFFLE_VERSION);exit(0);}
 else if(!config_file && (argc!=2 || argv[1][0]!='/'))
   {fprintf(stderr,"Usage: convert-cache <full-path-to-spool-dir> | -c <config-file>\n");exit(1);}

 /* Initialise */

 InitErrorHandler("convert-cache",0,1);

 InitConfigurationFile(config_file);

 if(config_file)
   {
    init_buffer(2);

    if(ReadConfigurationFile(2))
       PrintMessage(Fatal,"Error in configuration file '%s'.",config_file);
   }

 umask(0);

 /* Change to the spool directory. */

 if(config_file)
    spool_dir=ConfigString(SpoolDir);
 else
    spool_dir=argv[1];

 if(chdir(spool_dir))
    PrintMessage(Fatal,"Cannot change to the spool directory '%s' [%!s]; conversion failed.",spool_dir);

#if !defined(__CYGWIN__)
 fSpoolDir=open(".",O_RDONLY);
 if(fSpoolDir==-1)
    PrintMessage(Fatal,"Cannot open the spool directory '%s' [%!s]; conversion failed.",spool_dir);
#endif

 /* Create the new spool directory. */

 for(i=0;i<3;i++)
   {
    char *proto;

    if(i==0)
       proto="http";
    else if(i==1)
       proto="ftp";
    else
       proto="finger";

    if(stat(proto,&buf))
       PrintMessage(Inform,"Cannot stat the '%s' directory [%!s]; not converted.",proto);
    else
      {
       printf("Converting %s\n",proto);

       if(chdir(proto))
          PrintMessage(Warning,"Cannot change to the '%s' directory [%!s]; conversion failed.",proto);
       else
         {
          ConvertProto(proto);

          fchdir(fSpoolDir);
         }
      }
   }

 for(i=0;i<3;i++)
   {
    char *special;

    if(i==0)
       special="outgoing";
    else if(i==1)
       special="lasttime";
    else
       special="monitor";

    if(stat(special,&buf))
       PrintMessage(Inform,"Cannot stat the '%s' directory [%!s]; not convertd.",special);
    else
      {
       printf("Converting %s\n",special);

       if(chdir(special))
          PrintMessage(Warning,"Cannot change to the '%s' directory [%!s]; convert failed.",special);
       else
         {
          ConvertSpecial(special);

          fchdir(fSpoolDir);
         }
      }
   }

#if !defined(__CYGWIN__)
 close(fSpoolDir);
#endif

 return(0);
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
   {PrintMessage(Warning,"Cannot open spool directory '%s' [%!s]; conversion failed.",proto);return;}

 ent=readdir(dir);  /* skip .  */
 if(!ent)
   {PrintMessage(Warning,"Cannot read spool directory '%s' [%!s]; conversion failed.",proto);closedir(dir);return;}
 ent=readdir(dir);  /* skip .. */

 /* Go through each entry. */

 while((ent=readdir(dir)))
   {
    if(stat(ent->d_name,&buf))
       PrintMessage(Warning,"Cannot stat file '%s/%s' [%!s] not converted.",proto,ent->d_name);
    else if(S_ISDIR(buf.st_mode))
      {
       if(chdir(ent->d_name))
          PrintMessage(Warning,"Cannot change to the '%s/%s' directory [%!s]; conversion failed.",proto,ent->d_name);
       else
         {
          printf("  Converting %s\n",ent->d_name);

          ConvertHost(proto,ent->d_name);

          fchdir(fSpoolDir);
          chdir(proto);
         }
      }
   }

 closedir(dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a complete host directory.

  char *proto The protocol of the spool directory we are in.

  char *host The hostname of the spool directory we are in.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConvertHost(char *proto,char *host)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;
 int ufd;

 /* Open the spool directory. */

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open spool directory '%s/%s' [%!s]; conversion failed.",proto,host);return;}

 ent=readdir(dir);  /* skip .  */
 if(!ent)
   {PrintMessage(Warning,"Cannot read spool directory '%s/%s' [%!s]; conversion failed.",proto,host);closedir(dir);return;}
 ent=readdir(dir);  /* skip .. */

 /* Go through each entry. */

 while((ent=readdir(dir)))
   {
    if(!stat(ent->d_name,&buf) && *ent->d_name=='D' && S_ISREG(buf.st_mode))
      {
       char *url=FileNameToURL(ent->d_name);

       if(url)
         {
          URL *Url=SplitURL(url);
          char *oldname=ent->d_name;
          local_URLToFileName(Url,newname)

          if(strcmp(oldname+1,newname+1) || strcmp(url,Url->file))
            {
             printf("    - %s\n",url);
             printf("    + %s\n",Url->file);
             printf("\n");

#if DO_RENAME
             *oldname=*newname='D';
             rename(oldname,newname);
             *oldname=*newname='U';
             rename(oldname,newname);

             ufd=open(newname,O_WRONLY|O_TRUNC|O_BINARY);
             init_buffer(ufd);

             if(ufd!=-1)
               {
                write_string(ufd,Url->file);
                close(ufd);
               }
#endif
            }

          FreeURL(Url);
          free(url);
         }
      }
   }

 closedir(dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a complete special directory.

  char *special The name of the special directory.
  ++++++++++++++++++++++++++++++++++++++*/

static void ConvertSpecial(char *special)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;
 int ufd;

 /* Open the spool directory. */

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open spool directory '%s' [%!s]; conversion failed.",special);return;}

 ent=readdir(dir);  /* skip .  */
 if(!ent)
   {PrintMessage(Warning,"Cannot read spool directory '%s' [%!s]; conversion failed.",special);closedir(dir);return;}
 ent=readdir(dir);  /* skip .. */

 /* Go through each entry. */

 while((ent=readdir(dir)))
   {
    if(!stat(ent->d_name,&buf) && (*ent->d_name=='O' || *ent->d_name=='D')  && S_ISREG(buf.st_mode))
      {
       char *url=FileNameToURL(ent->d_name);

       if(url)
         {
          URL *Url=SplitURL(url);
          char *oldname=ent->d_name;
          local_URLToFileName(Url,newname)

          if(strcmp(oldname+1,newname+1) || strcmp(url,Url->file))
            {
             printf("  - %s\n",url);
             printf("  + %s\n",Url->file);
             printf("\n");

#if DO_RENAME
             *oldname=*newname='D';
             rename(oldname,newname);
             *oldname=*newname='O';
             rename(oldname,newname);
             *oldname=*newname='M';
             rename(oldname,newname);
             *oldname=*newname='U';
             rename(oldname,newname);

             ufd=open(newname,O_WRONLY|O_TRUNC|O_BINARY);
             init_buffer(ufd);

             if(ufd!=-1)
               {
                write_string(ufd,Url->file);
                close(ufd);
               }
#endif
            }

          FreeURL(Url);
          free(url);
         }
      }
   }

 closedir(dir);
}
