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

#include "configpriv.h"
#include "wwwoffle.h"
#include "errors.h"
#include "misc.h"


#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

/* Local functions */

static char *filename_or_symlink_target(char *name);

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

 if(!errmsg)
   {
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

    if(line)
       free(line);
   }

 if(parse_file)
    fclose(parse_file);
 if(parse_file_org)
    fclose(parse_file_org);

 if(errmsg)
    RestoreBackupConfigFile();
 else
    PurgeBackupConfigFile(!first_time);

 first_time=0;

 if(errmsg)
   {
    char *newerrmsg=(char*)malloc(strlen(errmsg)+64+strlen(parse_name));
    sprintf(newerrmsg,"Configuration file syntax error at line %d in '%s'; %s\n",parse_line,parse_name,errmsg); /* Used in wwwoffle.c */
    free(errmsg);
    errmsg=newerrmsg;
   }

#if CONFIG_DEBUG_DUMP
 DumpConfigFile();
#endif

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
 int s,rename_failed=0;

 /* Initialise the parser and open the new file. */

 errmsg=InitParser();

 if(!errmsg)
   {
    names[0]=filename_or_symlink_target(parse_name);

    strcat(names[0],".new");

    file=fopen(names[0],"w");
    if(!file)
      {
       errmsg=(char*)malloc(64+strlen(names[0]));
       sprintf(errmsg,"Cannot open the configuration file '%s' for writing.",names[0]);
      }
   }

 /* Parse the file */

 if(!errmsg)
   {
    char *line=NULL;

    do
      {
       if((errmsg=ParseLine(&line)))
          break;

       if(parse_section==section && line)
         {
          char *url_str,*key_str,*val_str;
          char *copy;

          /* Insert a new entry for a non-existing item. */

          if(newentry && !preventry && !sameentry && !nextentry && !matched &&
             (parse_state==InsideSectionCurly || parse_state==InsideSectionIncluded))
            {
             char *copyentry=(char*)malloc(strlen(newentry)+1);
             strcpy(copyentry,newentry);

             if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
                break;

             if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
                                   &dummy,
                                   url_str,key_str,val_str)))
                break;

             fprintf(file,"\n# WWWOFFLE Configuration Edit Inserted: %s\n %s\n\n",RFC822Date(time(NULL),0),newentry);

             matched=1;

             free(copyentry);
            }

          copy=(char*)malloc(strlen(line)+1);
          strcpy(copy,line);

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
                char *copyentry=(char*)malloc(strlen(newentry)+1);
                strcpy(copyentry,newentry);

                if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
                   break;

                if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
                                      &dummy,
                                      url_str,key_str,val_str)))
                   break;

                fprintf(file,"\n# WWWOFFLE Configuration Edit Inserted: %s\n %s\n\n",RFC822Date(time(NULL),0),newentry);
                fprintf(file,"%s",line);

                matched=1;

                free(copyentry);
               }

             /* Insert a new entry after the current one */

             else if(newentry && preventry && !strcmp(thisentry,preventry))
               {
                char *copyentry=(char*)malloc(strlen(newentry)+1);
                strcpy(copyentry,newentry);

                if((errmsg=ParseItem(copyentry,&url_str,&key_str,&val_str)))
                   break;

                if((errmsg=ParseEntry(&CurrentConfig.sections[section]->itemdefs[item],
                                      &dummy,
                                      url_str,key_str,val_str)))
                   break;

                fprintf(file,"%s",line);
                fprintf(file,"\n# WWWOFFLE Configuration Edit Inserted: %s\n %s\n\n",RFC822Date(time(NULL),0),newentry);

                matched=1;

                free(copyentry);
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
                char *copyentry=(char*)malloc(strlen(newentry)+1);
                strcpy(copyentry,newentry);

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

                free(copyentry);
               }
             else
                fprintf(file,"%s",line);

             free(thisentry);
            }
          else
             fprintf(file,"%s",line);

          free(copy);
         }
       else if(line)
          fprintf(file,"%s",line);

       if(parse_state==StartSectionIncluded && !file_org)
         {
          names[parse_section+1]=filename_or_symlink_target(parse_name);
          strcat(names[parse_section+1],".new");

          file_org=file;
          file=fopen(names[parse_section+1],"w");
          if(!file)
            {
             errmsg=(char*)malloc(48+strlen(names[parse_section+1]));
             sprintf(errmsg,"Cannot open the included file '%s' for writing.",names[parse_section+1]);
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

    if(line)
       free(line);
   }

 if(!errmsg && !matched && (preventry || sameentry || nextentry))
   {
    char *whichentry=sameentry?sameentry:preventry?preventry:nextentry;

    errmsg=(char*)malloc(64+strlen(whichentry));
    sprintf(errmsg,"No entry to match '%s' was found to make the change.",whichentry);
   }

 if(file)
    fclose(file);
 if(file_org)
    fclose(file_org);
 if(parse_file)
    fclose(parse_file);
 if(parse_file_org)
    fclose(parse_file_org);

 for(s=0;s<=CurrentConfig.nsections;s++)
    if(names[s])
      {
       if(!errmsg)
         {
          char *name=(char*)malloc(strlen(names[s])+1);
          char *name_bak=(char*)malloc(strlen(names[s])+16);

          strcpy(name,names[s]);
          name[strlen(name)-4]=0;

          strcpy(name_bak,names[s]);
          strcpy(name_bak+strlen(name_bak)-3,"bak");

          if(rename(name,name_bak))
            {
             rename_failed++;
             PrintMessage(Warning,"Cannot rename '%s' to '%s' when modifying configuration entry [%!s].",name,name_bak);
            }
          if(rename(names[s],name))
            {
             rename_failed++;
             PrintMessage(Warning,"Cannot rename '%s' to '%s' when modifying configuration entry [%!s].",name[s],name);
            }

          free(name);
          free(name_bak);
         }
       else
          unlink(names[s]);

       free(names[s]);
      }

 if(rename_failed)
   {
    errmsg=(char*)malloc(120);
    sprintf(errmsg,"There were problems renaming files, check the error log (this might stop the change you tried to make).");
   }

 FreeConfigItem(dummy);

 free(names);

 return(errmsg);
}


