/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/headbody.c 1.26 2006/07/16 08:38:07 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9a.
  Header and Body handling functions.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007 Paul A. Rombouts
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
#include "configpriv.h"
#include "headbody.h"


static int sort_qval(const HeaderListItem *a,const HeaderListItem *b);


/*++++++++++++++++++++++++++++++++++++++
  Create a new Header structure.

  int CreateHeader Returns 1 if OK, -1 if malformed, 0 if line is empty.

  const char *line The top line in the original header.

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

 /* new->n=0; */
 new->line=NULL;
 new->last=NULL;

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

   finish_rep: ;
   }

 upcase(new->version);

 return 1;
}


/*++++++++++++++++++++++++++++++++++++++
  Add a specified key and value to the header structure.

  Header *head The header structure to add to.

  const char *key The key to add.

  const char *val The value to add.
  ++++++++++++++++++++++++++++++++++++++*/

void AddToHeader(Header *head,const char *key,const char *val)
{
  KeyValueNode *new;
  size_t key_sz;

  key_sz=strlen(key)+1;
  new=malloc(sizeof(KeyValueNode)+key_sz);
  new->next=NULL;
  new->val=strdup(val);
  memcpy(new->key,key,key_sz);
 
  if(!head->line)
      head->line=new;
  else
      head->last->next=new;

  head->last=new;
}


/*++++++++++++++++++++++++++++++++++++++
  Add a raw line to a header.

  int AddToHeaderRaw Returns 1 if OK, 0 if line is empty, -1 if malformed.

  Header *head The header to add the line to.

  const char *line The raw line of data.
  ++++++++++++++++++++++++++++++++++++++*/

int AddToHeaderRaw(Header *head,const char *line)
{
 const char *key=NULL,*keyend=line,*val,*end=strchrnul(line,0);

 /* Remove whitespace at end of line. */

 do {if(--end<line) return 0;} while(isspace(*end));
 ++end;

 if(isspace(*line))
   val=line+1; /* continuation of previous line */
 else {
   /* split line */
   
   key=line;
   
   while(*keyend!=':') {if(!*++keyend) return -1;}
   val=keyend+1;

   /* Remove whitespace at end of key. */
   while(--keyend>=line && isspace(*keyend));
   ++keyend;
 }

 /* Remove whitespace at beginning of value. */
 while(val<end && isspace(*val)) ++val;

 /* Add to the header */

 {
   size_t strlen_val=end-val;

   if(key)
     {
       KeyValueNode *new;
       size_t strlen_key=keyend-key;

       new=malloc(sizeof(KeyValueNode)+strlen_key+1);
       new->next=NULL;
       new->val=strndup(val,strlen_val);
       *((char*)mempcpy(new->key,key,strlen_key))=0;
 
       if(!head->line)
	 head->line=new;
       else
	 head->last->next=new;

       head->last=new;
     }
   else
     {
       /* Append text to the last header line */
       KeyValueNode *last=head->last;

       if(!last)
	 return -1; /* weird: there must be a last header... */

       {
	 size_t strlen_last=strlen(last->val);
	 char *p;
	 last->val=(char*)realloc((void*)last->val,strlen_last+strlen_val+2);
	 p= last->val + strlen_last;
	 if(strlen_last) *p++=' ';
	 p=mempcpy(p,val,strlen_val);
	 *p=0;
       }
     }
 }

 return(1);
}


/* Added by Paul Rombouts */
/*++++++++++++++++++++++++++++++++++++++
  Add a specified key and value to the header structure.
  If a header line with the specified key already exists, combine
  the old and new values, otherwise add a new header line.

  Header *head The header structure to add to.

  const char *key The key to add.

  const char *val The value to add.
  ++++++++++++++++++++++++++++++++++++++*/
void AddToHeaderCombined(Header *head,const char *key,const char *val)
{
 KeyValueNode *line;
 size_t oldlen;

 line=head->line;
 while(line) {
   if(!strcasecmp(line->key,key))
     goto found;
   line=line->next;
 }

 AddToHeader(head,key,val);
 return;

found:
 oldlen= strlen(line->val);
 line->val= realloc(line->val, oldlen + strlen(val) + sizeof(", "));
 stpcpy(stpcpy(line->val + oldlen, ", "), val);
}


