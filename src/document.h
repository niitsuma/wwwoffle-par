/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/document.h 1.6 2002/06/23 14:54:54 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7c.
  Header file for document parsing functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#ifndef DOCUMENT_H
#define DOCUMENT_H    /*+ To stop multiple inclusions. +*/

#include "misc.h"


/*+ The type of document +*/
typedef enum _DocType
{
 DocUnknown=0,                  /*+ An unknown type. +*/
 DocHTML,                       /*+ An HTML document. +*/
 DocVRML,                       /*+ An VRML document. +*/
 DocXML,                        /*+ An XML document. +*/
 DocJavaClass,                  /*+ A Java class file. +*/
 NDocTypes                      /*+ The number of different document types. +*/
}
DocType;


/*+ The type of reference. +*/
typedef enum _RefType
{
 RefStyleSheet,                 /*+ A style sheet. +*/
 RefImage,                      /*+ An image. +*/
 RefFrame,                      /*+ The contents of a frame. +*/
 RefScript,                     /*+ An included script. +*/
 RefObject,                     /*+ An included object. +*/
 RefInlineObject,               /*+ An inlined object e.g. VRML: WWWInline, referenced java class. +*/
 RefLink,                       /*+ A link to another page. +*/
 NRefTypes                      /*+ The number of different reference types. +*/
}
RefType;

/* In document.c */

DocType ParseDocument(int fd,URL *Url);

void AddReference(/*@null@*/ char* name,RefType type);
void FinishReferences(void);
void SetBaseURL(URL *Url);
char /*@null@*/ **GetReferences(RefType type);
void ResetReferences(void);

/* In html.c (html.l) */

void ParseHTML(int fd,URL *Url);
char *MetaRefresh(void);

/* In javaclass.c */

int InspectJavaClass(int fd,URL *Url);

/* In vrml.c (vrml.l) */

void ParseVRML(int fd,URL *Url);

/* In xml.c (xml.l) */

void ParseXML(int fd,URL *Url);

/* In gifmodify.c */

int OutputGIFWithModifications(int client,int spool,URL *Url);

/* In htmlmodify.c (htmlmodify.l) */

int OutputHTMLWithModifications(int client,int spool,URL *Url);

#endif /* DOCUMENT_H */
