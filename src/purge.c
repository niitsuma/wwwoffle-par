/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9g.
  Purge old files from the cache.
  ******************/ /******************
  Originally written by Andrew M. Bishop.
  Extensively modified by Paul A. Rombouts.

  This file Copyright 1996-2010 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2004,2005,2006,2007,2008,2009 Paul A. Rombouts
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
#include "urlhash.h"
#include "io.h"
#include "misc.h"
#include "headbody.h"
#include "errors.h"
#include "config.h"
#include "proto.h"


#ifndef O_BINARY
/*+ A work-around for needing O_BINARY with Win32 to use binary mode. +*/
#define O_BINARY 0
#endif


/* Local functions */

static void PurgeProtocol(int fd,const char *proto,/*@out@*/ unsigned long *proto_file_blocks,/*@out@*/ unsigned long *proto_del_blocks,/*@out@*/ unsigned long *proto_compress_blocks,/*@out@*/ unsigned long *proto_dirs);
static int PurgeHost(int fd,int dirfd,const char *proto,const char *hostport,/*@out@*/ unsigned long *host_file_blocks,/*@out@*/ unsigned long *host_del_blocks,/*@out@*/ unsigned long *host_compress_blocks);
static void PurgeFiles(const char *proto,const char *hostport,int def_purge_age,int def_compress_age,/*@out@*/ unsigned long *remain,/*@out@*/ unsigned long *deleted,/*@out@*/ unsigned long *compressed);
static void PurgeTemp(int fd, const char *dirname);
static void PurgeLasttime(int fd);

static void what_purge_compress_age(const char *proto,const char *hostport,/*@null@*/ const char *file,/*@out@*/ int *purge_age,/*@out@*/ int *compress_age);
#if USE_ZLIB
static unsigned long compress_file(const char *proto,const char *hostport,const char *file);
#endif
static void MarkProto(const char *proto);
static void MarkHost(const char *proto,const char *host);
static void MarkDir(const char *dirname);


/*+ Set this to 0 for debugging so that nothing is deleted. +*/
#define DO_DELETE 1

/*+ Set this to 0 for debugging so that nothing is compressed. +*/
#define DO_COMPRESS 1


/* Local variables */

/*+ The number of blocks left of each age. +*/
static unsigned long blocks_by_age[102];

/*+ The scaling factor for the ages in the second pass. +*/
static double age_scale;

/*+ The blocksize. +*/
static unsigned long blocksize;

/*+ The configuration file option (looked up once then used) for +*/
static int purge_max_size,      /*+ maximum cache size. +*/
           purge_min_free,      /*+ minimum disk free space. +*/
           purge_use_mtime,     /*+ using modification time instead of access time. +*/
           purge_use_url,       /*+ using the whole URL rather than just the host. +*/
           purge_default_age,   /*+ the default purge age. +*/
           compress_default_age; /*+ the default compression age. +*/


/*
 Note: It is not possible to count bytes in case the total size exceeds 2GB.
       Counting blocks gives a more accurate result (compared to du) on most
       systems anyway.

 Note: Since some UNIX versions (Solaris) have st_size/st_blocks different for
       different files I have reverted to dividing size in bytes by blocksize
       instead of st_blocks for counting the number of blocks in a file.

 Note: The variable called blocksize can be only 512, 1024 or 2048, it is used
       for rounding up file sizes only.

 Note: To minimise the 'future timestamp' warnings we check if file times are
       more than one hour after the current time at the start of processing each
       directory.

 Note: To avoid deleting temporary files still in use we check if file times are
       more than one hour before the current time at the start of processing the
       outgoing directory.
*/

 /*+ Convert blocks to kilobytes. +*/
 /* Handle this carefully to avoid overflows from doing blocks*blocksize/1024
                                or any errors from doing (blocks/1024)*blocksize */
#define Blocks_to_kB(blocks) \
 (unsigned long)((blocksize==512)?((blocks)/2): \
                 ((blocksize==1024)?(blocks): \
                  (blocks)*2))

 /*+ Convert blocks to Megabytes. +*/

#define Blocks_to_MB(blocks) \
 (unsigned long)((blocksize==512)?((blocks)/2048): \
                 ((blocksize==1024)?((blocks)/1024): \
                  ((blocks)/512)))

 /*+ Convert bytes to blocks. +*/
 /* Divide, rounding up if needed. */

#define Bytes_to_Blocks(bytes) \
  ((unsigned long)((bytes+(blocksize-1))/blocksize))


/*++++++++++++++++++++++++++++++++++++++
  Purge files from the cache that meet the age criteria.

  int fd the file descriptor of the wwwoffle client.
  ++++++++++++++++++++++++++++++++++++++*/

