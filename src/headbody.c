/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/headbody.c 1.19 2002/10/13 14:42:04 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7g.
  Header and Body handling functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/

/* Note: large portions of this file have been rewritten by Paul Rombouts.
   See README.par for more information.
*/

#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "misc.h"
#include "headbody.h"


static int sort_qval(HeaderListItem *a,HeaderListItem *b);


/*++++++++++++++++++++++++++++++++++++++
  Create a new Header structure.

  int CreateHeader Returns 1 if OK, -1 if malformed, 0 if line is empty.

  char *line The top line in the original header.

  int type The type of header, request=1, reply=0;

  Head **head Returns the new header structure.
  ++++++++++++++++++++++++++++++++++++++*/

int CreateHeader(const char *line,int type,Header **head)
{
 const char *text,*p,*end;
 Header *new=(Header*)malloc(sizeof(Header));

 *head=new;

 new->type=type;
 new->method=NULL;
 new->url=NULL;
 new->status=0;
 new->note=NULL;
 new->version=NULL;

 new->n=0;
 new->line=NULL;

 /*trim the line*/
 end=strchrnul(line,0);
 do {if(--end<line) return 0;} while(isspace(*end));
 ++end;

 if(isspace(*line)) return -1;

 /* Parse the original header. */

 text=p=line;
 if(type==1)
   {
                                        /* GET http://www/xxx HTTP/1.0 */
    while(*++p && !isspace(*p));
    new->method=STRDUP2(text,p);        /*    ^                        */
    if(!*p) return -1;
    do {if(!*++p) return -1;} while(isspace(*p));

    text=p;                             /*     ^                       */
    while(*++p && !isspace(*p));
    new->url=STRDUP2(text,p);           /*                   ^         */
    if(!*p) goto defaultversion;
    do {if(!*++p) goto defaultversion;} while(isspace(*p));
                                        /*                    ^        */
    new->version=STRDUP2(p,end);
    goto finish_req;

   defaultversion:
    new->version=strdup("HTTP/1.0");

   finish_req:
    upcase(new->method);
   }
 else
   {
                                        /* HTTP/1.1 200 OK or something */
    while(*++p && !isspace(*p));
    if(*(p-1)==':')
      return AddToHeaderRaw(new,line);
    new->version=STRDUP2(text,p);       /*         ^                    */
    if(!*p) return -1;
    do {if(!*++p) return -1;} while(isspace(*p));
                                        /*          ^                   */
    new->status=atoi(p);

    while(isdigit(*p)) {if(!*++p) goto finish_rep;}
                                        /*             ^                */
    while(isspace(*p)) {if(!*++p) goto finish_rep;}
                                        /*              ^               */
    new->note=STRDUP2(p,end);

   finish_rep:
   }

 upcase(new->version);

 return 1;
}


/*++++++++++++++++++++++++++++++++++++++
  Add a specified key and value to the header structure.

  Header *head The header structure to add to.

  char *key The key to addL.

  char *val The value to add.
  ++++++++++++++++++++++++++++++++++++++*/

void AddToHeader(Header *head,const char *key,const char *val)
{
  int k = head->n++;

  /* To reduce the number of realloc calls, expand key and val arrays in steps of 8 */
  if((k&7) == 0)  /* k&7 == k mod 8 */
    {
      head->line=(KeyValuePair *)realloc((void*)head->line,sizeof(KeyValuePair)*(k+8));
    }

  head->line[k].key=strdup(key);
  head->line[k].val=strdup(val);
}


/*++++++++++++++++++++++++++++++++++++++
  Add a raw line to a header.

  int AddToHeaderRaw Returns 1 if OK, 0 if line is empty, -1 if malformed.

  Header *head The header to add the line to.

  char *line The raw line of data.
  ++++++++++++++++++++++++++++++++++++++*/

