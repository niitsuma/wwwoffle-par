/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/document.c 1.17 2002/06/23 14:56:49 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7c.
  Document parsing functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include "wwwoffle.h"
#include "document.h"
#include "errors.h"
#include "proto.h"
#include "misc.h"
#include "config.h"


/*+ The list of references. +*/
static char **references[NRefTypes];

/*+ The number of references. +*/
static int nreferences[NRefTypes];

/*+ The base URL from which references are related. +*/
static URL *baseUrl;


static DocType GetDocumentType(char *mimetype);
static char *GetMIMEType(int fd);


/*++++++++++++++++++++++++++++++++++++++
  Parse a document.

  DocType ParseDocument Return 1 if there was anything that could be parsed.

  int fd The file descriptor to read the document from.

  URL *Url The URL of the document.
  ++++++++++++++++++++++++++++++++++++++*/

DocType ParseDocument(int fd,URL *Url)
{
 char *mimetype;
 DocType doctype=DocUnknown;

 baseUrl=Url;

 if((mimetype = GetMIMEType(fd)) != NULL)
    doctype = GetDocumentType(mimetype);

 /* Check the file extension if we don't yet know the DocType. */

 if(doctype==DocUnknown)
   {
    /* Get MIME-Type from extension. */
    mimetype = WhatMIMEType(Url->path);
    doctype =  GetDocumentType(mimetype);
   }

 /* Parse the document if we do know the DocType. */

 if(doctype!=DocUnknown)
   {
    PrintMessage(Debug,"Parsing document of MIME Type '%s'.",mimetype);

    /* Free previous references. */

    ResetReferences();

    if(doctype==DocHTML)
       ParseHTML(fd,Url);
    else if(doctype==DocJavaClass)
       InspectJavaClass(fd,Url);
    else if(doctype==DocXML)
       ParseXML(fd,Url);
    else if(doctype==DocVRML)
       ParseVRML(fd,Url);

    /* Put a trailing NULL on the references. */
 
    FinishReferences();
   }

 return(doctype);
}

static struct {
  char *mimetype;
  DocType doctype;
} docTypeList[] = {
	{"text/html",DocHTML},
	{"application/java",DocJavaClass},
	{"text/xml",DocXML},
	{"application/xml",DocXML},
	{"x-world/x-vrml",DocVRML},
	{"model/vrml",DocVRML},
	{"",DocUnknown}
};


/*++++++++++++++++++++++++++++++++++++++
  Decide the current document type based on the mime type.

  DocType GetDocumentType Returns the document type.

  char *mimetype The mime type to be tested.
  ++++++++++++++++++++++++++++++++++++++*/

DocType GetDocumentType(char *mimetype)
{
 int i;

 for(i = 0; i < sizeof(docTypeList)/sizeof(docTypeList[0]); i++)
    if(!strcmp(mimetype,docTypeList[i].mimetype))
       return(docTypeList[i].doctype);

 return(DocUnknown);
}


/*++++++++++++++++++++++++++++++++++++++
  Decide the mime type of a document based on the header.

  char *GetMIMEType Returns the mime type.

  int fd The file descriptor to read the header from.
  ++++++++++++++++++++++++++++++++++++++*/

static char *GetMIMEType(int fd)
{
 Header *doc_header;
 char *contenttype,*mimetype=NULL;

 /* Get the header and examine it. */

 ParseReply(fd,&doc_header);

 contenttype=GetHeader(doc_header,"Content-Type");

 if(contenttype)
   {
    char *p;

    mimetype=(char*)malloc(strlen(contenttype)+1);
    strcpy(mimetype,contenttype);

    p=mimetype;
    while(*p && !isspace(*p) && *p!=';')
       p++;
    *p=0;
   }

 FreeHeader(doc_header);
 
 return(mimetype);
}
 

/*++++++++++++++++++++++++++++++++++++++
  A function to add a reference to a list.

  char* name The name to add.

  RefType type The type of reference.
  ++++++++++++++++++++++++++++++++++++++*/

void AddReference(char* name,RefType type)
{
 /* Check for badly formatted URLs */

 if(name)
   {
    char *p=name+strlen(name)-1;
    int onlychars=1;

    while(*name && isspace(*name))
       name++;

    while(p>name && isspace(*p))
       *p--=0;

    for(p=name;*p;p++)
       if(isspace(*p))
          return;
       else if(*p=='#')
         {
          *p=0;
          break;
         }
       else if(*p==':' && onlychars)
         {
          int i;
          for(i=0;i<NProtocols;i++)
             if(!strncmp(Protocols[i].name,name,p-name))
                break;

          if(i && i==NProtocols)
             return;
         }
       else if(onlychars && !isalpha(*p))
          onlychars=0;

    if(!*name)
       return;
   }

 /* Add it to the list. */

 if(name || references[type])
   {
    if(nreferences[type]==0)
       references[type]=(char**)malloc(16*sizeof(char*));
    else if((nreferences[type]%16)==0)
       references[type]=(char**)realloc(references[type],(nreferences[type]+16)*sizeof(char*));

    if(name)
      {
       references[type][nreferences[type]]=(char*)malloc(strlen(name)+1);
       strcpy(references[type][nreferences[type]],name);
      }
    else
       references[type][nreferences[type]]=NULL;

    nreferences[type]++;
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Finish the list of references and set the base URL if changed.
  ++++++++++++++++++++++++++++++++++++++*/

void FinishReferences(void)
{
 RefType i;

 for(i=0;i<NRefTypes;i++)
    AddReference(NULL,i);
}


/*++++++++++++++++++++++++++++++++++++++
  Set another base URL.

  URL *Url The new base URL.
  ++++++++++++++++++++++++++++++++++++++*/

void SetBaseURL(URL *Url)
{
 if(Url)
    baseUrl=Url;
}


/*++++++++++++++++++++++++++++++++++++++
  Get a list of the references of the specified type.

  char **GetReferences Returns the list of URLs.

  RefType type The type of list that is required.
  ++++++++++++++++++++++++++++++++++++++*/

char **GetReferences(RefType type)
{
 int i,j;

 if(!references[type])
    return(NULL);

 /* canonicalise the links */

 for(i=0;references[type][i];i++)
   {
    char *new=LinkURL(baseUrl,references[type][i]);
    if(new!=references[type][i])
      {
       free(references[type][i]);
       references[type][i]=new;
      }
   }

 /* remove the duplicates */

 for(i=0;references[type][i];i++)
   {
    for(j=i+1;references[type][j];j++)
       if(!strcmp(references[type][i],references[type][j]))
          break;

    if(references[type][j])
      {
       free(references[type][j]);
       do
         {
          references[type][j]=references[type][j+1];
         }
       while(references[type][j++]);
       i--;
       nreferences[type]--;
      }
   }

 return(references[type]);
}


/*++++++++++++++++++++++++++++++++++++++
  Reset all of the reference lists.
  ++++++++++++++++++++++++++++++++++++++*/

void ResetReferences(void)
{
 static int first=1;
 RefType i;

 for(i=0;i<NRefTypes;i++)
   {
    if(!first && references[i])
      {
       int j;

       for(j=0;references[i][j];j++)
          free(references[i][j]);
       free(references[i]);
      }

    references[i]=NULL;
    nreferences[i]=0;
   }

 first=0;
}