/*++++++++++++++++++++++++++++++++++++++
  Find the real filename of a potential symbolic link.

  char *filename_or_symlink_target Returns the real file name.

  char *name The file name that may be a symbolic link.
  ++++++++++++++++++++++++++++++++++++++*/

static char *filename_or_symlink_target(char *name)
{
 struct stat buf;
 char linkname[PATH_MAX+1];
 char *result=NULL;

 if(!stat(name,&buf) && buf.st_mode&&S_IFLNK)
   {
    int linklen=0;

    if((linklen=readlink(name,linkname,PATH_MAX))!=-1)
      {
       linkname[linklen]=0;

       if(*linkname=='/')
         {
          result=(char*)malloc(linklen+8);
          strcpy(result,linkname);
         }
       else
         {
          char *p;
          result=(char*)malloc(strlen(name)+linklen+8);
          strcpy(result,name);
          p=result+strlen(result)-1;
          while(p>=result && *p!='/')
             p--;
          strcpy(p+1,linkname);
          CanonicaliseName(result);
         }
      }
   }

 if(!result)
   {
    result=(char*)malloc(strlen(name)+8);
    strcpy(result,name);
   }

 return(result);
}


/*++++++++++++++++++++++++++++++++++++++
  Initialise the file parser.

  char *InitParser Return an error string in case of error.
  ++++++++++++++++++++++++++++++++++++++*/

