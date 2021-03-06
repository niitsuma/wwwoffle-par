%option noyywrap
%option stack
%option noyy_top_state

VAR    \$[a-z_0-9]+
FUNC   \%[a-z_0-9]+\[[a-z_0-9]+\]

%x ASSIGNMENT_START
%x ASSIGNMENT_WORD ASSIGNMENT_DOUBLE_QUOTES ASSIGNMENT_SINGLE_QUOTES
%x ALTERNATE_SKIP_FIRST ALTERNATE_USE_FIRST
%x ALTERNATE_SKIP_SECOND ALTERNATE_USE_SECOND
%x ALTERNATE_SKIP_REST

%{
/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Parse the HTML to create the messages.
  ******************/ /******************
  Originally written by Andrew M. Bishop.
  Extensively modified by Paul A. Rombouts.

  This file Copyright 1998,99,2000,01,02,03,04,05 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2005,2006,2007,2008,2011 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif

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

#include <sys/stat.h>
#include <fcntl.h>

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "headbody.h"
#include "errors.h"
#include "config.h"
#include "version.h"


#ifndef O_BINARY
/*+ A work-around for needing O_BINARY with Win32 to use binary mode. +*/
#define O_BINARY 0
#endif

typedef struct _VarValueNode
{
 struct _VarValueNode *next;    /*+ Pointer to the next node in the list. +*/
 char *value;                   /*+ Pointer to the value of the variable. +*/
 char variable[0];              /*+ The name of the variable. +*/
}
VarValueNode;


extern int msg_yylex(void);


/* Local functions */

static char *html_message_body(int fd,const char *template, va_list ap);

static void write_or_append_string(char *str);

static char /*@observer@*/ *add_variable(const char *var,const char *val);
static void delete_variables(void);
static char /*@observer@*/ *get_variable(const char *var);
static char *call_function(const char *func);


/* Local variables */

/*+ A known fixed empty string. +*/
static char empty[]="";

/*+ The file descriptor that we are reading from. +*/
static int msg_yyfd=-1;

/*+ The file descriptor that we are writing to. +*/
static int out_fd=-1;
ssize_t out_err=0;

/*+ The string we are appending to. +*/
static char *out_str=NULL;
static size_t out_len=0;

/*+ The list of variables and their values. +*/
static VarValueNode *varvaluelist=NULL;

/*+ The option to use compression or chunked encoding for the reply. +*/
static int client_compression=0,client_chunked=0;
int head_only=0;
int client_keep_connection=0;

/*++++++++++++++++++++++++++++++++++++++
  Set the options to use for the socket sending the message.

  int compressed Use compression (and type).

  int chunked Use chunked encoding.
 ++++++++++++++++++++++++++++++++++++++*/

void SetMessageOptions(int compressed,int chunked)
{
 client_compression=compressed;
 client_chunked=chunked;
}


/*++++++++++++++++++++++++++++++++++++++
  Create a simple message using the template in the html directory.

  int fd The file descriptor to write it to.

  int status_val The numeric status value.

  const char *status_str The status string.

  const char *location A Location: HTTP header or NULL for none.

  const char *template The name of the template for the message.

  ... A list of variable-value pairs to use in the parsing (NULL terminated).
  ++++++++++++++++++++++++++++++++++++++*/

void HTMLMessage(int fd,int status_val,const char *status_str,const char *location,const char *template, ...)
{
 va_list ap;

 if(location)
    HTMLMessageHead(fd,status_val,status_str,
                    "Location",location,
                    NULL);
 else
    HTMLMessageHead(fd,status_val,status_str,
                    NULL);

 if(out_err==-1 || head_only) return;

#ifdef __STDC__
 va_start(ap,template);
#else
 va_start(ap);
#endif

 html_message_body(fd,template,ap);

 va_end(ap);
}


/*++++++++++++++++++++++++++++++++++++++
  Create an html header using the specified fields.

  int fd The file descriptor to write it to.

  int status_val The numeric status value.

  const char *status_str The status string.

  ... A list of variable-value pairs to use in the header (NULL terminated).
  ++++++++++++++++++++++++++++++++++++++*/

int HTMLMessageHead(int fd,int status_val,const char *status_str, ...)
{
 va_list ap;
 Header *header;
 int content_type=0;
 int compression=client_compression, chunked=client_chunked;


#ifdef __STDC__
 va_start(ap,status_str);
#else
 va_start(ap);
#endif

 /* The start of the header */
 {
   char headline[sizeof("HTTP/1.0  \r\n")+strlen(status_str)+MAX_INT_STR];
   sprintf(headline,"HTTP/1.0 %d %s\r\n",status_val,status_str);
   CreateHeader(headline,0,&header);
 }

 AddToHeader(header,"Server","WWWOFFLE/" WWWOFFLE_VERSION);
 AddToHeader(header,"Date",RFC822Date(time(NULL),1));

 /* Start filling in the header. */
 {
   char *var,*val;

   while((var=va_arg(ap,char*)))
     {
       val=va_arg(ap,char*);

       if(!strcasecmp(var,"Content-Type"))
	 content_type=1;

       if(val)
	 AddToHeader(header,var,val);
     }
 }

 va_end(ap);

 /* Set up compression header for the client if available and required. */

#if USE_ZLIB
 if(compression && !head_only)
   {
    /* If it is not to be compressed then don't */
    char *len;
    if(NotCompressed(GetHeader(header,"Content-Type"),NULL) ||
       ((len=GetHeader(header,"Content-Length")) && atol(len)<=MINCOMPRSIZE))
       compression=0;

    /* Add the compression header for the client. */

    else
      {
       RemoveFromHeader(header,"Content-Length");

       AddToHeader(header,"Content-Encoding",(compression==1)?"deflate":"gzip");

       PrintMessage(Debug,"Using 'Content-Encoding: %s' for the client.",
                    compression==1?"deflate":"gzip");
      }
   }
#endif

 /* Set up chunked encoding header for the client if required. */

 if(!head_only)
   {
     /* If the length is already known don't bother with chunked encoding */
     if(GetHeader(header,"Content-Length"))
       chunked=0;
     else {
       struct stat buf;
       if(!fstat(fd,&buf) && S_ISREG(buf.st_mode))
	 /* Don't write chunked encoded to a spool file. */
	 chunked=0;
       else if(chunked) {
	 ChangeVersionInHeader(header,"HTTP/1.1");
	 AddToHeader(header,"Transfer-Encoding","chunked");

	 PrintMessage(Debug,"Using 'Transfer-Encoding: chunked' for the client.");
       }
       else
	 client_keep_connection=0;
     }
   }

 /* The end of the header. */

 if(!content_type)
    AddToHeader(header,"Content-Type","text/html");

 {
   const char *connectiontoken= (client_keep_connection? "Keep-Alive": "close");
   AddToHeader(header,"Connection",connectiontoken);
   AddToHeader(header,"Proxy-Connection",connectiontoken);
 }

 /* Write the header */
 {
   char *headerstr; size_t len;

   headerstr=HeaderString(header,&len);

   if(StderrLevel==ExtraDebug)
     PrintMessage(ExtraDebug,"Reply Head (internal page)\n%s",headerstr);

   out_err=write_data(fd,headerstr,len);
   free(headerstr);
 }

 FreeHeader(header);

 if(out_err!=-1 && !head_only) {
   /* Initialise the client compression. */

#if USE_ZLIB
   if(compression)
     configure_io_zlib(fd,-1,compression);
#endif

   /* Initialise the client chunked encoding. */

   if(chunked)
     configure_io_chunked(fd,-1,1);
 }
 else
   chunked=0;

 return chunked;
}


/*++++++++++++++++++++++++++++++++++++++
  Create a HTML message body by passing the specified template through the micro-language processor.

  int fd The file descriptor to write to.

  const char *template The name of the template for the message.

  ... A list of variable-value pairs to use in the parsing (NULL terminated).
  ++++++++++++++++++++++++++++++++++++++*/

void HTMLMessageBody(int fd,const char *template, ...)
{
 va_list ap;

#ifdef __STDC__
 va_start(ap,template);
#else
 va_start(ap);
#endif

 html_message_body(fd,template,ap);

 va_end(ap);
}


/*++++++++++++++++++++++++++++++++++++++
  Create a HTML message string by passing the specified template through the micro-language processor.

  char *HTMLMessageString Returns a string containing the parsed template.

  const char *template The name of the template for the message.

  ... A list of variable-value pairs to use in the parsing (NULL terminated).
  ++++++++++++++++++++++++++++++++++++++*/

char *HTMLMessageString(const char *template, ...)
{
 char *string;
 va_list ap;

#ifdef __STDC__
 va_start(ap,template);
#else
 va_start(ap);
#endif

 string=html_message_body(-1,template,ap);

 va_end(ap);

 return(string);
}


/*++++++++++++++++++++++++++++++++++++++
  Finish up and free memory.
  ++++++++++++++++++++++++++++++++++++++*/

void FinishMessages(void)
{
 delete_variables();
}


/*++++++++++++++++++++++++++++++++++++++
  Create a HTML message by passing the specified template through the micro-language processor.

  char *html_message_body Returns a string for the body or NULL when writing to file.

  int fd The file descriptor to write to (or -1 to create a string).

  const char *template The name of the template for the message.

  va_list ap A list of variable-value pairs to use in the parsing (NULL terminated).
  ++++++++++++++++++++++++++++++++++++++*/

static char *html_message_body(int fd,const char *template,va_list ap)
{
 char *var,*val;
 static int first=1;

 /* Set up the variables. */

 out_fd=fd; out_err=0;
 out_str=NULL; out_len=0;

 if(fd==-1)
    delete_variables();

 if(varvaluelist==NULL)
   {
    char *localurl=GetLocalURL();
    add_variable("localurl",localurl);
    free(localurl);
    add_variable("version",WWWOFFLE_VERSION);
   }

 while((var=va_arg(ap,char*)))
   {
    val=va_arg(ap,char*);

    add_variable(var,val);
   }

 /* Open the file */
 {
   char filename[strlen(template)+sizeof("messages/.html")];
   sprintf(filename,"messages/%s.html",template);

   msg_yyfd=OpenLanguageFile(filename);
 }

 if(msg_yyfd==-1)
    PrintMessage(Fatal,"Cannot open the message template '%s.html'.",template);

 init_io(msg_yyfd);

 /* Parse the template and fill in the gaps. */

 if(!first)
    msg_yyrestart(NULL);

 msg_yylex();

 finish_io(msg_yyfd);
 close(msg_yyfd);

 first=0;

 return(out_str);
}


/*++++++++++++++++++++++++++++++++++++++
  Write the string to the file descriptor out_fd or append to the string out_str.

  char *str The string to write or append.
  ++++++++++++++++++++++++++++++++++++++*/

static void write_or_append_string(char *str)
{
 if(out_fd==-1)
   {
     /* allocate a buffer of at least LINE_BUFFER_SIZE+1 bytes
	so that it can be re-used by read_line() */

    if(out_str)
      {
       size_t str_len=strlen(str);
       size_t new_len=out_len+str_len;
       if(new_len>LINE_BUFFER_SIZE)
	 out_str=(char*)realloc((void*)out_str,new_len+1);
       memcpy(out_str+out_len,str,str_len+1);
       out_len=new_len;
      }
    else
      {
       out_len=strlen(str);
       out_str=(char*)malloc(((out_len<LINE_BUFFER_SIZE)?LINE_BUFFER_SIZE:out_len)+1);
       memcpy(out_str,str,out_len+1);
      }
   }
 else if(out_err!=-1)
    out_err=write_buffer_data(out_fd,str,strlen(str)); /* expect to make lots of small writes */
}


/*++++++++++++++++++++++++++++++++++++++
  Add a new variable and value combination.
  The list of variable-value pairs is kept ordered so that on average
  we need to compare with only half the names in the list.

  char *add_variable Returns the newly allocated value.

  const char *var The variable to add.

  const char *val The value of the variable.
  ++++++++++++++++++++++++++++++++++++++*/

static char *add_variable(const char *var,const char *val)
{
  VarValueNode *p,**q,*new;
  int cmp;
  size_t var_sz;

  q= &varvaluelist;

  while((p= *q) && (cmp=strcmp(p->variable,var))<=0) {
    if(cmp==0) {
      if(p->value!=empty)
          free(p->value);
      return(p->value=(val?strdup(val):empty));
    }
    q= &p->next;
  }

  var_sz= strlen(var)+1;
  new= malloc(sizeof(VarValueNode)+var_sz);
  new->next= p;
  new->value= (val?strdup(val):empty);
  memcpy(new->variable,var,var_sz);
  *q= new;

  return(new->value);
}


/*++++++++++++++++++++++++++++++++++++++
  Delete all of the variables.
  ++++++++++++++++++++++++++++++++++++++*/

static void delete_variables(void)
{
  VarValueNode *p=varvaluelist;

  while(p) {
    VarValueNode *next=p->next;
    if(p->value!=empty)
      free(p->value);
    free(p);
    p=next;
  }

  varvaluelist=NULL;
}


/*++++++++++++++++++++++++++++++++++++++
  Get the value of the named variable.
  The list of variable-value pairs is kept ordered so it is
  not necessary to check every name in the list to conclude that
  the variable is not defined.

  char *get_variable Return the value of the variable or an empty string.

  const char *var The variable to get the value of.
  ++++++++++++++++++++++++++++++++++++++*/

static char *get_variable(const char *var)
{
  VarValueNode *p=varvaluelist;
  int cmp;

  while(p && (cmp=strcmp(p->variable,var))<=0) {
    if(cmp==0)
      return(p->value);
    p=p->next;
  }

  return(empty);
}


/*++++++++++++++++++++++++++++++++++++++
  Call the named function with the specificied variable.

  char *call_function Return the value of the function call.

  char *func The function name to call, including the argument.
  ++++++++++++++++++++++++++++++++++++++*/

static char *call_function(const char *func)
{
 const char *funcname,*bracket1,*bracket2,*var;
 char *val,*result=NULL,*retval;

 if(!(bracket1=strchr(func,'['))) return empty;
 funcname=STRDUPA2(func,bracket1);
 ++bracket1;
 if(!(bracket2=strchr(bracket1,']'))) return empty;
 var=STRDUPA2(bracket1,bracket2);
 val=get_variable(var);

 if(val) {
   if(!strcmp(funcname,"encode"))
     result=URLEncodeFormArgs(val);
   else if(!strcmp(funcname,"decode"))
     result=HTML_url(val);
   else if(!strcmp(funcname,"html"))
     result=HTMLString(val,1,NULL);
   else if(!strcmp(funcname,"htmlitems"))
     result=HTMLString(val,2,NULL);
   else if(!strcmp(funcname,"comment"))
     result=HTMLcommentstring(val,NULL);
 }

 retval=add_variable(func,result);

 if(result && result!=val)
    free(result);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Skip trailing white-space (this is done so that the lex is simpler, no variable trailing context).

  char *string Points to the beginning of the string.
  char *end    Points to the end of the string.
  ++++++++++++++++++++++++++++++++++++++*/

inline static char *skip_trailing_blanks(char *string, char *end)
{
 while(--end>=string && isspace(*end));
 return end+1;
}


#ifndef YY_SKIP_YYWRAP
#define YY_SKIP_YYWRAP 1 /* Remove error with prototype of ..._yywrap */
#endif
#ifndef msg_yywrap
/*+ Needed in lex but does nothing. +*/
#define msg_yywrap() 1
#endif

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
        if((result=read_data(msg_yyfd,buf,max_size))==-1) \
           result=0;

/*+ A macro to append a string to the end of the existing one. +*/
#define APPEND_VAL(xxx) str_append(&val,xxx)

%}

%%
 /* Can use local variables since the parser only returns at EOF. */
 char *var=NULL;
 char *val=NULL;
 int any=0;

[^$%?\\\n \t]+                                                      |
<ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>[^}$%?\\\n]+              { write_or_append_string(yytext); any++; }
\n                                                                  { write_or_append_string(yytext); any=0; }
<ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>\n                        { if(any) write_or_append_string(yytext); any=0; }
<INITIAL,ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>\\[\"\'$%?\\{}]   { write_or_append_string(yytext+1); any++; }
<INITIAL,ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>[$%\\?]           |
[ \t]+                                                              { write_or_append_string(yytext); any++; }
[ \t]*\\\r*\n                                                       { }
<INITIAL,ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>{VAR}             { write_or_append_string(get_variable(yytext+1));any++; }
<INITIAL,ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>{VAR}[ \t]*=      { char *p=yytext+1,*r=yytext+yyleng-1;
								      var=STRDUP2(p,skip_trailing_blanks(p,r));
								      val=NULL; yy_push_state(ASSIGNMENT_START); }
<INITIAL,ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>{VAR}[ \t]*\?[ \t\r\n]*\{  {
                                                                      char *p=yytext+1,*q=strchrnul(p,'?');
								      char *val1=STRDUP3(p,skip_trailing_blanks(p,q),get_variable);
								      if(*val1) yy_push_state(ALTERNATE_USE_FIRST);
								      else yy_push_state(ALTERNATE_SKIP_FIRST); }
<INITIAL,ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>{VAR}[ \t]*\?=[ \t]*{VAR}[ \t\r\n]*\{   {
								      char *p=yytext+1,*q=strchrnul(p,'?'),*r=yytext+yyleng-1;
                                                                      char *val1=STRDUP3(p,skip_trailing_blanks(p,q),get_variable),*val2;
								      q=strchrnul(q+2,'$')+1;
                                                                      val2=STRDUP3(q,skip_trailing_blanks(q,r),get_variable);
								      if(!strcmp(val1,val2)) yy_push_state(ALTERNATE_USE_FIRST);
								      else yy_push_state(ALTERNATE_SKIP_FIRST); }
<INITIAL,ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>{VAR}[ \t]*\?=[^$\'\"\r\n{]*[ \t\r\n]*\{   {
								      char *p=yytext+1,*q=strchrnul(p,'?'),*r=yytext+yyleng-1;
								      char *val1=STRDUP3(p,skip_trailing_blanks(p,q),get_variable); int len;
								      q+=2; while(isspace(*q)) ++q; len=skip_trailing_blanks(q,r)-q;
								      if(!strncmp(val1,q,len) && !val1[len]) yy_push_state(ALTERNATE_USE_FIRST);
								      else yy_push_state(ALTERNATE_SKIP_FIRST); }
<INITIAL,ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>{FUNC}            { write_or_append_string(call_function(yytext+1)); any++; }
<ALTERNATE_USE_FIRST>\}[ \t\r\n]*\{                                 { BEGIN(ALTERNATE_SKIP_SECOND); }
<ALTERNATE_USE_FIRST>\}[ \t\r\n]*{VAR}[ \t]*\?(=[ \t]*{VAR}|=[^$\'\"\r\n{]*)?[ \t\r\n]*\{   { BEGIN(ALTERNATE_SKIP_REST); }
<ALTERNATE_USE_SECOND>\}[ \t]*\r*\n                                 { yy_pop_state(); if(any) write_or_append_string("\n"); any=0; }
<ALTERNATE_USE_SECOND>\}[ \t]+                                      { yy_pop_state(); unput(' '); }
<ALTERNATE_USE_FIRST,ALTERNATE_USE_SECOND>\}                        { yy_pop_state(); }



<ASSIGNMENT_START>[ \t]*/[^\'\" \t\r\n}]    { BEGIN(ASSIGNMENT_WORD); }
<ASSIGNMENT_START>[ \t]*\"                  { BEGIN(ASSIGNMENT_DOUBLE_QUOTES); }
<ASSIGNMENT_START>[ \t]*\'                  { BEGIN(ASSIGNMENT_SINGLE_QUOTES); }
<ASSIGNMENT_START>[ \t]*/\}                 { add_variable(var,NULL); free(var); yy_pop_state(); }
<ASSIGNMENT_START>[ \t]*\r*\n?              { add_variable(var,NULL); free(var); yy_pop_state(); }



<ASSIGNMENT_WORD>[^\\}$% \t\r\n]+                                           |
<ASSIGNMENT_DOUBLE_QUOTES>[^\\\"$%]+                                        |
<ASSIGNMENT_SINGLE_QUOTES>[^\\\'$%]+                                        |
<ASSIGNMENT_WORD,ASSIGNMENT_DOUBLE_QUOTES,ASSIGNMENT_SINGLE_QUOTES>[$%\\]   { APPEND_VAL(yytext); }
<ASSIGNMENT_WORD>\\\}                                                       |
<ASSIGNMENT_DOUBLE_QUOTES>\\\"                                              |
<ASSIGNMENT_SINGLE_QUOTES>\\\'                                              { APPEND_VAL(yytext+1); }
<ASSIGNMENT_WORD,ASSIGNMENT_DOUBLE_QUOTES,ASSIGNMENT_SINGLE_QUOTES>{VAR}    { APPEND_VAL(get_variable(yytext+1)); }
<ASSIGNMENT_WORD,ASSIGNMENT_DOUBLE_QUOTES,ASSIGNMENT_SINGLE_QUOTES>{FUNC}   { APPEND_VAL(call_function(yytext+1)); }
<ASSIGNMENT_WORD>[ \t]*/\}                                                  { add_variable(var,val); if(val)free(val); free(var); yy_pop_state(); }
<ASSIGNMENT_WORD>[ \t]*\r*\n?                                               |
<ASSIGNMENT_DOUBLE_QUOTES>\"                                                |
<ASSIGNMENT_SINGLE_QUOTES>\'                                                { add_variable(var,val); if(val)free(val); free(var); yy_pop_state(); }



