/* This file contains mostly parts of misc.h that have been moved here by Paul Rombouts. */

#ifndef HEADBODY_H
#define HEADBODY_H    /* To prevent multiple inclusions. */

/*+ A header line. +*/
typedef struct _KeyValuePair
{
 char *key;                    /*+ The name of the header line. +*/
 char *val;                    /*+ The value of the header line. +*/
}
KeyValuePair;

/*+ A request or reply header type. +*/
struct _Header
{
 int type;                      /*+ The type of header, request=1 or reply=0. +*/

 char *method;                  /*+ The request method used. +*/
 char *url;                     /*+ The requested URL. +*/
 int status;                    /*+ The reply status. +*/
 char *note;                    /*+ The reply string. +*/
 char *version;                 /*+ The HTTP version. +*/

 int n;                         /*+ The number of header entries. +*/
 KeyValuePair *line;            /*+ The header lines +*/
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

/* The following function changed into an inline version by Paul Rombouts */
inline static void ChangeURLInHeader(Header *head,const char *url)
{
  free(head->url);
  head->url=strdup(url);
}

inline static void ChangeVersionInHeader(Header *head,const char *version)
{
  free(head->version);
  head->version=strdup(version);
}

/*++++++++++++++++++++++++++++++++++++++
  Remove the internal WWWOFFLE POST/PUT URL extensions.
  char *url A pointer to a string in the header.
  ++++++++++++++++++++++++++++++++++++++*/
/* Written by Paul Rombouts as a replacement for RemovePlingFromHeader() */
inline static void RemovePlingFromUrl(char *url)
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


void RemoveFromHeader(Header *head,const char* key);
void RemoveFromHeader2(Header *head,const char* key,const char *val);

/* Added by Paul Rombouts */
/*++++++++++++++++++++++++++++++++++++++
  Remove the specified key and its values.
  Header *head The header to remove from.
  int index: The index of the key to remove.
  ++++++++++++++++++++++++++++++++++++++*/
inline static void RemoveFromHeaderIndexed(Header *head,int index)
{
 free(head->line[index].key);
 head->line[index].key=NULL;
 free(head->line[index].val);
 head->line[index].val=NULL;
}

void ReplaceInHeader(Header *head,const char *key,const char *val);  /* Added by Paul Rombouts */

/*@null@*/ /*@observer@*/ char *GetHeader(Header *head,const char* key);
/*@null@*/ /*@observer@*/ char *GetHeader2(Header *head,const char* key,const char *val);
/*@null@*/ /*@observer@*/ char *GetHeader2Val(Header *head,const char* key,const char *subkey);
/*@only@*/ char *GetHeaderCombined(Header *head,const char* key);

/*@only@*/ char *HeaderString(Header *head,int *size);

void FreeHeader(/*@only@*/ Header *head);

inline static /*@only@*/ Body *CreateBody(int length)
{
 Body *new=(Body*)malloc(sizeof(Body)+length+1);

 new->length=length;
 return(new);
}

inline static /*@only@*/ Body *ReallocBody(/*@only@*/ Body *body,int length)
{
 Body *new=(Body*)realloc(body,sizeof(Body)+length+1);

 new->length=length;
 return(new);
}

inline static void FreeBody(/*@only@*/ Body *body)
{
  free(body);
}

/*@only@*/ HeaderList *GetHeaderList(Header *head,const char* key);
void FreeHeaderList(/*@only@*/ HeaderList *hlist);

inline static int offsetof_status(Header *reply_head)
{
  return ((reply_head->version)?strlen(reply_head->version):8)+1;
}

#endif