static char *InitParser(void)
{
 parse_name=CurrentConfig.name;
 parse_file=fopen(parse_name,"r");
 parse_line=0;

 if(!parse_file)
   {
    char *errmsg=(char*)malloc(64+strlen(parse_name));
    sprintf(errmsg,"Cannot open the configuration file '%s' for reading.",parse_name);
    return(errmsg);
   }

 parse_name_org=NULL;
 parse_file_org=NULL;
 parse_line_org=0;

 parse_section=-1;
 parse_item=-1;
 parse_state=OutsideSection;

 return(NULL);
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

 parse_line++;

 parse_item=-1;

 /* At the end of the file, finish, error or close included file. */

 if(!*line)
   {
    if(parse_state==OutsideSection)
       parse_state=Finished;
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
       errmsg=(char*)malloc(32);
       strcpy(errmsg,"Unexpected end of file.");
      }
   }
 else
   {
    char *l,*r;

    /* Trim the line. */

    l=*line;
    r=*line+strlen(*line)-1;

    while(isspace(*l))
       l++;

    while(r>l && isspace(*r))
       r--;
    r++;

    /* Outside of section, searching for a section. */

    if(parse_state==OutsideSection)
      {
       if(*l!='#' && *l!=0)
         {
          for(parse_section=0;parse_section<CurrentConfig.nsections;parse_section++)
             if(!strncmp(CurrentConfig.sections[parse_section]->name,l,r-l))
               {
                parse_state=StartSection;
                break;
               }

          if(parse_section==CurrentConfig.nsections)
            {
             errmsg=(char*)malloc(64+strlen(l));
             sprintf(errmsg,"Unrecognised text outside of section (not section label) '%s'.",l);
             parse_section=-1;
            }
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
          errmsg=(char*)malloc(48);
          strcpy(errmsg,"Start of section must be '{' or '['.");
         }
       else
         {
          errmsg=(char*)malloc(48);
          sprintf(errmsg,"Start of section '%c' has trailing junk.",*l);
         }
      }

    /* Inside a normal '{...}' section. */

    else if(parse_state==StartSectionCurly || parse_state==InsideSectionCurly)
      {
       parse_state=InsideSectionCurly;

       if(*l=='}' && (l+1)==r)
         {
          parse_state=OutsideSection;
          parse_section=-1;
         }
       else if(*l=='}')
         {
          errmsg=(char*)malloc(48);
          sprintf(errmsg,"End of section '%c' has trailing junk.",*l);
         }
      }

    /* Inside a include '[...]' section. */

    else if(parse_state==StartSectionSquare || parse_state==InsideSectionSquare)
      {
       parse_state=InsideSectionSquare;

       if(*l==']' && (l+1)==r)
         {
          parse_state=OutsideSection;
          parse_section=-1;
         }
       else if(*l==']')
         {
          errmsg=(char*)malloc(48);
          sprintf(errmsg,"End of section '%c' has trailing junk.",*l);
         }
       else if(*l!='#' && *l!=0)
         {
          char *rr;
          char *inc_parse_name;
          FILE *inc_parse_file;

          inc_parse_name=(char*)malloc(strlen(CurrentConfig.name)+(r-l)+1);

          strcpy(inc_parse_name,CurrentConfig.name);

          rr=inc_parse_name+strlen(inc_parse_name)-1;
          while(rr>inc_parse_name && *rr!='/')
             rr--;

          strncpy(rr+1,l,r-l);
          *((rr+1)+(r-l))=0;

          inc_parse_file=fopen(inc_parse_name,"r");

          if(!inc_parse_file)
            {
             errmsg=(char*)malloc(48+strlen(inc_parse_name));
             sprintf(errmsg,"Cannot open the included file '%s' for reading.",inc_parse_name);
            }
          else
            {
             parse_state=StartSectionIncluded;

             parse_name_org=parse_name;
             parse_file_org=parse_file;
             parse_line_org=parse_line;

             parse_name=inc_parse_name;
             parse_file=inc_parse_file;
             parse_line=0;
            }
         }
      }

    else if(parse_state==StartSectionIncluded || parse_state==InsideSectionIncluded)
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
    char *l,*r;

    l=line;

    while(isspace(*l))
       l++;

    if(!*l || *l=='#')
       return(NULL);

    r=line+strlen(line)-1;

    while(r>l && isspace(*r))
       *r--=0;

    if(*l=='<')
      {
       char *uu;

       uu=url=l+1;
       while(*uu && *uu!='>')
          uu++;
       if(!*uu)
         {
          char *errmsg=(char*)malloc(32);
          strcpy(errmsg,"No '>' to match the '<'.");
          return(errmsg);
         }

       *uu=0;
       key=uu+1;
       while(*key && isspace(*key))
          key++;
       if(!*key)
         {
          char *errmsg=(char*)malloc(48);
          strcpy(errmsg,"No configuration entry following the '<...>'.");
          return(errmsg);
         }
      }
    else
       key=l;

    for(parse_item=0;parse_item<CurrentConfig.sections[parse_section]->nitemdefs;parse_item++)
       if(!*CurrentConfig.sections[parse_section]->itemdefs[parse_item].name ||
          !strncmp(CurrentConfig.sections[parse_section]->itemdefs[parse_item].name,key,strlen(CurrentConfig.sections[parse_section]->itemdefs[parse_item].name)))
         {
          char *ll;

          if(*CurrentConfig.sections[parse_section]->itemdefs[parse_item].name)
            {
             ll=key+strlen(CurrentConfig.sections[parse_section]->itemdefs[parse_item].name);

             if(*ll && *ll!='=' && !isspace(*ll))
                continue;
            }
          else if(CurrentConfig.sections[parse_section]->itemdefs[parse_item].key_type==UrlSpecification)
            {
             char *equal;

             ll=key;

             while((equal=strchr(ll,'=')))
               {
                ll=equal;
                if(--equal>key && isspace(*equal))
                  {
                   while(isspace(*equal))
                      *equal--=0;
                   break;
                  }
                ll++;
               }

             while(*ll && *ll!='=' && !isspace(*ll))
                ll++;
            }
          else
            {
             ll=key;
             while(*ll && *ll!='=' && !isspace(*ll))
                ll++;
            }

          if(CurrentConfig.sections[parse_section]->itemdefs[parse_item].url_type==0 && url)
            {
             char *errmsg=(char*)malloc(48);
             strcpy(errmsg,"No URL context '<...>' allowed for this entry.");
             return(errmsg);
            }

          if(CurrentConfig.sections[parse_section]->itemdefs[parse_item].val_type==None)
            {
             if(strchr(ll,'='))
               {
                char *errmsg=(char*)malloc(40);
                strcpy(errmsg,"Equal sign seen but not expected.");
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
                char *errmsg=(char*)malloc(40);
                strcpy(errmsg,"No equal sign seen but expected.");
                return(errmsg);
               }

             *ll=0;
             if(!*key)
               {
                char *errmsg=(char*)malloc(48);
                strcpy(errmsg,"Nothing to the left of the equal sign.");
                return(errmsg);
               }

             val++;
             while(isspace(*val))
                val++;
            }

          *url_str=url;
          *key_str=key;
          *val_str=val;

          break;
         }

    if(parse_item==CurrentConfig.sections[parse_section]->nitemdefs)
      {
       char *errmsg=(char*)malloc(32+strlen(l));
       sprintf(errmsg,"Unrecognised entry '%s'.",l);
       return(errmsg);
      }
   }

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse an entry from the file.

  char *ParseEntry Return a string containing an error message in case of error.

  ConfigItemDef *itemdef The item definition for the item in the section.

  ConfigItem *item The item to add the entry to.

  char *url_str The string for the URL specification.

  char *key_str The string for the key.

  char *val_str The string to the value.
  ++++++++++++++++++++++++++++++++++++++*/