int PurgeCache(int fd)
{
 int i,p;
 struct STATFS sbuf;
 struct stat buf;
 unsigned long total_file_blocks,total_del_blocks,total_compress_blocks,total_dirs;

 /* Get the applicable configuration file entries */

 purge_max_size=ConfigInteger(PurgeMaxSize);
 purge_min_free=ConfigInteger(PurgeMinFree);
 purge_use_mtime=ConfigBoolean(PurgeUseMTime);
 purge_use_url=ConfigBoolean(PurgeUseURL);
 what_purge_compress_age("*","*",NULL,&purge_default_age,&compress_default_age);

 /* Set up the age scaling parameters */

 age_scale=-1;

 for(i=0;i<=101;i++)
    blocks_by_age[i]=0;

 /* Print a message about what we are going to do. */

 write_string(fd,"\n");

 write_string(fd,"Purge Parameters:\n");

 if(purge_use_mtime)
    write_string(fd,"  Using modification time (use-mtime=yes).\n");
 else
    write_string(fd,"  Using last access time (use-mtime=no).\n");

 if(purge_use_url)
    write_string(fd,"  Using the full URL (use-url=yes).\n");
 else
    write_string(fd,"  Using the hostname and protocol only (use-url=no).\n");

 if(purge_max_size>0)
    write_formatted(fd,"  Maximum cache size is %d MB (max-size=%d).\n",purge_max_size,purge_max_size);
 else
    write_string(fd,"  No limit on cache size (max-size=-1).\n");

 if(purge_min_free>0)
    write_formatted(fd,"  Minimum free disk space is %d MB (min-free=%d).\n",purge_min_free,purge_min_free);
 else
    write_string(fd,"  No limit on minimum free disk space (min-free=-1).\n");

 if(purge_default_age>0)
    write_formatted(fd,"  Default purge age is %d days (age=%d).\n",purge_default_age,purge_default_age);
 else if(purge_default_age==0)
    write_string(fd,"  Default is to delete all pages (age=0).\n");
 else
    write_string(fd,"  Default is not to purge any pages (age=-1).\n");

#if USE_ZLIB
 if(compress_default_age>0)
    write_formatted(fd,"  Default compress age is %d days (compress-age=%d).\n",compress_default_age,compress_default_age);
 else if(compress_default_age==0)
    write_string(fd,"  Default is to compress all pages (compress-age=0).\n");
 else
    write_string(fd,"  Default is not to compress any pages (compress-age=-1).\n");
#endif

 write_string(fd,"\n");

 /* Find out the blocksize of the disk allocation if we can.
    It isn't easy since some operating systems lie. */

 write_string(fd,"Disk block size (used for measuring cache size):\n");

 if(stat(".",&buf) || buf.st_size==-1 || buf.st_blocks==-1)
   {
    PrintMessage(Warning,"Cannot determine the disk block size [%!s]; using 1024 for block size.");
    write_string(fd,"  Cannot determine the disk block size; using 1024 for block size.\n");
    blocksize=1024;
   }
 else if(buf.st_blocks==0)
   {
    PrintMessage(Warning,"The number of blocks for directory entry is zero; using 1024 for block size.");
    write_string(fd,"  The number of blocks for directory entry is zero; using 1024 for block size.\n");
    blocksize=1024;
   }
 else
   {
    blocksize=buf.st_size/buf.st_blocks;

    if(blocksize!=512 && blocksize!=1024 && blocksize!=2048)
      {
       PrintMessage(Warning,"The blocksize (%d) looks wrong; using 1024 for block size.",blocksize);
       write_formatted(fd,"  The blocksize (%ld) looks wrong; using 1024 for block size.\n",blocksize);
       blocksize=1024;
      }
    else
      {
       PrintMessage(Debug,"The disk block size appears to be %d.",blocksize);
       write_formatted(fd,"  The disk block size appears to be %ld.\n",blocksize);
      }
   }

 write_string(fd,"\n----------------------------------------\n\n");

 /* Print a message about what we are going to do. */

 if(purge_max_size>0 || purge_min_free>0)
    write_string(fd,"Pass 1: ");
#if USE_ZLIB
 write_string(fd,"Purging and compressing files using specified ages.\n");
#else
 write_string(fd,"Purging files using specified ages.\n");
#endif

 write_string(fd,"\n");

 /* Check for each protocol subdirectory. */

 total_file_blocks=0,total_del_blocks=0,total_compress_blocks=0,total_dirs=0;

 for(p=0;p<NProtocols;p++)
   {
    unsigned long proto_file_blocks,proto_del_blocks,proto_compress_blocks,proto_dirs;

    PurgeProtocol(fd,Protocols[p].name,&proto_file_blocks,&proto_del_blocks,&proto_compress_blocks,&proto_dirs);

    total_file_blocks+=proto_file_blocks;
    total_del_blocks+=proto_del_blocks;
    total_compress_blocks+=proto_compress_blocks;
    total_dirs+=proto_dirs;
   }

 /* Print out a message about the total files */

#if USE_ZLIB
 write_formatted(fd,"Total of %lu directories ; %lu MB (%lu MB deleted) (%lu MB compressed)\n",total_dirs,
                 Blocks_to_MB(total_file_blocks),Blocks_to_MB(total_del_blocks),Blocks_to_MB(total_compress_blocks));
#else
 write_formatted(fd,"Total of %lu directories ; %lu MB (%lu MB deleted)\n",total_dirs,
                 Blocks_to_MB(total_file_blocks),Blocks_to_MB(total_del_blocks));
#endif

 write_string(fd,"\n");

 /* Purge the tmp.* files in outgoing and temp. */

 PurgeTemp(fd,"outgoing");
 PurgeTemp(fd,"temp");

 /* Purge the files in lasttime that have been purged from elsewhere in the cache. */

 PurgeLasttime(fd);

 /* Determine the age to purge with in the second pass */

 if(purge_max_size>0 || purge_min_free>0)
   {
    int age_for_size=-1,age_for_free=-1;
    unsigned long age_blocks_used=blocks_by_age[101];
    unsigned long age_blocks_free=total_file_blocks-blocks_by_age[101];
    unsigned long diskfree;

    write_string(fd,"----------------------------------------\n\n");

    /* Calculate the number of blocksize blocks there are free on the disk.
       Use statfs() to get the 'df' info of disk block size and free disk blocks. */

    write_string(fd,"Disk free space:\n");

    if(STATFS(".",&sbuf) || sbuf.f_bsize==~0 || sbuf.f_bavail==~0)
      {
       PrintMessage(Warning,"Cannot determine the disk free space [%!s]; assuming 0.");
       write_string(fd,"  Cannot determine the disk free space; assuming 0.\n");
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

       write_formatted(fd,"  The disk free space appears to be %lu blocks (%lu MB).\n",diskfree,Blocks_to_MB(diskfree));
      }

    age_blocks_free+=diskfree;

    write_string(fd,"\n");

    /* Print out the statistics. */

    write_string(fd,"Age Profile of cached pages:\n"
		 "  All ages scaled to the range 0% -> 100% of specified age\n"
		 "\n");
    write_formatted(fd,"Total can not be purged; %4lu MB (%5lu MB free)\n\n",
                    Blocks_to_MB(age_blocks_used),Blocks_to_MB(age_blocks_free));

    for(i=0;i<=101;i++)
      {
       age_blocks_used+=blocks_by_age[i];
       age_blocks_free-=blocks_by_age[i];

       if(purge_max_size>0 && age_for_size<0 &&
          Blocks_to_kB(age_blocks_used)>(1024*(unsigned long)purge_max_size))
         {
          age_for_size=i;

          write_formatted(fd,"Limit is %d%% of age for %d MB cache size\n",age_for_size,purge_max_size);
         }

       if(purge_min_free>0 && diskfree && age_for_free<0 &&
          Blocks_to_kB(age_blocks_free)<(1024*(unsigned long)purge_min_free))
         {
          age_for_free=i;

          write_formatted(fd,"Limit is %d%% of age for %d MB disk free\n",age_for_free,purge_min_free);
         }

       if(i==101)
          write_formatted(fd,"Total all ages         ; %4lu MB (%5lu MB free)\n",
                          Blocks_to_MB(age_blocks_used),
                          diskfree?Blocks_to_MB(age_blocks_free):0);
       else
          write_formatted(fd,"Newer than %3d%% of age ; %4lu MB (%5lu MB free)\n",i,
                          Blocks_to_MB(age_blocks_used),
                          diskfree?Blocks_to_MB(age_blocks_free):0);
      }

    write_string(fd,"\n");

    /* Decide the age scaling */

    if(age_for_size!=-1 && (age_for_size<=age_for_free || age_for_free==-1))
       age_scale=(double)age_for_size/100;
    else if(age_for_free!=-1 && (age_for_free<age_for_size || age_for_size==-1))
       age_scale=(double)age_for_free/100;
    else
       age_scale=-1;

    if(age_scale==-1)
       write_string(fd,"Requirements of min-free and/or max-size are both OK, no more to be purged.\n");
    else
       write_formatted(fd,"Requirements of min-free and/or max-size need ages to be scaled to %.0f%% and purge repeated.\n",age_scale*100);

    write_string(fd,"\n");
   }

 /* Perform the second pass with an age scaling. */

 if(age_scale!=-1)
   {
    total_file_blocks=0;
    total_del_blocks=0;
    total_compress_blocks=0;
    total_dirs=0;

    write_string(fd,"----------------------------------------\n\n");

    write_formatted(fd,"Pass 2: Purging files using %.0f%% of specified ages.\n\n",age_scale*100);

    /* Check for each protocol subdirectory. */

    total_file_blocks=0,total_del_blocks=0,total_compress_blocks=0,total_dirs=0;

    for(p=0;p<NProtocols;p++)
      {
       unsigned long proto_file_blocks,proto_del_blocks,proto_compress_blocks,proto_dirs;

       PurgeProtocol(fd,Protocols[p].name,&proto_file_blocks,&proto_del_blocks,&proto_compress_blocks,&proto_dirs);

       total_file_blocks+=proto_file_blocks;
       total_del_blocks+=proto_del_blocks;
       total_compress_blocks+=proto_compress_blocks;
       total_dirs+=proto_dirs;
      }

    /* Print out a message about the total files */

#if USE_ZLIB
    write_formatted(fd,"Total of %lu directories ; %lu MB (%lu MB deleted) (%lu MB compressed)\n",total_dirs,
                    Blocks_to_MB(total_file_blocks),Blocks_to_MB(total_del_blocks),Blocks_to_MB(total_compress_blocks));
#else
    write_formatted(fd,"Total of %lu directories ; %lu MB (%lu MB deleted)\n",total_dirs,
                    Blocks_to_MB(total_file_blocks),Blocks_to_MB(total_del_blocks));
#endif

    write_string(fd,"\n\n");

    /* Purge the files in lasttime that have been purged from elsewhere in the cache. */

    PurgeLasttime(fd);
   }

 /* Mark the hashes of the remaining spool files so that we can clean up the url hash table. */

 if(!urlhash_clearmarks()) {
   PrintMessage(Warning,"Clearing of the marks in the url hash table failed; cannot compact the table.");
   return 0;
 }

 for(p=0;p<NProtocols;p++)
   {
    char *proto=Protocols[p].name;

    if(stat(proto,&buf))
       PrintMessage(Inform,"Cannot stat directory '%s' [%!s]; not marked.",proto);
    else
      {
       if(chdir(proto))
          PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not marked.",proto);
       else
         {
          MarkProto(proto);

          ChangeBackToSpoolDir();
         }
      }
   }

#define nspecial 6
 for(i=0;i<nspecial;i++)
   {
    static const char *const special[nspecial]={"monitor","outgoing","lastout","prevout","lasttime","prevtime"};
    const char *dirname=special[i];
    int j,n=0;

    if(!strcmp_litbeg(special[i],"prev")) n=NUM_PREVTIME_DIR;

    for(j=1; j<=(n?n:1); ++j) {
      char dbuf[sizeof("prevtime")+MAX_INT_STR];

      if(n) {
	sprintf(dbuf,"%s%u",special[i],(unsigned)j);
	dirname=dbuf;
      }

      if(stat(dirname,&buf))
	PrintMessage(Inform,"Cannot stat directory '%s' [%!s]; not marked.",dirname);
      else
	{
	  if(chdir(dirname))
	    PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not marked.",dirname);
	  else
	    {
	      MarkDir(dirname);

	      ChangeBackToSpoolDir();
	    }
	}
    }
   }

 return 1;
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a protocol directory and check all hosts.

  int fd The file descriptor to write the purge messages to.

  const char *proto The name of the protocol directory to purge.

  unsigned long *proto_file_blocks Returns the total number of file and directory blocks.

  unsigned long *proto_del_blocks Returns the total number of deleted blocks.

  unsigned long *proto_compress_blocks Returns the total number of compressed blocks.

  unsigned long *proto_dirs Returns the total number of directories.
  ++++++++++++++++++++++++++++++++++++++*/

static void PurgeProtocol(int fd,const char *proto,unsigned long *proto_file_blocks,unsigned long *proto_del_blocks,unsigned long *proto_compress_blocks,unsigned long *proto_dirs)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;
 unsigned long dir_blocks;

 *proto_file_blocks=0;
 *proto_del_blocks=0;
 *proto_compress_blocks=0;
 *proto_dirs=0;

 /* Change to the spool directory and open it. */

 if(stat(proto,&buf))
   {PrintMessage(Inform,"Cannot stat directory '%s' [%!s]; not purged.",proto);return;}

 dir_blocks=Bytes_to_Blocks(buf.st_size);

 *proto_file_blocks+=dir_blocks;
 blocks_by_age[101]+=dir_blocks;

 if(chdir(proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not purged.",proto);return;}

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory '%s' [%!s]; not purged.",proto);ChangeBackToSpoolDir();return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read directory '%s' [%!s]; not purged.",proto);closedir(dir);ChangeBackToSpoolDir();return;}

 /* Search through all of the host directories. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(stat(ent->d_name,&buf))
       PrintMessage(Inform,"Cannot stat directory '%s/%s' [%!s]; race condition?",proto,ent->d_name);
    else if(S_ISDIR(buf.st_mode))
      {
       unsigned long host_file_blocks=0,host_del_blocks=0,host_compress_blocks=0;

       if(PurgeHost(fd,dirfd(dir),proto,ent->d_name,&host_file_blocks,&host_del_blocks,&host_compress_blocks)<0)
	 break;

       *proto_file_blocks+=host_file_blocks;
       *proto_del_blocks+=host_del_blocks;
       *proto_compress_blocks+=host_compress_blocks;
       if(host_file_blocks) (*proto_dirs)++;
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
 while((ent=readdir(dir)));

 closedir(dir);
 ChangeBackToSpoolDir();

 /* Print a message for the protocol directory. */

 write_string(fd,"\n");
#if USE_ZLIB
 write_formatted(fd,"  Protocol %s has %lu directories ; %lu MB (%lu MB deleted) (%lu MB compressed)\n",proto,*proto_dirs,
                 Blocks_to_MB(*proto_file_blocks),Blocks_to_MB(*proto_del_blocks),Blocks_to_MB(*proto_compress_blocks));
#else
 write_formatted(fd,"  Protocol %s has %lu directories ; %lu MB (%lu MB deleted)\n",proto,*proto_dirs,
                 Blocks_to_MB(*proto_file_blocks),Blocks_to_MB(*proto_del_blocks));
#endif
 write_string(fd,"\n");
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a host directory and check all files.

  int fd The file descriptor to write the purge messages to.

  int dirfd The file descriptor of the protocol directory.

  const char *proto The name of the protocol directory to purge.

  const char *hostport The name of the host:port directory to purge.

  unsigned long *host_file_blocks Returns the total number of file and directory blocks.

  unsigned long *host_del_blocks Returns the total number of deleted blocks.

  unsigned long *host_compress_blocks Returns the total number of compressed blocks.
  ++++++++++++++++++++++++++++++++++++++*/

static int PurgeHost(int fd,int dirfd,const char *proto,const char *hostport,unsigned long *host_file_blocks,unsigned long *host_del_blocks,unsigned long *host_compress_blocks)
{
 int purge_age,compress_age;
 struct stat buf;
 unsigned long dir_blocks;

 /* Change to the host directory. */

 if(stat(hostport,&buf))
   {PrintMessage(Inform,"Cannot stat directory '%s/%s' [%!s]; not purged.",proto,hostport);return 1;}

 if(chdir(hostport))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s]; not purged.",proto,hostport);return 1;}

 dir_blocks=Bytes_to_Blocks(buf.st_size);

 /* Get the purge and compress ages */

 what_purge_compress_age(proto,hostport,NULL,&purge_age,&compress_age);

 /* Purge or compress the files */

 PurgeFiles(proto,hostport,purge_age,compress_age,host_file_blocks,host_del_blocks,host_compress_blocks);

 /* Check if the directory can be deleted */

 if(fchdir(dirfd) && (ChangeBackToSpoolDir(), chdir(proto)))
   {PrintMessage(Warning,"Cannot change back to protocol directory '%s' [%!s].",proto);return -1;}

 if(*host_file_blocks==0)
   {
#if DO_DELETE
    if(rmdir(hostport))
       PrintMessage(Warning,"Cannot delete what should be an empty directory '%s/%s' [%!s].",proto,hostport);
#else
    PrintMessage(Debug,"rmdir(%s/%s).",proto,hostport);
#endif

    *host_del_blocks+=dir_blocks;
   }
 else
   {
    struct utimbuf utbuf;

    utbuf.actime=buf.st_atime;
    utbuf.modtime=buf.st_mtime;
    utime(hostport,&utbuf);

    *host_file_blocks+=dir_blocks;
    blocks_by_age[101]+=dir_blocks;
   }

 /* Print a message for the host directory. */

 if(purge_use_url)
   {
    if(*host_file_blocks==0)
       write_formatted(fd,"    Purged %6s://%-40s ; empty    (%4lu kB del)\n",proto,hostport,
                       Blocks_to_kB(*host_del_blocks));
    else
      {
#if USE_ZLIB
       if(*host_compress_blocks)
         {
          if(*host_del_blocks)
             write_formatted(fd,"    Purged %6s://%-40s ; %5lu kB (%4lu kB del) (%4lu kB compr)\n",proto,hostport,
                             Blocks_to_kB(*host_file_blocks),Blocks_to_kB(*host_del_blocks),Blocks_to_kB(*host_compress_blocks));
          else
             write_formatted(fd,"    Purged %6s://%-40s ; %5lu kB               (%4lu kB compr)\n",proto,hostport,
                             Blocks_to_kB(*host_file_blocks),Blocks_to_kB(*host_compress_blocks));
         }
       else
#endif
         {
          if(*host_del_blocks)
             write_formatted(fd,"    Purged %6s://%-40s ; %5lu kB (%4lu kB del)\n",proto,hostport,
                             Blocks_to_kB(*host_file_blocks),Blocks_to_kB(*host_del_blocks));
          else
             write_formatted(fd,"    Purged %6s://%-40s ; %5lu kB\n",proto,hostport,
                             Blocks_to_kB(*host_file_blocks));
         }
      }
   }
 else
   {
    if(age_scale!=-1 && purge_age>=0)
       purge_age=(int)(purge_age*age_scale+0.5);

    if(purge_age<0)
      {
#if USE_ZLIB
       if(*host_compress_blocks)
          write_formatted(fd,"    Not Purged       %6s://%-40s ; %4lu kB               (%4lu kB compr)\n",proto,hostport,
                          Blocks_to_kB(*host_file_blocks),Blocks_to_kB(*host_compress_blocks));
       else
#endif
          write_formatted(fd,"    Not Purged       %6s://%-40s ; %4lu kB\n",proto,hostport,
                          Blocks_to_kB(*host_file_blocks));
      }
    else if(*host_file_blocks==0)
       write_formatted(fd,"    Purged (%2d days) %6s://%-40s ; empty    (%4lu kB del)\n",purge_age,proto,hostport,
                       Blocks_to_kB(*host_del_blocks));
    else
      {
#if USE_ZLIB
       if(*host_compress_blocks)
         {
          if(*host_del_blocks)
             write_formatted(fd,"    Purged (%2d days) %6s://%-40s ; %5lu kB (%4lu kB del) (%4lu kB compr)\n",purge_age,proto,hostport,
                             Blocks_to_kB(*host_file_blocks),Blocks_to_kB(*host_del_blocks),Blocks_to_kB(*host_compress_blocks));
          else
             write_formatted(fd,"    Purged (%2d days) %6s://%-40s ; %5lu kB               (%4lu kB compr)\n",purge_age,proto,hostport,
                             Blocks_to_kB(*host_file_blocks),Blocks_to_kB(*host_compress_blocks));
         }
       else
#endif
         {
          if(*host_del_blocks)
             write_formatted(fd,"    Purged (%2d days) %6s://%-40s ; %5lu kB (%4lu kB del)\n",purge_age,proto,hostport,
                             Blocks_to_kB(*host_file_blocks),Blocks_to_kB(*host_del_blocks));
          else
             write_formatted(fd,"    Purged (%2d days) %6s://%-40s ; %5lu kB\n",purge_age,proto,hostport,
                             Blocks_to_kB(*host_file_blocks));
         }
      }
   }

 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Delete the files in the specified directory that are older than the specified age.

  const char *proto The name of the protocol directory to purge.

  const char *hostport The name of the host:port directory to purge.

  int def_purge_age The default purge age to use for this host.

  int def_compress_age The default compress age to use for this host.

  unsigned long *remain Returns the number of blocks in files that are left.

  unsigned long *deleted Returns the number of blocks in files that were deleted.

  unsigned long *compressed Returns the number of blocks in files that were compressed.
  ++++++++++++++++++++++++++++++++++++++*/

static void PurgeFiles(const char *proto,const char *hostport,int def_purge_age,int def_compress_age,unsigned long *remain,unsigned long *deleted,unsigned long *compressed)
{
 DIR *dir;
 struct dirent* ent;
 time_t now=time(NULL)+3600; /* Allow one hour margin on times */

 *remain=0;
 *deleted=0;
 *compressed=0;

 /* Open the spool subdirectory. */

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory '%s/%s' [%!s]; not purged.",proto,hostport);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read directory '%s/%s' [%!s]; not purged.",proto,hostport);goto closedir_return;}

 /* Check all of the files for age, and delete or compress as needed. */

 do
   {
    struct stat buf;

    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(stat(ent->d_name,&buf))
       ;
/*
       PrintMessage(Inform,"Cannot stat file '%s/%s/%s' [%!s]; race condition?",proto,hostport,ent->d_name);
*/
    else
      {
       int purge_age=purge_default_age;
       int compress_age=compress_default_age;
       time_t filetime=now;

       /* Fix future timestamps. */

       if(buf.st_mtime>now || buf.st_atime>now)
         {
          PrintMessage(Inform,"Cached file '%s/%s/%s' has a future timestamp; changing timestamp.",proto,hostport,ent->d_name);
#if DO_DELETE
          utime(ent->d_name,NULL);
#else
          PrintMessage(Debug,"utime(%s/%s/%s).",proto,hostport,ent->d_name);
#endif
         }

       if(*ent->d_name=='D')
         {
          if(purge_use_url)
             what_purge_compress_age(proto,hostport,ent->d_name,&purge_age,&compress_age);
          else
            {purge_age=def_purge_age;compress_age=def_compress_age;}

          if(purge_use_mtime)
             filetime=buf.st_mtime;
          else
             filetime=buf.st_atime;
         }
       else
         {
          PrintMessage(Inform,"Cached file '%s/%s/%s' is not valid (D* file); deleting it.",proto,hostport,ent->d_name);
          purge_age=0;compress_age=0;
         }

       if(age_scale!=-1 && purge_age>0)
          purge_age=(int)(purge_age*age_scale+0.5);

       if(purge_age==-1 || (now-filetime)<(purge_age*(24*3600)))
         {
          unsigned long size;

#if USE_ZLIB
          if(compress_age!=-1 && (now-filetime)>(compress_age*(24*3600)) && *ent->d_name=='D')
            {
#if DO_COMPRESS
             *compressed+=compress_file(proto,hostport,ent->d_name);
#else
             PrintMessage(Debug,"compress(%s/%s/%s).",proto,hostport,ent->d_name);
#endif

             stat(ent->d_name,&buf);
            }
#endif

          size=Bytes_to_Blocks(buf.st_size);

          *remain+=size;

          if(purge_age>0)
            {
             int days=(now-filetime)/(24*3600);

             days=days*100/purge_age+0.5; /* scale the age to fit into 0 -> 100% */

             blocks_by_age[days]+=size;
            }
          else
             blocks_by_age[101]+=size;
         }
       else
         {
          if(!stat(ent->d_name,&buf))
             *deleted+=Bytes_to_Blocks(buf.st_size);

#if DO_DELETE
          if(unlink(ent->d_name))
             PrintMessage(Warning,"Cannot unlink file '%s/%s/%s' (1) [%!s]; race condition?",proto,hostport,ent->d_name);
#else
          PrintMessage(Debug,"unlink(%s/%s/%s).",proto,hostport,ent->d_name);
#endif

         }
      }
   }
 while((ent=readdir(dir)));

