W               [ \t\r\n\f]

nonascii        [\200-\377]
ascii           [ -~]
alphanum        [a-z0-9]
punct           [][!\"#$%&\'()*+,-./:;<=>?@\\^_`{|}~]
safepunct       [][!\#$%&\*+,-./:;=?@\\^_`{|}~]

tag             {alphanum}+
key             ({alphanum}|-)+
val             ({alphanum}|{nonascii}|{safepunct})+

%x DOCTYPE
%x COMMENT COMMENT_BAD
%x TAG_START TAG TAG_ATTR_KEY TAG_ATTR_VAL
%x DQUOTED SQUOTED
%x SCRIPT_START SCRIPT SCRIPT_COMMENT_C SCRIPT_COMMENT_CPP SCRIPT_DQUOTED SCRIPT_SQUOTED
%x STYLE_START STYLE

%{
/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/htmlmodify.l 1.74 2004/09/29 18:07:36 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.8d.
  Parse the HTML and modify the source.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02,03,04 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include <sys/types.h>
#include <unistd.h>
#include <sys/stat.h>

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

#include "wwwoffle.h"
#include "io.h"
#include "misc.h"
#include "errors.h"
#include "config.h"
#include "document.h"


/* Parser outputs */

#define LEX_PLAINTEXT      1
#define LEX_COMMENT        2
#define LEX_DOCTYPE        3

#define LEX_TAG_BEGIN     11
#define LEX_TAG_END       12
#define LEX_TAG_END_XHTML 13

#define LEX_ATTR_KEY      21
#define LEX_ATTR_VAL      22
#define LEX_ATTR_VAL_SQ   23
#define LEX_ATTR_VAL_DQ   24

/*+ Tag types +*/

typedef enum _HTMLTags
{
 tag_a           /* "a"         */ ,
 tag__a          /* "/a"        */ ,
 tag_applet      /* "applet"    */ ,
 tag__applet     /* "/applet"   */ ,
 tag_base        /* "base"      */ ,
 tag__base       /* "/base"     */ ,
 tag_blink       /* "blink"     */ ,
 tag__blink      /* "/blink"    */ ,
 tag_body        /* "body"      */ ,
 tag__body       /* "/body"     */ ,
 tag_embed       /* "embed"     */ ,
 tag__embed      /* "/embed"    */ ,
 tag_html        /* "html"      */ ,
 tag__html       /* "/html"     */ ,
 tag_iframe      /* "iframe"    */ ,
 tag__iframe     /* "/iframe"   */ ,
 tag_img         /* "img"       */ ,
 tag__img        /* "/img"      */ ,
 tag_link        /* "link"      */ ,
 tag__link       /* "/link"     */ ,
 tag_marquee     /* "marquee"   */ ,
 tag__marquee    /* "/marquee"  */ ,
 tag_meta        /* "meta"      */ ,
 tag__meta       /* "/meta"     */ ,
 tag_noscript    /* "noscript"  */ ,
 tag__noscript   /* "/noscript" */ ,
 tag_object      /* "object"    */ ,
 tag__object     /* "/object"   */ ,
 tag_param       /* "param"     */ ,
 tag__param      /* "/param"    */ ,
 tag_script      /* "script"    */ ,
 tag__script     /* "/script"   */ ,
 tag_style       /* "style"     */ ,
 tag__style      /* "/style"    */
}
HTMLTags;

/*+ Tag strings. Maintain in alphabetical order. +*/

static char *tags[]=
{
  "a"          ,
  "applet"     ,
  "base"       ,
  "blink"      ,
  "body"       ,
  "embed"      ,
  "html"       ,
  "iframe"     ,
  "img"        ,
  "link"       ,
  "marquee"    ,
  "meta"       ,
  "noscript"   ,
  "object"     ,
  "param"      ,
  "script"     ,
  "style"
};

/* Table for complex tags, which are stored and processed as a whole. */
static char complex_tag[]=
{
  1  /* "a"        */ ,
  0  /* "applet"   */ ,
  0  /* "base"     */ ,
  0  /* "blink"    */ ,
  0  /* "body"     */ ,
  1  /* "embed"    */ ,
  0  /* "html"     */ ,
  1  /* "iframe"   */ ,
  1  /* "img"      */ ,
  1  /* "link"     */ ,
  0  /* "marquee"  */ ,
  1  /* "meta"     */ ,
  0  /* "noscript" */ ,
  1  /* "object"   */ ,
  0  /* "param"    */ ,
  1  /* "script"   */ ,
  1  /* "style"    */
};


/*+ Attribute types +*/

typedef enum _HTMLAttributes
{
 att_classid       /* "classid"     */ ,
 att_codetype      /* "codetype"    */ ,
 att_content       /* "content"     */ ,
 att_data          /* "data"        */ ,
 att_height        /* "height"      */ ,
 att_href          /* "href"        */ ,
 att_http_equiv    /* "http-equiv"  */ ,
 att_onblur        /* "onblur"      */ ,
 att_onchange      /* "onchange"    */ ,
 att_onclick       /* "onclick"     */ ,
 att_ondblclick    /* "ondblclick"  */ ,
 att_onfocus       /* "onfocus"     */ ,
 att_onkeydown     /* "onkeydown"   */ ,
 att_onkeypress    /* "onkeypress"  */ ,
 att_onload        /* "onload"      */ ,
 att_onmousedown   /* "onmousedown" */ ,
 att_onmousemove   /* "onmousemove" */ ,
 att_onmouseout    /* "onmouseout"  */ ,
 att_onmouseover   /* "onmouseover" */ ,
 att_onmouseup     /* "onmouseup"   */ ,
 att_onreset       /* "onreset"     */ ,
 att_onselect      /* "onselect"    */ ,
 att_onsubmit      /* "onsubmit"    */ ,
 att_onunload      /* "onunload"    */ ,
 att_rel           /* "rel"         */ ,
 att_src           /* "src"         */ ,
 att_style         /* "style"       */ ,
 att_type          /* "type"        */ ,
 att_width         /* "width"       */
}
HTMLAttributes;

/*+ Attribute strings. Maintain in alphabetical order. +*/

static char *attributes[]=
{
  "classid"     ,
  "codetype"    ,
  "content"     ,
  "data"        ,
  "height"      ,
  "href"        ,
  "http-equiv"  ,
  "onblur"      ,
  "onchange"    ,
  "onclick"     ,
  "ondblclick"  ,
  "onfocus"     ,
  "onkeydown"   ,
  "onkeypress"  ,
  "onload"      ,
  "onmousedown" ,
  "onmousemove" ,
  "onmouseout"  ,
  "onmouseover" ,
  "onmouseup"   ,
  "onreset"     ,
  "onselect"    ,
  "onsubmit"    ,
  "onunload"    ,
  "rel"         ,
  "src"         ,
  "style"       ,
  "type"        ,
  "width"
};

/*+ A structure to hold a tag and its attributes. +*/

typedef struct _Tag
{
 HTMLTags type;                 /*+ The type of the tag. +*/

 char *tag;                     /*+ The Tag itself. +*/

 int xhtml;                     /*+ A flag that is set for an XHTML closing tag '< ... />' +*/

 int nattr;                     /*+ The number of attributes. +*/
 int nattr_malloc;              /*+ The number of attributes that space is malloced for. +*/

 int *attr_type;                /*+ The list of attribute types. +*/
 char **attr_key;               /*+ The list of attribute keys. +*/
 char **attr_val;               /*+ The list of attribute values. +*/
 char **attr_quote;             /*+ The list of attribute quotes. +*/
}
Tag;

/*+ A stack for holding tags of possibly nested objects and their disabled status. +*/
typedef struct _TagStackNode
{
  struct _TagStackNode *next;
  HTMLTags type;
  int disabled;
}
TagStackNode;

typedef struct _TagStackNode *TagStack;


/* Microsoft Character mapping */

/*+ The option to convert the characters when seen. +*/
static int demoronise_ms_chars;
static int fix_mixed_cyrillic;


/* Definitions of why the output is disabled. */

#define DISABLE_NONE         0

#define DISABLE_META         1
#define DISABLE_LINK         2
#define DISABLE_OBJECT       4
#define DISABLE_A            8
#define DISABLE_IFRAME      16
#define DISABLE_IMG         32
#define DISABLE_SCRIPT      64
#define DISABLE_STYLE      128

#define DISABLE_PARSE      256

/* Local functions */

static void modify_html(URL *Url);

static /*@null@*/ char *htmlmodify_yylval=NULL;
extern int htmlmodify_yylex(void);

static /*@null@*/ char *extract_charset(char *content_type);

static int handle_a_tag(Tag *tag);
static int handle_iframe_tag(Tag *tag);
static int handle_script_tag(Tag *tag);
/* static void handle_img_tag(Tag *tag,int replace_dontget,char *dontget_replacement,
				       int replace_webbug,char *webbug_replacement); */
static int handle_object_tag(Tag *tag,int replace_dontget,char *dontget_replacement,
                                      int replace_webbug,char *webbug_replacement,
                                      int disable_applet,
                                      int disable_flash,
                                      int disable_dontget_iframes);
static void output_img_or_object_tag(Tag *tag,int src_att,
                                              int replace_dontget,char *dontget_replacement,
                                              int replace_webbug,char *webbug_replacement);
static void handle_link_tag(Tag *tag);
static int handle_style_script_tag(Tag *tag);
static void handle_meta_tag(Tag *tag,int disable_meta_refresh,int disable_meta_refresh_self,
			    int replacement_meta_refresh_time,int disable_meta_set_cookie);
static void output_tag(Tag *tag,char *prefix,char *suffix);

static void handle_high_bit(unsigned char ch);


/*+ The add-cache-info optional footer. +*/
static /*@null@*/ /*@observer@*/ char *cache_info=NULL;

/*+ The name of the insert-file optional footer. +*/
static char *insertfile=NULL;

/*+ The base URL of this page. +*/
static /*@null@*/ URL *baseUrl=NULL;

/*+ Set this to disable the output. +*/
static int disable_output=DISABLE_NONE;
static int commentout=0;

/* Prepend a string (pre) to an existing string (str).
   The result is stored in a newly malloced string and assigned to *str.
   The old value of *str is freed.
*/
inline static void str_prepend(const char *pre,char **str)
{
  char *oldstr=*str,*newstr;
  size_t lenpre=strlen(pre), szold=strlen(oldstr)+1;
  newstr=malloc(lenpre+szold);
  mempcpy(mempcpy(newstr,pre,lenpre),oldstr,szold);
  free(oldstr);
  *str=newstr;
}

inline static void setbaseUrl(URL *Url)
{
  if(Url) {
    if(baseUrl) FreeURL(baseUrl);
    baseUrl=CopyURL(Url);
  }
}

inline static void setbase_url(char *url)
{
  if(url) {
    if(baseUrl) FreeURL(baseUrl);
    baseUrl=SplitURL(url);
  }
}

inline static void cleanup_baseUrl()
{
  if(baseUrl) {
    FreeURL(baseUrl);
    baseUrl=NULL;
  }
}


#define ARRAYLEN(a) (sizeof(a)/sizeof(a[0]))

inline static HTMLTags lookuptag(const char *s)
{
  int i,j;
  int endtag=0;

  if(*s=='/') {endtag=1; ++s;}

  i=0; j=ARRAYLEN(tags);

  while(i<j) {
    int k=(i+j)/2;
    int cmp=strcasecmp(s,tags[k]);
    if(cmp<0)
      j=k;
    else if(cmp>0)
      i=k+1;
    else
      return 2*k+endtag;
  }

  return -1;
}

inline static HTMLAttributes lookupattribute(const char *s)
{
  int i,j;

  i=0; j=ARRAYLEN(attributes);

  while(i<j) {
    int k=(i+j)/2;
    int cmp=strcasecmp(s,attributes[k]);
    if(cmp<0)
      j=k;
    else if(cmp>0)
      i=k+1;
    else
      return k;
  }

  return -1;
}

inline static int is_complex_tag(HTMLTags t)
{
  if(t<0 || t&1) return 0;
  t /= 2;
  return (t<ARRAYLEN(complex_tag))?complex_tag[t]:0;
}


/* Some functions to enable the handling of tag data in a more abstract style. */

inline static void init_tagdata(Tag *tagdata)
{
  tagdata->type=-1;
  tagdata->tag=NULL;
  tagdata->xhtml=0;
  tagdata->nattr=0;
  tagdata->nattr_malloc=16;
  tagdata->attr_type=(int*)malloc(tagdata->nattr_malloc*sizeof(int));
  tagdata->attr_key=(char**)malloc(tagdata->nattr_malloc*sizeof(char*));
  tagdata->attr_val=(char**)malloc(tagdata->nattr_malloc*sizeof(char*));
  tagdata->attr_quote=(char**)malloc(tagdata->nattr_malloc*sizeof(char*));
}

inline static void reset_tagdata(Tag *tagdata, HTMLTags type,char *tagstr)
{
  int i;

  tagdata->type=type;
  if(tagdata->tag) free(tagdata->tag);
  tagdata->tag= (tagstr?strdup(tagstr):NULL);
  tagdata->xhtml=0;
  for(i=0;i<tagdata->nattr;i++)
    {
      if(tagdata->attr_key[i]) free(tagdata->attr_key[i]);
      if(tagdata->attr_val[i]) free(tagdata->attr_val[i]);
    }
  tagdata->nattr=0;
}

inline static void add_attribute(Tag *tag, int type, char *key, char *val, char *quote)
{
  if(tag->nattr==tag->nattr_malloc)
    {
      ++tag->nattr_malloc;
      tag->attr_type=(int*)realloc((void*)tag->attr_type,(tag->nattr_malloc)*sizeof(int));
      tag->attr_key=(char**)realloc((void*)tag->attr_key,(tag->nattr_malloc)*sizeof(char*));
      tag->attr_val=(char**)realloc((void*)tag->attr_val,(tag->nattr_malloc)*sizeof(char*));
      tag->attr_quote=(char**)realloc((void*)tag->attr_quote,(tag->nattr_malloc)*sizeof(char*));
    }

  tag->attr_type [tag->nattr]=type;
  tag->attr_key  [tag->nattr]=strdup(key);
  tag->attr_val  [tag->nattr]=(val?strdup(val):NULL);
  tag->attr_quote[tag->nattr]=quote;

  ++tag->nattr;
}

inline static void free_tagdata(Tag *tagdata)
{
  int i;

  if(tagdata->tag) free(tagdata->tag);

  for(i=0;i<tagdata->nattr;i++)
    {
      if(tagdata->attr_key[i]) free(tagdata->attr_key[i]);
      if(tagdata->attr_val[i]) free(tagdata->attr_val[i]);
    }

  free(tagdata->attr_type);
  free(tagdata->attr_key);
  free(tagdata->attr_val);
  free(tagdata->attr_quote);
}

/* Some functions for handling tag stacks. */

inline static HTMLTags endtag2tag(HTMLTags endtag)
{
  return (endtag>=0 && endtag&1)? endtag-1 : -1;
}

/* Push a tag and its value on a stack. */
static void pushtagstack(TagStack *s,HTMLTags t, int disabled)
{
  TagStack new=malloc(sizeof(TagStackNode));
  new->next= *s;
  new->type= t;
  new->disabled= disabled;

  *s= new;
}

/* Get the value of the first stack node of the matching tag type. */
static int toptagstack(TagStack s,HTMLTags endtag)
{
  HTMLTags tag=endtag2tag(endtag);

  if(tag<0)
    return 0;

  while(s) {
    if(s->type==tag)
      return s->disabled;
    s=s->next;
  }

  return 0;
}

/* Remove the first stack node of the matching tag type and return its value. */
static int poptagstack(TagStack *s,HTMLTags endtag)
{
  TagStack p, *q;
  HTMLTags tag=endtag2tag(endtag);

  if(tag<0)
    return 0;

  q= s;
  while((p= *q)) {
    if(p->type==tag) {
      int retval= p->disabled;
      *q= p->next;
      free(p);
      return retval;
    }
    q= &p->next;
  }

  return 0;
}

/* Free all the remaining nodes of a stack */
inline static void free_tagstack(TagStack s)
{
  while(s) {
    TagStack next= s->next;
    free(s);
    s=next;
  }
}

/*++++++++++++++++++++++++++++++++++++++
  Output the file with the modificatons if it is HTML, else just output.

  URL *Url The URL that we are parsing.

  int spool The file descriptor for the spool file to get the date from.

  char *content_type The HTTP header containing the content type (and perhaps the charset).
  ++++++++++++++++++++++++++++++++++++++*/

void OutputHTMLWithModifications(URL *Url,int spool,char *content_type)
{
 static int first=1;

 if(ConfigBooleanURL(AddCacheInfo,Url))
   {
    struct stat buf;
    time_t t_ago;
    char date[MAXDATESIZE],timeago[8],*timeunit;

    fstat(spool,&buf);

    t_ago=time(NULL)-buf.st_mtime;
    RFC822Date_r(buf.st_mtime,0,date);
    
    if(t_ago<0)
      {strcpy(timeago,"?");timeunit="";}
    else if(t_ago<3600)
      {sprintf(timeago,"%ld",t_ago/60);timeunit="m";}
    else if(t_ago<(24*3600))
      {sprintf(timeago,"%ld",t_ago/3600);timeunit="h";}
    else if(t_ago<(14*24*3600))
      {sprintf(timeago,"%ld",t_ago/(24*3600));timeunit="d";}
    else if(t_ago<(30*24*3600))
      {sprintf(timeago,"%ld",t_ago/(7*24*3600));timeunit="w";}
    else
      {sprintf(timeago,"%ld",t_ago/(30*24*3600));timeunit="M";}

    cache_info=HTMLMessageString("AddCacheInfo",
                                 "url",Url->name,
                                 "date",date,
                                 "time",timeago,
                                 "unit",timeunit,
                                 NULL);
   }

 insertfile=ConfigStringURL(InsertFile,Url);

 demoronise_ms_chars=0;
 if(ConfigBooleanURL(DemoroniseMSChars,Url))
   {
    char* charset=extract_charset(content_type);

    if(charset)
      {
       if(strcasecmp(charset,"utf-8") &&
          strcasecmp(charset,"koi8-r") &&
          strcasecmp(charset,"euc-kr") &&
          strcasecmp(charset,"big5") &&
          strcasecmp(charset,"iso-2022-jp") &&
          strcasecmp(charset,"chinesebig5"))
          demoronise_ms_chars=1;

       free(charset);
      }
    else
       demoronise_ms_chars=1;
   }

 fix_mixed_cyrillic=0;
 if(ConfigBooleanURL(FixMixedCyrillic,Url))
   {
    char* charset=extract_charset(content_type);

    if(charset)
      {
       if(!strcasecmp(charset,"koi8-r"))
          fix_mixed_cyrillic=1;

       free(charset);
      }
    else
       fix_mixed_cyrillic=1;
   }

 setbaseUrl(Url);

 if(!first)
    htmlmodify_yyrestart(NULL);

 modify_html(Url);

 if(cache_info) {free(cache_info); cache_info=NULL;}
 insertfile=NULL;

 cleanup_baseUrl();

 first=0;
}


/*+ A function to output the data if valid to do so. +*/
static void yy_output(char *text, int commentout)
{
  if(!disable_output && *text) {
    char *copy= ((commentout>0)?HTMLcommentstring(text):text);
    wwwoffles_write_data(copy,strlen(copy));
    if(copy!=text) free(copy);
  }
}

#define YY_OUTPUT(text) yy_output(text,commentout)

/* add_insertfile copies the content of an insert file to the output.
   char *pathname: the path of the file to insert.
*/
static void add_insertfile(char *pathname)
{
  int fd=OpenLocalFile(pathname);

  if(fd==-1)
    return;
  {
    int n;
    char buffer[READ_BUFFER_SIZE];

    while((n=read(fd,buffer,READ_BUFFER_SIZE))>0)
      wwwoffles_write_data(buffer,n);

    if(n<0)
      PrintMessage(Warning,"Could not read from insert-file '%s' [%!s].",pathname);
  }
  close(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Extract the charset from a MIME type and charset.

  char *extract_charset Returns the charset that it found or NULL if none.

  char *content_type The HTTP content type.
  ++++++++++++++++++++++++++++++++++++++*/

static char *extract_charset(char *content_type)
{
 char *p,*charset=NULL;

 /* ' *text/html *; *charset *= *["']?...["']?' */

 p=content_type;

 while(*p && *p!=';') p++;
 if(*p!=';') return(NULL); /* unparseable */
 p++;

 while(isspace(*p)) p++;
 if(!*p) return(NULL); /* unparseable */

 if(!strcasecmp_litbeg(p,"charset"))
   {
    p+=7;

    while(*p && *p!='=') p++;
    if(*p!='=') return(NULL); /* unparseable */
    p++;

    while(isspace(*p)) p++;
    if(!*p) return(NULL); /* unparseable */

    if(*p=='"' || *p=='\'') p++;

    charset=strdup(p);

    p=charset+strlen(p)-1;
    if(*p=='"' || *p=='\'') *p=0;
   }

 return(charset);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the anchor tag and check if
  the href attribute matches the DontGet list.
  Then output the original or disabled tag.

  int handle_a_tag Returns 1 if the anchor was disabled.

  Tag *tag The tag information.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_a_tag(Tag *tag)
{
 int i;
 int is_dontget=0;
 char *href=NULL;

 for(i=0;i<tag->nattr;i++)
   if(tag->attr_type[i]==att_href && (href=tag->attr_val[i])) {
     char *link=LinkURL(baseUrl,href);
     URL* linkUrl=SplitURL(link);

     is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

     if(link!=href)
       free(link);
     FreeURL(linkUrl);
     if(is_dontget)
       break;
   }

 /* Output the original or modified tag. */

 if(is_dontget)
    output_tag(tag,"!-- WWWOFFLE (disable-dontget-links) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return(is_dontget);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the iframe tag and check if
  the src attribute matches the DontGet list.
  Then output the original or disabled tag.

  int handle_iframe_tag Returns 1 if the iframe was disabled.

  Tag *tag The tag information.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_iframe_tag(Tag *tag)
{
 int i;
 int is_dontget=0;
 char *src=NULL;

 for(i=0;i<tag->nattr;i++)
   if(tag->attr_type[i]==att_src && (src=tag->attr_val[i])) {
     char *link=LinkURL(baseUrl,src);
     URL* linkUrl=SplitURL(link);

     is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

     if(link!=src)
       free(link);
     FreeURL(linkUrl);
     if(is_dontget)
       break;
   }

 /* Output the original or modified tag. */

 if(is_dontget)
    output_tag(tag,"!-- WWWOFFLE (disable-dontget-iframes) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return(is_dontget);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the script tag and check if
  the src attribute matches the DontGet list.
  Then output the original or disabled tag.

  int handle_script_tag Returns 1 if the script was disabled.

  Tag *tag The tag information.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_script_tag(Tag *tag)
{
 int i;
 int is_dontget=0;
 char *src=NULL;

 for(i=0;i<tag->nattr;i++)
   if(tag->attr_type[i]==att_src && (src=tag->attr_val[i])) {
     char *link=LinkURL(baseUrl,src);
     URL* linkUrl=SplitURL(link);

     is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

     if(link!=src)
       free(link);
     FreeURL(linkUrl);
     if(is_dontget)
       break;
   }

 /* Output the original or modified tag. */

 if(is_dontget)
    output_tag(tag,"!-- WWWOFFLE (disable-dontget-script) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return(is_dontget);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the img tag and parse it.

  Tag *tag The tag information.

  int replace_dontget The option to replace the images in the DontGet section.

  char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  char *webbug_replacement The webbug replacement image.
  ++++++++++++++++++++++++++++++++++++++*/

inline static void handle_img_tag(Tag *tag,int replace_dontget,char *dontget_replacement,
                                    int replace_webbug,char *webbug_replacement)
{
 output_img_or_object_tag(tag,att_src,replace_dontget,dontget_replacement,replace_webbug,webbug_replacement);
}


/*++++++++++++++++++++++++++++++++++++++
  Output an image tag or an object tag that contains an image.

  Tag *tag The tag.

  int attr_type The attribute type that contains the URI.

  int replace_dontget The option to replace the images in the DontGet section.

  char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  char *webbug_replacement The webbug replacement image.
  ++++++++++++++++++++++++++++++++++++++*/

static void output_img_or_object_tag(Tag *tag,int attr_type,
                                              int replace_dontget,char *dontget_replacement,
                                              int replace_webbug,char *webbug_replacement)
{
 int i;
 int is_dontget=0,is_webbug=0;

 if(attr_type>=0)
   {
     if(replace_dontget) {
       char *src;
       for(i=0;i<tag->nattr;i++)
	 if(tag->attr_type[i]==attr_type && (src=tag->attr_val[i])) {
	   char *link=LinkURL(baseUrl,src);
	   URL* linkUrl=SplitURL(link);

	   is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

	   if(link!=src)
	     free(link);
	   FreeURL(linkUrl);
	   if(is_dontget)
	     break;
	 }
     }

     if(replace_webbug) {
       int seen_src=0,width=1000,height=1000;

       for(i=0;i<tag->nattr;i++) {
	 if(tag->attr_type[i]==attr_type && tag->attr_val[i])
	   seen_src=1;
	 if(tag->attr_type[i]==att_width && tag->attr_val[i])
	   width=atoi(tag->attr_val[i]);
	 else if(tag->attr_type[i]==att_height && tag->attr_val[i])
	   height=atoi(tag->attr_val[i]);
       }

       if(seen_src && width<=1 && height<=1)
	 is_webbug=1;
     }
   }

 /* Modify the src attribute (if required). */

 if(is_dontget)
   {
    output_tag(tag,"!-- WWWOFFLE (replace-dontget-images) - "," --");

    if(!dontget_replacement)
      return;

    for(i=0;i<tag->nattr;i++)
      if(tag->attr_type[i]==attr_type) {
	if(tag->attr_val[i]) free(tag->attr_val[i]);
	tag->attr_val[i]=strdup(dontget_replacement);
      }
   }
 else if(is_webbug)
   {
    output_tag(tag,"!-- WWWOFFLE (replace-webbug-images) - "," --");

    if(!webbug_replacement)
      return;

    for(i=0;i<tag->nattr;i++)
      if(tag->attr_type[i]==attr_type) {
	if(tag->attr_val[i]) free(tag->attr_val[i]);
	tag->attr_val[i]=strdup(webbug_replacement);
      }
   }

 /* Blank the alt & title attributes or add an empty alt (if required). */

 if(is_dontget || is_webbug)
   {
    int seen_alt=0;

    for(i=0;i<tag->nattr;i++)
       if(!strcasecmp(tag->attr_key[i],"alt") || !strcasecmp(tag->attr_key[i],"title"))
         {
          if(tag->attr_val[i])
             free(tag->attr_val[i]);

          tag->attr_val  [i]=strdup("");
          tag->attr_quote[i]="\"";

          seen_alt=1;
         }

    if(!seen_alt)
      {
	add_attribute(tag, -1,"alt","","\"");
      }
   }

 /* Output the original or modified tag. */

 output_tag(tag,NULL,NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the object tag and parse it.

  int handle_object_tag Returns 1 if the object is Java and disabled,
                                2 if Flash and disabled,
                                3 if equivalent to an iframe and disabled.
				0 otherwise.

  Tag *tag The tag information.

  int replace_dontget The option to replace the images in the DontGet section.

  char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  char *webbug_replacement The webbug replacement image.

  int disable_applet The option to disable Java applets.

  int disable_flash The option to disable Flash animations.

  int disable_dontget_iframes The option to disable inline frames that are on the dontget list.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_object_tag(Tag *tag,int replace_dontget,char *dontget_replacement,
                                      int replace_webbug,char *webbug_replacement,
                                      int disable_applet,
                                      int disable_flash,
                                      int disable_dontget_iframes)
{
 int i;

 /* Check for images. */

 if(replace_dontget || replace_webbug)
   {
    for(i=0;i<tag->nattr;i++)
      if((tag->attr_type[i]==att_codetype || tag->attr_type[i]==att_type) &&
	 tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"image"))
	 {
	   /* found an image */
	   output_img_or_object_tag(tag,att_data,replace_dontget,dontget_replacement,replace_webbug,webbug_replacement);
	   return 0;
	 }
   }

 /* Check for Java */

 if(disable_applet)
   {
    for(i=0;i<tag->nattr;i++)
      if((tag->attr_type[i]==att_codetype && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"application/java")) ||
	 (tag->attr_type[i]==att_classid && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"java:")))
	{
	  /* found Java applet */
	  output_tag(tag,"!-- WWWOFFLE (disable-applet) - "," --");
	  return 1;
	}
   }

 /* Check for Flash */

 if(disable_flash)
   {
    int val_len;
    for(i=0;i<tag->nattr;i++)
      if(((tag->attr_type[i]==att_codetype || tag->attr_type[i]==att_type) &&
          tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"application/x-shockwave-flash")) ||
	 (tag->attr_type[i]==att_classid && tag->attr_val[i] &&
	  !strcasecmp_litbeg(tag->attr_val[i],"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000")) ||
	 (tag->attr_type[i]==att_src && tag->attr_val[i] && (val_len=strlen(tag->attr_val[i]))>=4 &&
	  !strcasecmp(tag->attr_val[i]+val_len-4,".swf")))
	{
	  /* found Flash */
	  output_tag(tag,"!-- WWWOFFLE (disable-flash) - "," --");
	  return 2;
	}
   }

 /* Check for inline HTML (text) object */

 if(disable_dontget_iframes)
   {
    int is_inline=0,is_dontget=0;
    for(i=0;i<tag->nattr;i++)
      if((tag->attr_type[i]==att_codetype || tag->attr_type[i]==att_type) &&
	 tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"text"))
          is_inline=1;
       else if(tag->attr_type[i]==att_data && tag->attr_val[i])
         {
          char *link=LinkURL(baseUrl,tag->attr_val[i]);
          URL* linkUrl=SplitURL(link);

          if(ConfigBooleanMatchURL(DontGet,linkUrl))
	    is_dontget=1;

          if(link!=tag->attr_val[i])
             free(link);
          FreeURL(linkUrl);
         }
    if(is_inline && is_dontget) {
      /* found dontget iframe */
      output_tag(tag,"!-- WWWOFFLE (disable-dontget-iframes) - "," --");
      return 3;
    }
   }

 /* Output the original tag. */
    output_tag(tag,NULL,NULL);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the link tag and check if
  the rel attribute equals "Stylesheet".
  Then output the original or disabled tag.

  Tag *tag The tag information.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_link_tag(Tag *tag)
{
 int i;
 int is_stylesheet=0;

 for(i=0;i<tag->nattr;i++)
   if(tag->attr_type[i]==att_rel && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"Stylesheet")) {
     is_stylesheet=1;
     break;
   }

 /* Output the original or modified tag. */

 if(is_stylesheet)
    output_tag(tag,"!-- WWWOFFLE (disable-style) - "," --");
 else
    output_tag(tag,NULL,NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the style tag and check if
  the type attribute contains the string "javascript".
  Then output the original or disabled tag.

  int handle_style_script_tag Returns true if a script was detected and disabled.

  Tag *tag The tag information.

  See "WhiteHat Security Advisory [Number: WH-08152001-1]" for the details of this.

  The HTML <style type="application/x-javascript"> or <style type="text/javascript">
  can introduce Javascript that WWWOFFLE would not otherwise have blocked.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_style_script_tag(Tag *tag)
{
 int i;
 int is_really_script=0;
 char *type;

 for(i=0;i<tag->nattr;i++)
   if(tag->attr_type[i]==att_type && (type=tag->attr_val[i]))
     {
       int j=strlen(type)-10;

       while(j>=0) /* strcasestr() */
	 {
	   if(!strcasecmp_litbeg(&type[j],"javascript"))
	     break;
	   j--;
	 }

       if(j>=0) {
	 is_really_script=1;
	 break;
       }	 
     }

 /* Output the original or modified tag. */

 if(is_really_script)
    output_tag(tag,"!-- WWWOFFLE (disable-script) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return(is_really_script);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the meta tag and parse it.

  Tag *tag The tag information.

  int disable_meta_refresh Set to the disable-meta-refresh option.

  int disable_meta_refresh_self Set to the disable-meta-refresh-self option.

  int disable_meta_set_cookie Set to the disable-meta-set-cookie option.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_meta_tag(Tag *tag,int disable_meta_refresh,int disable_meta_refresh_self,
			    int replacement_meta_refresh_time,int disable_meta_set_cookie)
{
 int i;
 int is_meta_http_equiv_refresh=0;
 int meta_refresh_content=-1;
 int is_meta_refresh_self=0;
 char *meta_refresh=NULL;
 int is_meta_http_equiv_set_cookie=0;
 int is_meta_http_equiv_content_type=0;

 /* Check for interesting meta tags */

 for(i=0;i<tag->nattr;i++)
   if(tag->attr_type[i]==att_http_equiv && tag->attr_val[i]) {
     if(!strcasecmp_litbeg(tag->attr_val[i],"Refresh"))
       is_meta_http_equiv_refresh=1;
     else if(!strcasecmp_litbeg(tag->attr_val[i],"Set-Cookie"))
       is_meta_http_equiv_set_cookie=1;
     else if(!strcasecmp_litbeg(tag->attr_val[i],"Content-Type"))
       is_meta_http_equiv_content_type=1;
   }

 /* Handle if they are refresh ones. */

 if(is_meta_http_equiv_refresh && (disable_meta_refresh_self || disable_meta_refresh))
   {
    for(i=0;i<tag->nattr;i++)
       if(tag->attr_type[i]==att_content && tag->attr_val[i] && tag->attr_val[i][0])
         {
	   char *p=tag->attr_val[i];

	   /* ' *[0-9].?[0-9]* *[;,] *(URL *= *|)http://...' */

	   while(isspace(*p)) p++;
	   if(isdigit(*p))
	     {
	       while(isdigit(*++p));
	       if(*p=='.')
		 {while(isdigit(*++p));}
	       while(isspace(*p)) p++;
	       if(!*p)
		 {
		   is_meta_refresh_self=1;
		 }
	       else if(*p==';' || *p==',')
		 {
		   while(isspace(*++p));
		   if(!strcasecmp_litbeg(p,"URL"))
		     {
		       char *q=p;
		       p+=3;
		       while(isspace(*p)) p++;
		       if(*p=='=')
			 {
			   while(isspace(*++p));
			 }
		       else
			 p=q;
		     }
                   if(*p)
                     {
		       meta_refresh=strdup(p);
                     }
		 }
	     }
	   meta_refresh_content=i;
	   break;
         }

    /* Check if link is to itself. */

    if(meta_refresh)
      {
       char *link=LinkURL(baseUrl,meta_refresh);
       URL* linkUrl=SplitURL(link);

       if(link!=meta_refresh)
          free(link);

       if(!strcmp(baseUrl->name,linkUrl->name))
          is_meta_refresh_self=1;

       FreeURL(linkUrl);
      }
   }

 /* Handle if they are charset ones. */

 if(is_meta_http_equiv_content_type && (demoronise_ms_chars || fix_mixed_cyrillic))
    for(i=0;i<tag->nattr;i++)
       if(tag->attr_type[i]==att_content && tag->attr_val[i] && tag->attr_val[i][0])
         {
          char* charset=extract_charset(tag->attr_val[i]);

          if(charset)
            {
             if(demoronise_ms_chars)
               {
                if(!strcasecmp(charset,"utf-8") ||
                   !strcasecmp(charset,"koi8-r") ||
                   !strcasecmp(charset,"euc-kr") ||
                   !strcasecmp(charset,"big5") ||
                   !strcasecmp(charset,"iso-2022-jp") ||
                   !strcasecmp(charset,"chinesebig5"))
                   demoronise_ms_chars=0;
               }
             else if(fix_mixed_cyrillic)
               {
                if(strcasecmp(charset,"koi8-r"))
                   fix_mixed_cyrillic=0;
               }

             free(charset);
            }
         }

 /* Output the original or modified tag. */

 if(is_meta_http_equiv_refresh && disable_meta_refresh)
   {
     if(replacement_meta_refresh_time<0 || (replacement_meta_refresh_time==0 && (is_meta_refresh_self || !meta_refresh)) || meta_refresh_content<0)
       output_tag(tag,"!-- WWWOFFLE (disable-meta-refresh) - "," --");
     else
       {
	 free(tag->attr_val[meta_refresh_content]);
	 tag->attr_val[meta_refresh_content]=(meta_refresh?
					      x_asprintf("%d;URL=%s",replacement_meta_refresh_time,meta_refresh):
					      x_asprintf("%d",replacement_meta_refresh_time));
	 output_tag(tag,NULL,NULL);
       }
   }
 else if(is_meta_refresh_self && disable_meta_refresh_self)
   {
     if(replacement_meta_refresh_time<=0 || meta_refresh_content<0)
       output_tag(tag,"!-- WWWOFFLE (disable-meta-refresh-self) - "," --");
     else
       {
	 free(tag->attr_val[meta_refresh_content]);
	 tag->attr_val[meta_refresh_content]=(meta_refresh?
					      x_asprintf("%d;URL=%s",replacement_meta_refresh_time,meta_refresh):
					      x_asprintf("%d",replacement_meta_refresh_time));
	 output_tag(tag,NULL,NULL);
       }
   }
 else if(is_meta_http_equiv_set_cookie && disable_meta_set_cookie)
    output_tag(tag,"!-- WWWOFFLE (disable-meta-set-cookie) - "," --");
 else
    output_tag(tag,NULL,NULL);

 if(meta_refresh)
   free(meta_refresh);
}


/*++++++++++++++++++++++++++++++++++++++
  Output a complete tag with optional custom head and/or tail.

  Tag *tag The tag to output.

  char *prefix The optional prefix of the tag.

  char *suffix The optional suffix of the tag.
  ++++++++++++++++++++++++++++++++++++++*/

static void output_tag(Tag *tag,char *prefix,char *suffix)
{
 int i;

 YY_OUTPUT("<");

 if(prefix)
   {YY_OUTPUT(prefix);}

 YY_OUTPUT(tag->tag);

 for(i=0;i<tag->nattr;i++)
   {
    char *attrval;
    YY_OUTPUT(" ");
    yy_output(tag->attr_key[i],prefix!=NULL);
    if((attrval=tag->attr_val[i]))
      {
       YY_OUTPUT("=");
       YY_OUTPUT(tag->attr_quote[i]);
       yy_output(attrval,prefix!=NULL);
       YY_OUTPUT(tag->attr_quote[i]);
      }
   }

 if(suffix)
   {YY_OUTPUT(suffix);}

 if(tag->xhtml && !suffix)
   {YY_OUTPUT(" />");}
 else
   {YY_OUTPUT(">");}
}


/*++++++++++++++++++++++++++++++++++++++
  Modify the HTML looking for all of the things to be changed.

  URL *Url The URL that this page comes from.
  ++++++++++++++++++++++++++++++++++++++*/

static void modify_html(URL *Url)
{
 HTMLTags tag=-1;
 HTMLAttributes key=-1;
 int url_cached=0;
 int yychar;
 char *key_string=NULL,*prefix=NULL,*suffix=NULL,*quote;
 Tag tagdata;
 TagStack tagstack=NULL;

 char *anchor_modify_begin[3];
 char *anchor_modify_end[3];
 int anchor_modify=0;
 int disable_script=ConfigBooleanURL(DisableHTMLScript,Url);
 int disable_dontget_script=ConfigBooleanURL(DisableHTMLDontGetScript,Url),disabled_dontget_script=0;
 int disable_applet=ConfigBooleanURL(DisableHTMLApplet,Url);
 int disable_style=ConfigBooleanURL(DisableHTMLStyle,Url),disable_style_script=0;
 int disable_blink=ConfigBooleanURL(DisableHTMLBlink,Url);
 int disable_marquee=ConfigBooleanURL(DisableHTMLMarquee,Url);
 int disable_flash=ConfigBooleanURL(DisableHTMLFlash,Url);
 int disable_meta_refresh=ConfigBooleanURL(DisableHTMLMetaRefresh,Url);
 int disable_meta_refresh_self=ConfigBooleanURL(DisableHTMLMetaRefreshSelf,Url);
 int replacement_meta_refresh_time=ConfigIntegerURL(ReplacementHTMLMetaRefreshTime,Url);
 int disable_meta_set_cookie=ConfigBooleanURL(DisableHTMLMetaSetCookie,Url);
 int disable_dontget_anchors=ConfigBooleanURL(DisableHTMLDontGetAnchors,Url),disable_anchor=0;
 int disable_dontget_iframes=ConfigBooleanURL(DisableHTMLDontGetIFrames,Url),disable_iframe=0;
 int replace_html_dontget_images=ConfigBooleanURL(ReplaceHTMLDontGetImages,Url);
 char *replacement_html_dontget_image=NULL,*replacement_html_dontget_image_copy=NULL;
 int replace_html_webbug_images=ConfigBooleanURL(ReplaceHTMLWebbugImages,Url);
 char *replacement_html_webbug_image=NULL,*replacement_html_webbug_image_copy=NULL;

 anchor_modify_begin[0]=ConfigStringURL(AnchorModifyBegin[0],Url);
 anchor_modify_begin[1]=ConfigStringURL(AnchorModifyBegin[1],Url);
 anchor_modify_begin[2]=ConfigStringURL(AnchorModifyBegin[2],Url);
 anchor_modify_end[0]=ConfigStringURL(AnchorModifyEnd[0],Url);
 anchor_modify_end[1]=ConfigStringURL(AnchorModifyEnd[1],Url);
 anchor_modify_end[2]=ConfigStringURL(AnchorModifyEnd[2],Url);

 if( (anchor_modify_begin[0] && anchor_modify_begin[0][0]) || 
     (anchor_modify_begin[1] && anchor_modify_begin[1][0]) || 
     (anchor_modify_begin[2] && anchor_modify_begin[2][0]) || 
     (anchor_modify_end[0] && anchor_modify_end[0][0]) || 
     (anchor_modify_end[1] && anchor_modify_end[1][0]) || 
     (anchor_modify_end[2] && anchor_modify_end[2][0]) )
   anchor_modify=1;

 if(replace_html_dontget_images) {
   replacement_html_dontget_image=ConfigStringURL(ReplacementHTMLDontGetImage,Url);
   if(replacement_html_dontget_image && *replacement_html_dontget_image=='/') {
     char *localhost=GetLocalHost(1);

     replacement_html_dontget_image_copy=(char*)malloc(strlen(replacement_html_dontget_image)+strlen(localhost)+sizeof("http://"));
     stpcpy(stpcpy(stpcpy(replacement_html_dontget_image_copy,"http://"),localhost),replacement_html_dontget_image);
     replacement_html_dontget_image=replacement_html_dontget_image_copy;
     free(localhost);
   }
 }

 if(replace_html_webbug_images) {
   replacement_html_webbug_image=ConfigStringURL(ReplacementHTMLWebbugImage,Url);
   if(replacement_html_webbug_image && *replacement_html_webbug_image=='/') {
     char *localhost=GetLocalHost(1);

     replacement_html_webbug_image_copy=(char*)malloc(strlen(replacement_html_webbug_image)+strlen(localhost)+sizeof("http://"));
     stpcpy(stpcpy(stpcpy(replacement_html_webbug_image_copy,"http://"),localhost),replacement_html_webbug_image);
     replacement_html_webbug_image=replacement_html_webbug_image_copy;
     free(localhost);
   }
 }

 /* Initialise the tagdata */

 init_tagdata(&tagdata);

 /* The actual parser. */

 while((yychar=htmlmodify_yylex()))
    switch(yychar)
      {
      case LEX_PLAINTEXT:
       break;

      case LEX_COMMENT:
       break;

      case LEX_DOCTYPE:
       break;

      case LEX_TAG_BEGIN:
       tag=lookuptag(htmlmodify_yylval);

       reset_tagdata(&tagdata,tag,is_complex_tag(tag)?htmlmodify_yylval:NULL);

       commentout=0;
       prefix=NULL;

       switch (tag)
	 {
	 case tag__a:
	   if(disable_anchor)
	     prefix="!-- WWWOFFLE (disable-dontget-links) - ";
	   else if(anchor_modify)
	     {
	       if(url_cached==1)
		 {if(anchor_modify_end[0]) {YY_OUTPUT(anchor_modify_end[0]);}}
	       else if(url_cached==2)
		 {if(anchor_modify_end[1]) {YY_OUTPUT(anchor_modify_end[1]);}}
	       else if(url_cached==-1)
		 {if(anchor_modify_end[2]) {YY_OUTPUT(anchor_modify_end[2]);}}
	       url_cached=0;
	     }
	   break;
	 case tag__iframe:
	   if(disable_iframe)
	     prefix="!-- WWWOFFLE (disable-dontget-iframes) - ";
	   break;
	 case tag__body: case tag__html:
	   if(cache_info)
	     {YY_OUTPUT(cache_info); free(cache_info); cache_info=NULL;}
	   if(insertfile)
	     {add_insertfile(insertfile); insertfile=NULL;}
	   break;
	 case tag_blink: case tag__blink:
	   if(disable_blink)
	     prefix="!-- WWWOFFLE (disable-blink) - ";
	   break;
	 case tag_marquee: case tag__marquee:
	   if(disable_marquee)
	     prefix="!-- WWWOFFLE (disable-marquee) - ";
	   break;
	 case tag_script:
	   if(disable_script)
	     prefix="!-- WWWOFFLE (disable-script) - ";
	   else if(disable_dontget_script)
	     disable_output|=DISABLE_SCRIPT;
	   break;
	 case tag__script:
	   if(disable_script)
	     prefix="!-- WWWOFFLE (disable-script) - ";
	   else if(disabled_dontget_script)
	     prefix="!-- WWWOFFLE (disable-dontget-script) - ";
	   break;
	 case tag_noscript: case tag__noscript:
	   if(disable_script)
	     prefix="!-- WWWOFFLE (disable-script) - ";
	   break;
	 case tag_applet: case tag__applet:
	   if(disable_applet)
	     prefix="!-- WWWOFFLE (disable-applet) - ";
	   break;
	 case tag_param:
	   if(tagstack)
	     switch (tagstack->disabled) {
	     case 1: prefix="!-- WWWOFFLE (disable-applet) - ";          break;
	     case 2: prefix="!-- WWWOFFLE (disable-flash) - ";           break;
	     case 3: prefix="!-- WWWOFFLE (disable-dontget-iframes) - "; break;
	     }
	   break;
	 case tag__object: case tag__embed:
	   switch (toptagstack(tagstack,tag)) {
	   case 1: prefix="!-- WWWOFFLE (disable-applet) - ";          break;
	   case 2: prefix="!-- WWWOFFLE (disable-flash) - ";           break;
	   case 3: prefix="!-- WWWOFFLE (disable-dontget-iframes) - "; break;
	   }
	   break;
	 case tag_style:
	   if(disable_style)
	     prefix="!-- WWWOFFLE (disable-style) - ";
	   else if(disable_script)
	     disable_output|=DISABLE_STYLE;
	   break;
	 case tag__style:
	   if(disable_style)
	     prefix="!-- WWWOFFLE (disable-style) - ";
	   else if(disable_style_script)
	     prefix="!-- WWWOFFLE (disable-script) - ";
	   break;
	 case tag_meta:
	   if(disable_meta_refresh_self || disable_meta_refresh || disable_meta_set_cookie)
	     disable_output|=DISABLE_META;
	   break;
	 case tag_link:
	   if(disable_style)
	     disable_output|=DISABLE_LINK;
	   break;
	 case tag_object: case tag_embed:
	   if(disable_applet || disable_flash ||
	      disable_dontget_iframes || replace_html_dontget_images || replace_html_webbug_images)
	     disable_output|=DISABLE_OBJECT;
	   break;
	 case tag_a:
	   if(disable_dontget_anchors)
	     disable_output|=DISABLE_A;
	   break;
	 case tag_iframe:
	   if(disable_dontget_iframes)
	     disable_output|=DISABLE_IFRAME;
	   break;
	 case tag_img:
	   if(replace_html_dontget_images || replace_html_webbug_images)
	     disable_output|=DISABLE_IMG;
	   break;
	 default: ;
	 }

       YY_OUTPUT("<");
       if(prefix)
         {YY_OUTPUT(prefix);}
       YY_OUTPUT(htmlmodify_yylval);
       break;

      case LEX_TAG_END_XHTML:
       tagdata.xhtml=1;

       /*@fallthrough@*/

      case LEX_TAG_END:
       suffix=NULL;

       switch (tag)
	 {
	 case tag__a:
	   if(disable_anchor)
	     {suffix=" --";disable_anchor=0;}
	   break;
	 case tag__iframe:
	   if(disable_iframe)
	     {suffix=" --";disable_iframe=0;}
	   break;
	 case tag_blink: case tag__blink:
	   if(disable_blink)
	     suffix=" --";
	   break;
	 case tag_marquee: case tag__marquee:
	   if(disable_marquee)
	     suffix=" --";
	   break;
	 case tag_script:
	 case tag_noscript: case tag__noscript:
	   if(disable_script)
	     suffix=" --";
	   break;
	 case tag__script:
	   if(disable_script)
	     suffix=" --";
	   else if(disabled_dontget_script)
	     {suffix="--"; disabled_dontget_script=0;}
	   break;
	 case tag_applet:
	   if(disable_applet)
	     suffix=" --";
	   if(!tagdata.xhtml)
	     pushtagstack(&tagstack,tag,disable_applet);
	   break;
	 case tag__applet:
	   if(disable_applet)
	     suffix=" --";
	   poptagstack(&tagstack,tag);
	   break;
	 case tag_param:
	   if(tagstack && tagstack->disabled)
	     suffix=" --";
	   break;
	 case tag__object: case tag__embed:
	   if(poptagstack(&tagstack,tag))
	     suffix=" --";
	   break;
	 case tag_style:
	   if(disable_style)
	     suffix=" --";
	   break;
	 case tag__style:
	   if(disable_style)
	     suffix=" --";
	   else if(disable_style_script)
	     {suffix="--"; disable_style_script=0;}
	   break;
	 default: ;
	 }

       if(suffix)
         {YY_OUTPUT(suffix);}
       if(yychar==LEX_TAG_END_XHTML && !suffix)
         {YY_OUTPUT(" />");}
       else
         {YY_OUTPUT(">");}

       switch (tag)
	 {
	 case tag_meta:
	   if(disable_meta_refresh_self || disable_meta_refresh || disable_meta_set_cookie ||
	      demoronise_ms_chars || fix_mixed_cyrillic)
	     {
	       disable_output&=~DISABLE_META;
	       handle_meta_tag(&tagdata,disable_meta_refresh,disable_meta_refresh_self,replacement_meta_refresh_time,disable_meta_set_cookie);
	     }
	   break;
	 case tag_link:
	   if(disable_style)
	     {
	       disable_output&=~DISABLE_LINK;
	       handle_link_tag(&tagdata);
	     }
	   break;
	 case tag_object: case tag_embed:
	   if(disable_applet || disable_flash ||
	      disable_dontget_iframes || replace_html_dontget_images || replace_html_webbug_images)
	     {
	       int disabled;
	       disable_output&=~DISABLE_OBJECT;
	       disabled= handle_object_tag(&tagdata,replace_html_dontget_images,replacement_html_dontget_image,
					   replace_html_webbug_images,replacement_html_webbug_image,
					   disable_applet,
					   disable_flash,
					   disable_dontget_iframes);
	       if(!tagdata.xhtml)
		 pushtagstack(&tagstack,tag,disabled);
	     }
	   break;
	 case tag_a:
	   if(disable_dontget_anchors)
	     {
	       disable_output&=~DISABLE_A;
	       disable_anchor=handle_a_tag(&tagdata);
	     }

	   if(!disable_anchor && anchor_modify)
	     {
	       if(url_cached==1)
		 {if(anchor_modify_begin[0]) {YY_OUTPUT(anchor_modify_begin[0]);}}
	       else if(url_cached==2)
		 {if(anchor_modify_begin[1]) {YY_OUTPUT(anchor_modify_begin[1]);}}
	       else if(url_cached==-1)
		 {if(anchor_modify_begin[2]) {YY_OUTPUT(anchor_modify_begin[2]);}}
	     }
	   break;
	 case tag_iframe:
	   if(disable_dontget_iframes)
	     {
	       disable_output&=~DISABLE_IFRAME;
	       disable_iframe=handle_iframe_tag(&tagdata);
	     }
	   break;
	 case tag_img:
	   if(replace_html_dontget_images || replace_html_webbug_images)
	     {
	       disable_output&=~DISABLE_IMG;
	       handle_img_tag(&tagdata,replace_html_dontget_images,replacement_html_dontget_image,
			      replace_html_webbug_images,replacement_html_webbug_image);
	     }
	   break;
	 case tag_script:
	   if(disable_script)
	     commentout=1;
	   else if(disable_dontget_script)
	     {
	       disable_output&=~DISABLE_SCRIPT;

	       disabled_dontget_script=handle_script_tag(&tagdata);

	       if(disabled_dontget_script)
		 commentout=1;
	     }
	   break;
	 case tag_style:
	   if(disable_style)
	     commentout=1;
	   else if(disable_script)
	     {
	       disable_output&=~DISABLE_STYLE;

	       disable_style_script=handle_style_script_tag(&tagdata);

	       if(disable_style_script)
		 commentout=1;
	     }
	   break;
	 default: ;
	 }

       tag=-1;
       key=-1;
       break;

      case LEX_ATTR_KEY:
       if(key_string) free(key_string);
       key_string=strdup(htmlmodify_yylval);
       key=lookupattribute(htmlmodify_yylval);
       break;

      case LEX_ATTR_VAL_DQ:
	quote="\""; goto lex_attr_val;
      case LEX_ATTR_VAL_SQ:
	quote="\'"; goto lex_attr_val;
      case LEX_ATTR_VAL:
	quote="";

      lex_attr_val:
       switch (key)
	 {
	 case att_href:
	   if(tag==tag_a) {       /* Links */
	     if(anchor_modify) {
	       if(htmlmodify_yylval) {
		 char *p,oldp=0;

		 for(p=htmlmodify_yylval;*p;p++)
		   if(*p=='#')
		     {
		       oldp=*p;
		       *p=0;
		       break;
		     }

		 if(*htmlmodify_yylval)
		   {
		     char *link=LinkURL(baseUrl,htmlmodify_yylval);
		     URL *Url=SplitURL(link);

		     if(!Url->Protocol)
		       url_cached=0;
		     else if(ExistsWebpageSpoolFile(Url,0) || IsLocalNetHost(Url->host))
		       url_cached=1;
		     else if(ExistsOutgoingSpoolFile(Url))
		       url_cached=2;
		     else
		       url_cached=-1;

		     if(link!=htmlmodify_yylval)
		       free(link);
		     FreeURL(Url);
		   }
		 else
		   url_cached=1;

		 if(oldp)
		   *p=oldp;
	       }
	       else
		 url_cached=0;
	     }
	   }
	   else if(tag==tag_base && htmlmodify_yylval)       /* Base tag */
	     setbase_url(htmlmodify_yylval);
	   break;

	 case att_onblur: case att_onchange: case att_onclick: case att_ondblclick: case att_onfocus:
	 case att_onkeydown: case att_onkeypress: case att_onload: case att_onmousedown:
	 case att_onmousemove: case att_onmouseout: case att_onmouseover: case att_onmouseup:
	 case att_onreset: case att_onselect: case att_onsubmit: case att_onunload:
	   if(disable_script)       /* Script events */
	     {str_prepend("wwwoffledisabled-", &key_string); key=-1;}
	   break;

	 case att_style:
	   if(disable_style)       /* Style references */
	     {str_prepend("wwwoffledisabled-", &key_string); key=-1;}
	   break;
	 default: ;
	 }

       /* Output the attribute and key or not. */

       /* More complicated tags that depend on other attributes are stored and done later. */
       if(is_complex_tag(tag))
	 add_attribute(&tagdata, key,key_string,htmlmodify_yylval,quote);

       yy_output(key_string,prefix!=NULL);
       if(htmlmodify_yylval)
	 {
	  YY_OUTPUT("=");
	  YY_OUTPUT(quote);
	  yy_output(htmlmodify_yylval,prefix!=NULL);
	  YY_OUTPUT(quote);
	 }

       key=-1;
       break;

      default:
       break;
      }

 if(cache_info)
   {YY_OUTPUT(cache_info); free(cache_info); cache_info=NULL;}

 if(insertfile)
   {add_insertfile(insertfile); insertfile=NULL;}

 if(key_string)
    free(key_string);

 /* Delete the tagdata */

 free_tagdata(&tagdata);
 free_tagstack(tagstack);

 /* Free replacement URLs */
 if(replacement_html_dontget_image_copy) free(replacement_html_dontget_image_copy);
 if(replacement_html_webbug_image_copy) free(replacement_html_webbug_image_copy);
}


/* The demoronise_ms_chars idea is taken from the public domain Demoroniser perl script */

/*************************************************/
/* De-moron-ise Text from Microsoft Applications */
/*         by John Walker -- January 1998        */
/*            http://www.fourmilab.ch/           */
/*************************************************/

 /*+ The list of characters to replace. +*/
 static char *demoronise_ms_chars_list[]={/* 0x80 */ "\200",
                                          /* 0x81 */ "\201",
                                          /* 0x82 */ ",",
                                          /* 0x83 */ "<em>f</em>",
                                          /* 0x84 */ ",,",
                                          /* 0x85 */ "...",
                                          /* 0x86 */ "\206",
                                          /* 0x87 */ "\207",
                                          /* 0x88 */ "^",
                                          /* 0x89 */ " °/°°",
                                          /* 0x8A */ "\212",
                                          /* 0x8B */ "<",
                                          /* 0x8C */ "Oe",
                                          /* 0x8D */ "\215",
                                          /* 0x8E */ "\216",
                                          /* 0x8F */ "\217",
                                          /* 0x90 */ "\220",
                                          /* 0x91 */ "`",
                                          /* 0x92 */ "'",
                                          /* 0x93 */ "\"",
                                          /* 0x94 */ "\"",
                                          /* 0x95 */ "*",
                                          /* 0x96 */ "-",
                                          /* 0x97 */ "--",
                                          /* 0x98 */ "<sup>~</sup>",
                                          /* 0x99 */ "<sup>TM</sup>",
                                          /* 0x9A */ "\232",
                                          /* 0x9B */ ">",
                                          /* 0x9C */ "oe",
                                          /* 0x9D */ "\235",
                                          /* 0x9E */ "\236",
                                          /* 0x9F */ "\237"};

/* This "fix-mixed-cyrillic" code is written by Ilya Dogolazky
 * e-mail: ilyad at math dot uni-bonn dot de
 *
 * There are weird Russian websites [ www.novayagazeta.ru for example ] using
 * koi8-r encoding for the letters of Russian alphabeth, and cp-1251 encoding
 * for the punctuations ("<<", ">>", "No", "\bullet" etc).
 *
 * We take all characters between 0x80 and 0xBF (there are three exceptions
 * below) and recode they from windows-1251 to Unicode (in "&#xCODE;" HTML
 * notation)
 *
 * The idea is similar to that of "demoronise-ms-chars", but these options are
 * clearly mutually exclusive
 */

 /* This table is generated by the following Perl statement:
  * print ord(decode("windows-1251",chr($_)) for (0x80..0xBF) ;
  * See 3 exceptions below...
  */                                         

static unsigned int fix_mixed_cyrillic_list[]={0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,
                                               0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
                                               0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,
                                               0x0   ,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
                                               0x00A0,0x040E,0x045E,0x0   ,0x00A4,0x0490,0x00A6,0x00A7,
                                               0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
                                               0x00B0,0x00B1,0x0406,0x0   ,0x0491,0x00B5,0x00B6,0x00B7,
                                               0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457};


/*++++++++++++++++++++++++++++++++++++++
  Handle the characters on the input with the high-bit set.

  unsigned char ch The character.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_high_bit(unsigned char ch)
{
 if(demoronise_ms_chars && ch<=0x9F)
   {
    YY_OUTPUT("<!-- WWWOFFLE (demoronise-ms-chars) - '");
    YY_OUTPUT(htmlmodify_yytext);
    YY_OUTPUT("' -->");
    YY_OUTPUT(demoronise_ms_chars_list[ch-0x80]);
   }
 else if(fix_mixed_cyrillic) /* && ch<=0xBF */
   {
    unsigned int unicode_value = fix_mixed_cyrillic_list[ch-0x80] ;

    if(unicode_value==0x0)
      {
       /* We have 3 such values for ch: the first and the second are koi8-r
          codes of \"e and \"E (ch==163 || ch==179), and the third is
          ch==152. This value is very mysterious: there is no Unicode-equivalent
          for character with code 152 of windows-1251 page (perl's decode fails
          for this code, ask Bill Gates for details :-) */
       YY_OUTPUT(htmlmodify_yytext);
      }
    else
      {
       char html_buffer[9];
       sprintf(html_buffer,"&#x%04X;",unicode_value);

       YY_OUTPUT("<!-- WWWOFFLE (fix-mixed-cyrillic) - '");
       YY_OUTPUT(htmlmodify_yytext);
       YY_OUTPUT("' -->");
       YY_OUTPUT(html_buffer);
      }
   }
 else
    YY_OUTPUT(htmlmodify_yytext);
}


#define YY_SKIP_YYWRAP 1 /* Remove error with prototype of ..._yywrap */
#ifndef htmlmodify_yywrap
/*+ Needed in lex but does nothing. +*/
#define htmlmodify_yywrap() 1
#endif

/*+ Reset the current string. +*/
#define reset_string \
{if(string) *string=0; stringlen=0;}

/*+ append information to the current string. +*/
#define append_string(xx) \
{size_t _str_len=strlen(xx),_new_len=stringlen+_str_len; \
 if(_new_len>=stringallocsize) string=(char*)realloc((void*)string,stringallocsize=_new_len+1); \
 memcpy(string+stringlen,xx,_str_len+1); \
 stringlen=_new_len;}

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
{if((result=wwwoffles_read_data(buf,max_size))==-1) result=0;}

%}

%%
 /* Must use static variables since the parser returns often. */
 static char *string=NULL;
 static size_t stringallocsize=0,stringlen=0;
 static int after_tag=INITIAL;

 /* Handle comments and other tags */

[\x80-\xBF]                 { handle_high_bit(*(unsigned char*)htmlmodify_yytext); }
[^<\x80-\xBF]+              { YY_OUTPUT(htmlmodify_yytext); /* htmlmodify_yylval=htmlmodify_yytext; return(LEX_PLAINTEXT); */ }

"<!DOCTYPE"                 { YY_OUTPUT(htmlmodify_yytext); BEGIN(DOCTYPE); reset_string; }
"<!--"                      { YY_OUTPUT(htmlmodify_yytext); BEGIN(COMMENT); reset_string; }
"<!"{W}*"-"*                { YY_OUTPUT(htmlmodify_yytext); BEGIN(COMMENT_BAD); reset_string; }
"<"{W}*                     { BEGIN(TAG_START); reset_string; append_string(htmlmodify_yytext); }

 /* Doctype (DTD) */

<DOCTYPE>">"                { YY_OUTPUT(htmlmodify_yytext); BEGIN(INITIAL); /* htmlmodify_yylval=string; return(LEX_DOCTYPE); */ }
<DOCTYPE>[^>]+              { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }

 /* Comments - COMMENT_BAD is not a legal comment format (except <!>) but people use it as one.
               COMMENT is not strictly correct, but works better than the real thing. */

<COMMENT>"--"{W}*">"        { YY_OUTPUT(htmlmodify_yytext); BEGIN(INITIAL); /* htmlmodify_yylval=string; return(LEX_COMMENT); */ }
<COMMENT>"-"                |
<COMMENT>[^-]+              { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }

<COMMENT_BAD>">"            { YY_OUTPUT(htmlmodify_yytext); BEGIN(INITIAL); /* htmlmodify_yylval=string; return(LEX_COMMENT); */ }
<COMMENT_BAD>[^>]+          { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }

 /* Tags */

<TAG_START>"script"/{W}     { BEGIN(TAG); after_tag=SCRIPT_START; htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"script"/">"     { BEGIN(TAG); after_tag=SCRIPT_START; htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"style"/{W}      { BEGIN(TAG); after_tag=STYLE_START;  htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"style"/">"      { BEGIN(TAG); after_tag=STYLE_START;  htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/" "    { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/\t     { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/\n     { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/\r     { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/">"    { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>(.|\n)           { BEGIN(INITIAL); YY_OUTPUT(string); YY_OUTPUT(htmlmodify_yytext); }

<TAG>{W}+                   { YY_OUTPUT(htmlmodify_yytext); }
<TAG>"/>"                   { BEGIN(after_tag);                              htmlmodify_yylval=""; return(LEX_TAG_END_XHTML); }
<TAG>">"                    { BEGIN(after_tag);                              htmlmodify_yylval=""; return(LEX_TAG_END); }
<TAG>"<"                    { BEGIN(after_tag); unput(htmlmodify_yytext[0]); htmlmodify_yylval=""; return(LEX_TAG_END); }
<TAG>{key}                  { BEGIN(TAG_ATTR_KEY); htmlmodify_yylval=htmlmodify_yytext; return(LEX_ATTR_KEY); }
<TAG>(.|\n)                 { YY_OUTPUT(htmlmodify_yytext); }

<TAG_ATTR_KEY>{W}*=         { BEGIN(TAG_ATTR_VAL); }
<TAG_ATTR_KEY>(.|\n)        { BEGIN(TAG); unput(htmlmodify_yytext[0]); htmlmodify_yylval=NULL; return(LEX_ATTR_VAL); }

<TAG_ATTR_VAL>\"            { BEGIN(DQUOTED); reset_string; }
<TAG_ATTR_VAL>\'            { BEGIN(SQUOTED); reset_string; }
<TAG_ATTR_VAL>{W}+          { }
<TAG_ATTR_VAL>{val}         { BEGIN(TAG);                              htmlmodify_yylval=htmlmodify_yytext; return(LEX_ATTR_VAL); }
<TAG_ATTR_VAL>(.|\n)        { BEGIN(TAG); unput(htmlmodify_yytext[0]); htmlmodify_yylval="";                return(LEX_ATTR_VAL); }

 /* Quoted strings */

<DQUOTED>\"                 { BEGIN(TAG); htmlmodify_yylval=string; return(LEX_ATTR_VAL_DQ); }
<DQUOTED>[^\"]+             { append_string(htmlmodify_yytext); }

<SQUOTED>\'                 { BEGIN(TAG); htmlmodify_yylval=string; return(LEX_ATTR_VAL_SQ); }
<SQUOTED>[^\']+             { append_string(htmlmodify_yytext); }

 /* Scripts */

<SCRIPT_START>{W}+          { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_START>"<"/"/script" { BEGIN(TAG_START); }
<SCRIPT_START>"<!--"        { if(commentout>0) commentout=-1; YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_START>(.|\n)        { if(commentout>0) yy_output("<!--\n",0);
                              unput(htmlmodify_yytext[0]); BEGIN(SCRIPT); }

<SCRIPT>\\\"                |
<SCRIPT>\\\'                |
<SCRIPT>\\                  { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT>\"                  { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_DQUOTED); }
<SCRIPT>\'                  { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_SQUOTED); }
<SCRIPT>"/*"                { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_COMMENT_C); }
<SCRIPT>"//"                { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_COMMENT_CPP); }
<SCRIPT>"/"                 { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT>"<"/"/script"       { if(commentout>0) {commentout=0; YY_OUTPUT("//-->\n");}
                              BEGIN(TAG_START); }
<SCRIPT>"<"                 |
<SCRIPT>[^\\\'\"</]+        { YY_OUTPUT(htmlmodify_yytext); }

 /* Comments in scripts, C or C++ style */

<SCRIPT_COMMENT_C>"*"+"/"   { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_COMMENT_C>"*"+      |
<SCRIPT_COMMENT_C>[^*]+     { YY_OUTPUT(htmlmodify_yytext); }

<SCRIPT_COMMENT_CPP>[\r\n]  { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_COMMENT_CPP>"<"/"/script" { if(commentout>0) {commentout=0; YY_OUTPUT("-->\n");}
                              BEGIN(TAG_START); }
<SCRIPT_COMMENT_CPP>"<"     { YY_OUTPUT(htmlmodify_yytext); }
 /* The following is not strictly correct, see previous remarks about COMMENT. */
<SCRIPT_COMMENT_CPP>"-"+"-"[ \t]*">" { YY_OUTPUT(htmlmodify_yytext);
                              if(commentout<0) {commentout=1; BEGIN(SCRIPT_START);}}
<SCRIPT_COMMENT_CPP>"-"+    |
<SCRIPT_COMMENT_CPP>[^-<\r\n]+ { YY_OUTPUT(htmlmodify_yytext); }

 /* Quoted strings in scripts - assume they follow usual rules */

<SCRIPT_DQUOTED>\\\"        |
<SCRIPT_DQUOTED>\\\\        |
<SCRIPT_DQUOTED>\\          { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_DQUOTED>\"          { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_DQUOTED>[^\\\"]+    { YY_OUTPUT(htmlmodify_yytext); }

<SCRIPT_SQUOTED>\\\'        |
<SCRIPT_SQUOTED>\\\\        |
<SCRIPT_SQUOTED>\\          { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_SQUOTED>\'          { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_SQUOTED>[^\\\']+    { YY_OUTPUT(htmlmodify_yytext); }

 /* Styles */

<STYLE_START>{W}+           { YY_OUTPUT(htmlmodify_yytext); }
<STYLE_START>"<"/"/style"   { BEGIN(TAG_START); }
<STYLE_START>"<!--"         { if(commentout>0) commentout=-1; YY_OUTPUT(htmlmodify_yytext); BEGIN(STYLE); }
<STYLE_START>(.|\n)         { if(commentout>0) yy_output("<!--\n",0);
                              unput(htmlmodify_yytext[0]); BEGIN(STYLE); }

<STYLE>"<"/"/style"         { if(commentout>0) {commentout=0; YY_OUTPUT("\n-->\n");}
                              BEGIN(TAG_START); }
<STYLE>"<"                  { YY_OUTPUT(htmlmodify_yytext); }
<STYLE>"-"+"-"[ \t]*">"     { YY_OUTPUT(htmlmodify_yytext);
                              if(commentout<0) {commentout=1; BEGIN(STYLE_START);}}
<STYLE>"-"+                 |
<STYLE>[^-<]+               { YY_OUTPUT(htmlmodify_yytext); }

 /* End of file */

<<EOF>>                     { free(string); string=NULL; stringallocsize=stringlen=0;
                              after_tag=INITIAL; BEGIN(INITIAL); htmlmodify_yylval=NULL; return(0); }

%%
