/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/configrdwr.c 1.50 2002/09/28 08:18:10 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7f.
  Configuration file reading and writing functions.
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

#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h>

#include "misc.h"
#include "configpriv.h"
#include "wwwoffle.h"
#include "errors.h"


#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Local functions */

static char *new_filename_or_symlink_target(char *name);

static /*@null@*/ char *InitParser(void);
static /*@null@*/ char *ParseLine(/*@out@*/ /*@null@*/ char **line);
static /*@null@*/ char *ParseItem(char *line,/*@out@*/ char **url_str,/*@out@*/ char **key_str,/*@out@*/ char **val_str);
static /*@null@*/ char *ParseEntry(ConfigItemDef *itemdef,/*@out@*/ ConfigItem *item,/*@null@*/ char *url_str,char *key_str,/*@null@*/ char *val_str);

static int isanumber(const char *string);


/* The state of the parser */

typedef enum _ParserState
{
 OutsideSection,

 StartSection,
 StartSectionCurly,
 StartSectionSquare,
 StartSectionIncluded,

 InsideSectionCurly,
 InsideSectionSquare,
 InsideSectionIncluded,

 Finished
}
ParserState;

static char *parse_name;        /*+ The name of the configuration file. +*/
static FILE *parse_file;        /*+ The file handle of the configuration file. +*/
static int parse_line;          /*+ The line number in the configuration file. +*/

static char *parse_name_org;    /*+ The name of the original configuration file when parsing an included one. +*/
static FILE *parse_file_org;    /*+ The file handle of the original configuration file when parsing an included one. +*/
static int parse_line_org;      /*+ The line number in the original configuration file when parsing an included one. +*/

static int parse_section;       /*+ The current section of the configuration file. +*/
static int parse_item;          /*+ The current item in the configuration file. +*/
static ParserState parse_state; /*+ The parser state. +*/


/*++++++++++++++++++++++++++++++++++++++
  Read the data from the file.

  char *ReadConfigFile Returns the error message or NULL if OK.
  ++++++++++++++++++++++++++++++++++++++*/

