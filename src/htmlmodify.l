W               [ \t\r\n\f]

nonascii        [\200-\377]
ascii           [ -~]
alphanum        [a-z0-9]
punct           [][!\"#$%&\'()*+,./:;<=>?@\\^_`{|}~-]
safepunct       [][!#$%&()*+,./:;=?@\\^_`{|}~-]

tag             ({alphanum}|:)+
tagdel          [ \t\r\n\f/>]
key             ({alphanum}|[-:])+
val             ({alphanum}|{nonascii}|{safepunct})+

%x DOCTYPE
%x COMMENT COMMENT_BAD
%x TAG_START TAG TAG_ATTR_KEY TAG_ATTR_VAL
%x DQUOTED SQUOTED
%x SCRIPT_START SCRIPT
%x STYLE_START STYLE

%{
/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/htmlmodify.l 1.96 2007/07/21 13:23:54 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9d.
  Parse the HTML and modify the source.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06,07 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007 Paul A. Rombouts
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
#include "configpriv.h"
#include "config.h"
#include "proto.h"
#include "errors.h"
#include "document.h"


/* Parser outputs */

#define LEX_PLAINTEXT      1
#define LEX_COMMENT        2
#define LEX_DOCTYPE        3
#define LEX_SCRIPTTEXT     4

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
 tag_head        /* "head"      */ ,
 tag__head       /* "/head"     */ ,
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

static const char* const tags[]=
{
  "a"          ,
  "applet"     ,
  "base"       ,
  "blink"      ,
  "body"       ,
  "embed"      ,
  "head"       ,
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
static const char complex_tag[]=
{
  1  /* "a"        */ ,
  0  /* "applet"   */ ,
  0  /* "base"     */ ,
  0  /* "blink"    */ ,
  0  /* "body"     */ ,
  1  /* "embed"    */ ,
  0  /* "head"     */ ,
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
 att_background    /* "background"  */ ,
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
 att_onerror       /* "onerror"     */ ,
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

static const char* const attributes[]=
{
  "background"  ,
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
  "onerror"     ,
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

typedef enum _disabled_t
{
  not_disabled,
  disabled_dontget_link,
  disabled_iframe,
  disabled_dontget_iframe,
  disabled_dontget_image,
  disabled_webbug_image,
  disabled_applet,
  disabled_flash,
  disabled_script,
  disabled_script_after_body,
  disabled_inner_script,
  disabled_following_script,
  disabled_all_following_scripts,
  disabled_dontget_script,
  disabled_matching_script,
  disabled_style,
  disabled_matching_tag
}
disabled_t;

static const char* const tagprefix[]=
{
  NULL,
  "!-- WWWOFFLE (disable-dontget-links) - ",
  "!-- WWWOFFLE (disable-iframes) - ",
  "!-- WWWOFFLE (disable-dontget-iframes) - ",
  "!-- WWWOFFLE (disable-dontget-images) - ",
  "!-- WWWOFFLE (disable-webbug-images) - ",
  "!-- WWWOFFLE (disable-applet) - ",
  "!-- WWWOFFLE (disable-flash) - ",
  "!-- WWWOFFLE (disable-script) - ",
  "!-- WWWOFFLE (disable-script-after-body) - ",
  "!-- WWWOFFLE (disable-inner-script) - ",
  "!-- WWWOFFLE (disable-following-script) - ",
  "!-- WWWOFFLE (disable-all-following-scripts) - ",
  "!-- WWWOFFLE (disable-dontget-script) - ",
  "!-- WWWOFFLE (disable-matching-script) - ",
  "!-- WWWOFFLE (disable-style) - ",
  "!-- WWWOFFLE (disable-matching-tag) - "
};

/*+ A structure to hold a tag and its attributes. +*/

typedef struct _Tag
{
 HTMLTags type;                 /*+ The type of the tag. +*/

 char *tag;                     /*+ The Tag itself. +*/

 unsigned short xhtml;          /*+ A flag that is set for an XHTML closing tag '< ... />' +*/
 unsigned short disable;        /*+ A flag that is set when this tag should be disabled. +*/

 int nattr;                     /*+ The number of attributes. +*/
 int nattr_malloc;              /*+ The number of attributes that space is malloced for. +*/

 int *attr_type;                /*+ The list of attribute types. +*/
 char **attr_key;               /*+ The list of attribute keys. +*/
 char **attr_val;               /*+ The list of attribute values. +*/
 char *attr_quote;              /*+ The list of attribute quotes. +*/
 unsigned char *attr_disable;   /*+ The list of attribute disable flags. +*/
}
Tag;

/*+ A stack for holding tags of possibly nested objects and their disabled status. +*/
typedef struct _TagStackNode
{
  struct _TagStackNode *next;
  int type;
  int val;
}
TagStackNode;

typedef struct _TagStackNode *TagStack;

/* struct used for lists of string-segment sequences. */
typedef struct _PattNode {
  struct _PattNode* next;
  unsigned len;
  char patt[0];   /* sequence of string segments, each segment terminated by null byte. */
}
PattNode;

/* Structs for storing tag-attribute patterns */

typedef struct _AttrPattNode {
  struct _AttrPattNode* next;
  char* attrname;
  PattNode* valpatt;
  short disableattr;
}
AttrPattNode;

typedef struct _AttrPattNode *AttrPattList;

typedef struct _TagAttrPattNode {
  struct _TagAttrPattNode *next;
  char *tagname;
  AttrPattList attrpatts;
  PattNode*  txtpatt;
  const char *stylerepl;
  short disable;
}
TagAttrPattNode;

typedef struct _CommentPattNode {
  struct _CommentPattNode *next;
  PattNode*  cmpatt;
  short disable;
}
CommentPattNode;

typedef struct _TagAttrPattList {
  TagAttrPattNode **taparr;  /* Points to array of length ntag */
  CommentPattNode *cmpl;
  char *disarr;              /* Points to array of length ntag */
  int ntag;
}
TagAttrPattList;


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
#define DISABLE_IFRAME    0x10
#define DISABLE_IMG       0x20
#define DISABLE_SCRIPT    0x40
#define DISABLE_STYLE     0x80

#define DISABLE_TAG      0x100
#define DISABLE_TEXT     0x200

/* Bit masks for the result of tag-attribute matching. */
#define TAGMATCH         2
#define TAGMAYBEDISABLE  1

#define DISABLEMATCHINGTAG          1
#define DISABLEINNERSCRIPT          2
#define DISABLEFOLLOWINGSCRIPT      4
#define DISABLEALLFOLLOWINGSCRIPTS  8
#define RESETFOLLOWINGSCRIPT     0x10
#define RESETALLFOLLOWINGSCRIPTS 0x20
#define DISABLEMASK (DISABLEMATCHINGTAG|DISABLEINNERSCRIPT|DISABLEFOLLOWINGSCRIPT|DISABLEALLFOLLOWINGSCRIPTS)

/* Local functions */

static void modify_html(URL *Url);

static /*@null@*/ char *htmlmodify_yylval=NULL;
extern int htmlmodify_yylex(void);

static /*@null@*/ char *extract_charset(const char *content_type);

static disabled_t handle_a_tag(const Tag *tag,int disable_dontget_anchors,int disable_script);
static disabled_t handle_iframe_tag(const Tag *tag,int disable_dontget_iframes,int disable_script);
static disabled_t handle_script_tag(const Tag *tag, disabled_t is_disabled_script);
/* static disabled_t handle_img_tag(Tag *tag,int replace_dontget,const char *dontget_replacement,
			            int replace_webbug,const char *webbug_replacement,
                                    int disable_script); */
static disabled_t handle_object_tag(Tag *tag,int replace_dontget,const char *dontget_replacement,
				    int replace_webbug,const char *webbug_replacement,
				    int disable_applet,
				    int disable_flash,
				    int disable_dontget_iframes,
				    int disable_script);
static disabled_t output_img_or_object_tag(Tag *tag,int src_att,
					   int replace_dontget,const char *dontget_replacement,
					   int replace_webbug,const char *webbug_replacement,
					   int disable_script);
static void handle_link_tag(const Tag *tag,int disable_style,int disable_script);
static disabled_t handle_style_script_tag(const Tag *tag);
static void handle_meta_tag(const Tag *tag,int disable_meta_refresh,int disable_meta_refresh_self,
			    int replacement_meta_refresh_time,int disable_meta_set_cookie);
static void output_tag(const Tag *tag,const char *prefix);

static void handle_high_bit(unsigned char ch);


/*+ The add-cache-info optional footer. +*/
static /*@null@*/ /*@observer@*/ char *cache_info=NULL;

/*+ The base URL of this page. +*/
static /*@null@*/ URL *baseUrl=NULL;

/*+ Set this to disable the output. +*/
static int disable_output=DISABLE_NONE;
static short int commentout=0, parsecomment=0;
static short int scriptcommopen=0, scriptcommclose=0;
static int scripttextstart=-1, scripttextend=-1;

#if 0
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
#endif

inline static void str_grow_cpy(char **dst, unsigned *allocsize, const char *src)
{
  unsigned size= strlen(src)+1;
  if(size>*allocsize)
    *dst= (char*)realloc(*dst, *allocsize=size);
  memcpy(*dst,src,size);
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
  tagdata->disable=0;
  tagdata->nattr=0;
  tagdata->nattr_malloc=16;
  tagdata->attr_type=(int*)malloc(tagdata->nattr_malloc*sizeof(int));
  tagdata->attr_key=(char**)malloc(tagdata->nattr_malloc*sizeof(char*));
  tagdata->attr_val=(char**)malloc(tagdata->nattr_malloc*sizeof(char*));
  tagdata->attr_quote=(char*)malloc(tagdata->nattr_malloc);
  tagdata->attr_disable=(unsigned char*)malloc(tagdata->nattr_malloc);
}

inline static void reset_tagdata(Tag *tagdata, HTMLTags type,const char *tagstr)
{
  int i;

  tagdata->type=type;
  if(tagdata->tag) free(tagdata->tag);
  tagdata->tag= (tagstr?strdup(tagstr):NULL);
  tagdata->xhtml=0;
  tagdata->disable=0;
  for(i=0; i<tagdata->nattr; ++i)
    {
      if(tagdata->attr_key[i]) free(tagdata->attr_key[i]);
      if(tagdata->attr_val[i]) free(tagdata->attr_val[i]);
    }
  tagdata->nattr=0;
}

static void add_attribute(Tag *tag, int type, const char *key, const char *val, char quote, unsigned char disable)
{
  if(tag->nattr==tag->nattr_malloc)
    {
      ++tag->nattr_malloc;
      tag->attr_type=(int*)realloc((void*)tag->attr_type,(tag->nattr_malloc)*sizeof(int));
      tag->attr_key=(char**)realloc((void*)tag->attr_key,(tag->nattr_malloc)*sizeof(char*));
      tag->attr_val=(char**)realloc((void*)tag->attr_val,(tag->nattr_malloc)*sizeof(char*));
      tag->attr_quote=(char*)realloc((void*)tag->attr_quote,(tag->nattr_malloc));
      tag->attr_disable=(unsigned char*)realloc((void*)tag->attr_disable,(tag->nattr_malloc));
    }

  tag->attr_type [tag->nattr]=type;
  tag->attr_key  [tag->nattr]=strdup(key);
  tag->attr_val  [tag->nattr]=(val?strdup(val):NULL);
  tag->attr_quote[tag->nattr]=quote;
  tag->attr_disable[tag->nattr]=disable;

  ++tag->nattr;
}

inline static void free_tagdata(Tag *tagdata)
{
  int i;

  if(tagdata->tag) free(tagdata->tag);

  for(i=0; i<tagdata->nattr; ++i)
    {
      if(tagdata->attr_key[i]) free(tagdata->attr_key[i]);
      if(tagdata->attr_val[i]) free(tagdata->attr_val[i]);
    }

  free(tagdata->attr_type);
  free(tagdata->attr_key);
  free(tagdata->attr_val);
  free(tagdata->attr_quote);
  free(tagdata->attr_disable);
}

/* Some functions for handling tag stacks. */

/* Push a tag and its value on a stack. */
static void pushtagstack(TagStack *s, int t, int val)
{
  TagStack new=malloc(sizeof(TagStackNode));
  new->next= *s;
  new->type= t;
  new->val= val;

  *s= new;
}

/* Remove the first stack node of the matching tag type and return its value. */
static int poptagstack(TagStack *s, int tag)
{
  TagStack p, *q;

  if(tag<0)
    return 0;

  q= s;
  while((p= *q)) {
    if(p->type==tag) {
      int retval= p->val;
      *q= p->next;
      free(p);
      return retval;
    }
    q= &p->next;
  }

  return 0;
}

inline static int popendtagstack(TagStack *s, int endtag)
{
  return (endtag>=0 && endtag&1)? poptagstack(s,endtag-1): 0;
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


static void free_pattlist(PattNode *p);

#define keywordmatch(str,len,key) (len==strlitlen(key) && !strncasecmp(str,key,strlitlen(key)))

struct tagattrpattvalues {
  int parsetype;              /* Type of parsing result:
				 0 error, 1 tag-attr pattern, 2 comment pattern. */
  union {
    TagAttrPattNode *tap;     /* Ordinary HTML element. */
    CommentPattNode *comp;    /* A comment (treated as "pseudo" tag). */
  };
};

/* Create a TagAttrPattNode or CommentPattNode data structure from parsing a string.
   parse_tag_attr_pattern returns the result in a struct tagattrpattvalues.
   If parsing fails, the parsetype field of this struct is set to zero.
   The pointer argument is updated to point to the position in the
   string just after the last char that was "consumed".
 */
static struct tagattrpattvalues parse_tag_attr_pattern(const char **str)
{
  struct tagattrpattvalues res = {0};
  const char *s= *str, *p, *cbr, *cl, *cm;

  cm= strunescapechr(s,',');
  p = strunescapechr2(s,cm,'[');

  if(s<p) {
    size_t len=p-s;
    if(!keywordmatch(s,len,"comment")) {
      /* Try to parse patterns for ordinary HTML element. */
      TagAttrPattNode* tap =  (TagAttrPattNode*)malloc(sizeof(TagAttrPattNode));
      AttrPattNode **last;
      tap->next = NULL;
      tap->tagname = strunescapedup2(s,p);
      tap->attrpatts = NULL;
      tap->txtpatt = NULL;
      tap->stylerepl = NULL;
      tap->disable=0;
      last = &tap->attrpatts;
      if(*p=='[') {
	++p; /* Skip '[' */
	cbr=strunescapechr(p,']');
	for(;;) {
	  short disableattr=0;
	  while(*p && isspace(*p)) ++p;  /* Skip blanks. */
	  if(*p=='-') {
	    disableattr=1;
	    ++p;
	  }
	  cm=strunescapechr2(p,cbr,',');
	  cl=strunescapechr2(p,cm,':');
	  if(cl>p) {
	    PattNode **lpatt;
	    len= cl-p;
	    if(!keywordmatch(p,len,"text")) {
	      AttrPattNode *new = (AttrPattNode*)malloc(sizeof(AttrPattNode));
	      new->next=NULL;
	      new->attrname=strunescapedup2(p,cl);
	      new->valpatt= NULL;
	      new->disableattr= disableattr;
	      *last = new;
	      last = &new->next;
	      lpatt = &new->valpatt;
	    }
	    else {
	      /* text "pseudo" attribute */
	      if(tap->txtpatt) {
		/* A new text pattern clobbers any possible previous one. */
		free_pattlist(tap->txtpatt);
		tap->txtpatt= NULL;
	      }
	      lpatt = &tap->txtpatt;
	    }
	    p=cl+1;
	    while(p<=cm) {
	      PattNode *newpatt;
	      const char *bar; char *q;
	      bar= strunescapechr2(p,cm,'|');
	      len= strunescapelen2(p,bar);
	      newpatt = (PattNode*)malloc(sizeof(PattNode)+len+1);
	      newpatt->next=NULL;
	      newpatt->len=len;
	      /* Copy the pattern, replacing '*'s by null bytes. */
	      q= newpatt->patt;
	      do {
		const char *star=strunescapechr2(p,bar,'*');
		q= strunescapecpy2(q,p,star)+1;  /* Move q past the null byte. */
		p=star+1;
	      } while(p<=bar);
	      *lpatt = newpatt;
	      lpatt = &newpatt->next;
	    }
	  }
	  if(cm>=cbr) break;
	  p=cm+1; /* Skip past ',' */
	}

	p=cbr;
	if(*p) ++p; /* Skip ']' */
      }
      res.parsetype=1;
      res.tap=tap;
    }
    else {
      /* Parse a pattern for a comment. */
      CommentPattNode *comp =  (CommentPattNode*)malloc(sizeof(CommentPattNode));
      PattNode **lpatt;
      comp->next = NULL;
      comp->cmpatt = NULL;
      comp->disable=0;
      lpatt = &comp->cmpatt;
      if(*p=='[') {
	++p; /* Skip '[' */
	cbr=strunescapechr(p,']');
	do {
	  PattNode *newpatt;
	  const char *bar; char *q;
	  bar= strunescapechr2(p,cbr,'|');
	  len= strunescapelen2(p,bar);
	  newpatt = (PattNode*)malloc(sizeof(PattNode)+len+1);
	  newpatt->next=NULL;
	  newpatt->len=len;
	  /* Copy the pattern, replacing '*'s by null bytes. */
	  q= newpatt->patt;
	  do {
	    const char *star=strunescapechr2(p,bar,'*');
	    q= strunescapecpy2(q,p,star)+1;  /* Move q past the null byte. */
	    p=star+1;
	  } while(p<=bar);
	  *lpatt = newpatt;
	  lpatt = &newpatt->next;
	} while(p<=cbr);

	p=cbr;
	if(*p) ++p; /* Skip ']' */
      }
      res.parsetype=2;
      res.comp=comp;
    }
  }

  *str=p;
  return res;
}


struct optionvalues {
  const char *stylerepl;
  short disable;
};

/* Parse of string containing modify options and return the result in a struct optionvalues. */
static struct optionvalues parse_tag_modify_options(const char *str)
{
  struct optionvalues res= {NULL,0};
  short int matchedoption=0;
  const char *p= str;

  for(;;) {
    const char *s;
    size_t len;
    while(*p && isspace(*p)) ++p; /* Skip blanks. */
    if(!*p) break;
    s=p;
    while(*p && (isalnum(*p) || *p=='-' || *p=='_')) ++p;
    len = p-s;
    while(*p && isspace(*p)) ++p; /* Skip blanks. */
    if(keywordmatch(s,len,"display-none"))
      res.stylerepl = "display:none !important";
    else if(keywordmatch(s,len,"visibility-hidden"))
      res.stylerepl = "visibility:hidden !important";
    else if(keywordmatch(s,len,"no-style"))
      res.stylerepl = "";
    else if(keywordmatch(s,len,"disable-tag"))
      res.disable |= DISABLEMATCHINGTAG;
    else if(keywordmatch(s,len,"disable-script"))
      res.disable |= DISABLEINNERSCRIPT;
    else if(keywordmatch(s,len,"disable-following-script"))
      res.disable |= DISABLEFOLLOWINGSCRIPT;
    else if(keywordmatch(s,len,"disable-all-following-scripts"))
      res.disable |= DISABLEALLFOLLOWINGSCRIPTS;
    else if(keywordmatch(s,len,"reset-following-script"))
      res.disable |= RESETFOLLOWINGSCRIPT;
    else if(keywordmatch(s,len,"reset-all-following-scripts"))
      res.disable |= RESETALLFOLLOWINGSCRIPTS;
    else if(keywordmatch(s,len,"pass"))
      /* No op */;
    else if(keywordmatch(s,len,"style") && *p=='=') {
      while(*++p && isspace(*p));
      res.stylerepl= p;
      break;
    }
    else if(!matchedoption) {
      /* If no option keyword matches, take the entire string as style replacement. */
      res.stylerepl= s;
      break;
    }
    /* else; */ /* Unknown option, ignore. */

    matchedoption=1;
    if(*p!=',') break;
    ++p;
  }

  return res;
}

/*
  Sort a list containing tag names using a merge sort algorithm.
*/
static void sort_tag_attr_patt_list(TagAttrPattNode **l)
{
  if(*l) {
    unsigned int m;

    for(m=1;; m *= 2) {
      unsigned int nmerge=0,i,j;
      TagAttrPattNode *p,*q=*l,**s= l, **t;

      do {
	++nmerge;
	p=q;
	i=m;
	do {
	  t= &q->next;
	  q= *t;
	} while(--i && q);

	if(!q) break;
      
	i=j=m;
	for(;;) {
	  if(strcasecmp(p->tagname,q->tagname) <= 0) {
	    *s= p;
	    s= &p->next;
	    p= *s;
	    --i;
	    if(!i) {
	      *s= q;
	      do {s= &q->next; q= *s;} while(--j && q);
	      break;
	    }
	  }
	  else { /* cmp(p,q) > 0 */
	    *s= q;
	    s= &q->next;
	    q= *s;
	    --j;
	    if(!j || !q) {
	      *s= p;
	      *t= q;
	      s= t;
	      break;
	    }
	  }
	}
      } while(q);

      if(nmerge<=1) break;
    }
  }
}

static TagAttrPattList make_tag_attr_patt_list(ConfigItem confitem,const URL *Url)
{
  TagAttrPattList res= {NULL,NULL,NULL,0};
  TagAttrPattNode *tapl= NULL;
  TagAttrPattNode **lasttap = &tapl;
  CommentPattNode **lastcomp= &res.cmpl;

  if(confitem) {
    int i;
    for(i=0;i<confitem->nentries;++i)
      if(!confitem->url[i] || MatchUrlSpecification(confitem->url[i],Url)) {
	struct optionvalues optval= parse_tag_modify_options(confitem->val[i].string);
	const char *s= confitem->key[i].string;
	/* Loop while there are tag-attribute patterns to parse. */
	for(;;) {
	  struct tagattrpattvalues pattval;
	  while(*s && isspace(*s)) ++s;  /* Skip blanks. */
	  if(!*s)
	    break;
	  pattval = parse_tag_attr_pattern(&s);
	  if(!pattval.parsetype)
	    break;
	  else if(pattval.parsetype==1) {
	    pattval.tap->stylerepl= optval.stylerepl;
	    pattval.tap->disable  = optval.disable;
	    *lasttap=pattval.tap;
	    lasttap= &pattval.tap->next;
	  }
	  else { /* comment */
	    pattval.comp->disable  = optval.disable;
	    *lastcomp=pattval.comp;
	    lastcomp= &pattval.comp->next;
	  }	    
	  while(*s && isspace(*s)) ++s;  /* Skip blanks. */
	  if(*s != ',')
	    break;
	  ++s;  /* Skip ',' */
	}
      }
  }

  if(tapl) {
    TagAttrPattNode *p,*q;
    int i;
    sort_tag_attr_patt_list(&tapl);

    /* Count number of unique tags. */
    res.ntag=1;
    for(p=tapl,q=p->next; q; q=q->next) {
      if(strcasecmp(p->tagname,q->tagname))
	++res.ntag;
      p=q;
    }

    /* Create tag lookup array, splitting the tapl list into segments, one for
       each unique tag name. */
    res.taparr = (TagAttrPattNode **)malloc(sizeof(TagAttrPattNode *)*res.ntag);
    res.disarr = (char*)malloc(res.ntag);
    res.taparr[0]= tapl;
    res.disarr[0]=0; if(tapl->disable&DISABLEMASK) res.disarr[0]=TAGMAYBEDISABLE;
    for(i=0,p=tapl,q=p->next; q; q=q->next) {
      if(strcasecmp(p->tagname,q->tagname)) {
	p->next=NULL;  /* Close off the list segment for the previous tag. */
	++i;
	res.taparr[i]=q;
	res.disarr[i]=0;
      }
      if(q->disable&DISABLEMASK) res.disarr[i]=TAGMAYBEDISABLE;
      p=q;
    }
  }

  return res;
}

struct tagmatchval {
  unsigned flags;
  int idx;
};

static struct tagmatchval tag_match_tag_attr_patt_list(const char* tag,TagAttrPattList *l)
{
  struct tagmatchval res= {0,-1};
  TagAttrPattNode **taparr= l->taparr;

  if(taparr) {
    unsigned int st=TAGMATCH;
    if(*tag == '/') {
      st=0;
      ++tag;
    }

    /* Do a binary search. */
    int i=0, j=l->ntag;

    while(i<j) {
      int k=(i+j)/2;
      int cmp=strcasecmp(tag,taparr[k]->tagname);
      if(cmp<0)
	j=k;
      else if(cmp>0)
	i=k+1;
      else {
	if(l->disarr)
	  st |= l->disarr[k];
	res.flags=st;
	res.idx=k;
	break;
      }
    }
  }

  return res;
}

static void free_pattlist(PattNode *p)
{
  while(p) {
    PattNode* next= p->next;
    free(p);
    p=next;
  }
}

static void free_tag_attr_patt_list(TagAttrPattList *l)
{
  TagAttrPattNode **taparr= l->taparr;
  CommentPattNode* c;

  if(taparr) {
    int i, n=l->ntag;
    for(i=0; i<n; ++i) {
      TagAttrPattNode* p = taparr[i];
      while(p) {
	TagAttrPattNode* next= p->next;
	AttrPattNode* a;
	free(p->tagname);
	a= p->attrpatts;
	while(a) {
	  AttrPattNode* nxt= a->next;
	  free(a->attrname);
	  free_pattlist(a->valpatt);
	  free(a);
	  a=nxt;
	}
	free_pattlist(p->txtpatt);
	/* p->stylerepl points to configuration data, do not free here. */
	free(p);
	p=next;
      }
    }
    free(taparr);
  }

  c = l->cmpl;
  while(c) {
    CommentPattNode* next= c->next;
    free_pattlist(c->cmpatt);
    free(c);
    c=next;
  }

  if(l->disarr) free(l->disarr);
}


/* Match a string with a pattern consisting of string segments seperated by null bytes. */
static int strsegmentscasematch(const char *string,size_t stringlen,const char *pattern,size_t pattlen)
{
  size_t lensegment;
  const char *midstr, *endstr;
  const char *nullcharp=memchr(pattern,0,pattlen);

  if(!nullcharp) return(stringlen==pattlen && !strncasecmp(string,pattern,pattlen));

  lensegment=nullcharp-pattern;
  if(stringlen<lensegment || strncasecmp(string,pattern,lensegment)) return 0;
  endstr=string+stringlen;
  midstr=string+lensegment;
  stringlen -= lensegment;

  while(pattern=nullcharp+1,pattlen -= lensegment+1, nullcharp=memchr(pattern,0,pattlen))
    {
      const char *match;
      lensegment = nullcharp-pattern;
      /* Note: pattern now points to a null terminated string of length lensegment. */
      if(!(match = strncasestr(midstr,stringlen,pattern))) return 0;
      midstr=match+lensegment;
      stringlen=endstr-midstr;
    }

  return(stringlen>=pattlen && !strncasecmp(endstr-pattlen,pattern,pattlen));
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
    char date[MAXDATESIZE],timeago[MAX_INT_STR+1],*timeunit;

    fstat(spool,&buf);

    t_ago=time(NULL)-buf.st_mtime;
    RFC822Date_r(buf.st_mtime,0,date);
    
    if(t_ago<0)
      {strcpy(timeago,"?");timeunit="";}
    else if(t_ago<3600)
      {sprintf(timeago,"%ld",(long)(t_ago/60));timeunit="m";}
    else if(t_ago<(24*3600))
      {sprintf(timeago,"%ld",(long)(t_ago/3600));timeunit="h";}
    else if(t_ago<(14*24*3600))
      {sprintf(timeago,"%ld",(long)(t_ago/(24*3600)));timeunit="d";}
    else if(t_ago<(30*24*3600))
      {sprintf(timeago,"%ld",(long)(t_ago/(7*24*3600)));timeunit="w";}
    else
      {sprintf(timeago,"%ld",(long)(t_ago/(30*24*3600)));timeunit="M";}

    cache_info=HTMLMessageString("AddCacheInfo",
                                 "url",Url->name,
                                 "date",date,
                                 "time",timeago,
                                 "unit",timeunit,
                                 NULL);
   }

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

 cleanup_baseUrl();

 first=0;
}


#define yy_write wwwoffles_write_data

inline static void yy_rawout(const char *text) __attribute__((always_inline));
inline static void yy_rawout(const char *text)
{
  yy_write(text,strlen(text));
}

/*+ A function to output the data if valid to do so. +*/
inline static void YY_WRITE(const char *text, size_t len) __attribute__((always_inline));
inline static void YY_WRITE(const char *text, size_t len)
{
  if(!disable_output && len>0) yy_write(text,len);
}

inline static void YY_OUTPUT(const char *text) __attribute__((always_inline));
inline static void YY_OUTPUT(const char *text)
{
  if(!disable_output && *(text)) yy_rawout(text);
}

/*+ A function to output the data possibly as part of a comment. +*/
static void yy_comout(const char *text, int iscomment)
{
  if(*text) {
    char *copy;
    size_t len;
    if(iscomment)
      {len= ~(size_t)0; copy= HTMLcommentstring((char*)text,&len);}
    else
      {copy= (char*)text; len=strlen(text);}
    yy_write(copy,len);
    if(copy!=text) free(copy);
  }
}

static void yy_comwrite(const char *text, size_t len, int iscomment)
{
  if(len>0) {
    char *copy;
    if(iscomment)
      copy= HTMLcommentstring((char*)text,&len);
    else
      copy= (char*)text;
    yy_write(copy,len);
    if(copy!=text) free(copy);
  }
}

/* add_insertfile copies the content of an insert file to the output.
   char *pathname: the path of the file to insert.
*/
static void add_insertfile(char *pathname)
{
  int fd=OpenLocalFile(pathname);

  if(fd==-1)
    return;
  {
    ssize_t n;
    char buffer[IO_BUFFER_SIZE];

    while((n=read(fd,buffer,IO_BUFFER_SIZE))>0)
      wwwoffles_write_data(buffer,n);

    if(n<0)
      PrintMessage(Warning,"Could not read from insert-file '%s' [%!s].",pathname);
  }
  close(fd);
}


/*++++++++++++++++++++++++++++++++++++++
  Extract the charset from a MIME type and charset.

  char *extract_charset Returns the charset that it found or NULL if none.

  const char *content_type The HTTP content type.
  ++++++++++++++++++++++++++++++++++++++*/

static char *extract_charset(const char *content_type)
{
 const char *p;
 char *charset=NULL;

 /* ' *text/html *; *charset *= *["']?...["']?' */

 p=content_type;

 while(*p && *p!=';') p++;
 if(*p!=';') return(NULL); /* unparseable */
 p++;

 while(isspace(*p)) p++;
 if(!*p) return(NULL); /* unparseable */

 if(!strcasecmp_litbeg(p,"charset"))
   {
    char *q;

    p+=7;

    while(*p && *p!='=') p++;
    if(*p!='=') return(NULL); /* unparseable */
    p++;

    while(isspace(*p)) p++;
    if(!*p) return(NULL); /* unparseable */

    if(*p=='"' || *p=='\'') p++;

    charset=strdup(p);

    q=charset+strlen(p)-1;
    if(*q=='"' || *q=='\'') *q=0;
   }

 return(charset);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the anchor tag and check if
  the href attribute matches the DontGet list or contains
  a script.
  Then output the original or disabled tag.

  disabled_t handle_a_tag Returns the type of disablement.

  const Tag *tag The tag information.

  int disable_dontget_anchors The option to disable links to URLs that are not got.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static disabled_t handle_a_tag(const Tag *tag,int disable_dontget_anchors,int disable_script)
{
 int i;
 disabled_t retval=0;
 char *href;

 for(i=0; i<tag->nattr; ++i)
   if(tag->attr_type[i]==att_href && (href=tag->attr_val[i])) {
     if(disable_dontget_anchors) {
       URL *linkUrl=LinkURL(baseUrl,href);

       if(ConfigBooleanMatchURL(DontGet,linkUrl))
	 retval=disabled_dontget_link;

       FreeURL(linkUrl);
       if(retval)
	 break;
     }
     if(disable_script && !strcasecmp_litbeg(href,"javascript:"))
       {retval=disabled_script; break;}
   }

 /* Output the original or modified tag. */

 output_tag(tag,tagprefix[retval]);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the iframe tag and check if
  the src attribute matches the DontGet list or contains
  a script.
  Then output the original or disabled tag.

  disabled_t handle_iframe_tag Returns the type of disablement.

  const Tag *tag The tag information.

  int disable_dontget_iframes The option to disable iframes to URLs that are not got.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static disabled_t handle_iframe_tag(const Tag *tag,int disable_dontget_iframes,int disable_script)
{
 int i;
 disabled_t retval=0;
 char *src;

 for(i=0; i<tag->nattr; ++i)
   if(tag->attr_type[i]==att_src && (src=tag->attr_val[i])) {
     if(disable_dontget_iframes) {
       URL *linkUrl=LinkURL(baseUrl,src);

       if(ConfigBooleanMatchURL(DontGet,linkUrl))
	 retval=disabled_dontget_iframe;

       FreeURL(linkUrl);
       if(retval)
	 break;
     }
     if(disable_script && !strcasecmp_litbeg(src,"javascript:"))
       {retval=disabled_script;break;}
   }

 /* Output the original or modified tag. */

 output_tag(tag,tagprefix[retval]);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the script tag and check if
  the src attribute matches the DontGet list.
  Then output the original or disabled tag.

  disabled_t handle_script_tag Returns nonzero if the script was disabled.

  const Tag *tag The tag information.
  ++++++++++++++++++++++++++++++++++++++*/

static disabled_t handle_script_tag(const Tag *tag, disabled_t is_disabled_script)
{
 int i;
 disabled_t retval=is_disabled_script;
 char *src;

 if(!retval)
   for(i=0; i<tag->nattr; ++i)
     if(tag->attr_type[i]==att_src && (src=tag->attr_val[i])) {
       URL *linkUrl=LinkURL(baseUrl,src);

       if(ConfigBooleanMatchURL(DontGet,linkUrl))
	 retval=disabled_dontget_script;

       FreeURL(linkUrl);
       if(retval)
	 break;
     }

 /* Output the original or modified tag. */

 output_tag(tag,tagprefix[retval]);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the img tag and parse it.

  Tag *tag The tag information.

  int replace_dontget The option to replace the images in the DontGet section.

  const char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  const char *webbug_replacement The webbug replacement image.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

inline static void handle_img_tag(Tag *tag,int replace_dontget,const char *dontget_replacement,
				  int replace_webbug,const char *webbug_replacement,
				  int disable_script)
{
 output_img_or_object_tag(tag,att_src,replace_dontget,dontget_replacement,replace_webbug,webbug_replacement,disable_script);
}


/*++++++++++++++++++++++++++++++++++++++
  Output an image tag or an object tag that contains an image.

  disabled_t output_img_or_object_tag Returns the type of disablement (0 if there is a replacement).

  Tag *tag The tag.

  int attr_type The attribute type that contains the URI.

  int replace_dontget The option to replace the images in the DontGet section.

  const char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  const char *webbug_replacement The webbug replacement image.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static disabled_t output_img_or_object_tag(Tag *tag,int attr_type,
					   int replace_dontget,const char *dontget_replacement,
					   int replace_webbug,const char *webbug_replacement,
					   int disable_script)
{
 int i;
 disabled_t retval=0;

 if(attr_type>=0)
   {
     if(replace_dontget || disable_script) {
       char *src;
       for(i=0; i<tag->nattr; ++i) {
	 if(tag->attr_type[i]==attr_type && (src=tag->attr_val[i])) {
	   if(replace_dontget ) {
	     URL *linkUrl=LinkURL(baseUrl,src);

	     if(ConfigBooleanMatchURL(DontGet,linkUrl))
	       retval=disabled_dontget_image;

	     FreeURL(linkUrl);
	     if(retval)
	       break;
	   }
	   if(disable_script && !strcasecmp_litbeg(src,"javascript:"))
	     {retval=disabled_script;break;}
	 }
       }
     }

     if(!retval && replace_webbug) {
       int seen_src=0,width=1000,height=1000;
       char *attrval;

       for(i=0; i<tag->nattr; ++i) {
	 if(tag->attr_type[i]==attr_type && tag->attr_val[i])
	   seen_src=1;
	 if(tag->attr_type[i]==att_width && (attrval=tag->attr_val[i])
	    && *attrval && isdigit(*attrval))
	   width=atoi(attrval);
	 else if(tag->attr_type[i]==att_height && (attrval=tag->attr_val[i])
		 && *attrval && isdigit(*attrval))
	   height=atoi(attrval);
       }

       if(seen_src && width<=1 && height<=1)
	 retval=disabled_webbug_image;
     }
   }

 /* Modify the src attribute (if required). */

 if(retval==disabled_dontget_image)
   {
     output_tag(tag,dontget_replacement?"!-- WWWOFFLE (replace-dontget-images) - ":
		tagprefix[retval]);

    if(!dontget_replacement)
      return retval;

    for(i=0; i<tag->nattr; ++i)
      if(tag->attr_type[i]==attr_type) {
	if(tag->attr_val[i]) free(tag->attr_val[i]);
	tag->attr_val[i]=strdup(dontget_replacement);
      }
   }
 else if(retval==disabled_webbug_image)
   {
     output_tag(tag,webbug_replacement?"!-- WWWOFFLE (replace-webbug-images) - ":
		tagprefix[retval]);

    if(!webbug_replacement)
      return retval;

    for(i=0; i<tag->nattr; ++i)
      if(tag->attr_type[i]==attr_type) {
	if(tag->attr_val[i]) free(tag->attr_val[i]);
	tag->attr_val[i]=strdup(webbug_replacement);
      }
   }
 else if(retval==disabled_script) {
    output_tag(tag,tagprefix[retval]);
    return retval;
 }

 /* Blank the alt & title attributes or add an empty alt (if required). */

 if(retval)
   {
    int seen_alt=0;

    for(i=0; i<tag->nattr; ++i)
       if(!strcasecmp(tag->attr_key[i],"alt") || !strcasecmp(tag->attr_key[i],"title"))
         {
          if(tag->attr_val[i])
             free(tag->attr_val[i]);

          tag->attr_val  [i]=strdup("");
          tag->attr_quote[i]='"';

          seen_alt=1;
         }

    if(!seen_alt)
      {
	add_attribute(tag, -1,"alt","",'"',0);
      }
   }

 /* Output the original or modified tag. */

 output_tag(tag,NULL);
 return 0;
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the object tag and parse it.

  disabled_t handle_object_tag Returns the type of disablement.

  Tag *tag The tag information.

  int replace_dontget The option to replace the images in the DontGet section.

  const char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  const char *webbug_replacement The webbug replacement image.

  int disable_applet The option to disable Java applets.

  int disable_flash The option to disable Flash animations.

  int disable_dontget_iframes The option to disable inline frames that are on the dontget list.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static disabled_t handle_object_tag(Tag *tag,int replace_dontget,const char *dontget_replacement,
				    int replace_webbug,const char *webbug_replacement,
				    int disable_applet,
				    int disable_flash,
				    int disable_dontget_iframes,
				    int disable_script)
{
 int i;

 /* Check for images. */

 if(replace_dontget || replace_webbug || disable_script)
   {
    for(i=0; i<tag->nattr; ++i)
      if((tag->attr_type[i]==att_codetype || tag->attr_type[i]==att_type) &&
	 tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"image"))
	 {
	   /* found an image */
	   return output_img_or_object_tag(tag,att_data,replace_dontget,dontget_replacement,replace_webbug,webbug_replacement,disable_script);
	 }
   }

 /* Check for Java */

 if(disable_applet)
   {
    for(i=0; i<tag->nattr; ++i)
      if((tag->attr_type[i]==att_codetype && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"application/java")) ||
	 (tag->attr_type[i]==att_classid && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"java:")))
	{
	  /* found Java applet */
	  output_tag(tag,tagprefix[disabled_applet]);
	  return disabled_applet;
	}
   }

 /* Check for Flash */

 if(disable_flash)
   {
    size_t val_len;
    for(i=0; i<tag->nattr; ++i)
      if(((tag->attr_type[i]==att_codetype || tag->attr_type[i]==att_type) &&
          tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"application/x-shockwave-flash")) ||
	 (tag->attr_type[i]==att_classid && tag->attr_val[i] &&
	  !strcasecmp_litbeg(tag->attr_val[i],"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000")) ||
	 (tag->attr_type[i]==att_src && tag->attr_val[i] && (val_len=strlen(tag->attr_val[i]))>=4 &&
	  !strcasecmp(tag->attr_val[i]+val_len-4,".swf")))
	{
	  /* found Flash */
	  output_tag(tag,tagprefix[disabled_flash]);
	  return disabled_flash;
	}
   }

 /* Check for inline HTML (text) object */

 if(disable_dontget_iframes || disable_script)
   {
    int is_inline=0;
    for(i=0; i<tag->nattr; ++i)
      if((tag->attr_type[i]==att_codetype || tag->attr_type[i]==att_type) &&
	 tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"text"))
	{is_inline=1;break;}

    if(is_inline) {
      disabled_t disabled=0;
      char *data;

      for(i=0; i<tag->nattr; ++i)
	if(tag->attr_type[i]==att_data && (data=tag->attr_val[i])) {
	  if(disable_dontget_iframes) {
	    URL *linkUrl=LinkURL(baseUrl,data);

	    if(ConfigBooleanMatchURL(DontGet,linkUrl))
	      disabled=disabled_dontget_iframe;

	    FreeURL(linkUrl);
	    if(disabled)
	      break;
	  }
	  if(disable_script && !strcasecmp_litbeg(data,"javascript:"))
	    {disabled=disabled_script;break;}
	}

      output_tag(tag,tagprefix[disabled]);
      return disabled;
    }
   }

 /* Output the original tag. */
 output_tag(tag,NULL);

 return(0);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the link tag and check if
  the rel attribute equals "Stylesheet" or contains
  a script.
  Then output the original or disabled tag.

  const Tag *tag The tag information.

  int disable_style Set to true if stylesheets are disabled.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_link_tag(const Tag *tag,int disable_style,int disable_script)
{
 int i;
 disabled_t disabled=0;
 char *rel;

 for(i=0; i<tag->nattr; ++i)
   if(tag->attr_type[i]==att_rel && (rel=tag->attr_val[i])) {
       if(disable_style && !strcasecmp_litbeg(rel,"Stylesheet"))
	 {disabled=disabled_style;break;}
       if(disable_script && !strcasecmp_litbeg(rel,"javascript:"))
	 {disabled=disabled_script;break;}
   }

 /* Output the original or modified tag. */

   output_tag(tag,tagprefix[disabled]);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the style tag and check if
  the type attribute contains the string "javascript".
  Then output the original or disabled tag.

  disabled_t handle_style_script_tag Returns nonzero if a script was detected and disabled.

  const Tag *tag The tag information.

  See "WhiteHat Security Advisory [Number: WH-08152001-1]" for the details of this.

  The HTML <style type="application/x-javascript"> or <style type="text/javascript">
  can introduce Javascript that WWWOFFLE would not otherwise have blocked.
  ++++++++++++++++++++++++++++++++++++++*/

static disabled_t handle_style_script_tag(const Tag *tag)
{
 int i;
 disabled_t retval=0;
 char *type;

 for(i=0; i<tag->nattr; ++i)
   if(tag->attr_type[i]==att_type && (type=tag->attr_val[i]) &&
      strcasestr(type,"javascript"))
     {
       retval=disabled_script;
       break;
     }

 /* Output the original or modified tag. */

 output_tag(tag,tagprefix[retval]);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the meta tag and parse it.

  const Tag *tag The tag information.

  int disable_meta_refresh Set to the disable-meta-refresh option.

  int disable_meta_refresh_self Set to the disable-meta-refresh-self option.

  int disable_meta_set_cookie Set to the disable-meta-set-cookie option.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_meta_tag(const Tag *tag,int disable_meta_refresh,int disable_meta_refresh_self,
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

 for(i=0; i<tag->nattr; ++i)
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
    for(i=0; i<tag->nattr; ++i)
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
       URL *linkUrl=LinkURL(baseUrl,meta_refresh);

       if(!strcmp(baseUrl->name,linkUrl->name))
          is_meta_refresh_self=1;

       FreeURL(linkUrl);
      }
   }

 /* Handle if they are charset ones. */

 if(is_meta_http_equiv_content_type && (demoronise_ms_chars || fix_mixed_cyrillic))
    for(i=0; i<tag->nattr; ++i)
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
       output_tag(tag,"!-- WWWOFFLE (disable-meta-refresh) - ");
     else
       {
	 free(tag->attr_val[meta_refresh_content]);
	 tag->attr_val[meta_refresh_content]=(meta_refresh?
					      x_asprintf("%d;URL=%s",replacement_meta_refresh_time,meta_refresh):
					      x_asprintf("%d",replacement_meta_refresh_time));
	 output_tag(tag,NULL);
       }
   }
 else if(is_meta_refresh_self && disable_meta_refresh_self)
   {
     if(replacement_meta_refresh_time<=0 || meta_refresh_content<0)
       output_tag(tag,"!-- WWWOFFLE (disable-meta-refresh-self) - ");
     else
       {
	 free(tag->attr_val[meta_refresh_content]);
	 tag->attr_val[meta_refresh_content]=(meta_refresh?
					      x_asprintf("%d;URL=%s",replacement_meta_refresh_time,meta_refresh):
					      x_asprintf("%d",replacement_meta_refresh_time));
	 output_tag(tag,NULL);
       }
   }
 else if(is_meta_http_equiv_set_cookie && disable_meta_set_cookie)
    output_tag(tag,"!-- WWWOFFLE (disable-meta-set-cookie) - ");
 else
    output_tag(tag,NULL);

 if(meta_refresh)
   free(meta_refresh);
}


static int process_tag_style_attr_replacement(Tag *tag, const char *text, size_t textlen, struct tagmatchval *tagm, TagAttrPattList *tapl, int *needtxt)
{
  int retval=0;

  if(tagm->flags&TAGMATCH) {
    TagAttrPattNode *p;
    for(p=tapl->taparr[tagm->idx]; p; p=p->next) {
      /* Note: tag->tag should now match p->tagname. */
      AttrPattNode *a;
      int i;
      for(a=p->attrpatts; a; a=a->next) {
	for(i=0; i<tag->nattr; ++i) {
	  if(!strcasecmp(tag->attr_key[i],a->attrname)) {
	    const char *attrval=tag->attr_val[i], *endptr;
	    size_t len;
	    if(attrval) {
	      if(a->valpatt) {
		PattNode *q;
		while(*attrval && isspace(*attrval)) ++attrval;  /* Skip blanks. */
		endptr=strchrnul(attrval,0);
		while(--endptr>=attrval && isspace(*endptr));  /* Trim trailing blanks. */
		++endptr;
		len= endptr-attrval;
		/* Try each pattern in the list to see if one matches. */
		q=a->valpatt;
		do {
		  if(strsegmentscasematch(attrval,len,q->patt,q->len))
		    goto nextattrpatt;
		  q=q->next;
		} while(q);
	      }
	    }
	    else if(!a->valpatt)
	      goto nextattrpatt;
	  }
	}
	goto nexttagattrpatt;
      nextattrpatt:;
      }
      if(p->txtpatt) {
	if(text) {
	  PattNode *q= p->txtpatt;
	  do {
	    if(strsegmentscasematch(text,textlen,q->patt,q->len))
	      goto textmatched;
	    q=q->next;
	  } while(q);
	  continue /* nexttagattrpatt */;
	textmatched:;
	}
	else if(needtxt) {
	  *needtxt=1;
	  break;
	}
	else
	  continue;
      }
      /* The tag matched and all the patterns in attribute pattern list matched,
	 modify the tag. */
      retval=p->disable;
      if(retval&DISABLEMATCHINGTAG)
	tag->disable=1;
      for(i=0; i<tag->nattr; ++i) {
	switch (tag->attr_type[i]) {
	case att_onblur: case att_onchange: case att_onclick: case att_ondblclick: case att_onfocus:
	case att_onerror: case att_onkeydown: case att_onkeypress: case att_onload: case att_onmousedown:
	case att_onmousemove: case att_onmouseout: case att_onmouseover: case att_onmouseup:
	case att_onreset: case att_onselect: case att_onsubmit: case att_onunload:
	  if(retval&DISABLEINNERSCRIPT) goto disableattr; /* Script events */
	  break;
	case att_style:
	  if(p->stylerepl) goto disableattr;
	  break;
	default:;
	}
	for(a=p->attrpatts; a; a=a->next) {
	  if(a->disableattr && !strcasecmp(tag->attr_key[i],a->attrname)) {
	    char *attrval=tag->attr_val[i], *endptr;
	    size_t len;
	    if(attrval) {
	      if(a->valpatt) {
		PattNode *q;
		while(*attrval && isspace(*attrval)) ++attrval;  /* Skip blanks. */
		endptr=strchrnul(attrval,0);
		while(--endptr>=attrval && isspace(*endptr));  /* Trim trailing blanks. */
		++endptr;
		len= endptr-attrval;
		q=a->valpatt;
		do {
		  if(strsegmentscasematch(attrval,len,q->patt,q->len))
		    goto disableattr;
		  q=q->next;
		} while(q);
	      }
	    }
	    else if(!a->valpatt)
	      goto disableattr;
	  }
	}
	continue;
      disableattr:
	tag->attr_disable[i]=1;
      }
      /* Add the new style attribute. */
      if(p->stylerepl && *(p->stylerepl))
	add_attribute(tag, -1,"style",p->stylerepl,'"',0);
      break;
    nexttagattrpatt:;
    }
  }

  return retval;
}


static int matchcomment(const char *cstr, size_t clen, TagAttrPattList *tapl)
{
  CommentPattNode *p;

  for(p=tapl->cmpl; p; p=p->next) {
    PattNode *q;
    for(q=p->cmpatt; q; q=q->next) {
      if(strsegmentscasematch(cstr,clen,q->patt,q->len))
	return p->disable;
    }
  }

  return 0;
}


static void output_attr_key_val(const char *key,const char *val, char quote, int disable, int iscomment)
{
  if(disable) {
    yy_rawout("wwwoffledisabled-");
    if(iscomment && *key=='-')
      yy_rawout(" ");
  }
  yy_comout(key,iscomment);

  if(val) {
    yy_rawout("=");
    if(quote) yy_write(&quote,1);
    yy_comout(val,iscomment);
    if(quote) yy_write(&quote,1);
  }
}

/*++++++++++++++++++++++++++++++++++++++
  Output a complete tag with optional custom head and/or tail.

  const Tag *tag The tag to output.

  const char *prefix The optional prefix of the tag.
  ++++++++++++++++++++++++++++++++++++++*/

static void output_tag(const Tag *tag,const char *prefix)
{
 int i;
 int iscomment;

 if(disable_output)
   return;

 if(tag->disable && !prefix)
   prefix=tagprefix[disabled_matching_tag];

 yy_rawout("<");

 if(prefix)
   yy_rawout(prefix);

 yy_comout(tag->tag, iscomment=(prefix!=NULL));

 for(i=0; i<tag->nattr; ++i)
   {
    yy_rawout(" ");
    output_attr_key_val(tag->attr_key[i],tag->attr_val[i],tag->attr_quote[i],tag->attr_disable[i],iscomment);
   }

 if(prefix)
   yy_rawout(" --");

 if(tag->xhtml && !prefix)
   yy_rawout(" />");
 else
   yy_rawout(">");
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
 unsigned key_allocsize=0;
 char *key_string=NULL;
 const char *prefix=NULL;
 short int disable_key;
 char quote;
 Tag tagdata;
 TagStack tagstack=NULL, tagpstack=NULL;

 char *anchor_modify_begin[3];
 char *anchor_modify_end[3];
 short int anchor_modify=0;
 int disable_script=ConfigBooleanURL(DisableHTMLScript,Url),disable_script_=disable_script;
 int disable_script_after_body=ConfigBooleanURL(DisableHTMLScriptAfterBody,Url);
 int disable_dontget_script=ConfigBooleanURL(DisableHTMLDontGetScript,Url),is_disabled_script=0;
 int disable_applet=ConfigBooleanURL(DisableHTMLApplet,Url);
 int disable_style=ConfigBooleanURL(DisableHTMLStyle,Url),is_disabled_style=0;
 int disable_blink=ConfigBooleanURL(DisableHTMLBlink,Url);
 int disable_marquee=ConfigBooleanURL(DisableHTMLMarquee,Url);
 int disable_flash=ConfigBooleanURL(DisableHTMLFlash,Url);
 int disable_iframes=ConfigBooleanURL(DisableHTMLIFrame,Url);
 int disable_meta_refresh=ConfigBooleanURL(DisableHTMLMetaRefresh,Url);
 int disable_meta_refresh_self=ConfigBooleanURL(DisableHTMLMetaRefreshSelf,Url);
 int replacement_meta_refresh_time=ConfigIntegerURL(ReplacementHTMLMetaRefreshTime,Url);
 int disable_meta_set_cookie=ConfigBooleanURL(DisableHTMLMetaSetCookie,Url);
 int disable_dontget_anchors=ConfigBooleanURL(DisableHTMLDontGetAnchors,Url);
 int disable_dontget_iframes=ConfigBooleanURL(DisableHTMLDontGetIFrames,Url);
 int replace_html_dontget_images=ConfigBooleanURL(ReplaceHTMLDontGetImages,Url);
 char *replacement_html_dontget_image=NULL,*replacement_html_dontget_image_copy=NULL;
 int replace_html_webbug_images=ConfigBooleanURL(ReplaceHTMLWebbugImages,Url);
 char *replacement_html_webbug_image=NULL,*replacement_html_webbug_image_copy=NULL;
 char *insertheadfile=ConfigStringURL(InsertHeadFile,Url);
 char *insertfile=ConfigStringURL(InsertFile,Url);
 short int insertedfile=0,after_body=0;
 short int tagcomplexmodify=0, disable_following_script=0, disable_all_following_scripts=0;
 int scriptdisablelevel=0;
 struct tagmatchval tagmatch = {0,-1};
 TagAttrPattList tagattrpattlist;

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
     char *localurl=GetLocalURL();

     replacement_html_dontget_image_copy=(char*)malloc(strlen(replacement_html_dontget_image)+strlen(localurl)+1);
     stpcpy(stpcpy(replacement_html_dontget_image_copy,localurl),replacement_html_dontget_image);
     replacement_html_dontget_image=replacement_html_dontget_image_copy;
     free(localurl);
   }
 }

 if(replace_html_webbug_images) {
   replacement_html_webbug_image=ConfigStringURL(ReplacementHTMLWebbugImage,Url);
   if(replacement_html_webbug_image && *replacement_html_webbug_image=='/') {
     char *localurl=GetLocalURL();

     replacement_html_webbug_image_copy=(char*)malloc(strlen(replacement_html_webbug_image)+strlen(localurl)+1);
     stpcpy(stpcpy(replacement_html_webbug_image_copy,localurl),replacement_html_webbug_image);
     replacement_html_webbug_image=replacement_html_webbug_image_copy;
     free(localurl);
   }
 }

 tagattrpattlist=make_tag_attr_patt_list(ReplacementStyleAttr,Url);
 disable_output=DISABLE_NONE;
 commentout=0;
 parsecomment= (tagattrpattlist.cmpl!=NULL);

 /* Initialise the tagdata */

 init_tagdata(&tagdata);

 /* The actual parser. */

 while((yychar=htmlmodify_yylex()))
    switch(yychar)
      {
      case LEX_PLAINTEXT:
       break;

      case LEX_COMMENT:
	if(parsecomment) {
	  char *comstr= htmlmodify_yylval, *endptr;
	  int disable;
	  while(*comstr && isspace(*comstr)) ++comstr;  /* Strip initial blanks. */
	  endptr= strchrnul(comstr,0);
	  while(--endptr>=comstr && isspace(*endptr));  /* Strip trailing blanks.*/
	  ++endptr;
	  disable= matchcomment(comstr,endptr-comstr,&tagattrpattlist);
	  if(disable&RESETFOLLOWINGSCRIPT)
	    disable_following_script=0;
	  if(disable&RESETALLFOLLOWINGSCRIPTS)
	    disable_all_following_scripts=0;
	  if(disable&DISABLEFOLLOWINGSCRIPT)
	    disable_following_script=1;
	  if(disable&DISABLEALLFOLLOWINGSCRIPTS)
	    disable_all_following_scripts=1;
	}
       break;

      case LEX_DOCTYPE:
       break;

      case LEX_TAG_BEGIN:
       tag=lookuptag(htmlmodify_yylval);
       tagmatch = tag_match_tag_attr_patt_list(htmlmodify_yylval,&tagattrpattlist);
       tagcomplexmodify= is_complex_tag(tag) || (tagmatch.flags & TAGMATCH);

       reset_tagdata(&tagdata,tag,tagcomplexmodify?htmlmodify_yylval:NULL);

       if(tag!=tag_script && tag!=tag__script)
	 disable_following_script=0;

       commentout=0;
       prefix=NULL;
       disable_script_ = (disable_script || scriptdisablelevel>0 || (after_body && disable_script_after_body));

       switch (tag)
	 {
	 case tag__a:
	   prefix=tagprefix[popendtagstack(&tagstack,tag)];
	   if(!prefix && anchor_modify)
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
	 case tag__object: case tag__embed:
	   prefix=tagprefix[popendtagstack(&tagstack,tag)];
	   break;
	 case tag__head:
	   if(insertheadfile)
	     add_insertfile(insertheadfile);
	   break;
	 case tag__body: case tag__html:
	   if(cache_info)
	     {YY_OUTPUT(cache_info); free(cache_info); cache_info=NULL;}
	   /* Normally there should be only one /body tag, at the end,
	      but I have seen broken HTML with multiple /body tags
	      with text in between. */
	   if(insertfile && (!insertedfile || tag==tag__body))
	     {add_insertfile(insertfile); insertedfile=1;}
	   after_body=1;
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
	     is_disabled_script= disabled_script;
	   else if(after_body && disable_script_after_body)
	     is_disabled_script= disabled_script_after_body;
	   else if(disable_all_following_scripts)
	     is_disabled_script= disabled_all_following_scripts;
	   else if(scriptdisablelevel>0)
	     is_disabled_script= disabled_inner_script;
	   else if(disable_following_script)
	     is_disabled_script= disabled_following_script;
	   else {
	     is_disabled_script= 0;
	     if(disable_dontget_script)
	       disable_output|=DISABLE_SCRIPT;
	   }
	   prefix=tagprefix[is_disabled_script];
	   break;
	 case tag__script:
	   prefix=tagprefix[is_disabled_script];
	   is_disabled_script=0;
	   break;
	 case tag_noscript: case tag__noscript:
	   if(disable_script)
	     prefix="!-- WWWOFFLE (disable-script) - ";
	   break;
	 case tag__applet:
	   popendtagstack(&tagstack,tag);
	   /* fall through */
	 case tag_applet:
	   if(disable_applet)
	     prefix="!-- WWWOFFLE (disable-applet) - ";
	   break;
	 case tag_param:
	   if(tagstack)
	     prefix=tagprefix[tagstack->val];
	   break;
	 case tag_style:
	   if(disable_style)
	     is_disabled_style= disabled_style;
	   else {
	     is_disabled_style= 0;
	     if(disable_script_) /* could be script pretending to be style */
	       disable_output|=DISABLE_STYLE;
	   }
	   prefix=tagprefix[is_disabled_style];
	   break;
	 case tag__style:
	   prefix=tagprefix[is_disabled_style];
	   is_disabled_style=0;
	   break;
	 case tag_meta:
	   if(disable_meta_refresh_self || disable_meta_refresh || disable_meta_set_cookie)
	     disable_output|=DISABLE_META;
	   break;
	 case tag_link:
	   if(disable_style || disable_script_)
	     disable_output|=DISABLE_LINK;
	   break;
	 case tag_object: case tag_embed:
	   if(replace_html_dontget_images || replace_html_webbug_images ||
	      disable_applet || disable_flash ||
	      disable_dontget_iframes || disable_script_)
	     disable_output|=DISABLE_OBJECT;
	   break;
	 case tag_a:
	   if(disable_dontget_anchors || disable_script_)
	     disable_output|=DISABLE_A;
	   break;
	 case tag_iframe:
	   if(disable_iframes)
	     prefix=tagprefix[disabled_iframe];
	   else if(disable_dontget_iframes || disable_script_)
	     disable_output|=DISABLE_IFRAME;
	   break;
	 case tag_img:
	   if(replace_html_dontget_images || replace_html_webbug_images || disable_script_)
	     disable_output|=DISABLE_IMG;
	   break;
	 default: ;
	 }

       if(tagmatch.flags) {
	 if(tagmatch.flags & TAGMATCH)
	   disable_output|=DISABLE_TAG;
	 else if(tagmatch.flags & TAGMAYBEDISABLE) {
	   int disable=poptagstack(&tagpstack,tagmatch.idx);
	   if((disable&DISABLEMATCHINGTAG) && !prefix)
	     prefix=tagprefix[disabled_matching_tag];
	   if((disable&DISABLEINNERSCRIPT) && scriptdisablelevel>0)
	     --scriptdisablelevel;
	   if(disable&DISABLEFOLLOWINGSCRIPT)
	     disable_following_script=1;
	   if(disable&DISABLEALLFOLLOWINGSCRIPTS)
	     disable_all_following_scripts=1;
	 }
       }

       if(!disable_output) {
	 yy_rawout("<");
	 if(prefix)
	   yy_rawout(prefix);
	 yy_comout(htmlmodify_yylval,prefix!=NULL);
       }
       break;

      case LEX_TAG_END_XHTML:
       tagdata.xhtml=1;

       /*@fallthrough@*/

      case LEX_TAG_END:
       if(!disable_output) {
	 if(prefix)
	   yy_rawout(" --");
	 if(yychar==LEX_TAG_END_XHTML && !prefix)
	   yy_rawout("/>");
	 else
	   yy_rawout(">");
       }

       if(tagmatch.flags & TAGMATCH) {
	 int disable, needtxt=0;
	 disable_output&=~DISABLE_TAG;

	 disable=process_tag_style_attr_replacement(&tagdata,NULL,0,&tagmatch,&tagattrpattlist,
						    (tag==tag_script && !tagdata.xhtml)? &needtxt: NULL);
	 if(needtxt) {
	   disable_output&=~DISABLE_SCRIPT;
	   disable_output|=DISABLE_TEXT;
	   is_disabled_script=0; /* Evaluate disabled status later. */
	   /* prefix=NULL; */
	 }
	 else {
	   if(disable&RESETFOLLOWINGSCRIPT) {
	     disable_following_script=0;
	     if(tag==tag_script && is_disabled_script==disabled_following_script) {
	       /* Re-evaluate disabled status. */
	       is_disabled_script= 0; prefix=NULL;
	       if(disable_dontget_script)
		 disable_output|=DISABLE_SCRIPT;
	     }
	   }
	   if(disable&RESETALLFOLLOWINGSCRIPTS) {
	     disable_all_following_scripts=0;
	     if(tag==tag_script && is_disabled_script==disabled_all_following_scripts) {
	       /* Re-evaluate disabled status. */
	       if(scriptdisablelevel>0)
		 is_disabled_script= disabled_inner_script;
	       else if(disable_following_script)
		 is_disabled_script= disabled_following_script;
	       else {
		 is_disabled_script= 0;
		 if(disable_dontget_script)
		   disable_output|=DISABLE_SCRIPT;
	       }
	       prefix=tagprefix[is_disabled_script];
	     }
	   }
	   if(disable&DISABLEINNERSCRIPT) {
	     disable_script_ = 1;
	     switch (tag) {
	     case tag_link:   disable_output|=DISABLE_LINK; break;
	     case tag_object: case tag_embed: disable_output|=DISABLE_OBJECT; break;
	     case tag_a:      disable_output|=DISABLE_A; break;
	     case tag_iframe: if(!disable_iframes) disable_output|=DISABLE_IFRAME; break;
	     case tag_img:    disable_output|=DISABLE_IMG; break;
	     case tag_script: if(!is_disabled_script)
				prefix=tagprefix[is_disabled_script=disabled_matching_script];
			      break;
	     case tag_style:  if(!disable_style) disable_output|=DISABLE_STYLE; break;
	     default:;
	     }
	   }

	   output_tag(&tagdata,prefix);

	   if(tagmatch.flags & TAGMAYBEDISABLE) {
	     if(!tagdata.xhtml) {
	       pushtagstack(&tagpstack,tagmatch.idx,disable);
	       if(disable&DISABLEINNERSCRIPT) ++scriptdisablelevel;
	     }
	     else {
	       if(disable&DISABLEFOLLOWINGSCRIPT)
		 disable_following_script=1;
	       if(disable&DISABLEALLFOLLOWINGSCRIPTS)
		 disable_all_following_scripts=1;
	     }
	   }
	 }
       }

       prefix=NULL;

       switch (tag)
	 {
	 case tag_meta:
	   if((disable_output&DISABLE_META) ||
	      demoronise_ms_chars || fix_mixed_cyrillic)
	     {
	       disable_output^=DISABLE_META;
	       handle_meta_tag(&tagdata,disable_meta_refresh,disable_meta_refresh_self,replacement_meta_refresh_time,disable_meta_set_cookie);
	       disable_output&=~DISABLE_META;
	     }
	   break;
	 case tag_link:
	   if(disable_output&DISABLE_LINK)
	     {
	       disable_output&=~DISABLE_LINK;
	       handle_link_tag(&tagdata,disable_style,disable_script_);
	     }
	   break;
	 case tag_applet:
	   if(!tagdata.xhtml)
	     pushtagstack(&tagstack,tag,disable_applet?disabled_applet:0);
	   break;
	 case tag_object: case tag_embed:
	   {
	     disabled_t disabled=0;
	     if(disable_output&DISABLE_OBJECT)
	       {
		 disable_output&=~DISABLE_OBJECT;
		 disabled= handle_object_tag(&tagdata,replace_html_dontget_images,replacement_html_dontget_image,
					     replace_html_webbug_images,replacement_html_webbug_image,
					     disable_applet,
					     disable_flash,
					     disable_dontget_iframes,
					     disable_script_);
	       }

	     if(!tagdata.xhtml)
	       pushtagstack(&tagstack,tag,disabled);
	   }
	   break;
	 case tag_a:
	   {
	     disabled_t disabled=0;
	     if(disable_output&DISABLE_A)
	       {
		 disable_output&=~DISABLE_A;
		 disabled=handle_a_tag(&tagdata,disable_dontget_anchors,disable_script_);
	       }

	     if(!tagdata.xhtml)
	       pushtagstack(&tagstack,tag,disabled);

	     if(!disabled && anchor_modify)
	       {
		 if(url_cached==1)
		   {if(anchor_modify_begin[0]) {YY_OUTPUT(anchor_modify_begin[0]);}}
		 else if(url_cached==2)
		   {if(anchor_modify_begin[1]) {YY_OUTPUT(anchor_modify_begin[1]);}}
		 else if(url_cached==-1)
		   {if(anchor_modify_begin[2]) {YY_OUTPUT(anchor_modify_begin[2]);}}
	       }
	   }
	   break;
	 case tag_iframe:
	   {
	     int disabled=0;
	     if(disable_iframes)
	       disabled=disabled_iframe;
	     else if(disable_output&DISABLE_IFRAME)
	       {
		 disable_output&=~DISABLE_IFRAME;
		 disabled=handle_iframe_tag(&tagdata,disable_dontget_iframes,disable_script_);
	       }

	     if(!tagdata.xhtml)
	       pushtagstack(&tagstack,tag,disabled);
	   }
	   break;
	 case tag_img:
	   if(disable_output&DISABLE_IMG)
	     {
	       disable_output&=~DISABLE_IMG;
	       handle_img_tag(&tagdata,replace_html_dontget_images,replacement_html_dontget_image,
			      replace_html_webbug_images,replacement_html_webbug_image,
			      disable_script_);
	     }
	   break;
	 case tag_script:
	   if(disable_output&DISABLE_SCRIPT)
	     {
	       disable_output&=~DISABLE_SCRIPT;

	       is_disabled_script=handle_script_tag(&tagdata,is_disabled_script);
	     }
	   if(is_disabled_script && !tagdata.xhtml)
	     commentout=1;
	   break;
	 case tag_style:
	   if(disable_output&DISABLE_STYLE)
	     {
	       disable_output&=~DISABLE_STYLE;

	       is_disabled_style=handle_style_script_tag(&tagdata);
	     }
	   if(is_disabled_style && !tagdata.xhtml)
	     commentout=1;
	   break;
	 default: ;
	 }

       tag=-1;
       key=-1;
       break;

      case LEX_ATTR_KEY:
       str_grow_cpy(&key_string,&key_allocsize,htmlmodify_yylval);
       key=lookupattribute(htmlmodify_yylval);
       break;

      case LEX_ATTR_VAL_DQ:
	quote='"'; goto lex_attr_val;
      case LEX_ATTR_VAL_SQ:
	quote='\''; goto lex_attr_val;
      case LEX_ATTR_VAL:
	quote=0;

      lex_attr_val:
       disable_key=0;

       switch (key)
	 {
	 case att_href:
	   if(tag==tag_a) {       /* Links */
	     if(anchor_modify) {
	       if(htmlmodify_yylval) {
		 if(*htmlmodify_yylval)
		   {
		     URL *linkUrl=LinkURL(baseUrl,htmlmodify_yylval);

		     if(!IsProtocolHandled(linkUrl))
		       url_cached=0;
		     else if(ExistsWebpageSpoolFile(linkUrl,0) || IsLocalNetHost(linkUrl->host))
		       url_cached=1;
		     else if(ExistsOutgoingSpoolFile(linkUrl))
		       url_cached=2;
		     else
		       {
			 URL *aliasUrl=GetAliasURL(linkUrl);

			 if(!aliasUrl)
			   url_cached=-1;
			 else
			   {
			     if(!IsProtocolHandled(aliasUrl))
			       url_cached=0;
			     else if(ExistsWebpageSpoolFile(aliasUrl,0) || IsLocalNetHost(aliasUrl->host))
			       url_cached=1;
			     else if(ExistsOutgoingSpoolFile(aliasUrl))
			       url_cached=2;
			     else
			       url_cached=-1;

			     FreeURL(aliasUrl);
			   }
		       }

		     FreeURL(linkUrl);
		   }
		 else
		   url_cached=1;
	       }
	       else
		 url_cached=0;
	     }
	   }
	   else if(tag==tag_base && htmlmodify_yylval)       /* Base tag */
	     setbase_url(htmlmodify_yylval);
	   break;

	 case att_onblur: case att_onchange: case att_onclick: case att_ondblclick: case att_onfocus:
	 case att_onerror: case att_onkeydown: case att_onkeypress: case att_onload: case att_onmousedown:
	 case att_onmousemove: case att_onmouseout: case att_onmouseover: case att_onmouseup:
	 case att_onreset: case att_onselect: case att_onsubmit: case att_onunload:
	   if(disable_script_)       /* Script events */
	     disable_key=1;
	   break;

	 case att_style:
	   if(disable_style)       /* Style references */
	     disable_key=1;
	   break;
	 default: ;
	 }

       /* Output the attribute and key or not. */

       /* More complicated tags that depend on other attributes are stored and done later. */
       if(tagcomplexmodify)
	 add_attribute(&tagdata, key,key_string,htmlmodify_yylval,quote,disable_key);

       if(!disable_output)
	 output_attr_key_val(key_string,htmlmodify_yylval,quote,disable_key,prefix!=NULL);

       key=-1;
       break;

      case LEX_SCRIPTTEXT:
	if(disable_output&DISABLE_TEXT) {
	  char *scrtext, *scriptend;
	  int disable;
	  disable_output&=~DISABLE_TEXT;
	  /* Delayed evaluation of script disabled status, based on script text. */
	  scrtext=htmlmodify_yylval; if(scripttextstart>0) scrtext+=scripttextstart;
	  /* Strip initial blanks. */
	  while(*scrtext && isspace(*scrtext)) ++scrtext;

	  if(scripttextend>=0) {
	    scriptend=htmlmodify_yylval+scripttextend;
	    if(scriptend<scrtext) scriptend=scrtext;
	  }
	  else
	    scriptend=strchrnul(scrtext,0);
	  /* Strip trailing blanks. */
	  while(--scriptend>=scrtext && isspace(*scriptend));
	  ++scriptend;

	  disable=process_tag_style_attr_replacement(&tagdata,scrtext,scriptend-scrtext,&tagmatch,&tagattrpattlist, NULL);

	  if(disable&RESETFOLLOWINGSCRIPT)
	    disable_following_script=0;
	  if(disable&RESETALLFOLLOWINGSCRIPTS)
	    disable_all_following_scripts=0;

	  /* Now we can finally evaluate the definitive disabled status. */
	  if(disable_script)
	    is_disabled_script= disabled_script;
	  else if(after_body && disable_script_after_body)
	    is_disabled_script= disabled_script_after_body;
	  else if(disable_all_following_scripts)
	    is_disabled_script= disabled_all_following_scripts;
	  else if(scriptdisablelevel>0)
	    is_disabled_script= disabled_inner_script;
	  else if(disable_following_script)
	    is_disabled_script= disabled_following_script;
	  else if(disable&DISABLEINNERSCRIPT)
	    is_disabled_script=disabled_matching_script;

	  is_disabled_script=handle_script_tag(&tagdata,is_disabled_script);

	  if(tagmatch.flags & TAGMAYBEDISABLE) {
#if 0
	    if(!tagdata.xhtml) {
#endif
	      pushtagstack(&tagpstack,tagmatch.idx,disable);
	      if(disable&DISABLEINNERSCRIPT) ++scriptdisablelevel;
#if 0
	    }
	    else {
	      if(disable&DISABLEFOLLOWINGSCRIPT)
		disable_following_script=1;
	      if(disable&DISABLEALLFOLLOWINGSCRIPTS)
		disable_all_following_scripts=1;
	    }
#endif
	  }

	  /* Now output the script text itself. */
	  if(is_disabled_script && scripttextstart>=0) {
	    if(scripttextstart>0) yy_write(htmlmodify_yylval,scripttextstart);
	    if(!scriptcommopen) yy_rawout("<!--\n");
	    if(scripttextend>scripttextstart)
	      yy_comwrite(htmlmodify_yylval+scripttextstart,scripttextend-scripttextstart,1);
	    if(!scriptcommclose) yy_rawout("//-->\n");
	    if(scripttextend>=0 && htmlmodify_yylval[scripttextend]) yy_rawout(htmlmodify_yylval+scripttextend);
	  }
	  else if(*htmlmodify_yylval)
	    yy_rawout(htmlmodify_yylval);
	}
	break;

      default:
       break;
      }

 if(cache_info)
   {YY_OUTPUT(cache_info); free(cache_info); cache_info=NULL;}

 if(insertfile && !insertedfile)
   {add_insertfile(insertfile); insertedfile=1;}

 if(key_string)
    free(key_string);

 /* Delete the tagdata */

 free_tagdata(&tagdata);
 free_tagstack(tagstack);
 free_tagstack(tagpstack);
 free_tag_attr_patt_list(&tagattrpattlist);

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
 static const char* const demoronise_ms_chars_list[]={/* 0x80 */ "\200",
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

static const unsigned int fix_mixed_cyrillic_list[]={0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,
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
    yy_rawout("<!-- WWWOFFLE (demoronise-ms-chars) - '");
    yy_rawout(htmlmodify_yytext);
    yy_rawout("' -->");
    yy_rawout(demoronise_ms_chars_list[ch-0x80]);
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
       yy_rawout(htmlmodify_yytext);
      }
    else
      {
       char html_buffer[9];
       sprintf(html_buffer,"&#x%04X;",unicode_value);

       yy_rawout("<!-- WWWOFFLE (fix-mixed-cyrillic) - '");
       yy_rawout(htmlmodify_yytext);
       yy_rawout("' -->");
       yy_rawout(html_buffer);
      }
   }
 else
    yy_rawout(htmlmodify_yytext);
}


#define YY_SKIP_YYWRAP 1 /* Remove error with prototype of ..._yywrap */
#ifndef htmlmodify_yywrap
/*+ Needed in lex but does nothing. +*/
#define htmlmodify_yywrap() 1
#endif

/*+ Reset the current string. +*/
#define RESET_STRING \
{if(!string) string=malloc(stringallocsize=64); *string=0; stringlen=0; \
 scriptcommopen=0; scriptcommclose=0; scripttextstart=-1; scripttextend=-1;}

/*+ append information to the current string. +*/
#define APPEND_STRING(xx,len)				 \
{size_t _new_len=stringlen+(len);					\
 if(_new_len>=stringallocsize) string=(char*)realloc((void*)string,stringallocsize=_new_len+1); \
 memcpy(string+stringlen,xx,(len)+1);					\
 stringlen=_new_len;}

#define APPEND_STRING_YYTEXT APPEND_STRING(htmlmodify_yytext,htmlmodify_yyleng)

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
{if((result=wwwoffles_read_data(buf,max_size))==-1) result=0;}

#define WRITE_RAW_YYTEXT yy_write(htmlmodify_yytext,htmlmodify_yyleng)
#define WRITE_YYTEXT YY_WRITE(htmlmodify_yytext,htmlmodify_yyleng)
#define WRITE_COMMENT_YYTEXT yy_comwrite(htmlmodify_yytext,htmlmodify_yyleng,commentout)
%}

%%
 /* Must use static variables since the parser returns often. */
 static char *string=NULL;
 static size_t stringallocsize=0,stringlen=0;
 static int after_tag=INITIAL;

 /* Handle comments and other tags */

[\x80-\xBF]                 { handle_high_bit(*(unsigned char*)htmlmodify_yytext); }
[^<\x80-\xBF]+              { WRITE_RAW_YYTEXT; /* htmlmodify_yylval=htmlmodify_yytext; return(LEX_PLAINTEXT); */ }

"<!DOCTYPE"                 { WRITE_RAW_YYTEXT; BEGIN(DOCTYPE); RESET_STRING; }
"<!--"                      { WRITE_RAW_YYTEXT; BEGIN(COMMENT); RESET_STRING; }
"<!"{W}*"-"*                { WRITE_RAW_YYTEXT; BEGIN(COMMENT_BAD); RESET_STRING; }
"<"{W}*                     { BEGIN(TAG_START); RESET_STRING; APPEND_STRING_YYTEXT; }

 /* Doctype (DTD) */

<DOCTYPE>">"                { WRITE_RAW_YYTEXT; BEGIN(INITIAL); /* htmlmodify_yylval=string; return(LEX_DOCTYPE); */ }
<DOCTYPE>[^>]+              { WRITE_RAW_YYTEXT; /* APPEND_STRING_YYTEXT; */ }

 /* Comments - COMMENT_BAD is not a legal comment format (except <!>) but people use it as one.
               COMMENT is not strictly correct, but works better than the real thing. */

<COMMENT>"--"{W}*">"        { WRITE_RAW_YYTEXT; BEGIN(INITIAL); htmlmodify_yylval=string; return(LEX_COMMENT); }
<COMMENT>"-"                |
<COMMENT>[^-]+              { WRITE_RAW_YYTEXT; if(parsecomment) APPEND_STRING_YYTEXT; }

<COMMENT_BAD>">"            { WRITE_RAW_YYTEXT; BEGIN(INITIAL); htmlmodify_yylval=string; return(LEX_COMMENT); }
<COMMENT_BAD>[^>]+          { WRITE_RAW_YYTEXT; if(parsecomment) APPEND_STRING_YYTEXT; }

 /* Tags */

<TAG_START>"script"/{tagdel}  { BEGIN(TAG); after_tag=SCRIPT_START; htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"style"/{tagdel}   { BEGIN(TAG); after_tag=STYLE_START;  htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/{tagdel} { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>(.|\n)             { BEGIN(INITIAL); YY_WRITE(string,stringlen); WRITE_YYTEXT; }

<TAG>{W}+                   { WRITE_YYTEXT; }
<TAG>"/>"                   { BEGIN(INITIAL); htmlmodify_yylval=""; return(LEX_TAG_END_XHTML); }
<TAG>">"                    { BEGIN(after_tag); if(after_tag!=INITIAL) {RESET_STRING;} htmlmodify_yylval=""; return(LEX_TAG_END); }
<TAG>"<"                    { BEGIN(after_tag); unput(htmlmodify_yytext[0]); if(after_tag!=INITIAL) {RESET_STRING;}
                              htmlmodify_yylval=""; return(LEX_TAG_END); }
<TAG>{key}                  { BEGIN(TAG_ATTR_KEY); htmlmodify_yylval=htmlmodify_yytext; return(LEX_ATTR_KEY); }
<TAG>(.|\n)                 { WRITE_YYTEXT; }

<TAG_ATTR_KEY>{W}*=         { BEGIN(TAG_ATTR_VAL); }
<TAG_ATTR_KEY>(.|\n)        { BEGIN(TAG); unput(htmlmodify_yytext[0]); htmlmodify_yylval=NULL; return(LEX_ATTR_VAL); }

<TAG_ATTR_VAL>\"            { BEGIN(DQUOTED); RESET_STRING; }
<TAG_ATTR_VAL>\'            { BEGIN(SQUOTED); RESET_STRING; }
<TAG_ATTR_VAL>{W}+          { }
<TAG_ATTR_VAL>{val}         { BEGIN(TAG);                              htmlmodify_yylval=htmlmodify_yytext; return(LEX_ATTR_VAL); }
<TAG_ATTR_VAL>(.|\n)        { BEGIN(TAG); unput(htmlmodify_yytext[0]); htmlmodify_yylval="";                return(LEX_ATTR_VAL); }

 /* Quoted strings */

<DQUOTED>\"                 { BEGIN(TAG); htmlmodify_yylval=string; return(LEX_ATTR_VAL_DQ); }
<DQUOTED>[^\"]+             { APPEND_STRING_YYTEXT; }

<SQUOTED>\'                 { BEGIN(TAG); htmlmodify_yylval=string; return(LEX_ATTR_VAL_SQ); }
<SQUOTED>[^\']+             { APPEND_STRING_YYTEXT; }

 /* Scripts */

<SCRIPT_START>{W}+          { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_RAW_YYTEXT;} }
<SCRIPT_START>"<"/"/script" { BEGIN(TAG_START); htmlmodify_yylval=string; return(LEX_SCRIPTTEXT); }
<SCRIPT_START>"<!--"        { if(disable_output) {APPEND_STRING_YYTEXT; scriptcommopen=1; scripttextstart=stringlen;}
                              else {WRITE_RAW_YYTEXT;}
                              BEGIN(SCRIPT); }
<SCRIPT_START>(.|\n)        { if(disable_output) {scriptcommopen=0; scripttextstart=stringlen;}
                              else {if(commentout) yy_rawout("<!--\n");}
                              unput(htmlmodify_yytext[0]); BEGIN(SCRIPT); }

<SCRIPT>"-->"{W}*/"</script" { if(disable_output) {scriptcommclose=1; scripttextend=stringlen; APPEND_STRING_YYTEXT;}
                               else {WRITE_RAW_YYTEXT;}
                               BEGIN(SCRIPT_START); }
<SCRIPT>"-"+                { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_COMMENT_YYTEXT;} }
<SCRIPT>"<"/"/script"       { if(disable_output) {scriptcommclose=0; scripttextend=stringlen;}
                              else {if(commentout) yy_rawout("//-->\n");}
                              BEGIN(TAG_START); htmlmodify_yylval=string; return(LEX_SCRIPTTEXT); }
<SCRIPT>"<"                 |
<SCRIPT>[^-<]+              { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_RAW_YYTEXT;} }

 /* Styles */

<STYLE_START>{W}+           { WRITE_RAW_YYTEXT; }
<STYLE_START>"<"/"/style"   { BEGIN(TAG_START); }
<STYLE_START>"<!--"         { WRITE_RAW_YYTEXT; BEGIN(STYLE); }
<STYLE_START>(.|\n)         { if(commentout) yy_rawout("<!--\n");
                              unput(htmlmodify_yytext[0]); BEGIN(STYLE); }

<STYLE>"-->"{W}*/"</style"  { WRITE_RAW_YYTEXT; BEGIN(STYLE_START); }
<STYLE>"-"+                 { WRITE_COMMENT_YYTEXT; }
<STYLE>"<"/"/style"         { if(commentout) yy_rawout("\n-->\n"); BEGIN(TAG_START); }
<STYLE>"<"                  |
<STYLE>[^-<]+               { WRITE_RAW_YYTEXT; }

 /* End of file */

<<EOF>>                     { free(string); string=NULL; stringallocsize=stringlen=0;
                              after_tag=INITIAL; BEGIN(INITIAL); htmlmodify_yylval=NULL; return(0); }

%%