closedir_return:
 closedir(dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a directory and check for temporary files.

  int fd The file descriptor to write the purge messages to.

  const char *dirname  The name of the directory.
  ++++++++++++++++++++++++++++++++++++++*/

static void PurgeTemp(int fd, const char *dirname)
{
 time_t now=time(NULL)-3600; /* Allow one hour margin on times */

 if(chdir(dirname))
   PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not purged.",dirname);
 else
   {
    DIR *dir;
    struct dirent* ent;
    unsigned int count=0;

    dir=opendir(".");
    if(!dir)
      PrintMessage(Warning,"Cannot open directory '%s' [%!s]; not purged.",dirname);
    else
      {
       ent=readdir(dir);
       if(!ent)
	 PrintMessage(Warning,"Cannot read directory '%s' [%!s]; not purged.",dirname);
       else
         {
          do
            {
             if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
                continue; /* skip . & .. */

             if(!strcmp_litbeg(ent->d_name,"tmp."))
               {
		struct stat buf;

                if(!stat(ent->d_name,&buf) && buf.st_mtime<now)
                  {
#if DO_DELETE
                   if(unlink(ent->d_name))
		     PrintMessage(Warning,"Cannot unlink file '%s/%s' [%!s]; race condition?",dirname,ent->d_name);
#else
                   PrintMessage(Debug,"unlink(%s/%s).",dirname,ent->d_name);
#endif
                   count++;
                  }
               }
            }
          while((ent=readdir(dir)));
         }

       closedir(dir);
      }

    if(count)
      write_formatted(fd,"\nDeleted %u temporary files from directory '%s'\n\n",count,dirname);
   }

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Search through the lasttime and prevtime directories and check for files that have been deleted from the main cache.

  int fd The file descriptor to write the purge messages to.
  ++++++++++++++++++++++++++++++++++++++*/

static void PurgeLasttime(int fd)
{
 int i;

 for(i=0;i<=NUM_PREVTIME_DIR;i++)
   {
    char lasttime[sizeof("prevtime")+MAX_INT_STR];

    if(i)
      sprintf(lasttime,"prevtime%u",(unsigned)i);
    else
       strcpy(lasttime,"lasttime");

    if(chdir(lasttime))
       PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not purged.",lasttime);
    else
      {
       DIR *dir;
       struct dirent* ent;
       unsigned int count=0;

       dir=opendir(".");
       if(!dir)
          PrintMessage(Warning,"Cannot open directory '%s' [%!s]; not purged.",lasttime);
       else
         {
          ent=readdir(dir);
          if(!ent)
             PrintMessage(Warning,"Cannot read directory '%s' [%!s]; not purged.",lasttime);
          else
            {
             do
               {
                if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
                   continue; /* skip . & .. */

                if(*ent->d_name=='D')
                  {
                   URL *Url;
                   int delete;

                   Url=FileNameToURL(ent->d_name); /* must be in lasttime dir */

                   ChangeBackToSpoolDir();

                   delete = (Url && !ExistsWebpageSpoolFile(Url,0)); /* must be in SpoolDir dir */

		   if(Url)
		     FreeURL(Url);

		   if(chdir(lasttime))
		     {PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; directory disappeared during purging!",lasttime);break;}

                   if(delete) /* must be in lasttime dir */
                     {
#if DO_DELETE
		      if(unlink(ent->d_name))
			PrintMessage(Warning,"Cannot unlink lasttime file '%s/%s' [%!s].",lasttime,ent->d_name);
		      else
			count++;
#else
		      PrintMessage(Debug,"unlink(%s/%s).",lasttime,ent->d_name);
#endif
                     }
                  }
               }
             while((ent=readdir(dir)));
            }

          closedir(dir);
         }

       if(count)
         {
          write_formatted(fd,"Deleted %u URLs from directory '%s' that are not in main cache.\n\n",count,lasttime);
         }
      }

    ChangeBackToSpoolDir();
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Determine the age to use when purging and compressing a specified URL.

  const char *proto The protocol directory to purge.

  const char *hostport The host and port number directory to purge.

  const char *file The filename to use (if purging by URL).

  int *purge_age Returns the age at which the file should be purged.

  int *compress_age Returns the age at which the file should be compressed.
  ++++++++++++++++++++++++++++++++++++++*/

static void what_purge_compress_age(const char *proto,const char *hostport,const char *file,int *purge_age,int *compress_age)
{
 URL *Url;

 *purge_age=-1;
 *compress_age=-1;

 /* Create a URL for the file or the host */

 if(file)
   {
    Url=FileNameToURL(file);

    if(!Url)
       return;
   }
 else
    Url=CreateURL(proto,hostport,"/",NULL,NULL,NULL);

 /* Choose the purge age. */

 if((ConfigBoolean(PurgeDontGet) && ConfigBooleanMatchURL(DontGet,Url)) ||
    (ConfigBoolean(PurgeDontCache) && (ConfigBooleanMatchURL(DontCache,Url) || IsLocalNetHost(Url->host))))
   *purge_age=0;
 else
   *purge_age=ConfigIntegerURL(PurgeAges,Url);

 /* Choose the compress age. */

 *compress_age=ConfigIntegerURL(PurgeCompressAges,Url);

 FreeURL(Url);
}


#if USE_ZLIB
/*++++++++++++++++++++++++++++++++++++++
  Compress the named file (if not already compressed).

  unsigned long compress_file Returns the number of blocks of data compressed.

  const char *proto The protocol directory to compress.

  const char *hostport The host and port number directory to compress.

  const char *file The name of the file in the current directory to compress.
  ++++++++++++++++++++++++++++++++++++++*/

static unsigned long compress_file(const char *proto,const char *hostport,const char *file)
{
 int ifd,ofd;
 char *zfile;
 Header *spool_head;
 struct stat buf;
 struct utimbuf ubuf;
 long orig_size=0,new_size=0;

 /* Get the file size and timestamps */

 if(!stat(file,&buf))
   {
    ubuf.actime=buf.st_atime;
    ubuf.modtime=buf.st_mtime;
    orig_size=buf.st_size;
   }
 else
   {
    PrintMessage(Inform,"Cannot stat file '%s/%s/%s' to compress it [%!s]; race condition?",proto,hostport,file);
    return(0);
   }

 /* Open the file */

 ifd=open(file,O_RDONLY|O_BINARY);

 if(ifd==-1)
   {
    PrintMessage(Inform,"Cannot open file '%s/%s/%s' to compress it [%!s]; race condition?",proto,hostport,file);
    return(0);
   }

 init_io(ifd);

 /* Read the header to decide if it can be compressed. */

 ParseReply(ifd,&spool_head);

 if(!spool_head ||
    GetHeader(spool_head,"Content-Encoding") ||
    GetHeader2(spool_head,"Pragma","wwwoffle-compressed") ||
    NotCompressed(GetHeader(spool_head,"Content-Type"),NULL))
   {
    if(spool_head)
       FreeHeader(spool_head);

    finish_io(ifd);
    close(ifd);

    utime(file,&ubuf);
    return(0);
   }

 /* Add the headers to say it is compressed. */

 AddToHeader(spool_head,"Content-Encoding","x-gzip");
 AddToHeaderCombined(spool_head,"Pragma","wwwoffle-compressed");
 RemoveFromHeader(spool_head,"Content-Length");

 /* Create a new file to write to. */

 zfile=(char*)alloca(strlen(file)+sizeof(".z"));
 stpcpy(stpcpy(zfile,file),".z");

 ofd=open(zfile,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,buf.st_mode&07777);

 if(ofd==-1)
   {
    PrintMessage(Inform,"Cannot open file '%s/%s/%s' to compress to [%!s].",proto,hostport,zfile);

    FreeHeader(spool_head);

    finish_io(ifd);
    close(ifd);

    utime(file,&ubuf);
    return(0);
   }

 init_io(ofd);

 /* Write the header to the new file. */

 {
   size_t headlen;
   char *head=HeaderString(spool_head,&headlen);
   FreeHeader(spool_head);

   write_data(ofd,head,headlen);
   free(head);
 }

 /* Enable compression and write the data to the new file. */

 configure_io_zlib(ofd,-1,2);

 {
    ssize_t n;
    char buffer[IO_BUFFER_SIZE];

    while((n=read_data(ifd,buffer,IO_BUFFER_SIZE))>0)
      write_data(ofd,buffer,n);
 }

 finish_io(ofd);
 close(ofd);

 finish_io(ifd);
 close(ifd);

 /* Rename the file back to the original location. */

 if(rename(zfile,file))
   {
    PrintMessage(Inform,"Cannot rename file '%s/%s/%s' to '%s/%s/%s' [%!s].",proto,hostport,zfile,proto,hostport,file);
    unlink(zfile);
   }

 /* Reset the timestamps. */

 utime(file,&ubuf);

 /* Calculate the amount of bytes compressed. */

 if(!stat(file,&buf))
    new_size=buf.st_size;

 if(new_size<orig_size)
    return(Bytes_to_Blocks(orig_size-new_size));
 else
    return(0);                  /* pathological file, larger when compressed. */
}
#endif


/*++++++++++++++++++++++++++++++++++++++
  Mark the hashes of all the files in a complete protocol directory.

  char *proto The protocol of the spool directory we are in.
  ++++++++++++++++++++++++++++++++++++++*/

static void MarkProto(const char *proto)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;

 /* Open the spool directory. */

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory '%s' [%!s]; not marked.",proto);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read directory '%s' [%!s]; not marked.",proto);goto close_return;}

 /* Go through each entry. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(stat(ent->d_name,&buf))
       PrintMessage(Warning,"Cannot stat file '%s/%s' [%!s]; not marked.",proto,ent->d_name);
    else if(S_ISDIR(buf.st_mode))
      {
       if(chdir(ent->d_name))
          PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s]; not marked.",proto,ent->d_name);
       else
         {
          MarkHost(proto,ent->d_name);

          if(fchdir(dirfd(dir)) && (ChangeBackToSpoolDir(), chdir(proto))) {
	    PrintMessage(Warning,"Cannot change back to protocol directory '%s' [%!s].",proto);
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
  Mark the hashes of all the files in a host directory.

  char *proto The protocol of the spool directory we are in.

  char *host The hostname of the spool directory we are in.
  ++++++++++++++++++++++++++++++++++++++*/

static void MarkHost(const char *proto,const char *host)
{
  char dir[strlen(proto)+strlen(host)+sizeof("/")];

  sprintf(dir,"%s/%s",proto,host);
  MarkDir(dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Mark the hashes of all the files in a directory.

  char *dir The name of the directory.
  ++++++++++++++++++++++++++++++++++++++*/

static void MarkDir(const char *dirname)
{
 DIR *dir;
 struct dirent* ent;
 struct stat buf;

 /* Open the spool directory. */

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory '%s' [%!s]; not marked.",dirname);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read directory '%s' [%!s]; not marked.",dirname);goto close_return;}

 /* Go through each entry. */

 do
   {
    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

    if(!stat(ent->d_name,&buf) && S_ISREG(buf.st_mode) &&
       (*ent->d_name=='D' || *ent->d_name=='O' || *ent->d_name=='M'))
      {
	FileMarkHash(ent->d_name);
      }
   }
 while((ent=readdir(dir)));

close_return:
 closedir(dir);
}
