/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/uncompress-cache.c 1.17 2004/02/14 14:03:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Uncompress the cache.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02,03,04 Andrew M. Bishop
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

#include <utime.h>
#include <fcntl.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "version.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif

/*+ Set this to 0 for debugging so that nothing is changed. +*/
#define DO_RENAME 1

#if USE_ZLIB
static void UncompressProto(char *proto);
static void UncompressHost(char *proto,char *host);
#endif


/*++++++++++++++++++++++++++++++++++++++
  The main program
  ++++++++++++++++++++++++++++++++++++++*/

int main(int argc,char** argv)
{
#if USE_ZLIB

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
   {fprintf(stderr,"Usage: uncompress-cache <full-path-to-spool-dir> | -c <config-file>\n");exit(0);}
 else if(argc>1 && !strcmp(argv[1],"--version"))
   {fprintf(stderr,"WWWOFFLE uncompress-cache version %s\n",WWWOFFLE_VERSION);exit(0);}
 else if(argc!=2 || argv[1][0]!='/')
   {fprintf(stderr,"Usage: uncompress-cache <full-path-to-spool-dir> | -c <config-file>\n");exit(1);}

 /* Initialise */

 InitErrorHandler("uncompress-cache",0,1);

 InitConfigurationFile(config_file);

 if(config_file)
   {
    init_io(STDERR_FILENO);

    if(ReadConfigurationFile(STDERR_FILENO))
       PrintMessage(Fatal,"Error in configuration file '%s'.",config_file);

    finish_io(STDERR_FILENO);
   }

 umask(0);

 /* Change to the spool directory. */

 if(config_file)
    spool_dir=ConfigString(SpoolDir);
 else
    spool_dir=argv[1];

 if(ChangeToSpoolDir(spool_dir))
    PrintMessage(Fatal,"Cannot change to the spool directory '%s' [%!s]; uncompression failed.",spool_dir);

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
       PrintMessage(Inform,"Cannot stat the '%s' directory [%!s]; not uncompressed.",proto);
    else
      {
       printf("Uncompressing %s\n",proto);

       if(chdir(proto))
          PrintMessage(Warning,"Cannot change to the '%s' directory [%!s]; uncompression failed.",proto);
       else
         {
          UncompressProto(proto);

          ChangeBackToSpoolDir();
         }
      }
   }

 return(0);

#else

 fprintf(stderr,"Error: uncompress-cache was compiled without zlib, no uncompression possible.\n");

 return(1);

#endif /* USE_ZLIB */
}


#if USE_ZLIB

/*++++++++++++++++++++++++++++++++++++++
  Uncompress a complete protocol directory.

  char *proto The protocol of the spool directory we are in.
  ++++++++++++++++++++++++++++++++++++++*/

static void UncompressProto(char *proto)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;

 /* Open the spool directory. */

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open spool directory '%s' [%!s]; uncompression failed.",proto);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read spool directory '%s' [%!s]; uncompression failed.",proto);closedir(dir);return;}

 /* Go through each entry. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(stat(ent->d_name,&buf))
       PrintMessage(Warning,"Cannot stat file '%s/%s' [%!s] not uncompressed.",proto,ent->d_name);
    else if(S_ISDIR(buf.st_mode))
      {
       if(chdir(ent->d_name))
          PrintMessage(Warning,"Cannot change to the '%s/%s' directory [%!s]; uncompression failed.",proto,ent->d_name);
       else
         {
          printf("  Uncompressing %s\n",ent->d_name);

          UncompressHost(proto,ent->d_name);

          ChangeBackToSpoolDir();
          chdir(proto);
         }
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Uncompress a complete host directory.

  char *proto The protocol of the spool directory we are in.

  char *host The hostname of the spool directory we are in.
  ++++++++++++++++++++++++++++++++++++++*/

static void UncompressHost(char *proto,char *host)
{
 DIR *dir;
 struct dirent* ent;
 int ifd,ofd;
 char *zfile;
 Header *spool_head;
 char *head,buffer[READ_BUFFER_SIZE];
 int n;
 struct stat buf;
 struct utimbuf ubuf;

 /* Open the spool directory. */

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open spool directory '%s/%s' [%!s]; uncompression failed.",proto,host);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read spool directory '%s/%s' [%!s]; uncompression failed.",proto,host);closedir(dir);return;}

 /* Go through each entry. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(!stat(ent->d_name,&buf) && *ent->d_name=='D' && S_ISREG(buf.st_mode))
      {
       ubuf.actime=buf.st_atime;
       ubuf.modtime=buf.st_mtime;

       ifd=open(ent->d_name,O_RDONLY|O_BINARY);

       if(ifd==-1)
         {
          PrintMessage(Inform,"Cannot open file '%s/%s/%s' to uncompress it [%!s]; race condition?",proto,host,ent->d_name);
          continue;
         }

       init_io(ifd);

       ParseReply(ifd,&spool_head);

       if(spool_head && GetHeader2(spool_head,"Pragma","wwwoffle-compressed"))
         {
          printf("    %s\n",ent->d_name);

          configure_io_read(ifd,-1,2,0);

          zfile=(char*)malloc(strlen(ent->d_name)+4);
          strcpy(zfile,ent->d_name);
          strcat(zfile,".z");

          ofd=open(zfile,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,buf.st_mode&07777);

          if(ofd==-1)
            {
             PrintMessage(Inform,"Cannot open file '%s/%s/%s' to uncompress to [%!s].",proto,host,zfile);
             FreeHeader(spool_head);
             finish_io(ifd);
             close(ifd);
             utime(ent->d_name,&ubuf);
             continue;
            }

          init_io(ofd);

          RemoveFromHeader(spool_head,"Content-Encoding");
          RemoveFromHeader2(spool_head,"Pragma","wwwoffle-compressed");

          head=HeaderString(spool_head);

          write_string(ofd,head);
          free(head);

          while((n=read_data(ifd,buffer,READ_BUFFER_SIZE))>0)
             write_data(ofd,buffer,n);

          finish_io(ifd);
          close(ifd);

          finish_io(ofd);
          close(ofd);

#if DO_RENAME
          if(rename(zfile,ent->d_name))
            {
             PrintMessage(Inform,"Cannot rename file '%s/%s/%s' to '%s/%s/%s' [%!s].",proto,host,zfile,proto,host,ent->d_name);
             unlink(zfile);
            }
          else
#endif
             utime(zfile,&ubuf);

          free(zfile);
         }

       if(spool_head)
          FreeHeader(spool_head);

       utime(ent->d_name,&ubuf);
      }
   }
 while((ent=readdir(dir)));

 closedir(dir);
}

#endif /* USE_ZLIB */
