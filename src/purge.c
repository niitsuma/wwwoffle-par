/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/purge.c 2.53 2002/08/04 10:26:06 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7c.
  Purge old files from the cache.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02 Andrew M. Bishop
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

#include <utime.h>
#include <sys/stat.h>
#include <fcntl.h>

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

#if HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif

#if HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif

#if HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#if HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#if HAVE_STATVFS
#define STATFS statvfs
#else
#define STATFS statfs
#endif

#include "wwwoffle.h"
#include "misc.h"
#include "proto.h"
#include "config.h"
#include "errors.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif


/*+ The file descriptor of the sppol directory. +*/
extern int fSpoolDir;


/* Local functions */

static void PurgeFiles(char *proto,char *host,int def_purge_age,int def_compress_age,/*@out@*/ unsigned long *remain,/*@out@*/ unsigned long *deleted,/*@out@*/ unsigned long *compressed);
static void what_purge_compress_age(char *proto,char *host,/*@null@*/ char *file,/*@out@*/ int *purge_age,/*@out@*/ int *compress_age);
#if USE_ZLIB
static unsigned long compress_file(char *proto,char *host,char *file);
#endif

/*+ Set this to 0 for debugging so that nothing is deleted. +*/
#define DO_DELETE 1

/*+ Set this to 0 for debugging so that nothing is compressed. +*/
#define DO_COMPRESS 1

/*+ The current time. +*/
static time_t now;

/*+ The number of blocks left of each age. +*/
static unsigned long *blocks_by_age;

/*+ The scaling factor for the ages in the second pass. +*/
static double age_scale;

/*+ The pass in the purge (1 or 2). +*/
static int pass;

/*+ The blocksize. +*/
static unsigned long blocksize;

/*+ The configuraion file options, looked up once then used. +*/
static int purge_use_mtime;
static int purge_use_url;
static int purge_default_age;
static int compress_default_age;


/*
 Note: It is not possible to count bytes in case the total size exceeds 2GB.
       Counting blocks gives a more accurate result (compared to du) on most
       systems anyway.
 Note: Since some UNIX versions (Solaris) have st_size/st_blocks different for
       different files I have reverted to dividing size in bytes by blocksize
       instead of st_blocks for counting the number of blocks in a file.
*/


/*++++++++++++++++++++++++++++++++++++++
  Purge files from the cache that meet the age criteria.

  int fd the file descriptor of the wwwoffle client.
  ++++++++++++++++++++++++++++++++++++++*/