char *ReadConfigFile(void)
{
 static int first_time=1;
 char *errmsg=NULL;

 CreateBackupConfigFile();

 errmsg=InitParser();

 if(!errmsg) {
   char *line=NULL;

   do
     {
       if((errmsg=ParseLine(&line)))
	 break;

       if(parse_section!=-1)
         {
	   char *url_str,*key_str,*val_str;

	   if((errmsg=ParseItem(line,&url_str,&key_str,&val_str)))
             break;

	   if(parse_item!=-1)
	     {
	       if((errmsg=ParseEntry(&CurrentConfig.sections[parse_section]->itemdefs[parse_item],
				     CurrentConfig.sections[parse_section]->itemdefs[parse_item].item,
				     url_str,key_str,val_str)))
		 break;
	     }
         }
     }
   while(parse_state!=Finished);

   if(line) free(line);
 }

 if(parse_file)
    fclose(parse_file);
 if(parse_file_org)
    fclose(parse_file_org);

 if(errmsg)
   {
     RestoreBackupConfigFile();
     {
       char *newerrmsg=x_asprintf("Configuration file syntax error at line %d in '%s'; %s\n",parse_line,parse_name,errmsg); /* Used in wwwoffle.c */
       free(errmsg);
       errmsg=newerrmsg;
     }
     
     if(parse_state==StartSectionIncluded || parse_state==InsideSectionIncluded) {
       free(parse_name);
     }
   }
 else
    PurgeBackupConfigFile(!first_time);

#if CONFIG_DEBUG_DUMP
 DumpConfigFile();
#endif

 first_time=0;

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Modify an entry in the configuration file.

  char *ModifyConfigFile Returns a string detailing the error if there is one.

  int section The section of the configuration file to modify.

  int item The item of the configuration to modify.

  char *newentry The new entry to insert or change to (or NULL if deleting one).

  char *preventry The previous entry in the current list (or NULL if not adding after one).

  char *sameentry The same entry in the current list (or NULL if not replacing one).

  char *nextentry The next entry in the current list (or NULL if not adding before one).
  ++++++++++++++++++++++++++++++++++++++*/

char *ModifyConfigFile(int section,int item,char *newentry,char *preventry,char *sameentry,char *nextentry)
{
 char *errmsg=NULL;
 char **names=(char**)calloc((1+CurrentConfig.nsections),sizeof(char*));
 FILE *file=NULL,*file_org=NULL;
 ConfigItem dummy=NULL;
 int matched=0;
 int s;

 /* Initialise the parser and open the new file. */

 errmsg=InitParser();

 if(!errmsg)
   {
    names[0]=new_filename_or_symlink_target(parse_name);

    file=fopen(names[0],"w");
    if(!file)
      {
       errmsg=x_asprintf("Cannot open the configuration file '%s' for writing [%s].",names[0],strerror(errno));
      }
   }

 /* Parse the file */

 if(!errmsg) {
    char *line=NULL;

    do
      {
       if((errmsg=ParseLine(&line)))
          break;

       if(parse_section==section && line)
         {
          char *url_str,*key_str,*val_str;

          /* Insert a new entry for a non-existing item. */

          if(newentry && !preventry && !sameentry && !nextentry && !matched &&
             (parse_state==InsideSectionCurly || parse_state==InsideSectionIncluded))
            {
             local_strdup(newentry,copyentry)

             if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
                break;

             if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
                                   &dummy,
                                   url_str,key_str,val_str)))
                break;

             fprintf(file,"\n# WWWOFFLE Configuration Edit Inserted: %s\n %s\n\n",RFC822Date(time(NULL),0),newentry);

             matched=1;
            }

          {
	    local_strdup(line,copy)

	    if((errmsg=ParseItem(copy,&url_str,&key_str,&val_str)))
	      break;

	    if(parse_item==item)
	      {
		char *thisentry;

		if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
				      &dummy,
				      url_str,key_str,val_str)))
		  break;

		thisentry=ConfigEntryString(dummy,dummy->nentries-1);

		/* Insert a new entry before the current one */

		if(newentry && nextentry && !strcmp(thisentry,nextentry))
		  {
		    local_strdup(newentry,copyentry)

		    if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
		      break;

		    if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
					  &dummy,
					  url_str,key_str,val_str)))
		      break;

		    fprintf(file,"\n# WWWOFFLE Configuration Edit Inserted: %s\n %s\n\n",RFC822Date(time(NULL),0),newentry);
		    fprintf(file,"%s",line);

		    matched=1;
		  }

		/* Insert a new entry after the current one */

		else if(newentry && preventry && !strcmp(thisentry,preventry))
		  {
		    local_strdup(newentry,copyentry)

		    if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
		      break;

		    if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
					  &dummy,
					  url_str,key_str,val_str)))
		      break;

		    fprintf(file,"%s",line);
		    fprintf(file,"\n# WWWOFFLE Configuration Edit Inserted: %s\n %s\n\n",RFC822Date(time(NULL),0),newentry);

		    matched=1;
		  }

		/* Delete an entry */

		else if(!newentry && sameentry && !strcmp(thisentry,sameentry))
		  {
		    fprintf(file,"\n# WWWOFFLE Configuration Edit Deleted: %s\n#%s",RFC822Date(time(NULL),0),line);

		    matched=1;
		  }

		/* Change an entry */

		else if(newentry && sameentry && !strcmp(thisentry,sameentry))
		  {
		    local_strdup(newentry,copyentry)

		    if(CurrentConfig.sections[section]->itemdefs[item].same_key==0 &&
		       CurrentConfig.sections[section]->itemdefs[item].url_type==0)
		      {
			FreeConfigItem(dummy);
			dummy=NULL;
		      }

		    if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
		      break;

		    if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
					  &dummy,
					  url_str,key_str,val_str)))
		      break;

		    fprintf(file,"# WWWOFFLE Configuration Edit Changed: %s\n#%s",RFC822Date(time(NULL),0),line);
		    fprintf(file," %s\n",newentry);

		    matched=1;
		  }
		else
		  fprintf(file,"%s",line);

		free(thisentry);
	      }
	    else
	      fprintf(file,"%s",line);
	  }
         }
       else if(line)
          fprintf(file,"%s",line);

       if(parse_state==StartSectionIncluded && !file_org)
         {
          names[parse_section+1]=new_filename_or_symlink_target(parse_name);

          file_org=file;
          file=fopen(names[parse_section+1],"w");
          if(!file)
            {
             errmsg=x_asprintf("Cannot open the included file '%s' for writing [%s].",names[parse_section+1],strerror(errno));
             break;
            }
         }
       else if(parse_state==StartSectionSquare && file_org)
         {
          fclose(file);
          file=file_org;
          file_org=NULL;
         }
      }
    while(parse_state!=Finished);

    if(line) free(line);
 }

 if(file) fclose(file);
 if(file_org) fclose(file_org);
 if(parse_file) fclose(parse_file);
 if(parse_file_org) fclose(parse_file_org);
 if(parse_state==StartSectionIncluded || parse_state==InsideSectionIncluded) {
   free(parse_name);
 }

 if(!errmsg && !matched && (preventry || sameentry || nextentry))
   {
    char *whichentry=sameentry?sameentry:preventry?preventry:nextentry;

    errmsg=x_asprintf("No entry to match '%s' was found to make the change.",whichentry);
   }

 for(s=0;s<=CurrentConfig.nsections;++s)
    if(names[s])
      {
       if(!errmsg)
         {
	  int name_len=strlen(names[s])-strlitlen(".new");
          char name[name_len+1], name_bak[name_len+sizeof(".bak")];

          *((char *)mempcpy(name,names[s],name_len))=0;
          mempcpy(mempcpy(name_bak,names[s],name_len),".bak",sizeof(".bak"));

          if(rename(name,name_bak)<0) {
	    errmsg=x_asprintf("Can't rename '%s' as '%s' [%s].",name,name_bak,strerror(errno));
	  }
	  else if(rename(names[s],name)<0) {
	    errmsg=x_asprintf("Can't rename '%s' as '%s' [%s].",names[s],name,strerror(errno));
	  }
         }
       else
          unlink(names[s]);

       free(names[s]);
      }

 FreeConfigItem(dummy);

 free(names);

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Find the real filename of a potential symbolic link.

  char *new_filename_or_symlink_target Returns the real file name with ".new" appended.

  char *name The file name that may be a symbolic link.
  ++++++++++++++++++++++++++++++++++++++*/