static char *ParseEntry(ConfigItemDef *itemdef,ConfigItem *item,char *url_str,char *key_str,char *val_str)
{
 UrlSpec *url=NULL;
 KeyOrValue key,val;
 char *errmsg=NULL;

 key.string=NULL;
 val.string=NULL;

 if(itemdef->same_key==0 && itemdef->url_type==0 && (*item) && (*item)->nentries)
   {
    errmsg=(char*)malloc(32+strlen(key_str));
    sprintf(errmsg,"Duplicated entry: '%s'.",key_str);
    return(errmsg);
   }

 if(!itemdef->url_type || !url_str)
    url=NULL;
 else
    if((errmsg=ParseKeyOrValue(url_str,UrlSpecification,(KeyOrValue*)&url)))
       return(errmsg);

 if(itemdef->key_type==Fixed)
   {
    if(strcmp(key_str,itemdef->name))
      {
       errmsg=(char*)malloc(32+strlen(key_str));
       sprintf(errmsg,"Unexpected key string: '%s'.",key_str);
       if(url) free(url);
       return(errmsg);
      }
    key.string=itemdef->name;
   }
 else
    if((errmsg=ParseKeyOrValue(key_str,itemdef->key_type,&key)))
      {
       if(url) free(url);
       return(errmsg);
      }

 if(!val_str)
    val.string=NULL;
 else if(itemdef->val_type==None)
    val.string=NULL;
 else
    if((errmsg=ParseKeyOrValue(val_str,itemdef->val_type,&val)))
      {
       if(url) free(url);
       FreeKeyOrValue(&key,itemdef->key_type);
       return(errmsg);
      }

 if(!item)
   {
    if(url) free(url);
    FreeKeyOrValue(&key,itemdef->key_type);
    FreeKeyOrValue(&val,itemdef->val_type);
    return(NULL);
   }

 if(!(*item))
   {
    *item=(ConfigItem)malloc(sizeof(struct _ConfigItem));
    (*item)->itemdef=itemdef;
    (*item)->nentries=0;
    (*item)->url=NULL;
    (*item)->key=NULL;
    (*item)->val=NULL;
    (*item)->def_val=NULL;
   }
 if(!(*item)->nentries)
   {
    (*item)->nentries=1;
    if(itemdef->url_type!=0)
       (*item)->url=(UrlSpec**)malloc(sizeof(UrlSpec*));
    (*item)->key=(KeyOrValue*)malloc(sizeof(KeyOrValue));
    if(itemdef->val_type!=None)
       (*item)->val=(KeyOrValue*)malloc(sizeof(KeyOrValue));
   }
 else
   {
    (*item)->nentries++;
    if(itemdef->url_type!=0)
       (*item)->url=(UrlSpec**)realloc((void*)(*item)->url,(*item)->nentries*sizeof(UrlSpec*));
    (*item)->key=(KeyOrValue*)realloc((void*)(*item)->key,(*item)->nentries*sizeof(KeyOrValue));
    if(itemdef->val_type!=None)
       (*item)->val=(KeyOrValue*)realloc((void*)(*item)->val,(*item)->nentries*sizeof(KeyOrValue));
   }

 if(itemdef->url_type!=0)
    (*item)->url[(*item)->nentries-1]=url;
 (*item)->key[(*item)->nentries-1]=key;
 if(itemdef->val_type!=None)
    (*item)->val[(*item)->nentries-1]=val;

 return(NULL);
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
      {errmsg=(char*)malloc(56);strcpy(errmsg,"Expecting a maximum server count, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a maximum server count, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<=0 || pointer->integer>MAX_SERVERS)
         {errmsg=(char*)malloc(48);sprintf(errmsg,"Invalid maximum server count: %d.",pointer->integer);}
      }
    break;

   case CfgMaxFetchServers:
    if(!*text)
      {errmsg=(char*)malloc(56);strcpy(errmsg,"Expecting a maximum fetch server count, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(56+strlen(text));sprintf(errmsg,"Expecting a maximum fetch server count, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<=0 || pointer->integer>MAX_FETCH_SERVERS)
         {errmsg=(char*)malloc(48);sprintf(errmsg,"Invalid maximum fetch server count: %d.",pointer->integer);}
      }
    break;

   case CfgLogLevel:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a log level, got nothing.");}
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
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a log level, got '%s'.",text);}
    break;

   case Boolean:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a Boolean, got nothing.");}
    else if(!strcasecmp(text,"yes") || !strcasecmp(text,"true"))
       pointer->integer=1;
    else if(!strcasecmp(text,"no") || !strcasecmp(text,"false"))
       pointer->integer=0;
    else
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a Boolean, got '%s'.",text);}
    break;

   case PortNumber:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a port number, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a port number, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<=0 || pointer->integer>65535)
         {errmsg=(char*)malloc(32);sprintf(errmsg,"Invalid port number %d.",pointer->integer);}
      }
    break;

   case AgeDays:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting an age in days, got nothing.");}
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
         {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting an age in days, got '%s'.",text);}
      }
    break;

   case TimeSecs:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a time in seconds, got nothing.");}
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
         {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a time in seconds, got '%s'.",text);}
      }
    break;

   case CacheSize:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a cache size in MB, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a cache size in MB, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<0)
         {errmsg=(char*)malloc(48);sprintf(errmsg,"Invalid cache size %d.",pointer->integer);}
      }
    break;

   case FileSize:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a file size in kB, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a file size in kB, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<0)
         {errmsg=(char*)malloc(48);sprintf(errmsg,"Invalid file size %d.",pointer->integer);}
      }
    break;

   case Percentage:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a percentage, got nothing.");}
    else if(!isanumber(text))
      {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a percentage, got '%s'.",text);}
    else
      {
       pointer->integer=atoi(text);
       if(pointer->integer<0)
         {errmsg=(char*)malloc(48);sprintf(errmsg,"Invalid percentage %d.",pointer->integer);}
      }
    break;

   case UserId:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a username or uid, got nothing.");}
    else
      {
       int uid;
       struct passwd *userInfo=getpwnam(text);
       if(userInfo)
          uid=userInfo->pw_uid;
       else
         {
          if(sscanf(text,"%d",&uid)!=1)
            {errmsg=(char*)malloc(32+strlen(text));sprintf(errmsg,"Invalid user %s.",text);}
          else if(uid!=-1 && !getpwuid(uid))
            {errmsg=(char*)malloc(32);sprintf(errmsg,"Unknown user id %d.",uid);}
         }
       pointer->integer=uid;
      }
    break;

   case GroupId:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a group name or gid, got nothing.");}
    else
      {
       int gid;
       struct group *groupInfo=getgrnam(text);
       if(groupInfo)
          gid=groupInfo->gr_gid;
       else
         {
          if(sscanf(text,"%d",&gid)!=1)
            {errmsg=(char*)malloc(32+strlen(text));sprintf(errmsg,"Invalid group %s.",text);}
          else if(gid!=-1 && !getgrgid(gid))
            {errmsg=(char*)malloc(32);sprintf(errmsg,"Unknown group id %d.",gid);}
         }
       pointer->integer=gid;
      }
    break;

   case String:
    if(!*text || !strcasecmp(text,"none"))
       pointer->string=NULL;
    else
      {
       pointer->string=(char*)malloc(strlen(text)+1);
       strcpy(pointer->string,text);
      }
    break;

   case PathName:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a pathname, got nothing.");}
    else if(*text!='/')
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting an absolute pathname, got '%s'.",text);}
    else
      {
       pointer->string=(char*)malloc(strlen(text)+1);
       strcpy(pointer->string,text);
      }
    break;

   case FileExt:
    if(!*text)
      {errmsg=(char*)malloc(40);strcpy(errmsg,"Expecting a file extension, got nothing.");}
    else if(*text!='.')
      {errmsg=(char*)malloc(40+strlen(text));sprintf(errmsg,"Expecting a file extension, got '%s'.",text);}
    else
      {
       pointer->string=(char*)malloc(strlen(text)+1);
       strcpy(pointer->string,text);
      }
    break;

   case FileMode:
    if(!*text)
      {errmsg=(char*)malloc(40);strcpy(errmsg,"Expecting a file permissions mode, got nothing.");}
    else if(!isanumber(text) || *text!='0')
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a file permissions mode, got '%s'.",text);}
    else
       sscanf(text,"%o",&pointer->integer);
    break;

   case MIMEType:
     if(!*text)
       {errmsg=(char*)malloc(40);strcpy(errmsg,"Expecting a MIME Type, got nothing.");}
     else
       {
        char *slash=strchr(text,'/');
        if(!slash)
          {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a MIME Type/Subtype, got '%s'.",text);}
        pointer->string=(char*)malloc(strlen(text)+1);
        strcpy(pointer->string,text);
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
      {errmsg=(char*)malloc(40);strcpy(errmsg,"Expecting a hostname, got nothing.");}
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
             colons++;
          p++;
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

       if(colons==1 && *host!='[')
         {errmsg=(char*)malloc(56+strlen(text));sprintf(errmsg,"Expecting a hostname without a port number, got '%s'.",text);}
       else
         {
          pointer->string=(char*)malloc(strlen(host)+1);
          if(*host=='[')
            {
             strcpy(pointer->string,host+1);
             pointer->string[strlen(pointer->string)-1]=0;
            }
          else
             strcpy(pointer->string,host);
          for(p=pointer->string;*p;p++)
             *p=tolower(*p);
         }

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
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a hostname (and port), got nothing.");}
    else
      {
       char *hoststr,*portstr;

       /*
         This is also tricky due to the IPv6 problem.
         We have to rely on the user using '[xxxx]:yyy' if they want an IPv6 address and port.
       */

       SplitHostPort(text,&hoststr,&portstr);
       RejoinHostPort(text,hoststr,portstr);

       if(*text==':')
         {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a hostname before the ':', got '%s'.",text);}
       else
         {
          if(portstr && (!isanumber(portstr) || atoi(portstr)<=0 || atoi(portstr)>65535))
            {errmsg=(char*)malloc(32+strlen(portstr));sprintf(errmsg,"Invalid port number %s.",portstr);}
          else
            {
             char *p;
             pointer->string=(char*)malloc(strlen(text)+1);
             strcpy(pointer->string,text);
             for(p=pointer->string;*p;p++)
                *p=tolower(*p);
            }
         }
      }
    break;

   case UserPass:
    if(!*text)
      {errmsg=(char*)malloc(48);strcpy(errmsg,"Expecting a username and password, got nothing.");}
    else if(!strchr(text,':'))
      {errmsg=(char*)malloc(48+strlen(text));sprintf(errmsg,"Expecting a username and password, got '%s'.",text);}
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
         {errmsg=(char*)malloc(32+strlen(text));sprintf(errmsg,"Expecting a URL, got '%s'.",text);}
       else
         {
          pointer->string=(char*)malloc(strlen(text)+1);
          strcpy(pointer->string,text);
         }
       FreeURL(tempUrl);
      }
    break;

   case UrlSpecification:
    if(!*text)
      {errmsg=(char*)malloc(64);strcpy(errmsg,"Expecting a URL-SPECIFICATION, got nothing.");}
    else
      {
       char *p,*orgtext=text;

       pointer->urlspec=(UrlSpec*)malloc(sizeof(UrlSpec));
       pointer->urlspec->null=0;
       pointer->urlspec->negated=0;
       pointer->urlspec->nocase=0;
       pointer->urlspec->proto=0;
       pointer->urlspec->host=0;
       pointer->urlspec->port=-1;
       pointer->urlspec->path=0;
       pointer->urlspec->args=0;

       /* !~ */

       while(*text)
         {
          if(*text=='!')
             pointer->urlspec->negated=1;
          else if(*text=='~')
             pointer->urlspec->nocase=1;
          else
             break;

          text++;
         }

       /* protocol */

       if(!strncmp(text,"*://",4))
          p=text+4;
       else if(!strncmp(text,"://",3))
          p=text+3;
       else if((p=strstr(text,"://")))
         {
          pointer->urlspec->proto=sizeof(UrlSpec);

          pointer->urlspec=(UrlSpec*)realloc((void*)pointer->urlspec,
                                             pointer->urlspec->proto+(p-text)+1);

          strncpy(UrlSpecProto(pointer->urlspec),text,p-text);
          *(UrlSpecProto(pointer->urlspec)+(p-text))=0;
          p+=3;
         }
       else
         {errmsg=(char*)malloc(64+strlen(orgtext));sprintf(errmsg,"Expecting a URL-SPECIFICATION, got this '%s'.",orgtext);
          free(pointer->urlspec); break;}

       if(pointer->urlspec->proto)
         {
          char *q;

          for(q=UrlSpecProto(pointer->urlspec);*q;q++)
             *q=tolower(*q);
         }

       text=p;

       /* host */

       if(*text=='*' && (*(text+1)=='/' || !*(text+1)))
          p=text+1;
       else if(*text==':')
          p=text;
       else if(*text=='[' && (p=strchr(text,']')))
         {
          p++;
          pointer->urlspec->host=1;
         }
       else if((p=strchr(text,':')) && p<strchr(text,'/'))
         {
          pointer->urlspec->host=1;
         }
       else if((p=strchr(text,'/')))
         {
          pointer->urlspec->host=1;
         }
       else if(*text)
         {
          p=text+strlen(text);
          pointer->urlspec->host=1;
         }
       else
          p=text;

       if(pointer->urlspec->host)
         {
          char *q;

          pointer->urlspec->host=(unsigned short)(sizeof(UrlSpec)+
                                                  (pointer->urlspec->proto?1+strlen(UrlSpecProto(pointer->urlspec)):0));

          pointer->urlspec=(UrlSpec*)realloc((void*)pointer->urlspec,
                                             pointer->urlspec->host+(p-text)+1);

          strncpy(UrlSpecHost(pointer->urlspec),text,p-text);
          *(UrlSpecHost(pointer->urlspec)+(p-text))=0;

          for(q=UrlSpecHost(pointer->urlspec);*q;q++)
             *q=tolower(*q);
         }

       text=p;

       /* port */

       if(*text==':' && isdigit(*(text+1)))
         {
          pointer->urlspec->port=atoi(text+1);
          p=text+1;
          while(isdigit(*p))
             p++;
         }
       else if(*text==':' && (*(text+1)=='/' || *(text+1)==0))
         {
          pointer->urlspec->port=0;
          p=text+1;
         }
       else if(*text==':' && *(text+1)=='*')
         {
          p=text+2;
         }
       else if(*text==':')
         {errmsg=(char*)malloc(64+strlen(orgtext));sprintf(errmsg,"Expecting a URL-SPECIFICATION, got this '%s'.",orgtext);
          free(pointer->urlspec); break;}

       text=p;

       /* path */

       if(!*text)
          ;
       else if(*text=='?')
          ;
       else if(*text=='/' && (p=strchr(text,'?')))
         {
          if(strncmp(text,"/*?",3))
             pointer->urlspec->path=1;
         }
       else if(*text=='/')
         {
          p=text+strlen(text);
          if(strcmp(text,"/*"))
             pointer->urlspec->path=1;
         }
       else
         {errmsg=(char*)malloc(64+strlen(orgtext));sprintf(errmsg,"Expecting a URL-SPECIFICATION, got this '%s'.",orgtext);
          free(pointer->urlspec); break;}

       if(pointer->urlspec->path)
         {
          char *temppath,*path,oldp;

          oldp=*p;
          *p=0;
          temppath=URLDecodeGeneric(text);
          *p=oldp;

          path=URLEncodePath(temppath);
          free(temppath);

          pointer->urlspec->path=(unsigned short)(sizeof(UrlSpec)+
                                                  (pointer->urlspec->proto?1+strlen(UrlSpecProto(pointer->urlspec)):0)+
                                                  (pointer->urlspec->host ?1+strlen(UrlSpecHost (pointer->urlspec)):0));

          pointer->urlspec=(UrlSpec*)realloc((void*)pointer->urlspec,
                                             pointer->urlspec->path+strlen(path)+1);

          strcpy(UrlSpecPath(pointer->urlspec),path);

          free(path);
         }

       text=p;

       /* args */

       if(!*text)
          ;
       else if(*text=='?' && !*(text+1))
         {
          p=text+1;

          pointer->urlspec->args=1;
         }
       else if(*text=='?')
         {
          p=text+1;

          pointer->urlspec->args=1;
         }
       else
         {errmsg=(char*)malloc(64+strlen(orgtext));sprintf(errmsg,"Expecting a URL-SPECIFICATION, got this '%s'.",orgtext);
          free(pointer->urlspec); break;}

       if(pointer->urlspec->args)
         {
          char *args=NULL;

          if(*p)
             args=URLRecodeFormArgs(p);
          else
             args=p;

          pointer->urlspec->args=(unsigned short)(sizeof(UrlSpec)+
                                                  (pointer->urlspec->proto?1+strlen(UrlSpecProto(pointer->urlspec)):0)+
                                                  (pointer->urlspec->host ?1+strlen(UrlSpecHost (pointer->urlspec)):0)+
                                                  (pointer->urlspec->path ?1+strlen(UrlSpecPath (pointer->urlspec)):0));

          pointer->urlspec=(UrlSpec*)realloc((void*)pointer->urlspec,
                                             pointer->urlspec->args+strlen(args)+1);

          strcpy(UrlSpecArgs(pointer->urlspec),args);

          if(*args)
             free(args);
         }
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