void PurgeCache(int fd)
{
 int i,p;
 struct STATFS sbuf;
 struct stat buf;

 what_purge_compress_age("*","*",NULL,&purge_default_age,&compress_default_age);
 purge_use_mtime=ConfigBoolean(PurgeUseMTime);
 purge_use_url=ConfigBoolean(PurgeUseURL);

 now=time(NULL)+600;

 age_scale=-1;

 blocks_by_age=(unsigned long*)malloc((purge_default_age+2)*sizeof(unsigned long));

 for(i=0;i<=purge_default_age+1;i++)
    blocks_by_age[i]=0;

 /* Find out the blocksize of the disk allocation if we can.
    It isn't easy since some operating systems lie. */

 if(stat(".",&buf) || buf.st_size==-1 || buf.st_blocks==-1)
   {
    PrintMessage(Warning,"Cannot determine the disk block size [%!s]; using 1024 instead.");
    blocksize=1024;
   }
 else if(buf.st_blocks==0)
   {
    PrintMessage(Warning,"The number of blocks (0) looks wrong; using 1024 for blocksize.");
    blocksize=1024;
   }
 else
   {
    blocksize=buf.st_size/buf.st_blocks;

    if(blocksize!=512 && blocksize!=1024 && blocksize!=2048)
      {
       PrintMessage(Warning,"The blocksize (%d) looks wrong; using 1024 instead.",blocksize);
       blocksize=1024;
      }
   }

 /* Note: blocksize can be only 512, 1024 or 2048, it is only used for rounding up file sizes. */

 /* Convert blocks to kilobytes. */
 /* Handle this carefully to avoid overflows from doing blocks*blocksize/1024
                                or any errors from doing (blocks/1024)*blocksize */
#define Blocks_to_kB(blocks) \
 (unsigned long)((blocksize==512)?((blocks)/2): \
                 ((blocksize==1024)?(blocks): \
                  (blocks)*2))

 /* Convert bytes to blocks. */
 /* Divide and then round up if needed. */

#define Bytes_to_Blocks(bytes) \
 ((unsigned long)((bytes)/blocksize)+(unsigned long)!!((bytes)%blocksize))


 for(pass=1;pass<=2;pass++)
   {
    unsigned long total_file_blocks=0,total_del_blocks=0,total_compress_blocks=0,total_dirs=0;
    long dir_blocks;

    write_string(fd,"\n");

    if(ConfigInteger(PurgeCacheSize))
       if(pass==1)
          write_string(fd,"Pass 1: Checking dates and sizes of files:\n");
       else
          write_string(fd,"Pass 2: Purging files down to specified size:\n");
    else
       write_string(fd,"Checking dates of files:\n");

    if(pass==1)
      {
       if(ConfigBoolean(PurgeUseMTime))
          write_string(fd,"  (Using modification time.)\n");
       else
          write_string(fd,"  (Using last access time.)\n");

       if(ConfigBoolean(PurgeUseURL))
          write_string(fd,"  (Using the full URL.)\n");
       else
          write_string(fd,"  (Using the hostname and protocol only.)\n");
      }

    write_string(fd,"\n");

    for(p=0;p<NProtocols;p++)
      {
       DIR *dir;
       struct dirent* ent;
       struct stat buf;
       char *proto=Protocols[p].name;

       /* Open the spool directory. */

       if(stat(proto,&buf))
         {PrintMessage(Inform,"Cannot stat directory '%s' [%!s]; not purged",proto);continue;}

       dir_blocks=Bytes_to_Blocks(buf.st_size);

       total_file_blocks+=dir_blocks;
       blocks_by_age[purge_default_age+1]+=dir_blocks;

       if(chdir(proto))
         {PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not purged.",proto);continue;}

       dir=opendir(".");
       if(!dir)
         {PrintMessage(Warning,"Cannot open directory '%s' [%!s]; not purged.",proto);fchdir(fSpoolDir);continue;}

       ent=readdir(dir);  /* skip .  */
       if(!ent)
         {PrintMessage(Warning,"Cannot read directory '%s' [%!s]; not purged.",proto);closedir(dir);fchdir(fSpoolDir);continue;}
       ent=readdir(dir);  /* skip .. */

       /* Search through all of the sub directories. */

       while((ent=readdir(dir)))
         {
          struct stat buf;

          if(stat(ent->d_name,&buf))
             PrintMessage(Inform,"Cannot stat directory '%s/%s' [%!s]; race condition?",proto,ent->d_name);
          else if(S_ISDIR(buf.st_mode))
            {
             unsigned long file_blocks,del_blocks,compress_blocks;
             int purge_age,compress_age;

             what_purge_compress_age(proto,ent->d_name,NULL,&purge_age,&compress_age);

             PurgeFiles(proto,ent->d_name,purge_age,compress_age,&file_blocks,&del_blocks,&compress_blocks);

             dir_blocks=Bytes_to_Blocks(buf.st_size);

             if(file_blocks==0)
               {
#if DO_DELETE
                if(rmdir(ent->d_name))
                   PrintMessage(Warning,"Cannot delete what should be an empty directory '%s/%s' [%!s].",proto,ent->d_name);
#else
                PrintMessage(Debug,"rmdir(%s/%s).",proto,ent->d_name);
#endif
                total_del_blocks+=del_blocks+dir_blocks;
               }
             else
               {
                struct utimbuf utbuf;

                utbuf.actime=buf.st_atime;
                utbuf.modtime=buf.st_mtime;
                utime(ent->d_name,&utbuf);

                blocks_by_age[purge_default_age+1]+=dir_blocks;
                total_file_blocks+=file_blocks+dir_blocks;
                total_del_blocks+=del_blocks;
                total_compress_blocks+=compress_blocks;
                total_dirs++;
               }

             if(purge_use_url)
               {
                if(file_blocks==0)
                   write_formatted(fd,"Purged %6s://%-40s ; empty    (%4lu kB del)\n",proto,ent->d_name,
                                   Blocks_to_kB(del_blocks+dir_blocks));
                else
                  {
#if USE_ZLIB
                   if(compress_blocks)
                     {
                      if(del_blocks)
                         write_formatted(fd,"Purged %6s://%-40s ; %5lu kB (%4lu kB del) (%4lu kB compr)\n",proto,ent->d_name,
                                         Blocks_to_kB(file_blocks+dir_blocks),Blocks_to_kB(del_blocks),Blocks_to_kB(compress_blocks));
                      else
                         write_formatted(fd,"Purged %6s://%-40s ; %5lu kB               (%4lu kB compr)\n",proto,ent->d_name,
                                         Blocks_to_kB(file_blocks+dir_blocks),Blocks_to_kB(compress_blocks));
                     }
                   else
#endif
                     {
                      if(del_blocks)
                         write_formatted(fd,"Purged %6s://%-40s ; %5lu kB (%4lu kB del)\n",proto,ent->d_name,
                                         Blocks_to_kB(file_blocks+dir_blocks),Blocks_to_kB(del_blocks));
                      else
                         write_formatted(fd,"Purged %6s://%-40s ; %5lu kB\n",proto,ent->d_name,
                                         Blocks_to_kB(file_blocks+dir_blocks));
                     }
                  }
               }
             else
               {
                if(pass==2 && purge_age>0)
                   purge_age=(int)(purge_age*age_scale+0.5);

                if(purge_age<0)
                  {
#if USE_ZLIB
                   if(compress_blocks)
                      write_formatted(fd,"Not Purged       %6s://%-40s ; %4lu kB               (%4lu kB compr)\n",proto,ent->d_name,
                                      Blocks_to_kB(file_blocks+dir_blocks),Blocks_to_kB(compress_blocks));
                   else
#endif
                      write_formatted(fd,"Not Purged       %6s://%-40s ; %4lu kB\n",proto,ent->d_name,
                                      Blocks_to_kB(file_blocks+dir_blocks));
                  }
                else if(file_blocks==0)
                   write_formatted(fd,"Purged (%2d days) %6s://%-40s ; empty    (%4lu kB del)\n",purge_age,proto,ent->d_name,
                                   Blocks_to_kB(del_blocks+dir_blocks));
                else
                  {
#if USE_ZLIB
                   if(compress_blocks)
                     {
                      if(del_blocks)
                         write_formatted(fd,"Purged (%2d days) %6s://%-40s ; %5lu kB (%4lu kB del) (%4lu kB compr)\n",purge_age,proto,ent->d_name,
                                         Blocks_to_kB(file_blocks+dir_blocks),Blocks_to_kB(del_blocks),Blocks_to_kB(compress_blocks));
                      else
                         write_formatted(fd,"Purged (%2d days) %6s://%-40s ; %5lu kB               (%4lu kB compr)\n",purge_age,proto,ent->d_name,
                                         Blocks_to_kB(file_blocks+dir_blocks),Blocks_to_kB(compress_blocks));
                     }
                   else
#endif
                     {
                      if(del_blocks)
                         write_formatted(fd,"Purged (%2d days) %6s://%-40s ; %5lu kB (%4lu kB del)\n",purge_age,proto,ent->d_name,
                                         Blocks_to_kB(file_blocks+dir_blocks),Blocks_to_kB(del_blocks));
                      else
                         write_formatted(fd,"Purged (%2d days) %6s://%-40s ; %5lu kB\n",purge_age,proto,ent->d_name,
                                         Blocks_to_kB(file_blocks+dir_blocks));
                     }
                  }
               }
            }
          else
            {
             PrintMessage(Warning,"Found an unexpected file instead of a directory '%s/%s' [%!s]; deleting it.",proto,ent->d_name);

#if DO_DELETE
             if(unlink(ent->d_name))
                PrintMessage(Warning,"Cannot delete the non-directory '%s/%s' [%!s].",proto,ent->d_name);
#else
             PrintMessage(Debug,"unlink(%s/%s).",proto,ent->d_name);
#endif
            }

         }

       closedir(dir);
       fchdir(fSpoolDir);
      }

    write_string(fd,"\n");
#if USE_ZLIB
    write_formatted(fd,"Total of %lu directories ; %lu kB (%lu kB deleted) (%lu kB compressed)\n",total_dirs,
                    Blocks_to_kB(total_file_blocks),Blocks_to_kB(total_del_blocks),Blocks_to_kB(total_compress_blocks));
#else
    write_formatted(fd,"Total of %lu directories ; %lu kB (%lu kB deleted)\n",total_dirs,
                    Blocks_to_kB(total_file_blocks),Blocks_to_kB(total_del_blocks));
#endif

    if(pass==1)
      {
       int age_for_size=-1,age_for_free=-1;
       unsigned long age_blocks_used=blocks_by_age[purge_default_age+1];
       unsigned long age_blocks_free=total_file_blocks-blocks_by_age[purge_default_age+1];
       unsigned long diskfree;

       /* Calculate the number of blocksize blocks there are free on the disk.
          Use statfs() to get the 'df' info of disk block size and free disk blocks. */

       if(STATFS(".",&sbuf) || sbuf.f_bsize==~0 || sbuf.f_bavail==~0)
         {
          PrintMessage(Warning,"Cannot determine the disk free space [%!s]; assuming 0.");
          diskfree=0;
         }
       else
         {
          unsigned long bs=blocksize,dbs=sbuf.f_bsize;

          /* Calculate (df_free_blocks*df_block_size)/blocksize carefully to avoid overflow. */

          while(!(dbs&1) && !(bs&1))  /* remove powers of 2. */
            {dbs>>=1;bs>>=1;}

          /* If both were powers of 2 then there should be no problem (either dbs or bs is now 1).
             If not then I am assuming that sbuf.f_bavail is larger than dbs so the error is smaller. */

          diskfree=dbs*(sbuf.f_bavail/bs);
         }

       age_blocks_free+=diskfree;

       /* Print out the statistics. */

       write_string(fd,"\n");
       write_string(fd,"Age Profile of cached pages:\n");
       write_formatted(fd,"  (All ages scaled to the range 0 -> %d (default age).)\n",purge_default_age);
       write_string(fd,"\n");

       write_formatted(fd,"Total not purged   ; %5lu kB (%6lu kB free)\n",
                       Blocks_to_kB(age_blocks_used),Blocks_to_kB(age_blocks_free));
       write_string(fd,"\n");

       for(i=0;i<=purge_default_age;i++)
         {
          age_blocks_used+=blocks_by_age[i];
          age_blocks_free-=blocks_by_age[i];

          if(ConfigInteger(PurgeCacheSize) && age_for_size<0 &&
             Blocks_to_kB(age_blocks_used)>(1024*ConfigInteger(PurgeCacheSize)))
            {
             age_for_size=i;

             write_formatted(fd,"Cutoff Age is %2d days for %3d MB cache size\n",age_for_size,ConfigInteger(PurgeCacheSize));
            }

          if(ConfigInteger(PurgeDiskFree) && diskfree && age_for_free<0 &&
             Blocks_to_kB(age_blocks_free)<(1024*ConfigInteger(PurgeDiskFree)))
            {
             age_for_free=i;

             write_formatted(fd,"Cutoff Age is %2d days for %3d MB disk free\n",age_for_free,ConfigInteger(PurgeDiskFree));
            }

          if(i==purge_default_age)
             write_formatted(fd,"Total all ages     ; %5lu kB (%6lu kB free)\n",
                             Blocks_to_kB(age_blocks_used-blocks_by_age[purge_default_age+1]),
                             diskfree?Blocks_to_kB(age_blocks_free):0);
          else
             write_formatted(fd,"Newer than %2d day%c ; %5lu kB (%6lu kB free)\n",i+1,i?'s':' ',
                             Blocks_to_kB(age_blocks_used-blocks_by_age[purge_default_age+1]),
                             diskfree?Blocks_to_kB(age_blocks_free):0);
         }

       if(purge_default_age)
         {
          if(age_for_size!=-1 && (age_for_size<=age_for_free || age_for_free==-1))
             age_scale=(double)age_for_size/(double)purge_default_age;
          else if(age_for_free!=-1 && (age_for_free<age_for_size || age_for_size==-1))
             age_scale=(double)age_for_free/(double)purge_default_age;
         }
       else if(age_for_size!=-1 || age_for_free!=-1)
          age_scale=0;
      }

    if(age_scale==-1)
       break;
   }

 write_string(fd,"\n");

 free(blocks_by_age);

 /* Purge the tmp.* files in outgoing. */

 if(chdir("outgoing"))
    PrintMessage(Warning,"Cannot change to directory 'outgoing' [%!s]; not purged.");
 else
   {
    DIR *dir;
    struct dirent* ent;

    dir=opendir(".");
    if(!dir)
      PrintMessage(Warning,"Cannot open directory 'outgoing' [%!s]; not purged.");
    else
      {
       ent=readdir(dir);  /* skip .  */
       if(!ent)
          PrintMessage(Warning,"Cannot read directory 'outgoing' [%!s]; not purged.");
       else
         {
          ent=readdir(dir);  /* skip .. */

          while((ent=readdir(dir)))
             if(!strncmp(ent->d_name,"tmp.",4))
               {
                struct stat buf;

                if(!stat(ent->d_name,&buf) && buf.st_mtime<(now-60))
                  {
#if DO_DELETE
                   if(unlink(ent->d_name))
                      PrintMessage(Warning,"Cannot unlink file 'outgoing/%s' [%!s]; race condition?",ent->d_name);
#else
                   PrintMessage(Debug,"unlink(outgoing/%s).",ent->d_name);
#endif
                  }
               }
         }

       closedir(dir);
      }
   }

 fchdir(fSpoolDir);

 /* Purge the tmp.* files in temp. */

 if(chdir("temp"))
    PrintMessage(Warning,"Cannot change to directory 'temp' [%!s]; not purged.");
 else
   {
    DIR *dir;
    struct dirent* ent;

    dir=opendir(".");
    if(!dir)
      PrintMessage(Warning,"Cannot open directory 'temp' [%!s]; not purged.");
    else
      {
       ent=readdir(dir);  /* skip .  */
       if(!ent)
          PrintMessage(Warning,"Cannot read directory 'temp' [%!s]; not purged.");
       else
         {
          ent=readdir(dir);  /* skip .. */

          while((ent=readdir(dir)))
             if(!strncmp(ent->d_name,"tmp.",4))
               {
                struct stat buf;

                if(!stat(ent->d_name,&buf) && buf.st_mtime<(now-60))
                  {
#if DO_DELETE
                   if(unlink(ent->d_name))
                      PrintMessage(Warning,"Cannot unlink file 'temp/%s' [%!s]; race condition?",ent->d_name);
#else
                   PrintMessage(Debug,"unlink(temp/%s).",ent->d_name);
#endif
                  }
               }
         }

       closedir(dir);
      }
   }

 fchdir(fSpoolDir);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete the file in the current directory that are older than the specified age.

  char *proto The name of the protocol directory to purge.

  char *host The name of the host directory to purge.

  int def_purge age The default purge age to use for this host.

  int def_compress age The default compress age to use for this host.

  unsigned long *remain Returns the number of blocks in files that are left.

  unsigned long *deleted Returns the number of blocks in files that were deleted.

  unsigned long *compressed Returns the number of blocks in files that were compressed.
  ++++++++++++++++++++++++++++++++++++++*/

static void PurgeFiles(char *proto,char *host,int def_purge_age,int def_compress_age,unsigned long *remain,unsigned long *deleted,unsigned long *compressed)
{
 DIR *dir;
 struct dirent* ent;

 *remain=0;
 *deleted=0;
 *compressed=0;

 /* Open the spool subdirectory. */

 if(chdir(host))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s]; not purged.",proto,host);return;}

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory '%s/%s' [%!s]; not purged.",proto,host);fchdir(fSpoolDir);chdir(proto);return;}

 ent=readdir(dir);  /* skip .  */
 if(!ent)
   {PrintMessage(Warning,"Cannot read directory '%s/%s' [%!s]; not purged.",proto,host);closedir(dir);fchdir(fSpoolDir);chdir(proto);return;}
 ent=readdir(dir);  /* skip .. */

 /* Check all of the files for age, and delete as needed. */

 while((ent=readdir(dir)))
   {
    struct stat buf,buf2;

    if(stat(ent->d_name,&buf))
       ;
/*
       PrintMessage(Inform,"Cannot stat file '%s/%s/%s' [%!s]; race condition?",proto,host,ent->d_name);
*/
    else
      {
       int purge_age=purge_default_age;
       int compress_age=compress_default_age;
       time_t filetime=now;

       if(buf.st_mtime>now || buf.st_atime>now)
         {
          PrintMessage(Inform,"Cached file '%s/%s/%s' has a future timestamp; changing timestamp.",proto,host,ent->d_name);
#if DO_DELETE
          utime(ent->d_name,NULL);
#else
          PrintMessage(Debug,"utime(%s/%s/%s).",proto,host,ent->d_name);
#endif
         }

       if(*ent->d_name=='U' || *ent->d_name=='D')
         {
          int s;

          *ent->d_name^='U'^'D';
          s=stat(ent->d_name,&buf2);
          *ent->d_name^='U'^'D';

          if(s)
            {
             PrintMessage(Inform,"Cached file '%s/%s/%s' is not complete (U* and D* files); deleting it.",proto,host,ent->d_name);
             purge_age=0;compress_age=0;
            }
          else if(*ent->d_name=='U')
             continue;
          else if(purge_use_url)
             what_purge_compress_age(proto,host,ent->d_name,&purge_age,&compress_age);
          else
            {purge_age=def_purge_age;compress_age=def_compress_age;}

          if(purge_use_mtime)
             filetime=buf.st_mtime;
          else
             filetime=buf.st_atime;
         }
       else
         {
          PrintMessage(Inform,"Cached file '%s/%s/%s' is not valid (U* or D* file); deleting it.",proto,host,ent->d_name);
          purge_age=0;compress_age=0;
         }

       if(pass==2 && purge_age>0)
          purge_age=(int)(purge_age*age_scale+0.5);

       if(purge_age==-1 || (now-filetime)<(purge_age*(24*3600)))
         {
          unsigned long size;

#if USE_ZLIB
          if(compress_age!=-1 && (now-filetime)>(compress_age*(24*3600)) && *ent->d_name=='D')
            {
#if DO_COMPRESS
             *compressed+=compress_file(proto,host,ent->d_name);
#else
             PrintMessage(Debug,"compress(%s/%s/%s).",proto,host,ent->d_name);
#endif

             stat(ent->d_name,&buf);
            }
#endif

          size=Bytes_to_Blocks(buf.st_size)+Bytes_to_Blocks(buf2.st_size);

          *remain+=size;

          if(purge_age>0)
            {
             int days=(now-filetime)/(24*3600);

             days=days*purge_default_age/purge_age; /* scale the age to fit into 0 -> DefaultPurgeAge */

             if(days>purge_default_age)
                days=purge_default_age;
             blocks_by_age[days]+=size;
            }
          else
             blocks_by_age[purge_default_age+1]+=size;
         }
       else
         {
          if(!stat(ent->d_name,&buf))
             *deleted+=Bytes_to_Blocks(buf.st_size);

#if DO_DELETE
          if(unlink(ent->d_name))
             PrintMessage(Warning,"Cannot unlink file '%s/%s/%s' (1) [%!s]; race condition?",proto,host,ent->d_name);
#else
          PrintMessage(Debug,"unlink(%s/%s/%s).",proto,host,ent->d_name);
#endif

          if(*ent->d_name=='U' || *ent->d_name=='D')
            {
             *ent->d_name^='U'^'D';

             if(!stat(ent->d_name,&buf))
                *deleted+=Bytes_to_Blocks(buf.st_size);

#if DO_DELETE
             if(unlink(ent->d_name))
                PrintMessage(Warning,"Cannot unlink file '%s/%s/%s' (2) [%!s]; race condition?",proto,host,ent->d_name);
#else
             PrintMessage(Debug,"unlink(%s/%s/%s).",proto,host,ent->d_name);
#endif
            }
         }
      }
   }

 closedir(dir);
 fchdir(fSpoolDir);
 chdir(proto);
}


/*++++++++++++++++++++++++++++++++++++++
  Determine the age to use when purging and compressing a specified URL.

  char *proto The protocol to use.

  char *host The host to use.

  char *file The filename to use (if purging by URL).

  int *purge_age The age at which the file should be purged.

  int *compress_age The age at which the file should be compressed.
  ++++++++++++++++++++++++++++++++++++++*/

static void what_purge_compress_age(char *proto,char *host,char *file,int *purge_age,int *compress_age)
{
 URL *Url;

 if(purge_age)
    *purge_age=-1;

 if(compress_age)
    *compress_age=-1;

 if(file)
   {
    char *url=FileNameToURL(file);

    if(!url)
       return;

    Url=SplitURL(url);
    free(url);
   }
 else
   {
    URL fakeUrl;

    /* cheat a bit here and poke in the values we need. */

    fakeUrl.proto=proto;
    fakeUrl.host=host;
    fakeUrl.path="/";
    fakeUrl.args=NULL;

    Url=&fakeUrl;
   }

 if(purge_age)
   {
    *purge_age=ConfigIntegerURL(PurgeAges,Url);

    if(ConfigBoolean(PurgeDontGet) && ConfigBooleanMatchURL(DontGet,Url))
       *purge_age=0;

    if(ConfigBoolean(PurgeDontCache) && (ConfigBooleanMatchURL(DontCache,Url) || IsLocalNetHost(host)))
       *purge_age=0;
   }

 if(compress_age)
    *compress_age=ConfigIntegerURL(PurgeCompressAges,Url);

 if(file)
    FreeURL(Url);
}


#if USE_ZLIB
/*++++++++++++++++++++++++++++++++++++++
  Compress the named file (if not already compressed).

  unsigned long compress_file Returns the number of blocks of data compressed.

  char *proto The protocol to use.

  char *host The host to use.

  char *file The name of the file in the current directory to compress.
  ++++++++++++++++++++++++++++++++++++++*/

static unsigned long compress_file(char *proto,char *host,char *file)
{
 int ifd,ofd;
 char *zfile;
 Header *spool_head;
 char *head,buffer[READ_BUFFER_SIZE];
 int n;
 struct stat buf;
 struct utimbuf ubuf;
 long orig_size=0,new_size=0;

 if(!stat(file,&buf))
   {
    ubuf.actime=buf.st_atime;
    ubuf.modtime=buf.st_mtime;
    orig_size=buf.st_size;
   }
 else
   {
    PrintMessage(Inform,"Cannot stat file '%s/%s/%s' to compress it [%!s]; race condition?",proto,host,file);
    return(0);
   }

 ifd=open(file,O_RDONLY|O_BINARY);
 init_buffer(ifd);

 if(ifd==-1)
   {
    PrintMessage(Inform,"Cannot open file '%s/%s/%s' to compress it [%!s]; race condition?",proto,host,file);
    return(0);
   }

 ParseReply(ifd,&spool_head);

 if(!spool_head ||
    GetHeader(spool_head,"Content-Encoding") ||
    GetHeader2(spool_head,"Pragma","wwwoffle-compressed") ||
    NotCompressed(GetHeader(spool_head,"Content-Type"),NULL))
   {
    if(spool_head)
       FreeHeader(spool_head);
    close(ifd);
    utime(file,&ubuf);
    return(0);
   }

 AddToHeader(spool_head,"Content-Encoding","x-gzip");
 AddToHeader(spool_head,"Pragma","wwwoffle-compressed");
 RemoveFromHeader(spool_head,"Content-Length");

 zfile=(char*)malloc(strlen(file)+4);
 strcpy(zfile,file);
 strcat(zfile,".z");

 ofd=open(zfile,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,buf.st_mode&07777);
 init_buffer(ofd);

 if(ofd==-1)
   {
    PrintMessage(Inform,"Cannot open file '%s/%s/%s' to compress to [%!s].",proto,host,zfile);
    FreeHeader(spool_head);
    free(zfile);
    close(ifd);
    utime(file,&ubuf);
    return(0);
   }

 head=HeaderString(spool_head);
 FreeHeader(spool_head);

 write_string(ofd,head);
 free(head);

 init_zlib_buffer(ofd,-2);

 while((n=read_data(ifd,buffer,READ_BUFFER_SIZE))>0)
    write_data(ofd,buffer,n);

 finish_zlib_buffer(ofd);

 close(ofd);
 close(ifd);

 if(rename(zfile,file))
   {
    PrintMessage(Inform,"Cannot rename file '%s/%s/%s' to '%s/%s/%s' [%!s].",proto,host,zfile,proto,host,file);
    unlink(zfile);
   }

 utime(file,&ubuf);

 if(!stat(file,&buf))
    new_size=buf.st_size;

 free(zfile);

 if(new_size<orig_size)
    return(Bytes_to_Blocks(orig_size-new_size));
 else
    return(0);                  /* pathological file, larger when compressed. */
}
#endif
