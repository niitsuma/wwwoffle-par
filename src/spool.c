/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/spool.c 2.81 2004/02/14 14:03:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8b.
  Handle all of the spooling of files in the spool directory.
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

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"


/*+ Need this for Win32 to use binary mode +*/
#ifndef O_BINARY
#define O_BINARY 0
#endif


#if defined(__CYGWIN__)

/*+ The name of the spool directory. +*/
static char* sSpoolDir=NULL;

#else

/*+ The file descriptor of the spool directory. +*/
static int fSpoolDir=-1;

#endif


/*++++++++++++++++++++++++++++++++++++++
  Open a file in the outgoing directory to write into / read from.

  int OpenOutgoingSpoolFile Returns a file descriptor, or -1 on failure.

  int rw Set to true to read, else false.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenOutgoingSpoolFile(int rw)
{
 struct stat buf;
 int fd=-1;
 char name[sizeof("tmp.")+10];

 sprintf(name,"tmp.%u",(unsigned)getpid());

 /* Create the outgoing directory if needed and change to it */

 if(stat("outgoing",&buf))
   {
    PrintMessage(Inform,"Directory 'outgoing' does not exist [%!s]; creating one.");
    if(mkdir("outgoing",(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory 'outgoing' [%!s].");return(-1);}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file 'outgoing' is not a directory.");return(-1);}

 if(chdir("outgoing"))
   {PrintMessage(Warning,"Cannot change to directory 'outgoing' [%!s].");return(-1);}

 /* Open the outgoing file */

 if(rw)
   {
    struct dirent* ent;
    DIR *dir=opendir(".");

    if(!dir)
      {PrintMessage(Warning,"Cannot open current directory 'outgoing' [%!s].");goto changedir_back;}

    ent=readdir(dir);
    if(!ent)
      {PrintMessage(Warning,"Cannot read current directory 'outgoing' [%!s].");closedir(dir);goto changedir_back;}

    do
      {
       if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
          continue; /* skip . & .. */

       if(*ent->d_name=='O')
         {
          if(rename(ent->d_name,name))
             PrintMessage(Inform,"Cannot rename file 'outgoing/%s' to 'outgoing/%s' [%!s]; race condition?",ent->d_name,name);
          else
            {
             fd=open(name,O_RDONLY|O_BINARY);
             /* init_io(fd) not called since fd is returned */

             if(fd==-1)
                PrintMessage(Inform,"Cannot open file 'outgoing/%s' to read [%!s]; race condition?",name);
             else
               {
                *ent->d_name='U';
                unlink(ent->d_name);
                unlink(name);
                break;
               }
            }
         }
      }
    while((ent=readdir(dir)));

    closedir(dir);
   }
 else
   {
    fd=open(name,O_WRONLY|O_CREAT|O_EXCL|O_BINARY,(mode_t)ConfigInteger(FilePerm));
    /* init_io(fd) not called since fd is returned */

    if(fd==-1)
       PrintMessage(Warning,"Cannot open file 'outgoing/%s' to write [%!s]",name);
   }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Close an outgoing spool file and rename it to the hashed name.

  int fd The file descriptor to close.

  URL *Url The URL to close.
           If Url is null, the (temporary) outgoing spool file is removed.
  ++++++++++++++++++++++++++++++++++++++*/

void CloseOutgoingSpoolFile(int fd,URL *Url)
{
 char oldname[sizeof("tmp.")+10];
 int ufd;

 /* finish_io(fd) not called since fd was returned */
 close(fd);

 /* Change to the outgoing directory. */

 if(chdir("outgoing"))
   {PrintMessage(Warning,"Cannot change to directory 'outgoing' [%!s].");return;}

 /* Create and rename the file */

 sprintf(oldname,"tmp.%u",(unsigned)getpid());

 if(Url) {
   local_URLToFileName(Url,newname)
   *newname='U';

   ufd=open(newname,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

   if(ufd!=-1)
     {
       if(write_all(ufd,Url->file,strlen(Url->file))<0)
	 {
	   PrintMessage(Warning,"Cannot write to file 'outgoing/%s' [%!s]; disk full?",newname);
	   unlink(newname);
	 }

       close(ufd);
     }

   *newname='O';
   if(rename(oldname,newname))
     {PrintMessage(Warning,"Cannot rename 'outgoing/%s' to 'outgoing/%s' [%!s].",oldname,newname);unlink(oldname);}
 }
 else if(unlink(oldname))
   PrintMessage(Warning,"Cannot unlink temporary outgoing file 'outgoing/%s' [%!s].",oldname);

 /* Change dir back. */

 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Check if a specified URL exists in the outgoing directory.

  int ExistsOutgoingSpoolFile Returns a boolean.

  URL *Url The URL to check for.
  ++++++++++++++++++++++++++++++++++++++*/

int ExistsOutgoingSpoolFile(URL *Url)
{
 struct stat buf;
 int existsO,existsU;

 if(chdir("outgoing"))
   {PrintMessage(Warning,"Cannot change to directory 'outgoing' [%!s].");return(0);}

 /* Stat the outgoing file */

 {
   local_URLToFileName(Url,name)

   *name='O';
   existsO=!stat(name,&buf);

   *name='U';
   existsU=!stat(name,&buf);
 }

 /* Change dir back. */

 ChangeBackToSpoolDir();

 return(existsO && existsU);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a hash value from the request for a specified URL in the outgoing directory.

  char *HashOutgoingSpoolFile Returns a hash string or NULL in error.

  URL *Url The URL to create the hash for.
  ++++++++++++++++++++++++++++++++++++++*/

char *HashOutgoingSpoolFile(URL *Url)
{
 char *req,*hash=NULL;
 size_t req_size;
 int fd;
 struct stat buf;

 if(chdir("outgoing"))
   {PrintMessage(Warning,"Cannot change to directory 'outgoing' [%!s].");return(NULL);}

 /* Read the outgoing file */

 {
   local_URLToFileName(Url,name)
   *name='O';

   fd=open(name,O_RDONLY|O_BINARY);

   if(fd==-1) {
     PrintMessage(Warning,"Cannot open outgoing request 'outgoing/%s' to create hash [%!s].",name);
     goto changedir_back;
   }

   if(fstat(fd,&buf)==-1) {
     PrintMessage(Warning,"Cannot stat outgoing request 'outgoing/%s' to create hash [%!s].",name);
     goto close_chdir_return;
   }

   req_size=buf.st_size;

   req=(char*)malloc(req_size+1);

   if(req) {
     if(read_all(fd,req,req_size)==req_size) {
       req[req_size]=0;
       hash=MakeHash(req);
     }
     else {
       PrintMessage(Warning,"Cannot read from outgoing request 'outgoing/%s' to create hash [%!s].",name);
     }
     free(req);
   }
   else
     PrintMessage(Warning,"Cannot allocate memory to read outgoing request 'outgoing/%s' [%!s].",name);


   /* close file and change dir back. */

 close_chdir_return:
   close(fd);
 changedir_back:
   ChangeBackToSpoolDir();
 }

 return(hash);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a specified URL request from the outgoing requests.

  char *DeleteOutgoingSpoolFile Returns NULL if OK else error message.

  URL *Url The URL to delete or NULL for all of them.
  ++++++++++++++++++++++++++++++++++++++*/

char *DeleteOutgoingSpoolFile(URL *Url)
{
 char *err=NULL;

 /* Change to the outgoing directory. */

 if(chdir("outgoing"))
   {err=GetPrintMessage(Warning,"Cannot change to directory 'outgoing' [%!s].");return(err);}

 /* Delete the file for the request or all of them. */

 if(Url)
   {
    local_URLToFileName(Url,name)
    *name='O';

    if(unlink(name))
       err=GetPrintMessage(Warning,"Cannot unlink outgoing request 'outgoing/%s' [%!s].",name);

    *name='U';
    unlink(name);
   }
 else
   {
    int any=0;
    struct dirent* ent;
    DIR *dir=opendir(".");

    if(!dir)
      {err=GetPrintMessage(Warning,"Cannot open current directory 'outgoing' [%!s].");goto changedir_back;}

    ent=readdir(dir);
    if(!ent)
      {err=GetPrintMessage(Warning,"Cannot read current directory 'outgoing' [%!s].");closedir(dir);goto changedir_back;}

    do
      {
       if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
          continue; /* skip . & .. */

       if(*ent->d_name=='U' || *ent->d_name=='O')
          if(unlink(ent->d_name))
            {
             any++;
             PrintMessage(Warning,"Cannot unlink outgoing request 'outgoing/%s' [%!s].",ent->d_name);
            }
      }
    while((ent=readdir(dir)));

    if(any)
       err=strdup("Cannot delete some outgoing requests.");

    closedir(dir);
   }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();

 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Open a file in a spool subdirectory to write into / read from.

  int OpenWebpageSpoolFile Returns a file descriptor.

  int rw Set to 1 to read, 0 to write.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenWebpageSpoolFile(int rw,URL *Url)
{
 struct stat buf;
 int fd=-1;

 /* Create the spool directory if needed and change to it. */

 if(stat(Url->proto,&buf))
   {
    PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.",Url->proto);
    if(mkdir(Url->proto,(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory '%s' [%!s].",Url->proto);return(-1);}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file '%s' is not a directory.",Url->proto);return(-1);}

 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return(-1);}

 if(stat(Url->dir,&buf))
   {
    PrintMessage(Inform,"Directory '%s/%s' does not exist [%!s]; creating one.",Url->proto,Url->dir);
    if(mkdir(Url->dir,(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory '%s/%s' [%!s].",Url->proto,Url->dir);goto changedir_back;}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file '%s/%s' is not a directory.",Url->proto,Url->dir);goto changedir_back;}

 if(chdir(Url->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,Url->dir);goto changedir_back;}

 /* Open the file for the web page. */

 {
   local_URLToFileName(Url,file)

   *file='D';
   if(rw)
     fd=open(file,O_RDONLY|O_BINARY);
   else
     fd=open(file,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

   /* init_io(fd) not called since fd is returned */

   if(!rw && fd!=-1)
     {
       int ufd;

       *file='U';
       ufd=open(file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

       if(ufd!=-1)
	 {
	   if(write_all(ufd,Url->file,strlen(Url->file))<0)
	     {
	       PrintMessage(Warning,"Cannot write to file '%s/%s/%s' [%!s]; disk full?",Url->proto,Url->dir,file);
	       unlink(file);
	       close(fd);
	       fd=-1;
	     }

	   close(ufd);
	 }
       else
	 {
	   close(fd);
	   fd=-1;
	 }
     }
 }

 /* Change the modification time on the directory. */

 if(!rw && fd!=-1)
   {
    utime(".",NULL);

    ChangeBackToSpoolDir();
    chdir(Url->proto);

    utime(".",NULL);
   }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a file in a spool subdirectory.

  char *DeleteWebpageSpoolFile Return NULL if OK else error message.

  URL *Url The URL to delete.

  int all If set then delete all pages from this host.
  ++++++++++++++++++++++++++++++++++++++*/

char *DeleteWebpageSpoolFile(URL *Url,int all)
{
 char *err=NULL;

 /* Change to the spool directory. */

 if(chdir(Url->proto))
   {err=GetPrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return(err);}

 if(chdir(Url->dir))
   {err=GetPrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,Url->dir);goto changedir_back;}

 /* Delete the file for the web page. */

 if(all)
   {
    struct dirent* ent;
    DIR *dir=opendir(".");

    if(!dir)
      {err=GetPrintMessage(Warning,"Cannot open current directory '%s/%s' [%!s].",Url->proto,Url->dir);goto changedir_back;}

    ent=readdir(dir);
    if(!ent)
      {err=GetPrintMessage(Warning,"Cannot read current directory '%s/%s' [%!s].",Url->proto,Url->dir);closedir(dir);goto changedir_back;}

    do
      {
       if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
          continue; /* skip . & .. */

       if(*ent->d_name=='D')
         {
          char *delurl;

          if((delurl=FileNameToURL(ent->d_name)))
            {
             char *err;
             URL *delUrl=SplitURL(delurl);

             ChangeBackToSpoolDir();

             err=DeleteLastTimeSpoolFile(delUrl);
             if(err) free(err);

             chdir(Url->proto);
             chdir(Url->dir);

             free(delurl);
             FreeURL(delUrl);
            }
         }

       if(unlink(ent->d_name)) {
	 if(err) free(err);
	 err=GetPrintMessage(Warning,"Cannot unlink cached file '%s/%s/%s' [%!s].",Url->proto,Url->dir,ent->d_name);
       }
      }
    while((ent=readdir(dir)));

    closedir(dir);

    ChangeBackToSpoolDir();
    chdir(Url->proto);

    if(rmdir(Url->dir)) {
      if(err) free(err);
      err=GetPrintMessage(Warning,"Cannot delete what should be an empty directory '%s/%s' [%!s].",Url->proto,Url->dir);
    }

   changedir_back:
    ChangeBackToSpoolDir();
   }
 else
   {
    struct stat buf;
    int didstat=1;
    local_URLToFileName(Url,file)

    if(stat(".",&buf))
       PrintMessage(Warning,"Cannot stat directory '%s/%s' [%!s].",Url->proto,Url->dir);
    else
       didstat=1;

    *file='D';
    if(unlink(file))
       err=GetPrintMessage(Warning,"Cannot unlink cached file '%s/%s/%s' [%!s].",Url->proto,Url->dir,file);

    *file='U';
    unlink(file);

    if(didstat)
      {
       struct utimbuf utbuf;

       utbuf.actime=time(NULL);
       utbuf.modtime=buf.st_mtime;
       utime(".",&utbuf);
      }

    ChangeBackToSpoolDir();

    {char *err=DeleteLastTimeSpoolFile(Url); if(err) free(err);}
   }

 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Touch a file in a spool subdirectory.

  URL *Url The URL to touch.

  time_t when The time to set the access time to.
  ++++++++++++++++++++++++++++++++++++++*/

void TouchWebpageSpoolFile(URL *Url,time_t when)
{
 /* Change to the spool directory. */

 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return;}

 if(chdir(Url->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,Url->dir);goto changedir_back;}

 /* Touch the file for the web page. */

 {
   local_URLToFileName(Url,file)

   *file='D';

   if(when)
     {
       struct stat buf;

       if(stat(file,&buf))
	 utime(file,NULL);
       else
	 {
	   struct utimbuf ubuf;

	   ubuf.actime=when;
	   ubuf.modtime=buf.st_mtime;

	   utime(file,&ubuf);
	 }
     }
   else
     utime(file,NULL);
 }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Check if a file in a spool subdirectory exists.

  time_t ExistsWebpageSpoolFile Return a the time the page was last accessed if the page exists.

  URL *Url The URL to check for.
  ++++++++++++++++++++++++++++++++++++++*/

time_t ExistsWebpageSpoolFile(URL *Url)
{
 struct stat buf;
 int existsD=0,existsU=0;

 /* Change to the spool directory. */

 if(chdir(Url->proto))
   return(0);

 if(chdir(Url->dir))
   goto changedir_back;

 /* Stat the file for the web page. */

 {
   local_URLToFileName(Url,file)

   *file='U';
   existsU=!stat(file,&buf);

   *file='D';
   existsD=!stat(file,&buf);
 }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();

 if(existsU&&existsD)
    return(buf.st_atime);
 else
    return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a backup copy of a file in a spool subdirectory.

  URL *Url The URL to make a copy of.
  int overwrite  If true, overwrite a possible existing backup. 
  ++++++++++++++++++++++++++++++++++++++*/

void CreateBackupWebpageSpoolFile(URL *Url, int overwrite)
{
 struct stat buf;

 /* Change to the spool directory. */

 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return;}

 if(chdir(Url->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,Url->dir);goto changedir_back;}

 /* Create the filenames and rename the files. */

 {
   local_URLToFileName(Url,orgfile)
   *orgfile='D';

   {
     size_t org_len=strlen(orgfile);
     char bakfile[org_len+2];

     {char *p= mempcpy(bakfile,orgfile,org_len); *p++='~'; *p=0;}

     if(!overwrite && !stat(bakfile,&buf))
       PrintMessage(Inform,"Backup already exists for '%s'.",Url->name);
     else
       {
	 if(rename(orgfile,bakfile))
	   PrintMessage(Warning,"Cannot rename backup cached file '%s/%s/%s' to %s [%!s].",Url->proto,Url->dir,bakfile,orgfile);

	 *bakfile=*orgfile='U';
	 rename(orgfile,bakfile);
       }
   }
 }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Restore the backup copy of a file in a spool subdirectory.

  URL *Url The URL to restore.
  ++++++++++++++++++++++++++++++++++++++*/

void RestoreBackupWebpageSpoolFile(URL *Url)
{
 struct stat buf;

 /* Change to the spool directory. */

 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return;}

 if(chdir(Url->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,Url->dir);goto changedir_back;}

 /* Create the filenames and rename the files. */

 {
   local_URLToFileName(Url,orgfile)
   *orgfile='D';

   {
     size_t org_len=strlen(orgfile);
     char bakfile[org_len+2];

     {char *p= mempcpy(bakfile,orgfile,org_len); *p++='~'; *p=0;}

     if(!stat(bakfile,&buf))
       {
	 if(rename(bakfile,orgfile))
	   PrintMessage(Warning,"Cannot rename backup cached file '%s/%s/%s' to %s [%!s].",Url->proto,Url->dir,bakfile,orgfile);

	 *bakfile=*orgfile='U';
	 rename(bakfile,orgfile);
       }
   }
 }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Open the backup copy of a file in a spool subdirectory to read from.

  int OpenBackupWebpageSpoolFile Returns a file descriptor.

  URL *Url The URL to open.
  ++++++++++++++++++++++++++++++++++++++*/

int OpenBackupWebpageSpoolFile(URL *Url)
{
 int fd=-1;

 /* Change to the spool directory. */

 if(chdir(Url->proto))
    return(-1);

 if(chdir(Url->dir))
   {ChangeBackToSpoolDir();return(-1);}

 /* Open the file for the web page. */
 {
   local_URLToFileName(Url,bakfile);

   *bakfile='D';
   strcat(bakfile,"~");

   fd=open(bakfile,O_RDONLY|O_BINARY);
 }
 /* init_io(fd) not called since fd is returned */

 /* Change dir back and tidy up. */

 ChangeBackToSpoolDir();

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a file in a spool subdirectory.

  URL *Url The URL to delete.
  ++++++++++++++++++++++++++++++++++++++*/

void DeleteBackupWebpageSpoolFile(URL *Url)
{
 /* Change to the spool directory. */

 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return;}

 if(chdir(Url->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,Url->dir);goto changedir_back;}

 /* Delete the file for the backup web page. */

 {
   local_URLToFileName(Url,bakfile)

   *bakfile='D';
   strcat(bakfile,"~");

   /* It might seem strange to touch a file just before deleting it, but there is
      a reason.  The backup file is linked to the files in the prevtime(x)
      directories.  Touching it here will update all of the linked files so that
      sorting the prevtime(x) index by date changed will distinguish files that
      have been fetched again since that prevtime(x) index. */

   utime(bakfile,NULL);

   if(unlink(bakfile))
     PrintMessage(Warning,"Cannot unlink backup cached file '%s/%s/%s' [%!s].",Url->proto,Url->dir,bakfile);

   *bakfile='U';
   unlink(bakfile);
 }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Create a lock file in a spool subdirectory.

  int CreateLockWebpageSpoolFile Returns 1 if created OK or 0 in case of error.

  URL *Url The URL to lock.
  ++++++++++++++++++++++++++++++++++++++*/

int CreateLockWebpageSpoolFile(URL *Url)
{
 struct stat buf;
 int retval=1;

 /* Check for configuration file option. */

 if(!ConfigBoolean(LockFiles))
    return(1);

 /* Create the spool directory if needed and change to it. */

 if(stat(Url->proto,&buf))
   {
    PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.",Url->proto);
    if(mkdir(Url->proto,(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory '%s' [%!s].",Url->proto);return(0);}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file '%s' is not a directory.",Url->proto);return(0);}

 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return(0);}

 if(stat(Url->dir,&buf))
   {
    PrintMessage(Inform,"Directory '%s/%s' does not exist [%!s]; creating one.",Url->proto,Url->dir);
    if(mkdir(Url->dir,(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory '%s/%s' [%!s].",Url->proto,Url->dir);retval=0;goto changedir_back;}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file '%s/%s' is not a directory.",Url->proto,Url->dir);retval=0;goto changedir_back;}

 if(chdir(Url->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,Url->dir);retval=0;goto changedir_back;}

 /* Create the lock file for the web page. */

 {
   local_URLToFileName(Url,lockfile)

   *lockfile='L';

   if(!stat(lockfile,&buf))
     {
       PrintMessage(Inform,"Lock file already exists for '%s'.",Url->name);
       retval=0;
     }
   else
     {
       int fd;

       /* Using open() instead of link() allows a race condition over NFS.
	  Using NFS for the WWWOFFLE spool is not recommended anyway. */

       fd=open(lockfile,O_WRONLY|O_CREAT|O_EXCL,(mode_t)ConfigInteger(FilePerm));

       if(fd==-1)
	 {
	   PrintMessage(Warning,"Cannot make a lock file for '%s/%s/%s' [%!s].",Url->proto,Url->dir,lockfile);
	   retval=0;
	 }
       else
	 close(fd);
     }
 }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Check for the existance of a lock file in a spool subdirectory.

  int ExistsLockWebpageSpoolFile Return a true value if the lock file exists.

  URL *Url The URL to check for the lock file for.
  ++++++++++++++++++++++++++++++++++++++*/

int ExistsLockWebpageSpoolFile(URL *Url)
{
 struct stat buf;
 int existsL=0;

 /* Check for configuration file option. */

 if(!ConfigBoolean(LockFiles))
   return(0);

 if(chdir(Url->proto))
   return(0);

 if(chdir(Url->dir))
   goto changedir_back;

 /* Stat the file for the web page. */

 {
   local_URLToFileName(Url,file)

   *file='L';
   existsL=!stat(file,&buf);
 }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();

 return(existsL);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a lock file in a spool subdirectory.

  URL *Url The URL with the lock to delete.
  ++++++++++++++++++++++++++++++++++++++*/

void DeleteLockWebpageSpoolFile(URL *Url)
{
 /* Check for configuration file option. */

 if(!ConfigBoolean(LockFiles))
    return;

 /* Change to the spool directory */

 if(chdir(Url->proto))
   {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",Url->proto);return;}

 if(chdir(Url->dir))
   {PrintMessage(Warning,"Cannot change to directory '%s/%s' [%!s].",Url->proto,Url->dir);goto changedir_back;}

 /* Delete the file for the backup web page. */

 {
   local_URLToFileName(Url,lockfile);

   *lockfile='L';

   if(unlink(lockfile))
     PrintMessage(Inform,"Cannot unlink lock file '%s/%s/%s' [%!s].",Url->proto,Url->dir,lockfile);
 }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();
}


/*++++++++++++++++++++++++++++++++++++++
  Create a file in the lasttime directory (removing any existing ones).

  int CreateLastTimeSpoolFile Returns 1 if it succeeds.

  URL *Url The URL to create.
  ++++++++++++++++++++++++++++++++++++++*/

int CreateLastTimeSpoolFile(URL *Url)
{
 int retval=0;

 if(!ConfigBoolean(CreateHistoryIndexes))
    return(1);

 /* Change to the last time directory */

 if(chdir("lasttime"))
   {PrintMessage(Warning,"Cannot change to directory 'lasttime' [%!s].");return(0);}

 /* Create the file. */

 {
   char *hash=GetHash(Url);
   char name[strlen(Url->proto)+strlen(Url->dir)+strlen(hash)+7];
   char *file,*p;

   p=stpcpy(stpcpy(name,"../"),Url->proto);
   *p++='/';
   p=stpcpy(p,Url->dir);
   *p++='/';
   file=p;
   *p++='D';
   p=stpcpy(p,hash);

   if(unlink(file) && errno!=ENOENT)
     PrintMessage(Warning,"Cannot remove file 'lasttime/%s' [%!s].",file);
   else
     {
       if(link(name,file))
	 PrintMessage(Warning,"Cannot create file 'lasttime/%s' [%!s].",file);
       else
	 {
	   *file='U';

	   if(unlink(file) && errno!=ENOENT)
	     PrintMessage(Warning,"Cannot remove file 'lasttime/%s' [%!s].",file);
	   else
	     {
	       if(link(name,file))
		 PrintMessage(Warning,"Cannot create file 'lasttime/%s' [%!s].",file);
	       else
		 retval=1;
	     }
	 }
     }
 }

 /* Change dir back. */

 ChangeBackToSpoolDir();

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a specified URL from the lasttime directory.

  char *DeleteLastTimeSpoolFile Returns NULL if OK else error message.

  URL *Url The URL to delete or NULL for all of them.
  ++++++++++++++++++++++++++++++++++++++*/

char *DeleteLastTimeSpoolFile(URL *Url)
{
 struct stat buf;
 char *err=NULL;
 int i;
 local_URLToFileName(Url,name)

 for(i=0;i<=NUM_PREVTIME_DIR;i++)
   {
    char lasttime[sizeof("prevtime")+10];

    if(i)
       sprintf(lasttime,"prevtime%u",(unsigned)i);
    else
       strcpy(lasttime,"lasttime");

    /* Change to the last time directory */

    if(chdir(lasttime))
      {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",lasttime);}
    else
      {
       *name='D';

       if(!stat(name,&buf))
         {
	   if(unlink(name)) {
	     if(err) free(err);
             err=GetPrintMessage(Warning,"Cannot unlink lasttime request '%s/%s' [%!s].",lasttime,name);
	   }

          *name='U';
          unlink(name);
         }

       ChangeBackToSpoolDir();
      }
   }

 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Cycle the URLs from the lasttime directory down to the prevtime directories.
  ++++++++++++++++++++++++++++++++++++++*/

void CycleLastTimeSpoolFile(void)
{
 char lasttime[sizeof("prevtime")+10],prevlasttime[sizeof("prevtime")+10];
 struct stat buf;
 int i,fd;

 /* Don't cycle if last done today. */

 if(ConfigBoolean(CycleIndexesDaily) && !stat("lasttime/.timestamp",&buf))
   {
    time_t timenow=time(NULL);
    struct tm *then,*now;
    long thenday,nowday;

    then=localtime(&buf.st_mtime);
    thenday=then->tm_year*400+then->tm_yday;

    now=localtime(&timenow);
    nowday=now->tm_year*400+now->tm_yday;

    if(thenday==nowday)
       return;
   }

 /* Cycle the lasttime/prevtime? directories */

 for(i=NUM_PREVTIME_DIR;i>=0;i--)
   {
    if(i)
       sprintf(lasttime,"prevtime%u",(unsigned)i);
    else
       strcpy(lasttime,"lasttime");

    /* Create it if it does not exist. */

    if(stat(lasttime,&buf))
      {
       PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.",lasttime);
       if(mkdir(lasttime,(mode_t)ConfigInteger(DirPerm)))
          PrintMessage(Warning,"Cannot create directory '%s' [%!s].",lasttime);
      }
    else
       if(!S_ISDIR(buf.st_mode))
          PrintMessage(Warning,"The file '%s' is not a directory.",lasttime);

    /* Delete the contents of the oldest one, rename the newer ones. */

    if(i==NUM_PREVTIME_DIR)
      {
       DIR *dir;
       struct dirent* ent;

       if(chdir(lasttime))
         {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",lasttime);continue;}

       dir=opendir(".");

       if(!dir)
         {PrintMessage(Warning,"Cannot open current directory '%s' [%!s].",lasttime);ChangeBackToSpoolDir();continue;}

       ent=readdir(dir);
       if(!ent)
         {PrintMessage(Warning,"Cannot read current directory '%s' [%!s].",lasttime);closedir(dir);ChangeBackToSpoolDir();continue;}

       do
         {
          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

          if(unlink(ent->d_name))
             PrintMessage(Warning,"Cannot unlink previous time page '%s/%s' [%!s].",lasttime,ent->d_name);
         }
       while((ent=readdir(dir)));

       closedir(dir);

       ChangeBackToSpoolDir();

       if(rmdir(lasttime))
          PrintMessage(Warning,"Cannot unlink previous time directory '%s' [%!s].",lasttime);
      }
    else
       if(rename(lasttime,prevlasttime))
          PrintMessage(Warning,"Cannot rename previous time directory '%s' to '%s' [%!s].",lasttime,prevlasttime);

    strcpy(prevlasttime,lasttime);
   }

 /* Create the lasttime directory and the timestamp. */

 if(mkdir("lasttime",(mode_t)ConfigInteger(DirPerm)))
    PrintMessage(Warning,"Cannot create directory 'lasttime' [%!s].");

 fd=open("lasttime/.timestamp",O_WRONLY|O_CREAT|O_TRUNC,(mode_t)ConfigInteger(FilePerm));
 if(fd!=-1)
    close(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Cycle the URLs from the outgoing directory to the lastout to prevout directories.
  ++++++++++++++++++++++++++++++++++++++*/

void CycleLastOutSpoolFile(void)
{
 char lastout[sizeof("prevout")+10],prevlastout[sizeof("prevout")+10];
 struct stat buf;
 int i,fd;

 /* Don't cycle if last done today. */

 if(ConfigBoolean(CycleIndexesDaily) && !stat("lastout/.timestamp",&buf))
   {
    time_t timenow=time(NULL);
    struct tm *then,*now;

    long thenday,nowday;

    then=localtime(&buf.st_mtime);
    thenday=then->tm_year*400+then->tm_yday;

    now=localtime(&timenow);
    nowday=now->tm_year*400+now->tm_yday;

    if(thenday==nowday)
       goto link_outgoing;
   }

 /* Cycle the lastout/prevout? directories */

 for(i=NUM_PREVTIME_DIR;i>=0;i--)
   {
    if(i)
       sprintf(lastout,"prevout%u",(unsigned)i);
    else
       strcpy(lastout,"lastout");

    /* Create it if it does not exist. */

    if(stat(lastout,&buf))
      {
       PrintMessage(Inform,"Directory '%s' does not exist [%!s]; creating one.",lastout);
       if(mkdir(lastout,(mode_t)ConfigInteger(DirPerm)))
          PrintMessage(Warning,"Cannot create directory '%s' [%!s].",lastout);
      }
    else
       if(!S_ISDIR(buf.st_mode))
          PrintMessage(Warning,"The file '%s' is not a directory.",lastout);

    /* Delete the contents of the oldest one, rename the newer ones. */

    if(i==NUM_PREVTIME_DIR)
      {
       DIR *dir;
       struct dirent* ent;

       if(chdir(lastout))
         {PrintMessage(Warning,"Cannot change to directory '%s' [%!s].",lastout);continue;}

       dir=opendir(".");

       if(!dir)
         {PrintMessage(Warning,"Cannot open current directory '%s' [%!s].",lastout);ChangeBackToSpoolDir();continue;}

       ent=readdir(dir);
       if(!ent)
         {PrintMessage(Warning,"Cannot read current directory '%s' [%!s].",lastout);closedir(dir);ChangeBackToSpoolDir();continue;}

       do
         {
          if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
             continue; /* skip . & .. */

          if(unlink(ent->d_name))
             PrintMessage(Warning,"Cannot unlink previous time page '%s/%s' [%!s].",lastout,ent->d_name);
         }
       while((ent=readdir(dir)));

       closedir(dir);

       ChangeBackToSpoolDir();

       if(rmdir(lastout))
          PrintMessage(Warning,"Cannot unlink previous time directory '%s' [%!s].",lastout);
      }
    else
       if(rename(lastout,prevlastout))
          PrintMessage(Warning,"Cannot rename previous time directory '%s' to '%s' [%!s].",lastout,prevlastout);

    strcpy(prevlastout,lastout);
   }

 /* Create the lastout directory and the timestamp. */

 if(mkdir("lastout",(mode_t)ConfigInteger(DirPerm)))
    PrintMessage(Warning,"Cannot create directory 'lastout' [%!s].");

 fd=open("lastout/.timestamp",O_WRONLY|O_CREAT|O_TRUNC,(mode_t)ConfigInteger(FilePerm));
 if(fd!=-1)
    close(fd);

 /* Link the files from the outgoing directory to the lastout directory. */

link_outgoing:

 if(!ConfigBoolean(CreateHistoryIndexes))
    return;

 if(chdir("outgoing"))
    PrintMessage(Warning,"Cannot change to directory 'outgoing' [%!s].");
 else
   {
    DIR *dir;
    struct dirent* ent;

    dir=opendir(".");

    if(!dir)
       PrintMessage(Warning,"Cannot open current directory '%s' [%!s].",lastout);
    else
      {
       ent=readdir(dir);
       if(!ent)
          PrintMessage(Warning,"Cannot read current directory '%s' [%!s].",lastout);
       else
         {
          do
            {
             if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
                continue; /* skip . & .. */

             if(*ent->d_name=='U' || *ent->d_name=='O')
               {
		char newname[sizeof("../lastout/")+strlen(ent->d_name)];

		stpcpy(stpcpy(newname,"../lastout/"),ent->d_name);

                unlink(newname);
                if(link(ent->d_name,newname))
                   PrintMessage(Warning,"Cannot create lastout page '%s' [%!s].",&newname[3]);
               }
            }
          while((ent=readdir(dir)));
         }

       closedir(dir);
      }

    ChangeBackToSpoolDir();
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Open a file in the monitor directory to write into.

  int CreateMonitorSpoolFile Returns a file descriptor, or -1 on failure.

  URL *Url The URL of the file to monitor.

  char MofY[13] A mask indicating the months of the year allowed.

  char DofM[32] A mask indicating the days of the month allowed.

  char DofW[8] A mask indicating the days of the week allowed.

  char HofD[25] A mask indicating the hours of the day allowed.
  ++++++++++++++++++++++++++++++++++++++*/

int CreateMonitorSpoolFile(URL *Url,char MofY[13],char DofM[32],char DofW[8],char HofD[25])
{
 struct stat buf;
 int fd=-1;

 /* Create the monitor directory if needed and change to it */

 if(stat("monitor",&buf))
   {
    PrintMessage(Inform,"Directory 'monitor' does not exist [%!s]; creating one.");
    if(mkdir("monitor",(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory 'monitor' [%!s].");return(-1);}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file 'monitor' is not a directory.");return(-1);}

 if(chdir("monitor"))
   {PrintMessage(Warning,"Cannot change to directory 'monitor' [%!s].");return(-1);}

 /* Open the monitor file */

 {
   local_URLToFileName(Url,file)

   *file='O';

   fd=open(file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

   if(fd==-1)
     {PrintMessage(Warning,"Cannot create file 'monitor/%s' [%!s].",file);}
   else
     {
       int ufd,mfd;

       /* init_io(fd) not called since fd is returned */

       *file='U';
       ufd=open(file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

       if(ufd!=-1)
	 {
	   if(write_all(ufd,Url->file,strlen(Url->file))<0)
	     {
	       PrintMessage(Warning,"Cannot write to file 'monitor/%s' [%!s]; disk full?",file);
	       unlink(file);
	       close(fd);
	       fd=-1;
	     }

	   close(ufd);
	 }
       else
	 {
	   close(fd);
	   fd=-1;
	 }

       *file='M';
       mfd=open(file,O_WRONLY|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

       if(mfd!=-1)
	 {
	   init_io(mfd);

	   if(write_formatted(mfd,"%s\n",MofY)<0 ||
	      write_formatted(mfd,"%s\n",DofM)<0 ||
	      write_formatted(mfd,"%s\n",DofW)<0 ||
	      write_formatted(mfd,"%s\n",HofD)<0)
	     {
	       PrintMessage(Warning,"Cannot write to file 'monitor/%s' [%!s]; disk full?",file);
	       unlink(file);
	       close(fd);
	       fd=-1;
	     }

	   finish_io(mfd);
	   close(mfd);
	 }
       else
	 {
	   close(fd);
	   fd=-1;
	 }
     }
 }

 /* Change dir back. */

 ChangeBackToSpoolDir();

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Read a file containing the time to be monitored.

  long ReadMonitorTimesSpoolFile Returns the timestamp of the file.

  URL *Url The URL to read from.

  char MofY[13] Returns a mask indicating the months of the year allowed.

  char DofM[32] Returns a mask indicating the days of the month allowed.

  char DofW[8] Returns a mask indicating the days of the week allowed.

  char HofD[25] Returns a mask indicating the hours of the day allowed.
  ++++++++++++++++++++++++++++++++++++++*/

long ReadMonitorTimesSpoolFile(URL *Url,char MofY[13],char DofM[32],char DofW[8],char HofD[25])
{
 time_t mtime;
 struct stat buf;
 int mfd;

 strcpy(MofY,"111111111111");
 strcpy(DofM,"1111111111111111111111111111111");
 strcpy(DofW,"1111111");
 strcpy(HofD,"100000000000000000000000");

 /* Change to the monitor directory. */

 if(chdir("monitor"))
   {PrintMessage(Warning,"Cannot change to directory 'monitor' [%!s].");return(0);}

 {
   local_URLToFileName(Url,file)
   *file='M';

   /* Check for 'M*' file, missing or in the old format. */

   if(stat(file,&buf))
     {
       /* file missing. */

       PrintMessage(Warning,"Monitor time file is missing for %s.",Url->name);

       /* force fetching again. */

       mtime=0;
     }
   else if(buf.st_size<8)
     {
       /* file in the old format. */

       PrintMessage(Warning,"Monitor time file is obsolete for %s.",Url->name);

       /* force fetching again. */

       mtime=0;
     }
   else
     {
       mtime=buf.st_mtime;

       /* Assume that the file is in the new format. */

       mfd=open(file,O_RDONLY|O_BINARY);

       if(mfd!=-1)
	 {
	   char line[80];
	   line[78]=0;

	   if(read_all(mfd,line,79)!=78 ||
	      sscanf(line,"%12s %31s %7s %24s",MofY,DofM,DofW,HofD)!=4 ||
	      strlen(MofY)!=12 || strlen(DofM)!=31 || strlen(DofW)!=7 || strlen(HofD)!=24)
	     {
	       PrintMessage(Warning,"Monitor time file is invalid for %s.",Url->name);
	     }

	   close(mfd);
	 }
     }
 }

 ChangeBackToSpoolDir();

 return(mtime);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete a specified URL from the monitor directory.

  char *DeleteMonitorSpoolFile Returns NULL if OK else error message.

  URL *Url The URL to delete or NULL for all of them.
  ++++++++++++++++++++++++++++++++++++++*/

char *DeleteMonitorSpoolFile(URL *Url)
{
 char *err=NULL;
 /* Change to the monitor directory. */

 if(chdir("monitor"))
   {err=GetPrintMessage(Warning,"Cannot change to directory 'monitor' [%!s].");return(err);}

 /* Delete the file for the request. */

 if(Url)
   {
    local_URLToFileName(Url,name)
    *name='O';

    if(unlink(name))
       err=GetPrintMessage(Warning,"Cannot unlink monitor request 'monitor/%s' [%!s].",name);

    *name='U';
    unlink(name);

    *name='M';
    unlink(name);
   }
 else
   {
    struct dirent* ent;
    DIR *dir=opendir(".");

    if(!dir)
      {err=GetPrintMessage(Warning,"Cannot open current directory 'monitor' [%!s].");goto changedir_back;}

    ent=readdir(dir);
    if(!ent)
      {err=GetPrintMessage(Warning,"Cannot read current directory 'monitor' [%!s].");closedir(dir);goto changedir_back;}

    do
      {
       if(ent->d_name[0]=='.' && (ent->d_name[1]==0 || (ent->d_name[1]=='.' && ent->d_name[2]==0)))
          continue; /* skip . & .. */

       if(*ent->d_name=='U' || *ent->d_name=='O' || *ent->d_name=='M')
	 if(unlink(ent->d_name)) {
	   if(err) free(err);
	   err=GetPrintMessage(Warning,"Cannot unlink monitor request 'monitor/%s' [%!s].",ent->d_name);
	 }
      }
    while((ent=readdir(dir)));

    closedir(dir);
   }

 /* Change dir back. */

changedir_back:
 ChangeBackToSpoolDir();

 return(err);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a temporary spool file.

  int CreateTempSpoolFile Returns the file descriptor.
  ++++++++++++++++++++++++++++++++++++++*/

int CreateTempSpoolFile(void)
{
 char name[sizeof("temp/tmp.")+10];
 struct stat buf;
 int fd;

 /* Create the outgoing directory if needed and change to it */

 if(stat("temp",&buf))
   {
    PrintMessage(Inform,"Directory 'temp' does not exist [%!s]; creating one.");
    if(mkdir("temp",(mode_t)ConfigInteger(DirPerm)))
      {PrintMessage(Warning,"Cannot create directory 'temp' [%!s].");return(-1);}
   }
 else
    if(!S_ISDIR(buf.st_mode))
      {PrintMessage(Warning,"The file 'temp' is not a directory.");return(-1);}

 /* Open the file */

 sprintf(name,"temp/tmp.%u",(unsigned)getpid());

 fd=open(name,O_RDWR|O_CREAT|O_TRUNC|O_BINARY,(mode_t)ConfigInteger(FilePerm));

 if(fd==-1)
    PrintMessage(Warning,"Cannot create temporary file '%s' [%!s].",name);

 /* init_io(fd) not called since fd is returned */

 return(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Close and delete the temporary spool file.

  int fd The file descriptor.
  ++++++++++++++++++++++++++++++++++++++*/

void CloseTempSpoolFile(int fd)
{
 char name[sizeof("temp/tmp.")+10];

 sprintf(name,"temp/tmp.%u",(unsigned)getpid());
 if(unlink(name)==-1)
    PrintMessage(Warning,"Cannot unlink temporary file '%s' [%!s].",name);

 /* finish_io(fd) not called since fd was returned */

 close(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a filename to a URL.

  char *FileNameToURL Returns the URL.

  char *file The file name.
  ++++++++++++++++++++++++++++++++++++++*/

char *FileNameToURL(char *file)
{
 char *path=NULL,*copy;
 size_t path_len;
 int ufd;
 struct stat buf;

 if(!file || !*file)
    return(NULL);

 copy=strdupa(file);

 *copy='U';

 ufd=open(copy,O_RDONLY|O_BINARY);

 if(ufd==-1)
   return(NULL);

 if(fstat(ufd,&buf)==-1) {
   goto close_return;
 }

 path_len=buf.st_size;

 path=malloc(path_len+1);

 if(path) {
   if(read_all(ufd,path,path_len)==path_len)
     path[path_len]=0;
   else {
     free(path); path=NULL;
   }
 }

close_return:
 close(ufd);

 return(path);
}


/*++++++++++++++++++++++++++++++++++++++
  Open the spool directory.

  int ChangeToSpoolDir Changes to the spool directory (and opens a file descriptor there for later).

  char *dir The directory to change to (and open).
  ++++++++++++++++++++++++++++++++++++++*/

int ChangeToSpoolDir(char *dir)
{
#if defined(__CYGWIN__)

 if(chdir(dir)==-1)
    return(-1);

 if(sSpoolDir) free(sSpoolDir);
 sSpoolDir=strdup(dir);

#else

 if(fSpoolDir!=-1) close(fSpoolDir);
 fSpoolDir=open(dir,O_RDONLY);
 if(fSpoolDir==-1 || fchdir(fSpoolDir)==-1)
    return(-1);

#endif

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Change back to the spool directory.

  int ChangeBackToSpoolDir Return -1 in case of error.
  ++++++++++++++++++++++++++++++++++++++*/

int ChangeBackToSpoolDir(void)
{
#if defined(__CYGWIN__)

  return chdir(sSpoolDir);

#else

 return fchdir(fSpoolDir);

#endif
}

int CloseSpoolDir()
{
  int err=0;
#if defined(__CYGWIN__)

  if(sSpoolDir) {
    free(sSpoolDir);
    sSpoolDir=NULL;
  }

#else

  if(fSpoolDir!=-1) {
    err=close(fSpoolDir);
    fSpoolDir=-1;
  }
#endif

  return err;
}