/*++++++++++++++++++++++++++++++++++++++
  Change the URL in the header.

  Header *head The header to change.

  const char *url The new URL.
  ++++++++++++++++++++++++++++++++++++++*/

void ChangeURLInHeader(Header *head,const char *url)
{
  free(head->url);
  head->url=strdup(url);
}


/*++++++++++++++++++++++++++++++++++++++
  Change the version string in the header.

  Header *head The header to change.

  const char *version The new version.
  ++++++++++++++++++++++++++++++++++++++*/

void ChangeVersionInHeader(Header *head,const char *version)
{
  free(head->version);
  head->version=strdup(version);
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the internal WWWOFFLE POST/PUT URL extensions.

  char *url A pointer to a string in the header.
  ++++++++++++++++++++++++++++++++++++++*/
/* Written by Paul Rombouts as a replacement for RemovePlingFromHeader() */
void RemovePlingFromUrl(char *url)
{
  char *pling,*pling2;

  if((pling=strstr(url,"?!"))) {
    if((pling2=strchr(pling+2,'!'))) {
      ++pling; --pling2;
      for(;pling<pling2;++pling)
	*pling=*(pling+1);
    }
    *pling=0;
  }
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the specified key and its values.

  Header *head The header to remove from.

  const char* key The key to look for and remove.
  ++++++++++++++++++++++++++++++++++++++*/

void RemoveFromHeader(Header *head,const char* key)
{
  KeyValueNode *prev,*line;

  prev=NULL;
  line=head->line;

  while(line) {
    if(!strcasecmp(line->key,key)) {
      /* Remove key-value node from list */
      line=RemoveLineFromHeader(head,line,prev);
    }
    else {
      prev=line;
      line=line->next;
    }
  }
}


/*++++++++++++++++++++++++++++++++++++++
  Remove the specified key and value pair from a header structure.

  int RemoveFromHeader2 returns the number of value instances removed.

  Header *head The header to remove from.

  const char* key The key to look for and remove.

  const char *val The value to look for and remove.
  ++++++++++++++++++++++++++++++++++++++*/

int RemoveFromHeader2(Header *head,const char* key,const char *val)
{
  int count=0;
  size_t strlen_val=strlen(val);
  KeyValueNode *prev,*line;

  prev=NULL;
  line=head->line;

  while(line) {
    if(!strcasecmp(line->key,key)) {
      char *str=line->val,*p=str,*q,*r=str;

      for(;;) {
	for(;;++p) {
	  if(!*p) goto nextline;
	  if(!isspace(*p)) break;
	}

	q=p;
	while(!strncasecmp(q,val,strlen_val)) {
	  ++count;
	  while(*q!=',') {if(!*++q) goto chop_line;}
	  do {if(!*++q) goto chop_line;} while(isspace(*q));
	}

	if(q!=p)
	  {char *t=p; while((*t++=*q++));}
	  
	while(*p!=',') {if(!*++p) goto nextline;}

	r=p++;  /* remember ending of previous item */
      }

    chop_line:
      do {
	if(--r<str) {
	  /* Remove key-value node from list */
	  line=RemoveLineFromHeader(head,line,prev);
	  goto skipline;
	}
      } while(isspace(*r));
      *++r=0;
    }

  nextline:
    prev=line;
    line=line->next;
  skipline: ;
  }

  return count;
}


/* Added by Paul Rombouts */
/*++++++++++++++++++++++++++++++++++++++
  Replace the value of an existing key with a new one, or add a new key and value combination.
  Header *head The header structure to modify.
  const char *key The key.
  const char *val The replacement value.
  ++++++++++++++++++++++++++++++++++++++*/
void ReplaceOrAddInHeader(Header *head,const char *key,const char *val)
{
 KeyValueNode *prev,*line;

 line=head->line;
 while(line) {
   if(!strcasecmp(line->key,key))
     goto found;
   line=line->next;
 }

 AddToHeader(head,key,val);
 return;

found:
 free(line->val);
 line->val=strdup(val);
 /* remove any remaining lines with the same key */
 prev=line;
 line=line->next;
 while(line) {
   if(!strcasecmp(line->key,key)) {
     line=RemoveLineFromHeader(head,line,prev);
   }
   else {
     prev=line;
     line=line->next;
   }
 }
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a specified key.

  char *GetHeader Returns the (first) value for the header key or NULL if none.

  const Header *head The header to search through.

  const char* key The key to look for.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetHeader(const Header *head,const char* key)
{
 KeyValueNode *line;

 for(line=head->line; line; line=line->next)
    if(!strcasecmp(line->key,key))
       return(line->val);

 return(NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a specified key and value pair.

  char *GetHeader2 Returns the value for the header key or NULL if none.

  const Header *head The header to search through.

  const char* key The key to look for.

  const char *val The value to look for (which may be in a list).
  ++++++++++++++++++++++++++++++++++++++*/

char *GetHeader2(const Header *head,const char* key,const char *val)
{
 size_t strlen_val=strlen(val);
 KeyValueNode *line;

 for(line=head->line; line; line=line->next) {
   if(!strcasecmp(line->key,key)) {
     char *p=line->val;

     for(;;) {
       for(;;++p) {
	 if(!*p) goto nextline;
	 if(!isspace(*p)) break;
       }

       if(!strncasecmp(p,val,strlen_val)) return p;

       while(*p!=',') {if(!*++p) goto nextline;}
       ++p;
     }
   }
 nextline: ;
 }

 return NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a specified key and sub-key.

  char *GetHeader2Val Returns a pointer to the value for the sub-key or NULL if none.

  const Header *head The header to search through.

  const char* key The key to look for.

  const char *subkey The sub-key to look for (which must be followed by '=' in the headerline).
  ++++++++++++++++++++++++++++++++++++++*/

char *GetHeader2Val(const Header *head,const char* key,const char *subkey)
{
 size_t strlen_subkey=strlen(subkey);
 KeyValueNode *line;

 for(line=head->line; line; line=line->next) {
   if(!strcasecmp(line->key,key)) {
     char *p=line->val;

     for(;;) {
       for(;;++p) {
	 if(!*p) goto nextline;
	 if(!isspace(*p)) break;
       }

       if(!strncasecmp(p,subkey,strlen_subkey)) {
	 p+=strlen_subkey;
	 for(;;++p) {
	   if(!*p) goto nextline;
	   if(!isspace(*p)) break;
	 }
	 if(*p=='=') {
	   while(*++p && isspace(*p));
	   return p;
	 }
       }

       while(*p!=',') {if(!*++p) goto nextline;}
       ++p;
     }
   }
 nextline: ;
 }

 return NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a specified key.

  char *GetHeader Returns the all the values combined for the header key or NULL if empty or none.

  const Header *head The header to search through.

  const char* key The key to look for.
  ++++++++++++++++++++++++++++++++++++++*/

char *GetHeaderCombined(const Header *head,const char* key)
{
  ssize_t length=-1;
  KeyValueNode *line;
  char *p,*val;

  for(line=head->line; line; line=line->next)
    if(!strcasecmp(line->key,key)) {
      length+=1+strlen(line->val);
    }

  if(length==-1) return NULL;

  val=(char *)malloc(length+1);

  p=val-1;
  for(line=head->line; line; line=line->next)
    if(!strcasecmp(line->key,key)) {
      if(p<val)
	p=val;
      else
	*p++=',';
      p=stpcpy(p,line->val);
    }

  return val;
}


/*
  Copy (append) header lines matching key from one header to another.
  A NULL value matches any key.
*/

void CopyHeader(const Header *fromhead, Header *tohead, const char *key)
{
 KeyValueNode *line;

 for(line=fromhead->line; line; line=line->next)
    if(!key || !strcasecmp(line->key,key))
      AddToHeader(tohead,line->key,line->val);
}


/*++++++++++++++++++++++++++++++++++++++
  Return a string that contains the whole of the header.

  char *HeaderString Returns the header as a string.

  const Header *head The header structure to convert.
  size_t *size    Returns the size of the header.
  ++++++++++++++++++++++++++++++++++++++*/

char *HeaderString(const Header *head, size_t *size)
{
  char *str,*p;
  size_t str_len;
  KeyValueNode *line;

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

  for(line=head->line; line; line=line->next)
    str_len += strlen(line->key) + strlen(line->val) + 4;

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
      char status[MAX_INT_STR+1];
      if(head->status>=100 && sprintf(status,"%3d",head->status)==3)
	p=stpcpy(p,status);
      else
	p=stpcpy(p,"502");
    }
    *p++ = ' ';
    if(head->note) p= stpcpy(p,head->note);
  }

  p= stpcpy(p,"\r\n");

  for(line=head->line; line; line=line->next)
    p= stpcpy(stpcpy(stpcpy(stpcpy(p,line->key),": "),line->val),"\r\n");

  stpcpy(p,"\r\n");

  return(str);
}


/*++++++++++++++++++++++++++++++++++++++
  Free a header structure.

  Header *head The header structure to free.
  ++++++++++++++++++++++++++++++++++++++*/

void FreeHeader(Header *head)
{
  KeyValueNode *line,*next;

 if(head->method)  free(head->method);
 if(head->url)     free(head->url);
 if(head->note)    free(head->note);
 if(head->version) free(head->version);

 line=head->line;
 while (line) {
   next=line->next;
   FreeKeyValueNode(line);
   line=next;
 }

 free(head);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a new Body structure.

  Body *CreateBody Returns the new body structure.

  int length The length of the body;
  ++++++++++++++++++++++++++++++++++++++*/

#if 0
Body *CreateBody(size_t length)
{
 Body *new=(Body*)malloc(sizeof(Body)+length+1);

 new->length=length;

 return(new);
}
#endif


/*++++++++++++++++++++++++++++++++++++++
  Free a body structure.

  Body *body The body structure to free.
  ++++++++++++++++++++++++++++++++++++++*/

#if 0
void FreeBody(Body *body)
{
 free(body);
}
#endif


/*++++++++++++++++++++++++++++++++++++++
  Construct a list of items from a header.

  HeaderList *GetHeaderList Returns a structure containing a list of items and q values
                            or NULL if key was not found.

  const Header *head The header to search through.

  const char* key The key to look for.
  ++++++++++++++++++++++++++++++++++++++*/
/* written by Paul Rombouts  as a replacement for SplitHeaderList */
HeaderList *GetHeaderList(const Header *head,const char* key)
{
  HeaderList *list;
  int nitems=0;
  KeyValueNode *line;
  const char *p;

  for(line=head->line; line; line=line->next)
    if(!strcasecmp(line->key,key))
      {
	++nitems;
	for(p=line->val; *p; ++p) if(*p==',') ++nitems;
      }
	
  if(nitems==0) return NULL;

  list=(HeaderList*)malloc(sizeof(HeaderList)+sizeof(HeaderListItem)*nitems);

  nitems=0;
  for(line=head->line; line; line=line->next)
    if(!strcasecmp(line->key,key))
      {
	p=line->val;
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

  const HeaderListItem *a The first header list item.

  const HeaderListItem *b The second header list item.
  ++++++++++++++++++++++++++++++++++++++*/

static int sort_qval(const HeaderListItem *a,const HeaderListItem *b)
{
  float aq=a->qval;
  float bq=b->qval;
  return (bq>aq)?1:(bq<aq)?-1:0;
}

/*++++++++++++++++++++++++++++++++++++++
  Search through a HTTP header for a key-value combination matching a pattern.

  char *MatchHeader Returns the (first) key-value node matching the pattern.

  const Header *head The header to search through.

  const char* pattern The pattern to match.
  ++++++++++++++++++++++++++++++++++++++*/

KeyValueNode *MatchHeader(const Header *head,const char* pattern)
{
 const char *val=strchr(pattern,':');
 KeyValueNode *line;

 if(val) {
   const char *keyend=val;
   size_t len_key;
   while(--keyend>=pattern && isspace(*keyend));
   ++keyend;
   len_key=keyend-pattern;
   do {if(!*++val) {val=NULL;break;}} while(isspace(*val));
   for(line=head->line; line; line=line->next)
     if(WildcardCaseMatchN(line->key,pattern,len_key) && (!val || WildcardCaseMatch(line->val,val)))
       return(line);
 }
 else {
   for(line=head->line; line; line=line->next)
     if(WildcardCaseMatch(line->key,pattern))
       return(line);
 }

 return(NULL);
}
