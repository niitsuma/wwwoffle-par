/***************************************

  WWWOFFLE - World Wide Web Offline Explorer.
  This file contains mostly parts of misc.h that have been moved here by Paul Rombouts.
  Some of these parts were originally written by Andrew M. Bishop.

  This file Copyright 2007,2008,2009 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/

#ifndef HEADBODY_H
#define HEADBODY_H    /* To prevent multiple inclusions. */

/*+ A header line. +*/
typedef struct _KeyValueNode
{
 struct _KeyValueNode *next;    /*+ Pointer to the next header line. +*/
 char *val;                     /*+ The value of the header line. +*/
 char key[0];                   /*+ The name of the header line. +*/
}
KeyValueNode;

/*+ A request or reply header type. +*/
struct _Header
{
 int type;                      /*+ The type of header, request=1 or reply=0. +*/

 char *method;                  /*+ The request method used. +*/
 char *url;                     /*+ The requested URL. +*/
 int status;                    /*+ The reply status. +*/
 char *note;                    /*+ The reply string. +*/
 char *version;                 /*+ The HTTP version. +*/

 /* int n; */                   /*+ The number of header entries. +*/
 KeyValueNode *line;            /*+ Pointer to the first node in the list of header lines +*/
 KeyValueNode *last;            /*+ Pointer to the last node in the list of header lines +*/
};


/*+ A header list item. +*/
typedef struct _HeaderListItem
{
 char *val;                     /*+ The string value. +*/
 float qval;                    /*+ The quality value. +*/
}
HeaderListItem;

/*+ A header value split into a list. +*/
struct _HeaderList
{
 int n;                         /*+ The number of items in the list. +*/
 HeaderListItem item[0];        /*+ The individual items (sorted into q value preference order). +*/
};


/* In headbody.c */

int CreateHeader(const char *line,int type,Header **head);

void AddToHeader(Header *head,/*@null@*/ const char *key,const char *val);
int AddToHeaderRaw(Header *head,const char *line);
void AddToHeaderCombined(Header *head,const char *key,const char *val);

void ChangeURLInHeader(Header *head,const char *url);
void ChangeVersionInHeader(Header *head,const char *version);

void RemovePlingFromUrl(char *url);

void RemoveFromHeader(Header *head,const char* key);
int RemoveFromHeader2(Header *head,const char* key,const char *val);

void ReplaceOrAddInHeader(Header *head,const char *key,const char *val);  /* Added by Paul Rombouts */

/*@null@*/ /*@observer@*/ char *GetHeader(const Header *head,const char* key);
/*@null@*/ /*@observer@*/ char *GetHeader2(const Header *head,const char* key,const char *val);
/*@null@*/ /*@observer@*/ char *GetHeader2Val(const Header *head,const char* key,const char *subkey);
/*@only@*/ char *GetHeaderCombined(const Header *head,const char* key);
void CopyHeader(const Header *fromhead, Header *tohead, const char *key);
KeyValueNode *MatchHeader(const Header *head,const char* pattern);

/*@only@*/ char *HeaderString(const Header *head, size_t *size);

void FreeHeader(/*@only@*/ Header *head);

inline static void FreeKeyValueNode(KeyValueNode *p)
{
  free(p->val);
  free(p);
}

/* RemoveLineFromHeader removes a key-value node from a header list.
   head: pointer to the header.
   line: pointer to the node to be removed.
   prev: pointer to the previous node in the list, or NULL if there is no previous one.
   Return value: pointer to the next node in the list.
*/
inline static KeyValueNode *RemoveLineFromHeader(Header *head,KeyValueNode *line,KeyValueNode *prev)
{
  KeyValueNode *next=line->next;
  if(!prev)
    head->line=next;
  else
    prev->next=next;
  if(!next)
    head->last=prev;
  FreeKeyValueNode(line);
  return next;
}

inline static /*@only@*/ Body *CreateBody(size_t length)
{
 Body *new=(Body*)malloc(sizeof(Body)+length+1);

 new->length=length;
 return(new);
}

inline static /*@only@*/ Body *ReallocBody(/*@only@*/ Body *body,size_t length)
{
 Body *new=(Body*)realloc(body,sizeof(Body)+length+1);

 new->length=length;
 return(new);
}

inline static void FreeBody(/*@only@*/ Body *body)
{
  free(body);
}

/*@only@*/ HeaderList *GetHeaderList(const Header *head,const char* key);
void FreeHeaderList(/*@only@*/ HeaderList *hlist);

inline static size_t offsetof_status(const Header *reply_head)
{
  return ((reply_head->version)?strlen(reply_head->version):8)+1;
}

#endif