<ALTERNATE_SKIP_FIRST,ALTERNATE_SKIP_SECOND,ALTERNATE_SKIP_REST>[^\\}$]+ { }
<ALTERNATE_SKIP_FIRST,ALTERNATE_SKIP_SECOND,ALTERNATE_SKIP_REST>\\\}    { }
<ALTERNATE_SKIP_FIRST,ALTERNATE_SKIP_SECOND,ALTERNATE_SKIP_REST>\\      { }
<ALTERNATE_SKIP_FIRST,ALTERNATE_SKIP_SECOND,ALTERNATE_SKIP_REST>\$      { }
<ALTERNATE_SKIP_FIRST,ALTERNATE_SKIP_SECOND,ALTERNATE_SKIP_REST>{VAR}[ \t]*\?(=[ \t]*{VAR}|=[^$\'\"\r\n{]*)?[ \t\r\n]*\{  { yy_push_state(ALTERNATE_SKIP_REST); }
<ALTERNATE_SKIP_FIRST>\}[ \t\r\n]*\{                                    { BEGIN(ALTERNATE_USE_SECOND); }
<ALTERNATE_SKIP_FIRST>\}[ \t\r\n]*{VAR}[ \t]*\?[ \t\r\n]*\{             { char *p=strchrnul(yytext+1,'$')+1,*q=strchrnul(p,'?');
						                          char *val1=STRDUP3(p,skip_trailing_blanks(p,q),get_variable);
						                          if(*val1) BEGIN(ALTERNATE_USE_FIRST); else BEGIN(ALTERNATE_SKIP_FIRST); }
<ALTERNATE_SKIP_FIRST>\}[ \t\r\n]*{VAR}[ \t]*\?=[ \t]*{VAR}[ \t\r\n]*\{ { char *p=strchrnul(yytext+1,'$')+1,*q=strchrnul(p,'?'),*r=yytext+yyleng-1;
                                                                          char *val1=STRDUP3(p,skip_trailing_blanks(p,q),get_variable),*val2;
						                          q=strchrnul(q+2,'$')+1;
                                                                          val2=STRDUP3(q,skip_trailing_blanks(q,r),get_variable);
								          if(!strcmp(val1,val2)) BEGIN(ALTERNATE_USE_FIRST);
								          else BEGIN(ALTERNATE_SKIP_FIRST); }
<ALTERNATE_SKIP_FIRST>\}[ \t\r\n]*{VAR}[ \t]*\?=[^$\'\"\r\n{]*[ \t\r\n]*\{  {
								          char *p=strchrnul(yytext+1,'$')+1,*q=strchrnul(p,'?'),*r=yytext+yyleng-1;
                                                                          char *val1=STRDUP3(p,skip_trailing_blanks(p,q),get_variable); int len;
								          q+=2; while(isspace(*q)) ++q; len=skip_trailing_blanks(q,r)-q;
								          if(!strncmp(val1,q,len) && !val1[len]) BEGIN(ALTERNATE_USE_FIRST);
								          else BEGIN(ALTERNATE_SKIP_FIRST); }
<ALTERNATE_SKIP_REST>\}[ \t\r\n]*\{                                     { BEGIN(ALTERNATE_SKIP_SECOND); }
<ALTERNATE_SKIP_REST>\}[ \t\r\n]*{VAR}[ \t]*\?(=[ \t]*{VAR}|=[^$\'\"\r\n{]*)?[ \t\r\n]*\{  { }
<ALTERNATE_SKIP_SECOND,ALTERNATE_SKIP_REST>\}[ \t]*\r*\n                { yy_pop_state(); if(any) write_or_append_string("\n"); any=0; }
<ALTERNATE_SKIP_SECOND,ALTERNATE_SKIP_REST>\}[ \t]+                     { yy_pop_state(); unput(' '); }
<ALTERNATE_SKIP_FIRST,ALTERNATE_SKIP_SECOND,ALTERNATE_SKIP_REST>\}      { yy_pop_state(); }

 /* End of file */

<<EOF>>                                 { BEGIN(INITIAL); return(0); }

%%