int AddToHeaderRaw(Header *head,const char *line)
{
 const char *key=NULL,*keyend=line,*val,*end=strchrnul(line,0);

 /* trim line */

 do {if(--end<line) return 0;} while(isspace(*end));
 ++end;

 if(!isspace(*line))
   {
    /* split line */
   
    key=line;
   
    while(*keyend!=':') {if(!*++keyend) return -1;}
   
 }

 val=keyend+1;
 while(val<end && isspace(*val)) ++val;   /* trim value */

 /* Add to the header */

 {
   int strlen_val=end-val;

   if(key)
     {
       int k = head->n++;

       /* To reduce the number of realloc calls, expand key and val arrays in steps of 8 */
       if((k&7) == 0)  /* k&7 == k mod 8 */
	 {
	   head->line=(KeyValuePair *)realloc((void*)head->line,sizeof(KeyValuePair)*(k+8));
	 }

       head->line[k].key=STRDUP2(key,keyend);
       head->line[k].val=strndup(val,strlen_val);
     }
   else
     {
       /* Append text to the last header line */

       if(head->n==0 || !head->line[head->n-1].key)
	 return -1; /* weird: there must be a last header... */

       {
	 int strlen_last=strlen(head->line[head->n-1].val);
	 char *p;
	 head->line[head->n-1].val=(char*)realloc((void*)head->line[head->n-1].val,strlen_last+strlen_val+2);
	 p= head->line[head->n-1].val + strlen_last;
	 if(strlen_last) *p++=' ';
	 p=mempcpy(p,val,strlen_val);
	 *p=0;
       }
     }
 }

 return(1);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the specified key and its values.

  Header *head The header to remove from.

  char* key The key to look for and remove.
  ++++++++++++++++++++++++++++++++++++++*/

void RemoveFromHeader(Header *head,const char* key)
{
 int i;

 for(i=0;i<head->n;++i)
   if(head->line[i].key && !strcasecmp(head->line[i].key,key))
     RemoveFromHeaderIndexed(head,i);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the specified key and value pair from a header structure.

  Header *head The header to remove from.

  char* key The key to look for and remove.

  char *val The value to look for and remove.
  ++++++++++++++++++++++++++++++++++++++*/

void RemoveFromHeader2(Header *head,const char* key,const char *val)
{
  int i;

  for(i=0;i<head->n;++i) {
    if(head->line[i].key && !strcasecmp(head->line[i].key,key)) {
      char *str=head->line[i].val,*p=str,*q,*r=str;
      int strlen_val=strlen(val);

      for(;;) {
	for(;;++p) {
	  if(!*p) goto nexti;
	  if(!isspace(*p)) break;
	}

	q=p;
	while(!strncasecmp(q,val,strlen_val)) {
	  while(*q!=',') {if(!*++q) goto chop_line;}
	  do {if(!*++q) goto chop_line;} while(isspace(*q));
	}

	if(q!=p)
	  {char *t=p; while((*t++=*q++));}
	  
	while(*p!=',') {if(!*++p) goto nexti;}

	r=p++;  /* remember ending of previous item */
      }

    chop_line:
      do {
	if(--r<str) {
	  RemoveFromHeaderIndexed(head,i);
	  goto nexti;
	}
      } while(isspace(*r));
      *++r=0;
    }
  nexti:
  }
}


/* Added by Paul Rombouts */
/*++++++++++++++++++++++++++++++++++++++
  Repace the value of an existing key with a new one, or add a new key and value combination.
  Header *head The header structure to modify.
  char *key The key.
  char *val The replacement value.
  ++++++++++++++++++++++++++++++++++++++*/
void ReplaceInHeader(Header *head,const char *key,const char *val)
{
 int i;

 for(i=0;i<head->n;++i)
   if(head->line[i].key && !strcasecmp(head->line[i].key,key))
     goto found;

 AddToHeader(head,key,val);
 return;

found:
 free(head->line[i].val);
 head->line[i].val=strdup(val);
 /* remove any remaining values with the same key */
 ++i;
 for(;i<head->n;++i)
   if(head->line[i].key && !strcasecmp(head->line[i].key,key))
     RemoveFromHeaderIndexed(head,i);
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a specified key.

  char *GetHeader Returns the (first) value for the header key or NULL if none.

  Header *head The header to search through.

  char* key The key to look for.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetHeader(Header *head,const char* key)
{
 int i;

 for(i=0;i<head->n;++i)
    if(head->line[i].key && !strcasecmp(head->line[i].key,key))
       return(head->line[i].val);

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a specified key and value pair.

  char *GetHeader2 Returns the value for the header key or NULL if none.

  Header *head The header to search through.

  char* key The key to look for.

  char *val The value to look for (which may be in a list).
  ++++++++++++++++++++++++++++++++++++++*/

char *GetHeader2(Header *head,const char* key,const char *val)
{
 int strlen_val=strlen(val);
 int i;

 for(i=0;i<head->n;++i) {
   if(head->line[i].key && !strcasecmp(head->line[i].key,key)) {
     char *p=head->line[i].val;

     for(;;) {
       for(;;++p) {
	 if(!*p) goto nexti;
	 if(!isspace(*p)) break;
       }

       if(!strncasecmp(p,val,strlen_val)) return p;

       while(*p!=',') {if(!*++p) goto nexti;}
       ++p;
     }
   }
 nexti:
 }

 return NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a specified key and sub-key.

  char *GetHeader2Val Returns a pointer to the value for the sub-key or NULL if none.

  Header *head The header to search through.

  char* key The key to look for.

  char *subkey The sub-key to look for (which must be followed by '=' in the headerline).
  ++++++++++++++++++++++++++++++++++++++*/

char *GetHeader2Val(Header *head,const char* key,const char *subkey)
{
 int strlen_subkey=strlen(subkey);
 int i;

 for(i=0;i<head->n;++i) {
   if(head->line[i].key && !strcasecmp(head->line[i].key,key)) {
     char *p=head->line[i].val;

     for(;;) {
       for(;;++p) {
	 if(!*p) goto nexti;
	 if(!isspace(*p)) break;
       }

       if(!strncasecmp(p,subkey,strlen_subkey)) {
	 p+=strlen_subkey;
	 for(;;++p) {
	   if(!*p) goto nexti;
	   if(!isspace(*p)) break;
	 }
	 if(*p=='=') {
	   while(*++p && isspace(*p));
	   return p;
	 }
       }

       while(*p!=',') {if(!*++p) goto nexti;}
       ++p;
     }
   }
 nexti:
 }

 return NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a specified key.

  char *GetHeader Returns the all the values combined for the header key or NULL if empty or none.

  Header *head The header to search through.

  char* key The key to look for.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetHeaderCombined(Header *head,const char* key)
{
  int i,length=-1;
  char *p,*val;

  for(i=0;i<head->n;++i)
    if(head->line[i].key && !strcasecmp(head->line[i].key,key)) {
      length+=1+strlen(head->line[i].val);
    }

  if(length==-1) return NULL;

  val=(char *)malloc(length+1);

  p=val-1;
  for(i=0;i<head->n;++i)
    if(head->line[i].key && !strcasecmp(head->line[i].key,key)) {
      if(p<val)
	p=val;
      else
	*p++=',';
      p=stpcpy(p,head->line[i].val);
    }

  return val;
}


/*++++++++++++++++++++++++++++++++++++++
  Return a string that contains the whole of the header.

  char *HeaderString Returns the header as a string.

  Header *head The header structure to convert.
  int *size    Returns the size of the header.
  ++++++++++++++++++++++++++++++++++++++*/

char *HeaderString(Header *head, int *size)
{
  char *str,*p;
  int str_len,i;

  if(head->type==1) {  /* request head */
    str_len=4;   /* "\r\n\r\n" */
    if(head->method) {
      str_len+=strlen(head->method)+1;  /* +" " */
      if(head->url) {
	str_len+=strlen(head->url)+1;  /* +" " */
	if(head->version) str_len+=strlen(head->version);
      }
    }
  }
  else {  /* reply head */
    str_len= ((head->version)?strlen(head->version):8) + 9;  /* "HTTP/1.0" + " 200 \r\n\r\n" */
    if(head->note) str_len+=strlen(head->note);
  }

  for(i=0;i<head->n;++i)
    if(head->line[i].key)
      str_len += strlen(head->line[i].key) + strlen(head->line[i].val) + 4;

  if(size) *size= str_len;
  str=p=(char*)malloc(str_len+1);

  if(head->type==1) {  /* request head */
    if(head->method) {
      p= stpcpy(p,head->method);
      *p++ = ' ';
      if(head->url) {
	p= stpcpy(p,head->url);
	*p++ = ' ';
	if(head->version) p= stpcpy(p,head->version);
      }
    }
  }
  else {  /* reply head */
    p= stpcpy(p,(head->version)?head->version:"HTTP/1.0");
    *p++ = ' ';
    {
      char status[12];
      if(head->status>=100 && sprintf(status,"%3d",head->status)==3)
	p=stpcpy(p,status);
      else
	p=stpcpy(p,"502");
    }
    *p++ = ' ';
    if(head->note) p= stpcpy(p,head->note);
  }

  p= stpcpy(p,"\r\n");

  for(i=0;i<head->n;++i)
    if(head->line[i].key)
      p= stpcpy(stpcpy(stpcpy(stpcpy(p,head->line[i].key),": "),head->line[i].val),"\r\n");

  stpcpy(p,"\r\n");

  return(str);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a header structure.

  Header *head The header structure to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeHeader(Header *head)
{
 int i;

 if(head->method)  free(head->method);
 if(head->url)     free(head->url);
 if(head->note)    free(head->note);
 if(head->version) free(head->version);

 for(i=0;i<head->n;++i)
   if(head->line[i].key)
     {
       free(head->line[i].key);
       free(head->line[i].val);
     }

 if(head->line) free(head->line);

 free(head);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a new Body structure.

  Header *CreateBody Returns the new body structure.

  int length The length of the body;
  ++++++++++++++++++++++++++++++++++++++*/

/* Body *CreateBody(int length)
{
 Body *new=(Body*)malloc(sizeof(Body)+length+1);

 new->length=length;

 return(new);
} */


/*++++++++++++++++++++++++++++++++++++++
  Free a body structure.

  Body *body The body structure to free.
  ++++++++++++++++++++++++++++++++++++++*/

/* void FreeBody(Body *body)
{
 free(body);
} */


/*++++++++++++++++++++++++++++++++++++++
  Construct a list of items from a header.

  HeaderList *GetHeaderList Returns a structure containing a list of items and q values
                            or NULL if key was not found.

  Header *head The header to search through.

  char* key The key to look for.
  ++++++++++++++++++++++++++++++++++++++*/
/* written by Paul Rombouts  as a replacement for SplitHeaderList */
HeaderList *GetHeaderList(Header *head,const char* key)
{
  HeaderList *list;
  int i,nitems=0;
  const char *p;

  for(i=0;i<head->n;++i)
    if(head->line[i].key && !strcasecmp(head->line[i].key,key))
      {
	++nitems;
	for(p=head->line[i].val; *p; ++p) if(*p==',') ++nitems;
      }
	
  if(nitems==0) return NULL;

  list=(HeaderList*)malloc(sizeof(HeaderList)+sizeof(HeaderListItem)*nitems);

  nitems=0;
  for(i=0;i<head->n;++i)
    if(head->line[i].key && !strcasecmp(head->line[i].key,key))
      {
	p=head->line[i].val;
	for(;;)
	  {
	    const char *q,*r;

	    while(*p && isspace(*p)) ++p;

	    q=p;

	    while(*p && *p!=',' && *p!=';') ++p;

	    r=p;
	    while(--r>=q && isspace(*r));
	    ++r;

	    {
	      float qval=1; 
 
	      if(*p==';')
		{
		  ++p;
		  sscanf(p," q=%f",&qval);
		  while(*p && *p!=',') ++p;
		}

	      list->item[nitems].val=STRDUP2(q,r);
	      list->item[nitems].qval=qval;

	      ++nitems;
	    }

	    if(!*p) break;
	    ++p; /* skip ',' */
	  }
      }

  list->n=nitems;
  qsort(list->item,nitems,sizeof(HeaderListItem),(int (*)(const void*,const void*))sort_qval);
 
  return(list);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a header list.

  HeaderList *hlist The list to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeHeaderList(HeaderList *hlist)
{
  int i;

  for(i=0;i<hlist->n;++i)
    free(hlist->item[i].val);

  free(hlist);
}


/*++++++++++++++++++++++++++++++++++++++
  Sort the header list items to put the highest q value first.

  int sort_qval Returns the sort preference of a and b.

  HeaderListItem *a The first header list item.

  HeaderListItem *b The second header list item.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_qval(HeaderListItem *a,HeaderListItem *b)
{
  float b_a=(b->qval)-(a->qval);
  return (b_a>0)?1:(b_a<0)?-1:0;
}
