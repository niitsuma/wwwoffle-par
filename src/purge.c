/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/purge.c 2.63 2004/02/14 14:03:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Purge old files from the cache.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1996,97,98,99,2000,01,02,03,04 Andrew M. Bishop
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


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif


/* Local functions */

static void PurgeFiles(char *proto,char *host,int def_purge_age,int def_compress_age,/*@out@*/ unsigned long *remain,/*@out@*/ unsigned long *deleted,/*@out@*/ unsigned long *compressed);
static void what_purge_compress_age(char *proto,char *hostport,/*@null@*/ char *file,/*@out@*/ int *purge_age,/*@out@*/ int *compress_age);
#if USE_ZLIB
static unsigned long compress_file(char *proto,char *host,char *file);
#endif
static void MarkProto(char *proto);
static void MarkHost(char *proto,char *host);
static void MarkDir(char *dirname);

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
*/


/*++++++++++++++++++++++++++++++++++++++
  Purge files from the cache that meet the age criteria.

  int fd the file descriptor of the wwwoffle client.
  ++++++++++++++++++++++++++++++++++++++*/

int PurgeCache(int fd)
{
 int i,p;
 struct STATFS sbuf;
 struct stat buf;

 purge_max_size=ConfigInteger(PurgeMaxSize);
 purge_min_free=ConfigInteger(PurgeMinFree);
 purge_use_mtime=ConfigBoolean(PurgeUseMTime);
 purge_use_url=ConfigBoolean(PurgeUseURL);
 what_purge_compress_age("*","*",NULL,&purge_default_age,&compress_default_age);

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

    if(purge_max_size>0 || purge_min_free>0)
       if(pass==1)
          write_string(fd,"Pass 1: Checking dates and sizes of files:\n");
       else
          write_string(fd,"Pass 2: Purging files down to specified size:\n");
    else
       write_string(fd,"Checking dates of files:\n");

    if(pass==1)
      {
       if(purge_use_mtime)
          write_string(fd,"  (Using modification time.)\n");
       else
          write_string(fd,"  (Using last access time.)\n");

       if(purge_use_url)
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
         {PrintMessage(Inform,"Cannot stat directory '%s' [%!s]; not purged.",proto);continue;}

       dir_blocks=Bytes_to_Blocks(buf.st_size);

       total_file_blocks+=dir_blocks;
       blocks_by_age[purge_default_age+1]+=dir_blocks;

       if(chdir(proto))
         {PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not purged.",proto);continue;}

       dir=opendir(".");
       if(!dir)
         {PrintMessage(Warning,"Cannot open directory '%s' [%!s]; not purged.",proto);ChangeBackToSpoolDir();continue;}

       ent=readdir(dir);
       if(!ent)
         {PrintMessage(Warning,"Cannot read directory '%s' [%!s]; not purged.",proto);closedir(dir);ChangeBackToSpoolDir();continue;}

       /* Search through all of the sub directories. */

       do
         {
          struct stat buf;

          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

          if(stat(ent->d_name,&buf))
             PrintMessage(Inform,"Cannot stat directory '%s/%s' [%!s]; race condition?",proto,ent->d_name);
          else if(S_ISDIR(buf.st_mode))
            {
             unsigned long file_blocks,del_blocks,compress_blocks;
             int purge_age,compress_age;

             what_purge_compress_age(proto,ent->d_name,NULL,&purge_age,&compress_age);

	     if(chdir(ent->d_name))
	       {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s]; not purged.",proto,ent->d_name);}
	     else {
	       PurgeFiles(proto,ent->d_name,purge_age,compress_age,&file_blocks,&del_blocks,&compress_blocks);
	       if(fchdir(dirfd(dir)) && (ChangeBackToSpoolDir() || chdir(proto)))
		 {PrintMessage(Warning,"Cannot change back to protocol directory '%s' [%!s].",proto);break;}
	     }

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
       while((ent=readdir(dir)));

       closedir(dir);
       ChangeBackToSpoolDir();
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

          if(purge_max_size>0 && age_for_size<0 &&
             Blocks_to_kB(age_blocks_used)>(1024*purge_max_size))
            {
             age_for_size=i;

             write_formatted(fd,"Cutoff Age is %2d days for %3d MB cache size\n",age_for_size,purge_max_size);
            }

          if(purge_min_free>0 && diskfree && age_for_free<0 &&
             Blocks_to_kB(age_blocks_free)<(1024*purge_min_free))
            {
             age_for_free=i;

             write_formatted(fd,"Cutoff Age is %2d days for %3d MB disk free\n",age_for_free,purge_min_free);
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

 /* Purge the tmp.* files in outgoing and temp. */

 for (i=0; i<2; ++i) {
   static char *dirnames[2]={"outgoing","temp"};
   char *dirname=dirnames[i];

   if(chdir(dirname))
     PrintMessage(Warning,"Cannot change to directory '%s' [%!s]; not purged.",dirname);
   else
     {
       DIR *dir;
       struct dirent* ent;
       int count=0;

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

		       if(!stat(ent->d_name,&buf) && buf.st_mtime<(now-60))
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
	 write_formatted(fd,"\nDeleted %d temporary files from directory '%s'.\n\n",count,dirname);
     }

   ChangeBackToSpoolDir();
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
    static char *special[nspecial]={"monitor","outgoing","lastout","prevout","lasttime","prevtime"};
    char *dirname=special[i];
    int j,n=0;

    if(!strcmp_litbeg(special[i],"prev")) n=NUM_PREVTIME_DIR;

    for(j=1; j<=(n?n:1); ++j) {
      char dbuf[sizeof("prevtime")+10];

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
  Delete the file in the current directory that are older than the specified age.

  char *proto The name of the protocol directory to purge.

  char *host The name of the host directory to purge.

  int def_purge_age The default purge age to use for this host.

  int def_compress_age The default compress age to use for this host.

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

 dir=opendir(".");
 if(!dir)
   {PrintMessage(Warning,"Cannot open directory '%s/%s' [%!s]; not purged.",proto,host);return;}

 ent=readdir(dir);
 if(!ent)
   {PrintMessage(Warning,"Cannot read directory '%s/%s' [%!s]; not purged.",proto,host);goto closedir_return;}

 /* Check all of the files for age, and delete as needed. */

 do
   {
    struct stat buf;

    if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
       continue; /* skip . & .. */

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

       if(*ent->d_name=='D')
         {
          if(purge_use_url)
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
          PrintMessage(Inform,"Cached file '%s/%s/%s' is not valid (D* file); deleting it.",proto,host,ent->d_name);
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

          size=Bytes_to_Blocks(buf.st_size);

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

         }
      }
   }
 while((ent=readdir(dir)));

closedir_return:
 closedir(dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Determine the age to use when purging and compressing a specified URL.

  char *proto The protocol to use.

  char *hostport The host (and port) to use.

  char *file The filename to use (if purging by URL).

  int *purge_age The age at which the file should be purged.

  int *compress_age The age at which the file should be compressed.
  ++++++++++++++++++++++++++++++++++++++*/

static void what_purge_compress_age(char *proto,char *hostport,char *file,int *purge_age,int *compress_age)
{
 URL *Url=NULL;

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
    /* free(url); */
   }

 if(purge_age)
   {
    if((ConfigBoolean(PurgeDontGet) &&
	(file?ConfigBooleanMatchURL(DontGet,Url):ConfigBooleanMatchProtoHostPort(DontGet,proto,hostport)))
       ||
       (ConfigBoolean(PurgeDontCache) &&
	((file?ConfigBooleanMatchURL(DontCache,Url):ConfigBooleanMatchProtoHostPort(DontCache,proto,hostport)) ||
	 IsLocalNetHostPort(hostport))))
      *purge_age=0;
    else
      *purge_age=file?ConfigIntegerURL(PurgeAges,Url):ConfigIntegerProtoHostPort(PurgeAges,proto,hostport);
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

 if(ifd==-1)
   {
    PrintMessage(Inform,"Cannot open file '%s/%s/%s' to compress it [%!s]; race condition?",proto,host,file);
    return(0);
   }

 init_io(ifd);

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

 AddToHeader(spool_head,"Content-Encoding","x-gzip");
 AddToHeader(spool_head,"Pragma","wwwoffle-compressed");
 RemoveFromHeader(spool_head,"Content-Length");

 zfile=(char*)alloca(strlen(file)+sizeof(".z"));
 stpcpy(stpcpy(zfile,file),".z");

 ofd=open(zfile,O_WRONLY|O_BINARY|O_CREAT|O_TRUNC,buf.st_mode&07777);

 if(ofd==-1)
   {
    PrintMessage(Inform,"Cannot open file '%s/%s/%s' to compress to [%!s].",proto,host,zfile);

    FreeHeader(spool_head);

    finish_io(ifd);
    close(ifd);

    utime(file,&ubuf);
    return(0);
   }

 init_io(ofd);

 head=HeaderString(spool_head,NULL);
 FreeHeader(spool_head);

 write_string(ofd,head);
 free(head);

 configure_io_write(ofd,-1,2,0);

 while((n=read_data(ifd,buffer,READ_BUFFER_SIZE))>0)
    write_data(ofd,buffer,n);

 finish_io(ofd);
 close(ofd);

 finish_io(ifd);
 close(ifd);

 if(rename(zfile,file))
   {
    PrintMessage(Inform,"Cannot rename file '%s/%s/%s' to '%s/%s/%s' [%!s].",proto,host,zfile,proto,host,file);
    unlink(zfile);
   }

 utime(file,&ubuf);

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

static void MarkProto(char *proto)
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

          if(fchdir(dirfd(dir)) && (ChangeBackToSpoolDir() || chdir(proto))) {
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

static void MarkHost(char *proto,char *host)
{
  char dir[strlen(proto)+strlen(host)+sizeof("/")];

  sprintf(dir,"%s/%s",proto,host);
  MarkDir(dir);
}


/*++++++++++++++++++++++++++++++++++++++
  Mark the hashes of all the files in a directory.

  char *dir The name of the directory.
  ++++++++++++++++++++++++++++++++++++++*/

static void MarkDir(char *dirname)
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