static char *new_filename_or_symlink_target(char *name)
{
  char *result=canonicalize_file_name(name);

  if(result)
    str_append(&result,".new");
  else {
    result=(char*)malloc(strlen(name)+sizeof(".new"));
    stpcpy(stpcpy(result,name),".new");
  }

  return(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Initialise the file parser.

  char *InitParser Return an error string in case of error.
  ++++++++++++++++++++++++++++++++++++++*/

static char *InitParser(void)
{
 char *errmsg=NULL;

 parse_name=CurrentConfig.name;
 parse_file=fopen(parse_name,"r");
 parse_line=0;

 parse_name_org=NULL;
 parse_file_org=NULL;
 parse_line_org=0;

 parse_section=-1;
 parse_item=-1;
 parse_state=OutsideSection;

 if(!parse_file)
   {
    errmsg=x_asprintf("Cannot open the configuration file '%s' for reading [%s].",parse_name,strerror(errno));
   }

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the current line of the configuration file.

  char *ParseLine Returns an error message if there is one.

  char **line The line just read from the file or NULL.
  ++++++++++++++++++++++++++++++++++++++*/

static char *ParseLine(char **line)
{
 char *errmsg=NULL;

 /* Read from the line and make a copy */

 *line=fgets_realloc(*line,parse_file);

 ++parse_line;

 parse_item=-1;

 /* At the end of the file, finish, error or close included file. */

 if(!*line)
   {
    if(parse_state==OutsideSection)
      {
       parse_state=Finished;
      }
    else if(parse_state==StartSectionIncluded || parse_state==InsideSectionIncluded)
      {
       fclose(parse_file);
       free(parse_name);

       parse_file=parse_file_org;
       parse_file_org=NULL;

       parse_name=parse_name_org;
       parse_name_org=NULL;

       parse_line=parse_line_org;
       parse_line_org=0;

       parse_state=StartSectionSquare;
      }
    else
      {
       errmsg=strdup("Unexpected end of file.");
      }
   }
 else
   {
    char *l,*r;

    /* Trim the line. */

    l=*line;
    while(*l && isspace(*l)) ++l;

    r=strchrnul(l,0);
    while(--r>=l && isspace(*r));
    ++r;

    /* Outside of section, searching for a section. */

    if(parse_state==OutsideSection)
      {
       if(*l!=0 && *l!='#')
         {
          for(parse_section=0;parse_section<CurrentConfig.nsections;parse_section++)
             if(!strncmp(CurrentConfig.sections[parse_section]->name,l,r-l))
               {
                parse_state=StartSection;
                goto found_section;
               }

	  errmsg=x_asprintf("Unrecognised text outside of section (not section label) '%s'.",l);
	  parse_section=-1;
	 found_section:
         }
      }

    /* The start of a section, undecided which type. */

    else if(parse_state==StartSection)
      {
       if(*l=='{' && (l+1)==r)
         {
          parse_state=StartSectionCurly;
         }
       else if(*l=='[' && (l+1)==r)
         {
          parse_state=StartSectionSquare;
         }
       else if(*l!='{' && *l!='[')
         {
          errmsg=strdup("Start of section must be '{' or '['.");
         }
       else
         {
          errmsg=x_asprintf("Start of section '%c' has trailing junk.",*l);
         }
      }

    /* Inside a normal '{...}' section. */

    else if(parse_state==StartSectionCurly || parse_state==InsideSectionCurly)
      {
       parse_state=InsideSectionCurly;

       if(*l=='}')
	 {
	   if((l+1)==r)
	     {
	       parse_state=OutsideSection;
	       parse_section=-1;
	     }
	   else
	     {
	       errmsg=x_asprintf("End of section '%c' has trailing junk.",*l);
	     }
	 }
      }

    /* Inside a include '[...]' section. */

    else if(parse_state==StartSectionSquare || parse_state==InsideSectionSquare)
      {
       parse_state=InsideSectionSquare;

       if(*l==']')
	 {
	   if((l+1)==r)
	     {
	       parse_state=OutsideSection;
	       parse_section=-1;
	     }
	   else
	     {
	       errmsg=x_asprintf("End of section '%c' has trailing junk.",*l);
	     }
	 }
       else if(*l!=0 && *l!='#')
         {
	  char *parse_name_new; FILE *parse_file_new;

	  if(*l=='/') parse_name_new=strndup(l,r-l);
	  else {
	    char *p=strchrnul(CurrentConfig.name,0); int pathlen;
	    while(--p>=CurrentConfig.name && *p!='/');
	    pathlen=p+1-CurrentConfig.name;
	    parse_name_new=(char*)malloc(pathlen+(r-l)+1);
	    *((char *)mempcpy(mempcpy(parse_name_new,CurrentConfig.name,pathlen),l,r-l))=0;
	  }

          parse_file_new=fopen(parse_name_new,"r");

          if(!parse_file_new)
            {
             errmsg=x_asprintf("Cannot open included file '%s' for reading [%s].",parse_name_new,strerror(errno));
	     free(parse_name_new);
            }
          else
	    {
	      parse_name_org=parse_name;
	      parse_file_org=parse_file;
	      parse_line_org=parse_line;
	      parse_name=parse_name_new;
	      parse_file=parse_file_new;
	      parse_line=0;

	      parse_state=StartSectionIncluded;
	    }
         }
      }

    else if(parse_state==StartSectionIncluded)
      {
       parse_state=InsideSectionIncluded;
      }
   }

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the item from the current line.

  char *ParseItem Returns an error message string in case of a problem.

  char *line The line to parse.

  char **url_str Returns the URL string or NULL.

  char **key_str Returns the key string or NULL.

  char **val_str Returns the value string or NULL.
  ++++++++++++++++++++++++++++++++++++++*/

static char *ParseItem(char *line,char **url_str,char **key_str,char **val_str)
{
 *url_str=NULL;
 *key_str=NULL;
 *val_str=NULL;

 if(parse_state==InsideSectionCurly || parse_state==InsideSectionIncluded)
   {
    char *url=NULL,*key=NULL,*val=NULL;
    char *l;

    l=line;

    while(*l && isspace(*l)) ++l;

    if(!*l || *l=='#')
       return(NULL);

    {char *r=strchrnul(l,0); while(--r>=l && isspace(*r)) *r=0;}

    if(*l=='<')
      {
       char *uu;

       uu=url=l+1;
       while(*uu && *uu!='>')
          uu++;
       if(!*uu)
         {
          char *errmsg=strdup("No '>' to match the '<'.");
          return(errmsg);
         }

       *uu=0;
       key=uu+1;
       while(*key && isspace(*key))
          key++;
       if(!*key)
         {
          char *errmsg=strdup("No configuration entry following the '<...>'.");
          return(errmsg);
         }
      }
    else
       key=l;

    for(parse_item=0;parse_item<CurrentConfig.sections[parse_section]->nitemdefs;++parse_item) {
       ConfigItemDef *itemdef=&CurrentConfig.sections[parse_section]->itemdefs[parse_item];

       if(!*itemdef->name ||
          !strcmp_beg(key,itemdef->name))
         {
          char *ll;

          if(*itemdef->name)
            {
             ll=key+strlen(itemdef->name);

             if(*ll && *ll!='=' && !isspace(*ll))
                continue;
            }
          else
            {
	     char delimiter = (itemdef->key_type==UrlSpecification)? 0 : '=';
             ll=key;
             while(*ll && *ll!=delimiter && !isspace(*ll))
                ll++;
            }

          if(itemdef->url_type==0 && url)
            {
             char *errmsg=strdup("No URL context '<...>' allowed for this entry.");
             return(errmsg);
            }

          if(itemdef->val_type==None)
            {
             if(itemdef->key_type!=UrlSpecification && strchr(ll,'='))
               {
                char *errmsg=strdup("Equal sign seen but not expected.");
                return(errmsg);
               }

             *ll=0;
             val=NULL;
            }
          else
            {
             val=strchr(ll,'=');
             if(!val)
               {
                char *errmsg=strdup("No equal sign seen but expected.");
                return(errmsg);
               }

             *ll=0;
             if(!*key)
               {
                char *errmsg=strdup("Nothing to the left of the equal sign.");
                return(errmsg);
               }

             do {++val;} while(*val && isspace(*val));
            }

          *url_str=url;
          *key_str=key;
          *val_str=val;

          return(NULL);
         }
    }

    {
      char *errmsg=x_asprintf("Unrecognised entry '%s'.",key);
      return(errmsg);
    }
   }

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse an entry from the file.

  char *ParseEntry Return a string containing an error message in case of error,
                   or NULL in case of no error.

  ConfigItemDef *itemdef The item definition for the item in the section.

  ConfigItem *item The item to add the entry to.

  char *url_str The string for the URL specification.

  char *key_str The string for the key.

  char *val_str The string to the value.
  ++++++++++++++++++++++++++++++++++++++*/

static char *ParseEntry(ConfigItemDef *itemdef,ConfigItem *item_p,char *url_str,char *key_str,char *val_str)
{
 ConfigItem item=*item_p;
 UrlSpec *url;
 KeyOrValue key,val;
 char *errmsg=NULL;

 if(!itemdef->same_key && !itemdef->url_type && item && item->nentries)
   {
    errmsg=x_asprintf("Duplicated entry: '%s'.",key_str);
    return(errmsg);
   }

 if(!itemdef->url_type || !url_str)
    url=NULL;
 else if((errmsg=ParseKeyOrValue(url_str,UrlSpecification,(KeyOrValue*)&url)))
   return(errmsg);

 if(itemdef->key_type==Fixed)
   {
    if(strcmp(key_str,itemdef->name))
      {
       errmsg=x_asprintf("Unexpected key string: '%s'.",key_str);
       goto freeurlspec_return;
      }
    key.string=NULL;
   }
 else if((errmsg=ParseKeyOrValue(key_str,itemdef->key_type,&key)))
   goto freeurlspec_return;

 if(itemdef->val_type==None || !val_str)
    val.string=NULL;
 else if((errmsg=ParseKeyOrValue(val_str,itemdef->val_type,&val)))
   goto freekey_freeurlspec_return;

 if(!item_p)
   goto freeval_freekey_freeurlspec_return;

 if(!item)
   {
    *item_p=item=(ConfigItem)malloc(sizeof(struct _ConfigItem));
    item->itemdef=itemdef;
    item->nentries=0;
    item->url=NULL;
    item->key=NULL;
    item->val=NULL;
    item->def_val.string=NULL;
   }

 {
   int k= item->nentries++;
   int newnum=0;

   if(k==0 && !itemdef->same_key && !itemdef->url_type)
     newnum=1;
   else if((k&7)==0)   /* Note: k&7 == k mod 8 */
     newnum=k+8;       /* grow arrays in increments of 8 to reduce number of realloc calls */

   if(itemdef->url_type) {
     if(newnum)
       item->url=(UrlSpec**)realloc((void*)item->url,sizeof(UrlSpec*)*newnum);
     item->url[k]=url;
   }

   if(itemdef->key_type!=Fixed) {
     if(newnum)
       item->key=(KeyOrValue*)realloc((void*)item->key,sizeof(KeyOrValue)*newnum);
     item->key[k]=key;
   }

   if(itemdef->val_type!=None) {
     if(newnum)
       item->val=(KeyOrValue*)realloc((void*)item->val,sizeof(KeyOrValue)*newnum);
     item->val[k]=val;
   }
 }

 return(NULL);

freeval_freekey_freeurlspec_return:
 FreeKeysOrValues(&val,itemdef->val_type,1);

freekey_freeurlspec_return:
 FreeKeysOrValues(&key,itemdef->key_type,1);

freeurlspec_return:
 FreeUrlSpecification(url);

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the text and put a value into the location.

  char *ParseKeyOrValue Returns a string containing the error message.

  char *text The text string to parse.

  ConfigType type The type we are looking for.

  KeyOrValue *pointer The location to store the key or value.
  ++++++++++++++++++++++++++++++++++++++*/

char *ParseKeyOrValue(char *text,ConfigType type,KeyOrValue *pointer)
{
 char *errmsg=NULL;

 switch(type)
   {
   case Fixed:
   case None:
    break;

   case CfgMaxServers:
    if(!*text)
      {errmsg=strdup("Expecting a maximum server count, got nothing.");}
    else if(!isanumber(text))
      {errmsg=x_asprintf("Expecting a maximum server count, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<=0 || pointer->integer>MAX_SERVERS)
         {errmsg=x_asprintf("Invalid maximum server count: %d.",pointer->integer);}
      }
    break;

   case CfgMaxFetchServers:
    if(!*text)
      {errmsg=strdup("Expecting a maximum fetch server count, got nothing.");}
    else if(!isanumber(text))
      {errmsg=x_asprintf("Expecting a maximum fetch server count, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<=0 || pointer->integer>MAX_FETCH_SERVERS)
         {errmsg=x_asprintf("Invalid maximum fetch server count: %d.",pointer->integer);}
      }
    break;

   case CfgLogLevel:
    if(!*text)
      {errmsg=strdup("Expecting a log level, got nothing.");}
    else if(strcasecmp(text,"debug")==0)
       pointer->integer=Debug;
    else if(strcasecmp(text,"info")==0)
       pointer->integer=Inform;
    else if(strcasecmp(text,"important")==0)
       pointer->integer=Important;
    else if(strcasecmp(text,"warning")==0)
       pointer->integer=Warning;
    else if(strcasecmp(text,"fatal")==0)
       pointer->integer=Fatal;
    else
      {errmsg=x_asprintf("Expecting a log level, got '%s'.",text);}
    break;

   case Boolean:
    if(!*text)
      {errmsg=strdup("Expecting a Boolean, got nothing.");}
    else if(!strcasecmp(text,"yes") || !strcasecmp(text,"true"))
       pointer->integer=1;
    else if(!strcasecmp(text,"no") || !strcasecmp(text,"false"))
       pointer->integer=0;
    else
      {errmsg=x_asprintf("Expecting a Boolean, got '%s'.",text);}
    break;

   case PortNumber:
    if(!*text)
      {errmsg=strdup("Expecting a port number, got nothing.");}
    else if(!isanumber(text))
      {errmsg=x_asprintf("Expecting a port number, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<=0 || pointer->integer>65535)
         {errmsg=x_asprintf("Invalid port number %d.",pointer->integer);}
      }
    break;

   case AgeDays:
    if(!*text)
      {errmsg=strdup("Expecting an age in days, got nothing.");}
    else if(isanumber(text))
       pointer->integer=atoi(text);
    else
      {
       int val,len;
       char suffix;

       if(sscanf(text,"%d%1c%n",&val,&suffix,&len)==2 && len==strlen(text) &&
          (suffix=='d' || suffix=='w' || suffix=='m' || suffix=='y'))
         {
          if(suffix=='y')
             pointer->integer=val*365;
          else if(suffix=='m')
             pointer->integer=val*30;
          else if(suffix=='w')
             pointer->integer=val*7;
          else /* if(suffix=='d') */
             pointer->integer=val;
         }
       else
         {errmsg=x_asprintf("Expecting an age in days, got '%s'.",text);}
      }
    break;

   case TimeSecs:
    if(!*text)
      {errmsg=strdup("Expecting a time in seconds, got nothing.");}
    else if(isanumber(text))
       pointer->integer=atoi(text);
    else
      {
       int val,len;
       char suffix;

       if(sscanf(text,"%d%1c%n",&val,&suffix,&len)==2 && len==strlen(text) &&
          (suffix=='s' || suffix=='m' || suffix=='h' || suffix=='d' || suffix=='w'))
         {
          if(suffix=='w')
             pointer->integer=val*3600*24*7;
          else if(suffix=='d')
             pointer->integer=val*3600*24;
          else if(suffix=='h')
             pointer->integer=val*3600;
          else if(suffix=='m')
             pointer->integer=val*60;
          else /* if(suffix=='s') */
             pointer->integer=val;
         }
       else
         {errmsg=x_asprintf("Expecting a time in seconds, got '%s'.",text);}
      }
    break;

   case CacheSize:
    if(!*text)
      {errmsg=strdup("Expecting a cache size in MB, got nothing.");}
    else if(!isanumber(text))
      {errmsg=x_asprintf("Expecting a cache size in MB, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<0)
         {errmsg=x_asprintf("Invalid cache size %d.",pointer->integer);}
      }
    break;

   case FileSize:
    if(!*text)
      {errmsg=strdup("Expecting a file size in kB, got nothing.");}
    else if(!isanumber(text))
      {errmsg=x_asprintf("Expecting a file size in kB, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<0)
         {errmsg=x_asprintf("Invalid file size %d.",pointer->integer);}
      }
    break;

   case Percentage:
    if(!*text)
      {errmsg=strdup("Expecting a percentage, got nothing.");}
    else if(!isanumber(text))
      {errmsg=x_asprintf("Expecting a percentage, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<0)
         {errmsg=x_asprintf("Invalid percentage %d.",pointer->integer);}
      }
    break;

   case UserId:
    if(!*text)
      {errmsg=strdup("Expecting a username or uid, got nothing.");}
    else
      {
       int uid;
       struct passwd *userInfo=getpwnam(text);
       if(userInfo)
          uid=userInfo->pw_uid;
       else
         {
          if(sscanf(text,"%d",&uid)!=1)
            {errmsg=x_asprintf("Invalid user %s.",text);}
          else if(uid!=-1 && !getpwuid(uid))
            {errmsg=x_asprintf("Unknown user id %d.",uid);}
         }
       pointer->integer=uid;
      }
    break;

   case GroupId:
    if(!*text)
      {errmsg=strdup("Expecting a group name or gid, got nothing.");}
    else
      {
       int gid;
       struct group *groupInfo=getgrnam(text);
       if(groupInfo)
          gid=groupInfo->gr_gid;
       else
         {
          if(sscanf(text,"%d",&gid)!=1)
            {errmsg=x_asprintf("Invalid group %s.",text);}
          else if(gid!=-1 && !getgrgid(gid))
            {errmsg=x_asprintf("Unknown group id %d.",gid);}
         }
       pointer->integer=gid;
      }
    break;

   case CompressSpec:
    if(!*text)
      {errmsg=strdup("Expecting a CompressionSpecifier, got nothing.");}
    else if(!strcasecmp(text,"no") || !strcasecmp(text,"false") || !strcasecmp(text,"none"))
       pointer->integer=0;
    else if(!strcasecmp(text,"deflate"))
       pointer->integer=1;
    else if(!strcasecmp(text,"gzip"))
       pointer->integer=2;
    else if(!strcasecmp(text,"yes") || !strcasecmp(text,"true"))
       pointer->integer=3;
    else
      {errmsg=x_asprintf("Expecting a CompressionSpecifier, got '%s'.",text);}
    break;

   case String:
    if(!*text || !strcasecmp(text,"none"))
       pointer->string=NULL;
    else
      {
       pointer->string=strdup(text);
      }
    break;

   case PathName:
    if(!*text)
      {errmsg=strdup("Expecting a pathname, got nothing.");}
    else if(*text!='/')
      {errmsg=x_asprintf("Expecting an absolute pathname, got '%s'.",text);}
    else
      {
       pointer->string=strdup(text);
      }
    break;

   case FileExt:
    if(!*text)
      {errmsg=strdup("Expecting a file extension, got nothing.");}
    else if(*text!='.')
      {errmsg=x_asprintf("Expecting a file extension, got '%s'.",text);}
    else
      {
       pointer->string=strdup(text);
      }
    break;

   case FileMode:
    if(!*text)
      {errmsg=strdup("Expecting a file permissions mode, got nothing.");}
    else if(!isanumber(text) || *text!='0')
      {errmsg=x_asprintf("Expecting a file permissions mode, got '%s'.",text);}
    else
       sscanf(text,"%o",&pointer->integer);
    break;

   case MIMEType:
     if(!*text)
       {errmsg=strdup("Expecting a MIME Type, got nothing.");}
     else
       {
        char *slash=strchr(text,'/');
        if(!slash)
          {errmsg=x_asprintf("Expecting a MIME Type/Subtype, got '%s'.",text);}
        pointer->string=strdup(text);
       }
    break;

   case HostOrNone:
    if(!*text || !strcasecmp(text,"none"))
      {
       pointer->string=NULL;
       break;
      }

    /*@fallthrough@*/

   case Host:
    if(!*text)
      {errmsg=strdup("Expecting a hostname, got nothing.");}
    else
      {
       char *p,*host;
       int wildcard=0,colons=0;

       p=text;
       while(*p)
         {
          if(*p=='*')
             wildcard=1;
          else if(*p==':')
             ++colons;
          ++p;
         }

       /*
         This is tricky, we should check for a host:port combination and disallow it.
         But the problem is that a single colon could be an IPv6 wildcard or an IPv4 host and port.
         If there are 2 or more ':' then it is IPv6, if it starts with '[' it is IPv6, if it has zero
         or one ':' then it must be a hostname/IPv4 and port
         We also need a canonical hostname so that matching works correctly.
       */

       if(wildcard)
          host=text;
       else
          host=CanonicaliseHost(text);

       if(*host=='[') {
	 if(!(p=strchr(host+1,']'))) goto hosterr;		
	 pointer->string=strndup(host+1,p-(host+1));
       }
       else if(colons==1)
	 {goto hosterr;}
       else {
	 pointer->string=strdup(host);
       }

       downcase(pointer->string);
       goto free_host;

      hosterr:
       errmsg=x_asprintf("Expecting a hostname without a port number, got '%s'.",text);
      free_host:
       if(host!=text)
	 free(host);
      }
     break;

   case HostAndPortOrNone:
    if(!*text || !strcasecmp(text,"none"))
      {
       pointer->string=NULL;
       break;
      }

    /*@fallthrough@*/

   case HostAndPort:
    if(!*text)
      {errmsg=strdup("Expecting a hostname (and port), got nothing.");}
    else
      {
       /*
         This is also tricky due to the IPv6 problem.
         We have to rely on the user using '[xxxx]:yyy' if they want an IPv6 address and port.
       */

       if(*text==':')
         {errmsg=x_asprintf("Expecting a hostname before the ':', got '%s'.",text);}
       else
         {
	  char *portstr=ExtractPort(text);

          if(portstr && (!isanumber(portstr) || atoi(portstr)<=0 || atoi(portstr)>65535))
            {errmsg=x_asprintf("Invalid port number %s.",portstr);}
          else
            {
             pointer->string= strdup(text);
             downcase(pointer->string);
            }
         }
      }
    break;

   case UserPass:
    if(!*text)
      {errmsg=strdup("Expecting a username and password, got nothing.");}
    else if(!strchr(text,':'))
      {errmsg=x_asprintf("Expecting a username and password, got '%s'.",text);}
    else
       pointer->string=Base64Encode(text,strlen(text));
    break;

   case Url:
    if(!*text || !strcasecmp(text,"none"))
       pointer->string=NULL;
    else
      {
       URL *tempUrl=SplitURL(text);
       if(!tempUrl->Protocol)
         {errmsg=x_asprintf("Expecting a URL, got '%s'.",text);}
       else
         {
          pointer->string=strdup(text);
         }
       FreeURL(tempUrl);
      }
    break;

   case UrlSpecification:
    if(!*text)
      {errmsg=strdup("Expecting a URL-SPECIFICATION, got nothing.");}
    else
      {
       char *p,*orgtext=text;
       char negated=0,nocase=0;
       char *proto=NULL,*host=NULL;
       int port=-1;
       char *path=NULL,*params=NULL,*args=NULL;

       /* !~ */

       for(;;)
         {
          if(*text=='!')
             negated=1;
          else if(*text=='~')
             nocase=1;
          else
             break;

          ++text;
         }

       /* protocol */

       if(!strncmp(text,"*://",4))
	 p=text+4;
       else if(!strncmp(text,"://",3))
	 p=text+3;
       else if((p=strstr(text,"://"))) {
	 proto=strndupa(text,p-text);
	 p+=3;

	 downcase(proto);
       }
       else
         goto urlspec_error;

       text=p;

       /* host */

       if(*text) {
	 if(*text=='*' && (!*(text+1) || *(text+1)==':' || *(text+1)=='/'))
	   p=text+1;
	 else if(*text!=':' && *text!='/') {
	   if(*text=='[' && (p=strchr(text+1,']')))
	     ++p;
	   else {
	     char *q;
	     p=strchrnul(text,':'); q=strchrnul(text,'/');
	     if(p>q) p=q;
	   }

	   host=strndupa(text,p-text);	   

	   downcase(host);
	 }

	 text=p;

	 /* port */
	 if(*text) {
	   if(*text==':') {
	     ++text;
	     if(isdigit(*text)) {
	       port=atoi(text);
	       p=text;
	       while(isdigit(*++p));
	     }
	     else if(*text==0 || *text=='/') {
	       port=0;
	       p=text;
	     }
	     else if(*text=='*')
	       p=text+1;
	     else
	       goto urlspec_error;
	   }

	   text=p;

	   /* path */
	   if(*text) {
	     char *semi=strchrnul(text,';');
	     char *ques=strchrnul(text,'?');
	     if(*text=='/') {
	       p=ques;
	       if(semi<ques) p=semi;
	       if(*(text+1)!='*' || (++text)+1!=p) {
		 char *temp1,*temp2;

		 temp1=(*p)?strndupa(text,p-text):text;
		 temp2=URLDecodeGeneric(temp1);
		 path=URLEncodePath(temp2);
		 free(temp2);
	       }
	     }
	     else if(text!=semi && text!=ques)
	       goto urlspec_error;

	     text=p;

	     /* params */

	     if(*text==';') {
	       ++text;
	       p=ques;
	       {
		 char *temp=(*p)?strndupa(text,p-text):text;
		 params=URLRecodeFormArgs(temp);
	       }
	     }

	     text=p;

	     /* args */

	     if(*text=='?') {
	       ++text;
	       args=URLRecodeFormArgs(text);
	     }
	   }
	 }
       }

       {
	 UrlSpec* urlspec;
	 size_t size=sizeof(UrlSpec);
	 unsigned short proto_offset=0,host_offset=0,path_offset=0,params_offset=0,args_offset=0;

	 if(proto) {
	   proto_offset=size;
	   size+=strlen(proto)+1;
	 }
	 if(host) {
	   host_offset=size;
	   size+=strlen(host)+1;
	 }
	 if(path) {
	   path_offset=size;
	   size+=strlen(path)+1;
	 }
	 if(params) {
	   params_offset=size;
	   size+=strlen(params)+1;
	 }
	 if(args) {
	   args_offset=size;
	   size+=strlen(args)+1;
	 }

	 urlspec=(UrlSpec*)malloc(size);
	 urlspec->negated=negated;
	 urlspec->nocase=nocase;
	 urlspec->proto=proto_offset;
	 urlspec->host=host_offset;
	 urlspec->port=port;
	 urlspec->path=path_offset;
	 urlspec->params=params_offset;
	 urlspec->args=args_offset;

	 if(proto)
	   strcpy(UrlSpecProto(urlspec),proto);
	 if(host)
	   strcpy(UrlSpecHost(urlspec),host);
	 if(path) {
	   strcpy(UrlSpecPath(urlspec),path);
	   free(path);
	 }
	 if(params) {
	   strcpy(UrlSpecParams(urlspec),params);
	   free(params);
	 }
	 if(args) {
	   strcpy(UrlSpecArgs(urlspec),args);
	   free(args);
	 }

	 pointer->urlspec=urlspec;
       }
       break;

      urlspec_error:
       errmsg=x_asprintf("Expecting a URL-SPECIFICATION, got this '%s'.",orgtext);
      }
    break;
   }

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide if a string is an integer.

  int isanumber Returns 1 if it is, 0 if not.

  const char *string The string that may be an integer.
  ++++++++++++++++++++++++++++++++++++++*/

static int isanumber(const char *string)
{
 int i=0;

 if(string[i]=='-' || string[i]=='+')
    i++;

 for(;string[i];i++)
    if(!isdigit(string[i]))
       return(0);

 return(1);
}
