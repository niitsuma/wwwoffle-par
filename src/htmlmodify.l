W               [ \t\r\n\f]

nonascii        [\200-\377]
ascii           [ -~]
alpha           [[:alpha:]]
alphanum        [[:alnum:]]
punct           [][!\"#$%&\'()*+,./:;<=>?@\\^_`{|}~-]
safepunct       [][!#$%&()*+,./:;=?@\\^_`{|}~-]

tag             {alpha}({alphanum}|[-_:.])*
tagdel          [ \t\r\n\f/>]
key             ({alphanum}|[][!#$%&()*+,.:;?@\\^_`{|}~-])+
val             ({alphanum}|{nonascii}|{safepunct}|<)+

%x DOCTYPE
%x COMMENT COMMENT_BAD
%x MISC
%x TAG_START TAG TAG_ATTR_KEY TAG_ATTR_VAL
%x DQUOTED SQUOTED
%x SCRIPT_START SCRIPT
%x STYLE_START STYLE

%{
/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9f.
  Parse the HTML and modify the source.
  ******************/ /******************
  Originally written by Andrew M. Bishop.
  Extensively modified by Paul A. Rombouts.

  This file Copyright 1997-2009 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007,2008,2009,2011 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>  /* For offsetof() */

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
#define LEX_SCRIPTTEXT     2
#define LEX_STYLETEXT      3
#define LEX_COMMENT        4
#define LEX_DOCTYPE        5
#define LEX_MISC           6

#define LEX_TAG_BEGIN     11
#define LEX_TAG_END       12
#define LEX_TAG_END_XHTML 13
#define LEX_TAG_END_TRUNC 14

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
  disabled_meta_refresh,
  disabled_meta_refresh_self,
  disabled_meta_set_cookie,
  disabled_blink,
  disabled_marquee,
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
  "!-- WWWOFFLE (disable-meta-refresh) - ",
  "!-- WWWOFFLE (disable-meta-refresh-self) - ",
  "!-- WWWOFFLE (disable-meta-set-cookie) - ",
  "!-- WWWOFFLE (disable-blink) - ",
  "!-- WWWOFFLE (disable-marquee) - ",
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


typedef unsigned short Bool_t;  /* Type used to store boolean values (only 0 or 1). */

typedef enum {htokenTag, htokenText, htokenScriptText, htokenStyleText, htokenComment, htokenDoctype, htokenMisc }
 HTokenType;

/*+ A structure to hold a tag and its attributes. +*/

typedef struct _Tag {
  char *tagname;              /*+ The Tag name. +*/
  short type;                 /*+ The type of the tag (enum HTMLTags, may be -1). +*/
  unsigned short resolved;    /*+ Flag which is false if there are unmatched descendent patterns. +*/
  unsigned short ending;      /*+ Indicates whether tag ends with '>' (1), '/>' (2) or has no ending (0). +*/
  unsigned short disable;     /*+ tag disabled status (enum disabled_t). +*/
  int idx;                    /*+ Tag index (for tag-attribute patterns) +*/

  struct _AttrNode *attrl;    /* points to first node in attribute list. */
  struct _AttrNode **last;    /* points to the last 'next' field in attribute list. */
}
Tag;

typedef struct _AttrNode {
  struct _AttrNode *next;
  char *val;                  /*+ attribute value (may be null). +*/
  short type;                 /*+ attribute key type (enum HTMLAttributes, may be -1). +*/
  unsigned char disable;      /*+ attribute disable flag +*/
  char quote;                 /*+ attribute quote (may be null char). +*/
  char key[0];                /*+ attribute key name. +*/
}
AttrNode;

typedef struct _TextStr {
  unsigned len;   /* Length of string data (character array). */
  char str[0];    /* String data follows here. It is not necessarily terminated by a null byte! */
}
TextStr;

#define STEXTCOMMOPEN    4
#define STEXTCOMMCLOSE   2
#define STEXTISEMPTY     1

typedef struct _STextStr {
  unsigned len;   /* Length of string data (character array). */
  unsigned short beginlen;
  unsigned short closelen;
  unsigned short flags;
  char str[0];    /* String data follows here. It is not necessarily terminated by a null byte! */
}
STextStr;

typedef struct _HToken {
  HTokenType tokentype;
  struct _HToken *next;
  struct _HTokenPattNode *matchingpatt; /* if this token was matched to a pattern
					   this points to the pattern node. */
  union {
    Tag        tag;    /* htokenTag */
    TextStr    text;   /* htokenText, htokenComment, htokenDoctype, htokenMisc */
    STextStr   stext;  /* htokenScriptText, htokenStyleText */
  };
}
HToken;

struct numberedhtoken {
  HToken *htoken;
  unsigned serialnum;
};

typedef struct _HTokenQueue {
  HToken *front;
  HToken **rear;
  unsigned frontserialnum,nextserialnum;
}
HTokenQueue;

typedef struct _HTokenListNode {
  struct _HTokenListNode *next;
  HToken *htoken;
  unsigned serialnum;
}
HTokenListNode;

/*+ A stack for holding tags of possibly nested objects and their disabled status. +*/
typedef struct _TagStackNode {
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
  PattNode* valpatt;
  unsigned short disableattr;
  char attrname[0];
}
AttrPattNode;

typedef struct _TagAttrPattNode {
  struct _AttrPattNode *attrpatts;
  int idx;
  char tagname[0];
}
TagAttrPattNode;

typedef struct _HTokenPattNode {
  unsigned short tokentype;            /* Either htokenTag, htokenText, htokenScriptText, htokenStyleText or htokenComment */
  unsigned char active;
  unsigned char firstlinedesc;         /* Flag indicating first-line descendancy from target pattern. */
  unsigned short targdesc;             /* Degree of descendancy from target pattern. */
  unsigned short hasdesc;              /* 0 means no descendant pattern, 1 means this pattern node has a child pattern,
					  2 means means this pattern node has a descendant pattern (child or child of child etc.) */
  union {
    struct _HTokenPattNode *desc;      /* Points to possible pattern for descendant html token if hasdesc!=0. */
    struct _HTokenPattNode *target;    /* Points to pattern for target token if hasdesc==0. */
  };
  struct _PattOptions *pattopts;       /* Points to option values. */
  struct _HTokenPattNode* next;        /* Next pattern to try. */
  union {
    TagAttrPattNode tagpattn;
    PattNode       *txtpatt;
    /* PattNode    *compatt; */
  };
}
HTokenPattNode;

typedef struct _HTokenPattListNode {
  struct _HTokenPattListNode *next;
  HTokenPattNode *pattn;
}
HTokenPattListNode;

typedef struct _PattOptionsListNode {
  struct _PattOptionsListNode *next;
  struct _PattOptions *pattopts;
  unsigned short active;
}
PattOptionsListNode;

typedef struct {
  PattOptionsListNode *first;
  PattOptionsListNode **last;
}
PattOptionsList;

typedef struct _StateListNode {
  struct _StateListNode  *next;
  unsigned serialnum;
  unsigned lastm;  /* Target descendancy of last matched HTokenPattNode. */
  union {
    struct numberedhtoken numtok;          /* Used when HASSIBLINGPATTERN bit in optionflags is not set. */
    PattOptionsListNode *pattoptslistnode; /* Used when HASSIBLINGPATTERN bit in optionflags is set. */
  };
}
StateListNode;

typedef struct _StateRefListNode {
  struct _StateRefListNode *next;
  unsigned serialnum;
  unsigned lastm;  /* Saved lastm value of StateListNode with the same serial number. */
}
StateRefListNode;

typedef struct _PattStateListNode {
  struct _PattStateListNode *next;
  HTokenPattNode *pattn;
  struct _PattOptions *pattopts;
  StateRefListNode *streflist;
  unsigned short active;       /* Saved active value of HTokenPattNode. */
}
PattStateListNode;

typedef struct _PattOptions {
  unsigned short optionflags;
  union {
      const char *stylerepl;    /* Used when HASSIBLINGPATTERN bit in optionflags is not set. */
      HTokenPattNode *sibling;  /* Points to possible sibling pattern.
				   Used when HASSIBLINGPATTERN bit in optionflags is set .*/
  };
  StateListNode *stlist;        /* List of state nodes representing (partial) matches that are not yet fully resolved because of descendant patterns. */
}
PattOptions;

typedef struct _PattInfo {
  HTokenPattNode **taparr;   /* Points to array of length ntag containing pointers to lists of tag-attribute patterns. */
  HTokenPattNode *txtpl;     /* Points to list of text patterns. */
  HTokenPattNode *scrtxtpl;  /* Points to list of script-text patterns. */
  HTokenPattNode *styltxtpl; /* Points to list of style-text patterns. */
  HTokenPattNode *compl;     /* Points to list of comment patterns. */
  HTokenPattListNode *activechildren;  /* Points to list of activated first child patterns. */
  HTokenListNode *firstlinetarglist;   /* Points to list of unresolved target tokens which are first-line ancestors. */
  HTokenListNode **lastfirstlinetarg;
  PattOptionsListNode *activesiblings; /* Points to list of activated sibling patterns. */
  PattOptionsListNode **lastactivesibs;
  unsigned char *stacktag;   /* Points to array of length ntag with flags specifying which tags need to be stacked. */
  int ntag;
  unsigned stateserialnum;   /* Counter used to generate serial numbers for StateListNodes. */
}
PattInfo;

typedef struct _TagPStackNode {
  struct _TagPStackNode *next;
  int idx;                           /* Tag index */
  PattStateListNode *pattstatelist;  /* List of old pattern state info that needs to be restored
					when this node is popped off the stack. */
  PattOptionsListNode *pattoptslist; /* List of sibling patterns that need to be activated
					when this node is popped off the stack. */
  HToken *htoken;                    /* Reference to the HToken that is resolved when this node is popped off the stack. */
  unsigned serialnum;                /* The serial number used to determine whether the HToken is
					still present in the queue. */
  HTokenListNode *parenthtoklist;    /* List of references to possible parent HTokens that are resolved
					when this node is popped off the stack. */
}
TagPStackNode;

typedef struct _TagPStackNode *TagPStack;


/* Microsoft Character mapping */

/*+ The option to convert the characters when seen. +*/
static int demoronise_ms_chars;
static int fix_mixed_cyrillic;


/* Definitions of why the output is disabled. */

#define DISABLE_NONE          0

#define DISABLE_A             1
#define DISABLE_IFRAME        2
#define DISABLE_IMG           4
#define DISABLE_LINK          8
#define DISABLE_META       0x10
#define DISABLE_OBJECT     0x20
#define DISABLE_PARAM      0x40
#define DISABLE_SCRIPT     0x80
#define DISABLE_STYLE     0x100

#define DISABLE_TAG       0x200
#define DISABLE_ALLOUTPUT 0x400

/* Bit masks for the result of tag-attribute matching. */
#define TAGDSTACK         1
#define TAGPSTACK         2

#define DISABLEMATCHINGTAG            1
#define DISABLEMATCHINGATTRIBUTES     2
#define DISABLEINNERSCRIPT            4
#define DISABLEFOLLOWINGSCRIPT        8
#define DISABLEALLFOLLOWINGSCRIPTS 0x10
#define RESETFOLLOWINGSCRIPT       0x20
#define RESETALLFOLLOWINGSCRIPTS   0x40
#define HASSIBLINGPATTERN          0x80
#define DISABLEMASK (DISABLEMATCHINGTAG|DISABLEINNERSCRIPT|DISABLEFOLLOWINGSCRIPT|DISABLEALLFOLLOWINGSCRIPTS)

/* Local functions */

static void modify_html(URL *Url);

static /*@null@*/ char *htmlmodify_yylval=NULL;
static size_t htmlmodify_yylval_len=0;
extern int htmlmodify_yylex(void);

static /*@null@*/ char *extract_charset(const char *content_type);

static disabled_t handle_a_tag(Tag *tag,int disable_dontget_anchors,int disable_script);
static disabled_t handle_iframe_tag(Tag *tag,int disable_dontget_iframes,int disable_script);
static disabled_t handle_script_tag(Tag *tag, disabled_t is_disabled_script);
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
static void handle_link_tag(Tag *tag,int disable_style,int disable_script);
static disabled_t handle_style_script_tag(Tag *tag);
static void handle_meta_tag(Tag *tag,int disable_meta_refresh,int disable_meta_refresh_self,
			    int replacement_meta_refresh_time,int disable_meta_set_cookie);
static void output_tag(const Tag *tag,const char *prefix);

static void handle_high_bit(const char *p);
static void handle_text(const char *str, size_t len);


/*+ The add-cache-info optional footer. +*/
static /*@null@*/ /*@observer@*/ char *cache_info=NULL;

/*+ The base URL of this page. +*/
static /*@null@*/ URL *baseUrl=NULL;

/*+ Set this to disable the output. +*/
static int disable_output=DISABLE_NONE;

static Bool_t parsecomment=0;
static Bool_t commentout=0;
static Bool_t stextcommopen=0, stextcommclose=0;
static int stextstart=-1, stextend=-1;

inline static void str_grow_cpy(char **dst, unsigned *allocsize, const char *src)
{
  unsigned size= strlen(src)+1;
  if(size>*allocsize)
    *dst= (char*)realloc(*dst, *allocsize=size);
  memcpy(*dst,src,size);
}

inline static void setbaseUrl(const URL *Url)
{
  if(Url) {
    if(baseUrl) FreeURL(baseUrl);
    baseUrl=CopyURL(Url);
  }
}

inline static void setbase_url(const char *url)
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

inline static int lookuptag(const char *s)
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

/* This is a little dangerous, because tag->tagname may be NULL! */
inline static int tag_is_endtag(const Tag *tag)
{
  return *(tag->tagname)=='/';
}

inline __attribute__((always_inline))
static void free_tagdata(Tag *tagdata)
{
  AttrNode *p;

  if(tagdata->tagname) free(tagdata->tagname);
  p= tagdata->attrl;
  while(p) {
    AttrNode *next= p->next;
    /* free(p->key); */
    if(p->val) free(p->val);
    free(p);
    p= next;
  }
}

/* Reuse an HToken of type htokenTag or allocate a new one and initialize it. */
static void reset_tagtoken(HToken **tokp, const char *tagname, HTMLTags type, int idx)
{
  HToken *tok= *tokp;

  if(tok) {
    /* tok->tokentype should be htokenTag. */
    free_tagdata(&tok->tag);
  }
  else {
    *tokp= tok= (HToken *)malloc(offsetof(HToken,tag)+sizeof(Tag));
    tok->tokentype= htokenTag;
  }

  tok->next=NULL;
  tok->matchingpatt=NULL;
  tok->tag.tagname= (tagname?strdup(tagname):NULL);
  tok->tag.type= type;
  tok->tag.resolved=0;
  tok->tag.ending=0;
  tok->tag.disable=0;
  tok->tag.idx= idx;
  tok->tag.attrl=NULL;
  tok->tag.last= &tok->tag.attrl;
}

static void add_attribute(Tag *tag, HTMLAttributes type, const char *key, const char *val, char quote, unsigned short disable)
{
  size_t keysz= strlen(key)+1;
  AttrNode *new= malloc(sizeof(AttrNode)+keysz);

  new->next=NULL;
  new->val= (val?strdup(val):NULL);
  new->type=type;
  new->disable=disable;
  new->quote=quote;
  memcpy(new->key,key,keysz);

  *(tag->last) = new;
  tag->last = &new->next;
}

static void free_htoken(HToken *htokp)
{
  /* Ignore p->next and p->matchingpatt, these should be freed elsewhere. */
  if(htokp->tokentype==htokenTag)
    free_tagdata(&htokp->tag);

  free(htokp);
}

inline static void htokenqueue_init(HTokenQueue *q)
{
  q->front=NULL;
  q->rear= &q->front;
  q->frontserialnum=q->nextserialnum=0;
}

inline static int htokenqueue_isempty(HTokenQueue *q)
{
  return q->front==NULL;
}

/* Make sure tok->next==NULL before calling htokenqueue_push()! */
inline static void htokenqueue_push(HTokenQueue *q, HToken *tok)
{
  *(q->rear)= tok;
  q->rear= &tok->next;
  ++(q->nextserialnum);
}

/* Only use if htokenqueue_isempty(q) is false! */
inline static HToken *htokenqueue_pop(HTokenQueue *q)
{
  HToken *front= q->front;
  if(!(q->front= front->next))
    q->rear= &q->front;
  ++(q->frontserialnum);
  front->next=NULL;
  return front;
}

/* Increment the serial numbers. This only makes sense if the queue is empty. */
inline static void htokenqueue_serialincr(HTokenQueue *q)
{
  ++(q->nextserialnum);
  ++(q->frontserialnum);
}

/* tp should be htokenText, htokenComment, htokenDoctype or htokenMisc */
static void htokenqueue_push_texttoken(HTokenQueue *q, HTokenType tp, const char *str, unsigned len,
				       HTokenPattNode *matchingpatt)
{
  HToken *tok= malloc(offsetof(HToken,text)+sizeof(TextStr)+len);
  tok->tokentype= tp;
  tok->next= NULL;
  tok->matchingpatt= matchingpatt;
  tok->text.len= len;
  memcpy(tok->text.str,str,len);
  htokenqueue_push(q,tok);
}

/* tp should be htokenScriptText or htokenStyleText */
static void htokenqueue_push_stexttoken(HTokenQueue *q, HTokenType tp, const char *str, unsigned len,
					unsigned short beginlen, unsigned short closelen, unsigned short flags,
					HTokenPattNode *matchingpatt)
{
  HToken *tok= malloc(offsetof(HToken,stext)+sizeof(STextStr)+len);
  tok->tokentype= tp;
  tok->next= NULL;
  tok->matchingpatt= matchingpatt;
  tok->stext.len= len;
  tok->stext.beginlen= beginlen;
  tok->stext.closelen= closelen;
  tok->stext.flags= flags;
  memcpy(tok->stext.str,str,len);
  htokenqueue_push(q,tok);
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

inline  __attribute__((always_inline))
static int popendtagstack(TagStack *s, int endtag)
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

/* Repeat stack function definitions for type TagPStack. */

/* Push a tag index and corresponding pattern info on a stack. */
static void pushtagpstack(TagPStack *s, int idx,
			  PattStateListNode *pattstatelist, PattOptionsListNode *pattoptslist,
			  HToken *htoken, unsigned serialnum,
			  HTokenListNode *parenthtoklist)
{
  TagPStack new=malloc(sizeof(TagPStackNode));
  new->next= *s;
  new->idx= idx;
  new->pattstatelist= pattstatelist;
  new->pattoptslist= pattoptslist;
  new->htoken= htoken;
  new->serialnum= serialnum;
  new->parenthtoklist= parenthtoklist;
  *s= new;
}


/* Make a copy of a token list, leaving out the nodes referring to tokens that are already resolved. */
inline static HTokenListNode *copy_htoken_list(HTokenListNode *tl, unsigned frontserialnum)
{
  HTokenListNode *res;
  HTokenListNode **last= &res;

  while(tl) {
    if(tl->serialnum>=frontserialnum && !tl->htoken->tag.resolved) {
      HTokenListNode *new= malloc(sizeof(HTokenListNode));
      new->next=NULL;
      new->htoken=tl->htoken;
      new->serialnum=tl->serialnum;
      *last=new;
      last= &new->next;
    }
    tl=tl->next;
  }

  return res;
}

/* Activate the patterns on a pattern list. */
inline __attribute__((always_inline))
static void activate_pattern_list(HTokenPattListNode *pl) {
  while(pl) {
    pl->pattn->active=1;
    pl=pl->next;
  }
}

/* Activate the patterns on a pattern state list. */
inline __attribute__((always_inline))
static void activate_pattstate_list(PattStateListNode *pl) {
  while(pl) {
    pl->pattn->active=1;
    pl=pl->next;
  }
}

/* Restore the old pattern state using the info on a pattern state list and free the list nodes. */
inline static void unwind_pattstate_list(PattStateListNode *pl) {
  while(pl) {
    PattStateListNode *next;
    PattOptions *pattopts;
    pl->pattn->active=pl->active;
    if((pattopts= pl->pattopts)) {
      StateRefListNode *p= pl->streflist;
      StateListNode    *q= pattopts->stlist, **last= &pattopts->stlist;
      while(p) {
	StateRefListNode *next;
	if(q && p->serialnum==q->serialnum) {
	  q->lastm= p->lastm;
	  last= &q->next;
	  q=q->next;
	}
	next=p->next;
	free(p);
	p=next;
      }
      /* Free any remaining state nodes that where not on the old list. */
      if(q) {
	*last=NULL;
	do {
	  StateListNode *next= q->next;
	  free(q);
	  q=next;
	}
	while(q);
      }
    }

    next= pl->next;
    free(pl);
    pl=next;
  }
}

inline __attribute__((always_inline))
static void unwind_htoken_list(HTokenListNode *tl, unsigned frontserialnum)
{
  while(tl) {
    HTokenListNode *next;
    if(tl->serialnum>=frontserialnum)
      tl->htoken->tag.resolved=1;
    next=tl->next;
    free(tl);
    tl=next;
  }
}

/* Activate siblings patterns referred by active nodes in PattOptions list and remove inactive nodes.
   All the PattOptions must have the HASSIBLINGPATTERN flag set!
   unwind_pattoptions_list() returns a pointer to the last 'next' field in the resulting list.
*/
static PattOptionsListNode **unwind_pattoptions_list(PattOptionsListNode **pl)
{
  PattOptionsListNode *p= *pl;

  while(p) {
    PattOptionsListNode* next= p->next;
    PattOptions *pattopts= p->pattopts;
    if(p->active) {
      pattopts->sibling->active=1;
      pl= &p->next;
    }
    else {
      StateListNode *stlist= pattopts->stlist;
      if(stlist && stlist->pattoptslistnode==p) {
	pattopts->stlist= stlist->next;
	free(stlist);
      }
      *pl= next;
      free(p);
    }
    p=next;
  }

  return pl;
}

/* Remove the first stack node of the matching tag type and
   update the pattern status info accordingly. In particular,
   the corresponding start tag is marked as resolved.
   poptagpstack() returns a PattOptions list that refers to the activated sibling patterns.
*/
static PattOptionsList poptagpstack(TagPStack *s, int idx, unsigned serialnum)
{
  PattOptionsList retval={NULL,NULL};
  TagPStack p, *q;

  q= s;
  while((p= *q)) {
    if(p->idx==idx) {
      PattOptionsListNode **last= unwind_pattoptions_list(&p->pattoptslist);
      if((retval.first= p->pattoptslist))
	retval.last=last;
      /* Important: call unwind_pattstate_list() after unwind_pattoptions_list(). */
      unwind_pattstate_list(p->pattstatelist);
      if(p->serialnum>=serialnum && p->htoken)
	p->htoken->tag.resolved=1;
      unwind_htoken_list(p->parenthtoklist,serialnum);
      *q= p->next;
      free(p);
      break;
    }
    q= &p->next;
  }

  return retval;
}

inline __attribute__((always_inline))
static void free_pattern_list(HTokenPattListNode *p)
{
  while(p) {
    HTokenPattListNode *next= p->next;
    /* Don't free p->pattn here, should be freed elsewhere. */
    free(p);
    p=next;
  }
}

inline __attribute__((always_inline))
static void free_pattstate_list(PattStateListNode *p)
{
  while(p) {
    PattStateListNode *next;
    StateRefListNode *pr= p->streflist;
    /* Don't free p->pattn and p->pattopts here, should be freed elsewhere. */

    while(pr) {
      StateRefListNode *next= pr->next;
      free(pr);
      pr=next;
    }
    next= p->next;
    free(p);
    p=next;
  }
}

inline __attribute__((always_inline))
static void free_pattoptions_list(PattOptionsListNode *p)
{
  while(p) {
    PattOptionsListNode* next= p->next;
    /* p->pattopts should be freed elsewhere. */
    free(p);
    p=next;
  }
}

inline __attribute__((always_inline))
static void free_htoken_list(HTokenListNode *p)
{
  while(p) {
    HTokenListNode* next= p->next;
    /* p->htoken should be freed elsewhere. */
    free(p);
    p=next;
  }
}

/* Free all the remaining nodes of a TagPStack */
inline static void free_tagpstack(TagPStack s)
{
  while(s) {
    TagPStack next= s->next;
    free_pattstate_list(s->pattstatelist);
    free_pattoptions_list(s->pattoptslist);
    /* Don't free s->htoken , should be freed elsewhere. */
    free_htoken_list(s->parenthtoklist);
    free(s);
    s=next;
  }
}


/* Parse a sequence of wildcard patterns separated by vertical bars '|'.
   The starting point in the string to parse is given by the pointer 'p', the position
   following the last character in the string by the pointer 'end'.
   If p>end this is interpreted as an empty sequence and the return value is NULL.
   The information of each pattern is stored in a newly allocated PattNode struct, and
   appended to a linked list.
   The return value of parse_wildcardpatt_list() is the resulting list.
*/
static PattNode *parse_wildcardpatt_list(const char *p, const char *end)
{
  PattNode *retval= NULL;
  PattNode **lpatt= &retval;

  while(p<=end) {
    PattNode *newpatt;
    const char *bar; char *q;
    size_t len;
    bar= strunescapechr2(p,end,'|');
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

  return retval;
}

#define keywordmatch(str,len,key) (len==strlitlen(key) && !strncasecmp(str,key,strlitlen(key)))

/* Create a HTokenPattNode data structure from parsing a string.
   parse_htoken_pattern returns a pointer to a newly allocated struct..
   If parsing fails, the return value is NULL.
   The pointer argument is updated to point to the position in the
   string just after the last char that was "consumed".
*/
static HTokenPattNode *parse_htoken_pattern(const char **str, unsigned short *seenstag, unsigned short *seendisableattr)
{
  HTokenPattNode *res=NULL;
  const char *s= *str, *p, *cbr, *eq, *cm;

  p = strunescapepbrk(s," \t[]<>/,;");

  if(s<p) {
    size_t len=p-s;
    unsigned short tokentype = keywordmatch(s,len,"text")?
                                 (*seenstag==0?               htokenText:
				  *seenstag==1?               htokenScriptText:
				                              htokenStyleText):
                               keywordmatch(s,len,"comment")? htokenComment:
                                                              htokenTag;

    if(tokentype==htokenTag) {
      /* Try to parse patterns for ordinary HTML element. */
      AttrPattNode **last;
      size_t tagnamesz;
      if(keywordmatch(s,len,"script"))
	*seenstag=1;
      else if(keywordmatch(s,len,"style"))
	*seenstag=2;
      tagnamesz=strunescapelen2(s,p)+1;
      res =  (HTokenPattNode*)malloc(offsetof(HTokenPattNode,tagpattn)+sizeof(TagAttrPattNode)+tagnamesz);
      res->tokentype=tokentype;
      res->active=0;
      res->firstlinedesc=0;
      res->targdesc=0;
      res->hasdesc=0;
      res->target=NULL;
      res->pattopts=NULL;
      res->next = NULL;
      res->tagpattn.attrpatts = NULL;
      res->tagpattn.idx = 0;
      strunescapecpy2(res->tagpattn.tagname,s,p);
      last = &res->tagpattn.attrpatts;
      if(*p=='[') {
	++p; /* Skip '[' */
	cbr=strunescapechr(p,']');
	for(;;) {
	  unsigned short disableattr=0;
	  while(*p && isspace(*p)) ++p;  /* Skip blanks. */
	  if(*p=='-') {
	    *seendisableattr=disableattr=1;
	    ++p;
	  }
	  cm=strunescapechr2(p,cbr,',');
	  eq=strunescapechr2(p,cm,'=');
	  if(eq>p) {
	    size_t attrnamesz=strunescapelen2(p,eq)+1;
	    AttrPattNode *new = (AttrPattNode*)malloc(sizeof(AttrPattNode)+attrnamesz);
	    new->next=NULL;
	    new->valpatt= parse_wildcardpatt_list(eq+1,cm);
	    new->disableattr= disableattr;
	    strunescapecpy2(new->attrname,p,eq);
	    *last = new;
	    last = &new->next;
	  }
	  if(cm>=cbr) break;
	  p=cm+1; /* Skip past ',' */
	}

	p=cbr;
	if(*p) ++p; /* Skip ']' */
      }
    }
    else {
      /* Parse a pattern for text or comment. */
      res =  (HTokenPattNode*)malloc(offsetof(HTokenPattNode,txtpatt)+sizeof(PattNode*));
      res->tokentype=tokentype;
      res->active=0;
      res->firstlinedesc=0;
      res->targdesc=0;
      res->hasdesc=0;
      res->target=NULL;
      res->pattopts=NULL;
      res->next = NULL;
      res->txtpatt = NULL;
      if(*p=='[') {
	++p; /* Skip '[' */
	cbr=strunescapechr(p,']');
	res->txtpatt= parse_wildcardpatt_list(p,cbr);
	p=cbr;
	if(*p) ++p; /* Skip ']' */
      }
    }
  }

  *str=p;
  return res;
}


struct modifyoptions {
  unsigned short optionflags;
  const char *stylerepl;
};

/* Parse of string containing modify options and return the result in a struct modifyoptions. */
static struct modifyoptions parse_tag_modify_options(const char *str)
{
  struct modifyoptions res= {0,NULL};
  unsigned short matchedoption=0;
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
      res.stylerepl = "display:none !important;";
    else if(keywordmatch(s,len,"visibility-hidden"))
      res.stylerepl = "visibility:hidden !important;";
    else if(keywordmatch(s,len,"no-style"))
      res.stylerepl = "";
    else if(keywordmatch(s,len,"disable-tag"))
      res.optionflags |= DISABLEMATCHINGTAG;
    else if(keywordmatch(s,len,"disable-script"))
      res.optionflags |= DISABLEINNERSCRIPT;
    else if(keywordmatch(s,len,"disable-following-script"))
      res.optionflags |= DISABLEFOLLOWINGSCRIPT;
    else if(keywordmatch(s,len,"disable-all-following-scripts"))
      res.optionflags |= DISABLEALLFOLLOWINGSCRIPTS;
    else if(keywordmatch(s,len,"reset-following-script"))
      res.optionflags |= RESETFOLLOWINGSCRIPT;
    else if(keywordmatch(s,len,"reset-all-following-scripts"))
      res.optionflags |= RESETALLFOLLOWINGSCRIPTS;
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
  Sort a list containing HTokenPattNodes by tag name using a merge sort algorithm.
  All the HTokenPattNodes must be of type htokenTag!
*/
static void sort_htoken_patt_list(HTokenPattNode **l)
{
  if(*l) {
    unsigned int m;

    for(m=1;; m *= 2) {
      unsigned int nmerge=0,i,j;
      HTokenPattNode *p,*q=*l,**s= l, **t;

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
	  if(strcasecmp(p->tagpattn.tagname,q->tagpattn.tagname) <= 0) {
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


/* Only use if opts->optionflags does not have the HASSIBLINGPATTERN bit set. */
static PattOptions *make_pattoptions(struct modifyoptions *opts, unsigned short disableattr)
{
  PattOptions *retval= malloc(sizeof(PattOptions));
  retval->optionflags= opts->optionflags;
  if(disableattr) retval->optionflags|=DISABLEMATCHINGATTRIBUTES;
  retval->stylerepl= opts->stylerepl;
  retval->stlist= NULL;
  return retval;
}


static void make_tag_pattern_info(PattInfo *pattinfo, ConfigItem confitem,const URL *Url)
{
  HTokenPattNode *tapl= NULL;
  HTokenPattNode **lasttap = &tapl;
  HTokenPattNode **lasttxtp, **lastscrtxtp, **laststyltxtp, **lastcomp;

  pattinfo->taparr= NULL;
  pattinfo->txtpl= NULL;
  pattinfo->scrtxtpl= NULL;
  pattinfo->styltxtpl= NULL;
  pattinfo->compl= NULL;
  pattinfo->activechildren= NULL;
  pattinfo->firstlinetarglist= NULL;
  pattinfo->lastfirstlinetarg= &pattinfo->firstlinetarglist;
  pattinfo->activesiblings= NULL;
  pattinfo->lastactivesibs= &pattinfo->activesiblings;
  pattinfo->stacktag= NULL;
  pattinfo->ntag= 0;
  pattinfo->stateserialnum= 0;
  lasttxtp= &pattinfo->txtpl;
  lastscrtxtp= &pattinfo->scrtxtpl;
  laststyltxtp= &pattinfo->styltxtpl;
  lastcomp= &pattinfo->compl;

  if(confitem) {
    int i;
    for(i=0;i<confitem->nentries;++i)
      if(!confitem->url[i] || MatchUrlSpecification(confitem->url[i],Url)) {
	struct modifyoptions optval= parse_tag_modify_options(confitem->val[i].string);
	const char *s= confitem->key[i].string;
	HTokenPattNode *prevpatt=NULL, *target=NULL;
	PattOptions *prevpattopts=NULL;
	unsigned short seenstag=0, seendisableattr, disablematchingattrs=0;
	unsigned short hasdesc=2;
	/* Loop while there are tag-attribute patterns to parse. */
	for(;;) {
	  HTokenPattNode *pattval;
	  while(*s && isspace(*s)) ++s;  /* Skip blanks. */
	  if(!*s)
	    break;
	  seendisableattr=0;
	  pattval = parse_htoken_pattern(&s, &seenstag, &seendisableattr);
	  if(!pattval)
	    break;
	  if(prevpatt) {
	    /* Make the previous HTokenPattNode point to this one as a descendant. */
	    prevpatt->hasdesc=hasdesc;
	    prevpatt->desc=pattval;
	    if(prevpatt==target || prevpatt->targdesc) {
	      pattval->targdesc= prevpatt->targdesc+1;
	      pattval->pattopts=target->pattopts;
	    }
	    if(hasdesc==1 && (prevpatt==target || prevpatt->firstlinedesc))
	      pattval->firstlinedesc=1;
	  }
	  else if(prevpattopts) {
	    /* Make the PattOptions of the previous pattern sequence point to this one as a sibling. */
	    prevpattopts->optionflags= HASSIBLINGPATTERN;
	    prevpattopts->sibling= pattval;
	  }
	  else
	    pattval->active=1;  /* Mark first pattern in sequence as active. */

	  /* Collect all the patterns into lists, one list for each type. */
	  switch(pattval->tokentype) {
	  case htokenTag:
	    *lasttap=pattval;
	    lasttap= &pattval->next;
	    break;
	  case htokenText:
	    *lasttxtp= pattval;
	    lasttxtp= &pattval->next;
	    break;
	  case htokenScriptText:
	    *lastscrtxtp= pattval;
	    lastscrtxtp= &pattval->next;
	    break;
	  case htokenStyleText:
	    *laststyltxtp= pattval;
	    laststyltxtp= &pattval->next;
	    break;
	  case htokenComment:
	    *lastcomp= pattval;
	    lastcomp= &pattval->next;
	    break;
	  default:
	    PrintMessage(Fatal,"Error in %s line %d: unhandled pattern type.",__FILE__,__LINE__);
	  }
	  prevpatt=pattval; disablematchingattrs=seendisableattr;
	  hasdesc=2;
	  while(*s && isspace(*s)) ++s;  /* Skip blanks. */
	  if(*s == '/') {
	    if(!target) {
	      target=prevpatt;
	      prevpatt->pattopts= make_pattoptions(&optval,disablematchingattrs);
	    }
	    ++s;  /* Skip '/' */
	  }
	  else if(*s == ',' || *s == ';') {
	    /* New patt data sequence, close off the previous one. */
	    prevpatt->hasdesc=0;
	    if(target)
	      prevpatt->target=target;
	    else {
	      /* The last tag pattern is itself the target */
	      prevpatt->target=target=prevpatt;
	      prevpatt->pattopts=make_pattoptions(&optval,disablematchingattrs);
	    }
	    if(*s == ';')
	      prevpattopts= target->pattopts;
	    else
	      prevpattopts= NULL;
	    prevpatt=NULL;
	    target=NULL;
	    seenstag=0; disablematchingattrs=0;
	    ++s;  /* Skip ',' or ';' */
	  }
	  if(*s == '>') {
	    hasdesc=1;
	    ++s; /* Skip '>' */
	  }
	}
	if(prevpatt) {
	  /* Close off patt data sequence. */
	  prevpatt->hasdesc=0;
	  if(target)
	    prevpatt->target=target;
	  else {
	    prevpatt->target=prevpatt;
	    prevpatt->pattopts=make_pattoptions(&optval,disablematchingattrs);
	  }
	}
      }
  }

  if(tapl) {
    HTokenPattNode *p,*q;
    HTokenPattNode **taparr;
    unsigned char *stacktag;
    int ntag, i;
    sort_htoken_patt_list(&tapl);

    /* Count number of unique tags. */
    ntag=1;
    for(p=tapl,q=p->next; q; q=q->next) {
      if(strcasecmp(p->tagpattn.tagname,q->tagpattn.tagname))
	++ntag;
      p=q;
    }

    /* Create tag-name lookup array with one entry for
       each unique tag name. */
    pattinfo->taparr = taparr = (HTokenPattNode **)calloc(ntag,sizeof(HTokenPattNode *));
    pattinfo->stacktag = stacktag = (unsigned char*)calloc(ntag,sizeof(unsigned char));
    pattinfo->ntag= ntag;
    i=0; q=tapl;
    taparr[i]=q;
    for(;;) {
      if(q->hasdesc || (q->pattopts && (q->pattopts->optionflags&HASSIBLINGPATTERN)))
	stacktag[i]|=TAGPSTACK;
      if(q->pattopts && (q->pattopts->optionflags&DISABLEMASK))
	stacktag[i]|=TAGDSTACK;
      q->tagpattn.idx = i;
      p=q;
      q=q->next;
      if(!q) break;

      if(strcasecmp(p->tagpattn.tagname,q->tagpattn.tagname)) {
	p->next=NULL;  /* Close off the list segment for the previous tag. */
	++i;
	taparr[i]=q;
      }
    }
  }
}


static void handle_mismatching_firstline_sequence(HTokenPattNode *pattn);


/* De-activate all the patterns referred by the activechildren and activesiblings lists and free the lists.
   Mark a possible parent token as resolved.
   Then set the activechildren list to fchildlist and activate the patterns referred by the new list.
*/
static HTokenListNode *reset_fchild_sibling_patt_lists(PattInfo *pattinfo, int tagidx, HTokenPattListNode *fchildlist,
						       unsigned maxfirstlinehasdesc, HToken *htoken, HTokenQueue *tokq)
{
  HTokenListNode *retval=NULL;

  {
    PattOptionsListNode *p= pattinfo->activesiblings;

    pattinfo->activesiblings= NULL;
    pattinfo->lastactivesibs= &pattinfo->activesiblings;

    while(p) {
      PattOptionsListNode *next= p->next;
      p->pattopts->sibling->active=0;
      free(p);
      p=next;
    }
  }

  /* Important: first de-activate the patterns on the old list, then activate the ones on the new list,
     because the old list and the new list may overlap! */
  {
    HTokenPattListNode *p= pattinfo->activechildren, *next;
    while(p) {
      HTokenPattNode *pattn= p->pattn;
      if((pattn->tokentype!=htokenTag || tagidx<0 || pattn->tagpattn.idx!=tagidx) && pattn->firstlinedesc)
	handle_mismatching_firstline_sequence(pattn);
      pattn->active=0;
      next= p->next;
      free(p);
      p=next;
    }
  }
  if(maxfirstlinehasdesc==0) {
    /* Mark tokens in firstlinetarglist as resolved and throw away the list. */
    unwind_htoken_list(pattinfo->firstlinetarglist,tokq->frontserialnum);
    pattinfo->firstlinetarglist=NULL;
    pattinfo->lastfirstlinetarg= &pattinfo->firstlinetarglist;
  }
  else if(maxfirstlinehasdesc==1) {
    /* Keep firstlinetarglist and make a copy to put on pattern stack. */
    retval= copy_htoken_list(pattinfo->firstlinetarglist,tokq->frontserialnum);
  }
  else /* maxfirstlinehasdesc > 1 */ {
    /* First-line descendancy broken. Return firstlinetarglist to put on pattern stack. */
    retval= pattinfo->firstlinetarglist;
    pattinfo->firstlinetarglist=NULL;
    pattinfo->lastfirstlinetarg= &pattinfo->firstlinetarglist;
  }
  pattinfo->activechildren= fchildlist;
  if(htoken) {
    HTokenListNode *new= malloc(sizeof(HTokenListNode));
    new->next=NULL;
    new->htoken=htoken;
    new->serialnum= tokq->nextserialnum;
    *(pattinfo->lastfirstlinetarg)= new;
    pattinfo->lastfirstlinetarg = &new->next;
  }
  activate_pattern_list(fchildlist);

  return retval;
}


inline __attribute__((always_inline))
static void append_sibling_patt_list(PattInfo *pattinfo, PattOptionsListNode *slist, PattOptionsListNode **last)
{
  if(slist) {
    *(pattinfo->lastactivesibs)= slist;
    pattinfo->lastactivesibs= last;
  }
}


/* Note the range of i is not checked here, so use with care!
   Also, p->stacktag may be NULL! */
inline static unsigned stackflags(PattInfo *p, int i)
{
  return p->stacktag[i];
}


static int tag_match_index(const char* tag, PattInfo *l)
{
  HTokenPattNode **taparr= l->taparr;

  if(taparr) {
    if(*tag == '/')
      ++tag;

    /* Do a binary search. */
    int i=0, j=l->ntag;

    while(i<j) {
      int k=(i+j)/2;
      int cmp=strcasecmp(tag,taparr[k]->tagpattn.tagname);
      if(cmp<0)
	j=k;
      else if(cmp>0)
	i=k+1;
      else
	return k;
    }
  }

  return -1;
}

inline static void free_pattoptions(PattOptions *pattopts)
{
  if(pattopts) {
    /* pattopts->stylerepl points to configuration data, do not free here. */
    /* pattopts->sibling should be freed elsewhere. */
    StateListNode *stl= pattopts->stlist;
    while(stl) {
      StateListNode *next = stl->next;
      /* stl->numtok.htoken or stl->pattoptslistnode should be freed elsewhere. */
      free(stl);
      stl=next;
    }
    free(pattopts);
  }
}

static void free_pattlist(PattNode *p)
{
  while(p) {
    PattNode* next= p->next;
    free(p);
    p=next;
  }
}

static void free_htokenpatt_list(HTokenPattNode *p)
{
  while(p) {
    HTokenPattNode *next;
    /* Descendant patterns are freed elsewhere. */
    if(p->targdesc==0)  /* If p->targdesc>0, p->pattopts points to the same PattOptions node as the target and should already have been freed! */
      free_pattoptions(p->pattopts);
    if(p->tokentype==htokenTag) {
      /* Tag-attribute pattern */
      AttrPattNode* a;
      /* free(p->tagpattn.tagname); */
      a= p->tagpattn.attrpatts;
      while(a) {
	AttrPattNode* nxt= a->next;
	/* free(a->attrname); */
	free_pattlist(a->valpatt);
	free(a);
	a=nxt;
      }
    }
    else {
      /* Text pattern */
      free_pattlist(p->txtpatt);
    }
    next= p->next;
    free(p);
    p=next;
  }
}

static void free_pattern_info(PattInfo *l)
{
  HTokenPattNode **taparr= l->taparr;

  if(taparr) {
    int i, n=l->ntag;
    for(i=0; i<n; ++i)
      free_htokenpatt_list(taparr[i]);
    free(taparr);
  }

  free_htokenpatt_list(l->txtpl);
  free_htokenpatt_list(l->scrtxtpl);
  free_htokenpatt_list(l->styltxtpl);
  free_htokenpatt_list(l->compl);
  free_pattern_list(l->activechildren);
  free_htoken_list(l->firstlinetarglist);
  free_pattoptions_list(l->activesiblings);

  if(l->stacktag) free(l->stacktag);
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
 static Bool_t first=1;

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

inline __attribute__((always_inline))
static void yy_rawout(const char *text)
{
  yy_write(text,strlen(text));
}

#if 0
/*+ A function to output the data if valid to do so. +*/
inline __attribute__((always_inline))
static void YY_WRITE(const char *text, size_t len)
{
  if(!disable_output && len>0) yy_write(text,len);
}

inline __attribute__((always_inline))
static void YY_OUTPUT(const char *text)
{
  if(!disable_output && *(text)) yy_rawout(text);
}
#endif

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
      yy_write(buffer,n);

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

  Tag *tag The tag information.

  int disable_dontget_anchors The option to disable links to URLs that are not got.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static disabled_t handle_a_tag(Tag *tag,int disable_dontget_anchors,int disable_script)
{
 AttrNode *a;
 disabled_t retval=0;
 char *href;

 for(a=tag->attrl; a; a=a->next)
   if(a->type==att_href && (href=a->val)) {
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

 if(retval) tag->disable= retval;
 output_tag(tag,NULL);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the iframe tag and check if
  the src attribute matches the DontGet list or contains
  a script.
  Then output the original or disabled tag.

  disabled_t handle_iframe_tag Returns the type of disablement.

  Tag *tag The tag information.

  int disable_dontget_iframes The option to disable iframes to URLs that are not got.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static disabled_t handle_iframe_tag(Tag *tag,int disable_dontget_iframes,int disable_script)
{
 AttrNode *a;
 disabled_t retval=0;
 char *src;

 for(a=tag->attrl; a; a=a->next)
   if(a->type==att_src && (src=a->val)) {
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

 if(retval) tag->disable= retval;
 output_tag(tag,NULL);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the script tag and check if
  the src attribute matches the DontGet list.
  Then output the original or disabled tag.

  disabled_t handle_script_tag Returns nonzero if the script was disabled.

  Tag *tag The tag information.
  ++++++++++++++++++++++++++++++++++++++*/

static disabled_t handle_script_tag(Tag *tag, disabled_t is_disabled_script)
{
 AttrNode *a;
 disabled_t retval=is_disabled_script;
 char *src;

 if(!retval)
   for(a=tag->attrl; a; a=a->next)
     if(a->type==att_src && (src=a->val)) {
       URL *linkUrl=LinkURL(baseUrl,src);

       if(ConfigBooleanMatchURL(DontGet,linkUrl))
	 retval=disabled_dontget_script;

       FreeURL(linkUrl);
       if(retval)
	 break;
     }

 /* Output the original or modified tag. */

 if(retval) tag->disable= retval;
 output_tag(tag,NULL);

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
 AttrNode *a;
 disabled_t retval=0;

 if(attr_type>=0)
   {
     if(replace_dontget || disable_script) {
       char *src;
       for(a=tag->attrl; a; a=a->next) {
	 if(a->type==attr_type && (src=a->val)) {
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
       int seen_src=0,width=1024,height=1024;
       char *attrval;

       for(a=tag->attrl; a; a=a->next) {
	 if(a->type==attr_type && a->val)
	   seen_src=1;
	 if(a->type==att_width) {
	   if((attrval=a->val) && *attrval && isdigit(*attrval))
	     width=atoi(attrval);
	 }
	 else if(a->type==att_height) {
	   if((attrval=a->val) && *attrval && isdigit(*attrval))
	     height=atoi(attrval);
	 }
       }

       if(seen_src && width<=1 && height<=1)
	 retval=disabled_webbug_image;
     }
   }

 /* Modify the src attribute (if required). */

 if(retval==disabled_dontget_image)
   {
    const char *prefix=NULL;
    if(dontget_replacement)
      prefix= "!-- WWWOFFLE (replace-dontget-images) - ";
    else
      tag->disable= retval;

    output_tag(tag,prefix);

    if(!dontget_replacement)
      return retval;

    for(a=tag->attrl; a; a=a->next)
      if(a->type==attr_type) {
	if(a->val) free(a->val);
	a->val=strdup(dontget_replacement);
      }
   }
 else if(retval==disabled_webbug_image)
   {
    const char *prefix=NULL;
    if(webbug_replacement)
      prefix= "!-- WWWOFFLE (replace-webbug-images) - ";
    else
      tag->disable= retval;

    output_tag(tag,prefix);

    if(!webbug_replacement)
      return retval;

    for(a=tag->attrl; a; a=a->next)
      if(a->type==attr_type) {
	if(a->val) free(a->val);
	a->val=strdup(webbug_replacement);
      }
   }
 else if(retval==disabled_script) {
    tag->disable= retval;
    output_tag(tag,NULL);
    return retval;
 }

 /* Blank the alt & title attributes or add an empty alt (if required). */

 if(retval)
   {
    unsigned short seen_alt=0;

    for(a=tag->attrl; a; a=a->next)
       if(!strcasecmp(a->key,"alt") || !strcasecmp(a->key,"title"))
         {
          if(a->val)
             free(a->val);

          a->val=strdup("");
          a->quote='"';

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
 AttrNode *a;

 /* Check for images. */

 if(replace_dontget || replace_webbug || disable_script)
   {
    for(a=tag->attrl; a; a=a->next)
      if((a->type==att_codetype || a->type==att_type) &&
	 a->val && !strcasecmp_litbeg(a->val,"image"))
	 {
	   /* found an image */
	   return output_img_or_object_tag(tag,att_data,replace_dontget,dontget_replacement,replace_webbug,webbug_replacement,disable_script);
	 }
   }

 /* Check for Java */

 if(disable_applet)
   {
    for(a=tag->attrl; a; a=a->next)
      if((a->type==att_codetype && a->val && !strcasecmp_litbeg(a->val,"application/java")) ||
	 (a->type==att_classid && a->val && !strcasecmp_litbeg(a->val,"java:")))
	{
	  /* found Java applet */
	  tag->disable=disabled_applet;
	  output_tag(tag,NULL);
	  return disabled_applet;
	}
   }

 /* Check for Flash */

 if(disable_flash)
   {
    size_t val_len;
    for(a=tag->attrl; a; a=a->next)
      if(((a->type==att_codetype || a->type==att_type) &&
          a->val && !strcasecmp_litbeg(a->val,"application/x-shockwave-flash")) ||
	 (a->type==att_classid && a->val &&
	  !strcasecmp_litbeg(a->val,"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000")) ||
	 (a->type==att_src && a->val && (val_len=strlen(a->val))>=4 &&
	  !strcasecmp(a->val+val_len-4,".swf")))
	{
	  /* found Flash */
	  tag->disable=disabled_flash;
	  output_tag(tag,NULL);
	  return disabled_flash;
	}
   }

 /* Check for inline HTML (text) object */

 if(disable_dontget_iframes || disable_script)
   {
    unsigned short is_inline=0;
    for(a=tag->attrl; a; a=a->next)
      if((a->type==att_codetype || a->type==att_type) &&
	 a->val && !strcasecmp_litbeg(a->val,"text"))
	{is_inline=1;break;}

    if(is_inline) {
      disabled_t disabled=0;
      char *data;

      for(a=tag->attrl; a; a=a->next)
	if(a->type==att_data && (data=a->val)) {
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

      if(disabled) tag->disable=disabled;
      output_tag(tag,NULL);
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

  Tag *tag The tag information.

  int disable_style Set to true if stylesheets are disabled.

  int disable_script Set to true if scripts are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_link_tag(Tag *tag,int disable_style,int disable_script)
{
 AttrNode *a;
 disabled_t disabled=0;
 char *rel;

 for(a=tag->attrl; a; a=a->next)
   if(a->type==att_rel && (rel=a->val)) {
       if(disable_style && !strcasecmp_litbeg(rel,"Stylesheet"))
	 {disabled=disabled_style;break;}
       if(disable_script && !strcasecmp_litbeg(rel,"javascript:"))
	 {disabled=disabled_script;break;}
   }

 /* Output the original or modified tag. */

 if(disabled) tag->disable=disabled;
 output_tag(tag,NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the style tag and check if
  the type attribute contains the string "javascript".
  Then output the original or disabled tag.

  disabled_t handle_style_script_tag Returns nonzero if a script was detected and disabled.

  Tag *tag The tag information.

  See "WhiteHat Security Advisory [Number: WH-08152001-1]" for the details of this.

  The HTML <style type="application/x-javascript"> or <style type="text/javascript">
  can introduce Javascript that WWWOFFLE would not otherwise have blocked.
  ++++++++++++++++++++++++++++++++++++++*/

static disabled_t handle_style_script_tag(Tag *tag)
{
 AttrNode *a;
 disabled_t retval=0;
 char *type;

 for(a=tag->attrl; a; a=a->next)
   if(a->type==att_type && (type=a->val) &&
      strcasestr(type,"javascript"))
     {
       tag->disable=retval=disabled_script;
       break;
     }

 /* Output the original or modified tag. */

 output_tag(tag,NULL);

 return(retval);
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
 AttrNode *a;
 int is_meta_http_equiv_refresh=0;
 int is_meta_refresh_self=0;
 AttrNode *meta_refresh_content=NULL;
 char *meta_refresh=NULL;
 int is_meta_http_equiv_set_cookie=0;
 int is_meta_http_equiv_content_type=0;

 /* Check for interesting meta tags */

 for(a=tag->attrl; a; a=a->next)
   if(a->type==att_http_equiv && a->val) {
     if(!strcasecmp_litbeg(a->val,"Refresh"))
       is_meta_http_equiv_refresh=1;
     else if(!strcasecmp_litbeg(a->val,"Set-Cookie"))
       is_meta_http_equiv_set_cookie=1;
     else if(!strcasecmp_litbeg(a->val,"Content-Type"))
       is_meta_http_equiv_content_type=1;
   }

 /* Handle if they are refresh ones. */

 if(is_meta_http_equiv_refresh && (disable_meta_refresh_self || disable_meta_refresh))
   {
    for(a=tag->attrl; a; a=a->next)
       if(a->type==att_content && a->val && a->val[0])
         {
	   char *p=a->val;

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
	   meta_refresh_content=a;
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
    for(a=tag->attrl; a; a=a->next)
       if(a->type==att_content && a->val && a->val[0])
         {
          char* charset=extract_charset(a->val);

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
     if(replacement_meta_refresh_time<0 || (replacement_meta_refresh_time==0 && (is_meta_refresh_self || !meta_refresh)) || !meta_refresh_content)
       tag->disable=disabled_meta_refresh;
     else
       {
	 free(meta_refresh_content->val);
	 meta_refresh_content->val= (meta_refresh?
				     x_asprintf("%d;URL=%s",replacement_meta_refresh_time,meta_refresh):
				     x_asprintf("%d",replacement_meta_refresh_time));
       }
   }
 else if(is_meta_refresh_self && disable_meta_refresh_self)
   {
     if(replacement_meta_refresh_time<=0 || !meta_refresh_content)
       tag->disable=disabled_meta_refresh_self;
     else
       {
	 free(meta_refresh_content->val);
	 meta_refresh_content->val= (meta_refresh?
				     x_asprintf("%d;URL=%s",replacement_meta_refresh_time,meta_refresh):
				     x_asprintf("%d",replacement_meta_refresh_time));
       }
   }
 else if(is_meta_http_equiv_set_cookie && disable_meta_set_cookie)
   tag->disable=disabled_meta_set_cookie;

 if(meta_refresh)
   free(meta_refresh);

 if(!(disable_output&DISABLE_META)) output_tag(tag,NULL);
}


static int get_url_cached_type(const char *url)
{
  if(url) {
    if(*url) {
      URL *linkUrl=LinkURL(baseUrl,url);
      int retval;

      if(!IsProtocolHandled(linkUrl))
	retval=0;
      else if(ExistsWebpageSpoolFile(linkUrl,0) || IsLocalNetHost(linkUrl->host))
	retval=1;
      else if(ExistsOutgoingSpoolFile(linkUrl))
	retval=2;
      else {
	URL *aliasUrl=GetAliasURL(linkUrl);

	if(!aliasUrl)
	  retval=-1;
	else {
	  if(!IsProtocolHandled(aliasUrl))
	    retval=0;
	  else if(ExistsWebpageSpoolFile(aliasUrl,0) || IsLocalNetHost(aliasUrl->host))
	    retval=1;
	  else if(ExistsOutgoingSpoolFile(aliasUrl))
	    retval=2;
	  else
	    retval=-1;

	  FreeURL(aliasUrl);
	}
      }

      FreeURL(linkUrl);
      return retval;
    }

    return 1;
  }

  return 0;
}


inline static int tag_get_url_cached_type(Tag *tag)
{
  int retval=0;
  AttrNode *a;

  for(a= tag->attrl; a; a=a->next) {
    if(a->type==att_href)
      retval= get_url_cached_type(a->val);
  }

  return retval;
}

inline static void tag_setbase_url(Tag *tag)
{
  AttrNode *a;

  for(a= tag->attrl; a; a=a->next) {
    if(a->type==att_href)
      setbase_url(a->val);
  }
}


static void handle_last_targdesc_match(HTokenPattNode *target, unsigned targdesc, unsigned frontserialnum);


struct activepattlists {
  PattStateListNode *pattstatelist;
  HTokenPattListNode *fchildlist;
  PattOptionsListNode *siblist;
  unsigned short maxtarghasdesc;
  unsigned short maxfirstlinehasdesc;
};

/* match_tag_pattern_info() returns a list of descendant patterns that have been made active during the matching process
   and a list of sibling patterns (to be more precise a list of PattOptions that refer to the sibling patterns)
   that should be made active after the tag has been popped off the pattern stack (tagpstack).
   Only use match_tag_pattern_info() if htok->tokentype equals htokenTag!
*/
static struct activepattlists match_tag_pattern_info(HToken *htok, PattInfo *pattinf, HTokenQueue *tokq)
{
  struct activepattlists pattlists= {NULL,NULL,NULL,0,0};
  unsigned short resolved=1;
  Tag *tag= &htok->tag;

  if(tag->idx>=0) {
    HTokenPattNode *p;
    PattStateListNode **lastpattstate= &pattlists.pattstatelist;
    HTokenPattListNode **lastactfchild= &pattlists.fchildlist;
    PattOptionsListNode **lastactsib= &pattlists.siblist;
    unsigned targdesc;
    for(p=pattinf->taparr[tag->idx]; p; p=p->next) {
      /* Note: p->tokentype should equal htokenTag and tag->tagname should match p->tagname. */
      AttrNode *a;
      AttrPattNode *ap;

      if(!p->active || (htok->matchingpatt && !p->hasdesc && p->target==p &&
			!(p->pattopts->optionflags&HASSIBLINGPATTERN)))
	continue; /* Skip pattern matching if nothing to be done anyway. */
      for(ap=p->tagpattn.attrpatts; ap; ap=ap->next) {
	for(a=tag->attrl; a; a=a->next) {
	  if(!strcasecmp(a->key,ap->attrname)) {
	    const char *attrval=a->val, *endptr;
	    size_t len;
	    if(attrval) {
	      if(ap->valpatt) {
		PattNode *q;
		while(*attrval && isspace(*attrval)) ++attrval;  /* Skip blanks. */
		endptr=strchrnul(attrval,0);
		while(--endptr>=attrval && isspace(*endptr));  /* Trim trailing blanks. */
		++endptr;
		len= endptr-attrval;
		/* Try each pattern in the list to see if one matches. */
		q=ap->valpatt;
		do {
		  if(strsegmentscasematch(attrval,len,q->patt,q->len))
		    goto nextattrpatt;
		  q=q->next;
		} while(q);
	      }
	    }
	    else if(!ap->valpatt)
	      goto nextattrpatt;
	  }
	}
	goto mismatch;
      nextattrpatt:;
      }

      /* The tag matched and all the patterns in attribute pattern list matched,
	 process the tag. */
      targdesc= p->targdesc;

      if(p->hasdesc) {
	if(tag->ending==1) {
	  HTokenPattNode *desc= p->desc;
	  if(desc) {
	    PattOptions *pattopts= p->pattopts;

	    if(p->firstlinedesc && p->hasdesc > pattlists.maxfirstlinehasdesc)
	      pattlists.maxfirstlinehasdesc= p->hasdesc;

	    /* Save pattern state information on the appropiate lists. */
	    if(p->hasdesc==1) {
	      /* Only applies to first children. */
	      HTokenPattListNode *new= malloc(sizeof(HTokenPattListNode));
	      new->next=NULL;
	      new->pattn=desc;
	      *lastactfchild=new;
	      lastactfchild = &new->next;
	    }
	    else {  /* general descendant */
	      PattStateListNode *new= malloc(sizeof(PattStateListNode));
	      new->next=NULL;
	      new->pattn=desc;
	      new->pattopts= pattopts;
	      new->streflist= NULL;
	      new->active=desc->active;
	      *lastpattstate=new;
	      lastpattstate = &new->next;

	      if(pattopts) {
		/* Create state reference list. */
		StateListNode *stl;
		StateRefListNode **lastref= &new->streflist;

		for(stl= pattopts->stlist; stl; stl=stl->next) {
		  StateRefListNode *new= malloc(sizeof(StateRefListNode));
		  new->next=NULL;
		  new->serialnum=stl->serialnum;
		  new->lastm=stl->lastm;
		  *lastref=new;
		  lastref= &new->next;
		}
	      }
	    }

	    if(targdesc==0) {
	      if(pattopts) {
		if(!(pattopts->optionflags&HASSIBLINGPATTERN)) {
		  /* This pattern is a target with a descendant pattern. */
		  if(p->hasdesc > pattlists.maxtarghasdesc) pattlists.maxtarghasdesc= p->hasdesc;

		  if(!htok->matchingpatt) {
		    StateListNode *stl, **last;
		    /* This token is an unresolved target, add it
		       to the list of (unresolved) targets in the pattopts node. */
		    resolved=0;
		    stl= pattopts->stlist; last=&pattopts->stlist;
		    while(stl) {
		      StateListNode *next= stl->next;
		      /* Remove members of the list that have been resolved in the meantime. */
		      if(stl->numtok.serialnum<tokq->frontserialnum || stl->numtok.htoken->tag.resolved) {
			*last=next;
			free(stl);
		      }
		      else
			last=&stl->next;
		      stl=next;
		    }
		    stl=(StateListNode *)malloc(sizeof(StateListNode));
		    stl->next=NULL;
		    stl->serialnum= pattinf->stateserialnum++;
		    stl->lastm= targdesc;
		    stl->numtok.htoken= htok;
		    stl->numtok.serialnum= tokq->nextserialnum;
		    *last=stl;
		  }
		}
		else if(pattopts->sibling) {
		  /* Extend list of sibling patterns. */
		  PattOptionsListNode *new= malloc(sizeof(PattOptionsListNode));
		  StateListNode *newst= malloc(sizeof(StateListNode));
		  new->next=NULL;
		  new->pattopts=pattopts;
		  new->active=0; /* Now yet active, because descendant patterns still need to be matched. */
		  *lastactsib=new;
		  lastactsib = &new->next;

		  newst->next= pattopts->stlist;
		  newst->serialnum= pattinf->stateserialnum++;
		  newst->lastm= targdesc;
		  newst->pattoptslistnode= new;
		  pattopts->stlist= newst;
		}
		else
		  PrintMessage(Warning,"Error in %s line %d: invalid pattern sequence: expected sibling pattern."
			       " Coding error in make_tag_pattern_info()?",__FILE__,__LINE__);
	      }
	    }
	    else /* targdesc!=0 */ {
	      /* Update states of references to targets. */
	      unsigned prev= targdesc-1;
	      if(pattopts) {
		StateListNode *stl;
		for(stl= pattopts->stlist; stl; stl=stl->next) {
		  if(stl->lastm==prev)
		    stl->lastm=targdesc;
		}
	      }
	      /* else PrintMessage(Warning,"pattopts missing"); */
	    }
	  }
	  else
	    /* p->last is false but there is no descendant pattern.
	       Faulty pattern sequence, error in the pattern parsing algorithm? */
	    PrintMessage(Warning,"Error in %s line %d: invalid pattern sequence: expected descendant pattern."
				 " Coding error in make_tag_pattern_info()?",__FILE__,__LINE__);
	}
      }
      else /* p->hasdesc==0 */ {
	HTokenPattNode *target= p->target;
	if(target) {
	  if(target==p) {
	    PattOptions *pattopts= target->pattopts;
	    if(!(pattopts->optionflags&HASSIBLINGPATTERN)) {
	      /* Direct match */
	      /* if(!htok->matchingpatt) */ htok->matchingpatt= p;
	      resolved=1;
	    }
	    else if(pattopts->sibling) {
	      /* Extend list of sibling patterns. */
	      PattOptionsListNode *new= malloc(sizeof(PattOptionsListNode));
	      new->next=NULL;
	      new->pattopts=pattopts;
	      new->active=1;
	      *lastactsib=new;
	      lastactsib = &new->next;
	    }
	    else
	      PrintMessage(Warning,"Error in %s line %d: invalid pattern sequence: expected sibling pattern."
			   " Coding error in make_tag_pattern_info()?",__FILE__,__LINE__);
	  }
	  else if(targdesc)
	    handle_last_targdesc_match(target, targdesc-1, tokq->frontserialnum);
	  else
	    PrintMessage(Warning,"Error in %s line %d: invalid pattern sequence: expected nonzero target descendancy."
			 " Coding error in make_tag_pattern_info()?",__FILE__,__LINE__);
	}
	else
	  PrintMessage(Warning,"Error in %s line %d: invalid pattern sequence: expected target pattern."
		       " Coding error in make_tag_pattern_info()?",__FILE__,__LINE__);
      }
      continue;

    mismatch:
      if(p->firstlinedesc)
	handle_mismatching_firstline_sequence(p);
    }

    /* Only activate descendant patterns after all the patterns have been matched against this tag. */
    activate_pattstate_list(pattlists.pattstatelist);
  }

  tag->resolved=resolved;
  return pattlists;
}


/* In the case that all the patterns in a pattern sequence matched,
   where the last pattern is a descendant of the target pattern,
   scan the lists of references to the targets in the PattOptions node.
   Mark as resolved or activate the targets of the references that have
   reached the final state (as specified by targdesc argument), and
   remove those references from the lists.
*/
static void handle_last_targdesc_match(HTokenPattNode *target, unsigned targdesc, unsigned frontserialnum)
{
  PattOptions *pattopts= target->pattopts;
  StateListNode *stl= pattopts->stlist, **last=&pattopts->stlist;

  if(!(pattopts->optionflags&HASSIBLINGPATTERN)) {
    /* Target token is a tag that is possibly queued. */

    while(stl) {
      StateListNode *next= stl->next;
      if(stl->lastm==targdesc && stl->numtok.serialnum>=frontserialnum) {
	HToken *ht=stl->numtok.htoken;
	if(!ht->tag.resolved) {
	  ht->tag.resolved=1;
	  ht->matchingpatt=target;
	}
      }
      if(stl->lastm==targdesc || stl->numtok.serialnum<frontserialnum || stl->numtok.htoken->tag.resolved) {
	/* Remove list node. */
	*last=next;
	free(stl);
      }
      else
	last=&stl->next;
      stl=next;
    }
  }
  else {
    if(!pattopts->sibling)
      PrintMessage(Warning,"Error in %s line %d: invalid pattern sequence: expected sibling pattern."
		   " Coding error in make_tag_pattern_info()?",__FILE__,__LINE__);

    /* Activate the PattOptionsListNodes referenced by pattopts->stlist. */

    while(stl) {
      StateListNode *next= stl->next;
      if(stl->lastm==targdesc) {
	stl->pattoptslistnode->active=1;
	*last=next;
	free(stl);
      }
      else
	last=&stl->next;
      stl=next;
    }
  }
}


/* If a first-line descendancy pattern sequence mismatched, remove references to target in PattOptions node. */
static void handle_mismatching_firstline_sequence(HTokenPattNode *pattn)
{
  unsigned targdesc= pattn->targdesc;

  if(targdesc) {
    unsigned prev=targdesc-1;
    PattOptions *pattopts= pattn->pattopts;
    if(pattopts) {
      StateListNode *stl= pattopts->stlist, **last= &pattopts->stlist;
      while(stl) {
	StateListNode *next= stl->next;
	if(stl->lastm==prev) {
	  *last=next;
	  free(stl);
	  /* break; */  /* There should be at most one. */
	}
	else
	  last=&stl->next;
	stl=next;
      }
    }
    /* else PrintMessage(Warning,"pattopts missing"); */
  }
}


inline static int get_optionflags(HTokenPattNode *matchingpatt)
{
  PattOptions *pattopts= matchingpatt->pattopts;
  return pattopts? pattopts->optionflags: 0;
}

/* Modify a tag structure using the options contained in the matching pattern.
   If disable_script is true, all the script-like attributes of the tag are disabled.
   matchingpatt->tokentype should equal htokenTag! */
static void process_tag_style_replacement(Tag *tag, HTokenPattNode *matchingpatt, int disable_script)
{
  const char *stylerepl= NULL;
  AttrNode *a;
  Bool_t disablematchingattrs=0;

  if(matchingpatt) {
    PattOptions *pattopts= matchingpatt->pattopts;
    if(pattopts) {
      stylerepl= pattopts->stylerepl;
      if((pattopts->optionflags&DISABLEMATCHINGTAG) && !tag->disable)
	tag->disable=disabled_matching_tag;
      if(pattopts->optionflags&DISABLEMATCHINGATTRIBUTES)
	disablematchingattrs=1;
    }
  }

  for(a=tag->attrl; a; a=a->next) {
    switch (a->type) {
    case att_onblur: case att_onchange: case att_onclick: case att_ondblclick: case att_onfocus:
    case att_onerror: case att_onkeydown: case att_onkeypress: case att_onload: case att_onmousedown:
    case att_onmousemove: case att_onmouseout: case att_onmouseover: case att_onmouseup:
    case att_onreset: case att_onselect: case att_onsubmit: case att_onunload:
      if(disable_script) goto disableattr; /* Script events */
      break;
    case att_style:
      if(stylerepl) goto disableattr;
      break;
    default:;
    }
    if(disablematchingattrs) {
      AttrPattNode *ap;
      for(ap=matchingpatt->tagpattn.attrpatts; ap; ap=ap->next) {
	if(ap->disableattr && !strcasecmp(a->key,ap->attrname)) {
	  char *attrval=a->val, *endptr;
	  size_t len;
	  if(attrval) {
	    if(ap->valpatt) {
	      PattNode *q;
	      while(*attrval && isspace(*attrval)) ++attrval;  /* Skip blanks. */
	      endptr=strchrnul(attrval,0);
	      while(--endptr>=attrval && isspace(*endptr));  /* Trim trailing blanks. */
	      ++endptr;
	      len= endptr-attrval;
	      q=ap->valpatt;
	      do {
		if(strsegmentscasematch(attrval,len,q->patt,q->len))
		  goto disableattr;
		q=q->next;
	      } while(q);
	    }
	  }
	  else if(!ap->valpatt)
	    goto disableattr;
	}
      }
    }
    continue;
  disableattr:
    a->disable=1;
  }

  /* Add the new style attribute. */
  if(stylerepl && *(stylerepl))
    add_attribute(tag, -1,"style",stylerepl,'"',0);
}


struct matchtextresult {
  HTokenPattNode *matchingpatt;
  PattOptionsListNode *siblist;
};

/* match_text_pattern_info() returns a pointer to the matching pattern node. */
static struct matchtextresult match_text_pattern_info(HTokenType type, const char *txtstr, size_t txtlen,
						      PattInfo *pattinf, HTokenQueue *tokq)
{
  struct matchtextresult res= {NULL,NULL};
  PattOptionsListNode **lastactsib= &res.siblist;
  HTokenPattNode *p, *target;

  switch(type) {
  case htokenText:         p= pattinf->txtpl;     break;
  case htokenScriptText:   p= pattinf->scrtxtpl;  break;
  case htokenStyleText:    p= pattinf->styltxtpl; break;
  case htokenComment:      p= pattinf->compl;     break;
  default:                 p= NULL;
  }

  for(; p; p=p->next) {
    /* p->tokentype should equal htokenText, htokenScriptText, htokenStyleText or htokenComment. */
    if(p->active && !p->hasdesc && !(res.matchingpatt && p->target==p && !(p->pattopts->optionflags&HASSIBLINGPATTERN))) {
      PattNode *q;
      for(q=p->txtpatt; q; q=q->next) {
	if(strsegmentscasematch(txtstr,txtlen,q->patt,q->len))
	  goto match;
      }
      continue;

    match:
      /* This works because p->hasdesc==0. */
      target= p->target;
      if(target) {
	if(target==p) {
	  PattOptions *pattopts= target->pattopts;
	  if(!(pattopts->optionflags&HASSIBLINGPATTERN)) {
	    /* Direct match */
	    /* if(!res.matchingpatt) */ res.matchingpatt= p;
	  }
	  else if(pattopts->sibling) {
	    /* Extend list of sibling patterns. */
	    PattOptionsListNode *new= malloc(sizeof(PattOptionsListNode));
	    new->next=NULL;
	    new->pattopts=pattopts;
	    new->active=1;
	    *lastactsib=new;
	    lastactsib = &new->next;
	  }
	  else
	    PrintMessage(Warning,"Error in %s line %d: invalid pattern sequence: expected sibling pattern."
			 " Coding error in make_tag_pattern_info()?",__FILE__,__LINE__);
	}
	else {
	  unsigned targdesc= p->targdesc;
	  if(targdesc)
	    handle_last_targdesc_match(target, targdesc-1, tokq->frontserialnum);
	  else
	    PrintMessage(Warning,"Error in %s line %d: invalid pattern sequence: expected nonzero target descendancy."
			 " Coding error in make_tag_pattern_info()?",__FILE__,__LINE__);
	}
      }
      else
	PrintMessage(Warning,"Error in %s line %d: invalid pattern sequence: expected target pattern."
		     " Coding error in make_tag_pattern_info()?",__FILE__,__LINE__);
    }
  }

  return res;
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
  Output a complete tag, possibly converted into an HTML comment.

  const Tag *tag The tag to output.

  const char *prefix The optional prefix of the tag.
  ++++++++++++++++++++++++++++++++++++++*/

static void output_tag(const Tag *tag,const char *prefix)
{
 AttrNode *a;
 int iscomment;

 yy_rawout("<");

 if(tag->disable && !prefix)
   prefix=tagprefix[tag->disable];

 if(prefix)
   yy_rawout(prefix);

 yy_comout(tag->tagname, iscomment=(prefix!=NULL));

 for(a=tag->attrl; a; a=a->next)
   {
    yy_rawout(" ");
    output_attr_key_val(a->key,a->val,a->quote,a->disable,iscomment);
   }

 if(prefix)
   yy_rawout(" --");

 if(tag->ending==1 || prefix)
   yy_rawout(">");
 else if(tag->ending==2)
   yy_rawout(" />");
}


/*++++++++++++++++++++++++++++++++++++++
  Modify the HTML looking for all of the things to be changed.

  URL *Url The URL that this page comes from.
  ++++++++++++++++++++++++++++++++++++++*/

static void modify_html(URL *Url)
{
 HTMLTags tagtype=-1;
 HTMLAttributes key=-1;
 int tagidx=-1;
 int url_cached=0;
 int yychar;
 unsigned key_allocsize=0;
 char *key_string=NULL;
 HToken *tagtoken;
 Tag *tagdata;
 TagStack tagstack=NULL, tagdstack=NULL;
 TagPStack tagpstack=NULL;
 HTokenQueue tokenqueue;

 char *anchor_modify_begin[3];
 char *anchor_modify_end[3];
 Bool_t anchor_modify=0;
 Bool_t disable_all_scripts=ConfigBooleanURL(DisableHTMLScript,Url),disable_script=disable_all_scripts;
 Bool_t disable_script_after_body=ConfigBooleanURL(DisableHTMLScriptAfterBody,Url);
 Bool_t disable_dontget_script=ConfigBooleanURL(DisableHTMLDontGetScript,Url);
 unsigned short is_disabled_script=0;
 Bool_t disable_applet=ConfigBooleanURL(DisableHTMLApplet,Url);
 Bool_t disable_style=ConfigBooleanURL(DisableHTMLStyle,Url);
 unsigned short is_disabled_style=0;
 Bool_t disable_blink=ConfigBooleanURL(DisableHTMLBlink,Url);
 Bool_t disable_marquee=ConfigBooleanURL(DisableHTMLMarquee,Url);
 Bool_t disable_flash=ConfigBooleanURL(DisableHTMLFlash,Url);
 Bool_t disable_iframes=ConfigBooleanURL(DisableHTMLIFrame,Url);
 Bool_t disable_meta_refresh=ConfigBooleanURL(DisableHTMLMetaRefresh,Url);
 Bool_t disable_meta_refresh_self=ConfigBooleanURL(DisableHTMLMetaRefreshSelf,Url);
 int replacement_meta_refresh_time=ConfigIntegerURL(ReplacementHTMLMetaRefreshTime,Url);
 Bool_t disable_meta_set_cookie=ConfigBooleanURL(DisableHTMLMetaSetCookie,Url);
 Bool_t disable_dontget_anchors=ConfigBooleanURL(DisableHTMLDontGetAnchors,Url);
 Bool_t disable_dontget_iframes=ConfigBooleanURL(DisableHTMLDontGetIFrames,Url);
 Bool_t replace_html_dontget_images=ConfigBooleanURL(ReplaceHTMLDontGetImages,Url);
 char *replacement_html_dontget_image=NULL,*replacement_html_dontget_image_copy=NULL;
 Bool_t replace_html_webbug_images=ConfigBooleanURL(ReplaceHTMLWebbugImages,Url);
 char *replacement_html_webbug_image=NULL,*replacement_html_webbug_image_copy=NULL;
 char *insertheadfile=ConfigStringURL(InsertHeadFile,Url);
 char *insertfile=ConfigStringURL(InsertFile,Url);
 Bool_t insertedfile=0,after_body=0;
 Bool_t is_endtag=0, tag_maybe_matchpatt=0, tagcomplexmodify=0;
 Bool_t disable_following_script=0, disable_all_following_scripts=0;
 int scriptdisablelevel=0, optionflags=0;
 PattInfo tagpatterninfo;

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

 make_tag_pattern_info(&tagpatterninfo, ReplacementStyleAttr,Url);
 disable_output=DISABLE_NONE;
 parsecomment= (tagpatterninfo.compl!=NULL);
 commentout=0;

 /* Initialise the tag data and the token queue. */

 tagtoken=NULL;
 tagdata=NULL;
 htokenqueue_init(&tokenqueue);

 /* The actual parser. */

 do {
   const char *txtstr, *endptr;
   HTokenType htoktype=0;
   HTokenPattNode *matchingpatt=NULL;
   Bool_t maybe_disable_script, disable_key;
   char quote;

   yychar=htmlmodify_yylex();
   switch(yychar)
     {
      case LEX_PLAINTEXT:
       htoktype=htokenText;
       if(tagpatterninfo.txtpl) {
	 txtstr=htmlmodify_yylval; endptr=htmlmodify_yylval+htmlmodify_yylval_len;
	 goto match_text;
       }
       goto queue_push_text_token;

      case LEX_SCRIPTTEXT:
       htoktype=htokenScriptText;
       if(disable_output) {
	 if(tagpatterninfo.scrtxtpl)
	   goto match_stext;
         goto queue_push_stext_token;
       }
       break;

      case LEX_STYLETEXT:
       htoktype=htokenStyleText;
       if(disable_output) {
	 if(tagpatterninfo.styltxtpl)
	   goto match_stext;
         goto queue_push_stext_token;
       }
       break;

      case LEX_COMMENT:
       htoktype=htokenComment;
       if(tagpatterninfo.compl)
	 goto match_stext;
       goto queue_push_text_token;

      case LEX_DOCTYPE:
       htoktype=htokenDoctype;
       goto queue_push_text_token;

      case LEX_MISC:
       htoktype=htokenMisc;
      queue_push_text_token:
       if(disable_output)
	 htokenqueue_push_texttoken(&tokenqueue,htoktype,htmlmodify_yylval,htmlmodify_yylval_len,NULL);
       break;

      queue_push_stext_token:
       {
	 unsigned beginlen=0, closelen=0, flags=0;
	 if(stextstart>0) beginlen=stextstart;
	 if(stextend>=0 && htmlmodify_yylval_len>stextend) closelen=htmlmodify_yylval_len-stextend;
	 if(stextstart<0)           flags|= STEXTISEMPTY;
	 if(stextcommopen)          flags|= STEXTCOMMOPEN;
	 if(stextcommclose)         flags|= STEXTCOMMCLOSE;
	 htokenqueue_push_stexttoken(&tokenqueue,htoktype,htmlmodify_yylval,htmlmodify_yylval_len,beginlen,closelen,flags,matchingpatt);
       }
       break;

      match_stext:
       txtstr=htmlmodify_yylval; if(stextstart>0) txtstr+=stextstart;
       endptr=htmlmodify_yylval + (stextend>=0? stextend: htmlmodify_yylval_len);
      match_text:
       while(txtstr<endptr && isspace(*txtstr)) ++txtstr;  /* Strip initial blanks. */
       while(--endptr>=txtstr && isspace(*endptr));  /* Strip trailing blanks. */
       ++endptr;
       {
	 struct matchtextresult matchres= match_text_pattern_info(htoktype,txtstr,endptr-txtstr,&tagpatterninfo,&tokenqueue);
	 PattOptionsListNode **last;
	 matchingpatt= matchres.matchingpatt;
	 last= unwind_pattoptions_list(&matchres.siblist);
	 append_sibling_patt_list(&tagpatterninfo,matchres.siblist,last);
       }
       goto unwind_token_queue;

      case LEX_TAG_BEGIN:
       tagtype=lookuptag(htmlmodify_yylval);
       tagidx = tag_match_index(htmlmodify_yylval,&tagpatterninfo);
       is_endtag= (*htmlmodify_yylval=='/');
       tag_maybe_matchpatt= (tagidx>=0 && !is_endtag);
       tagcomplexmodify= !htokenqueue_isempty(&tokenqueue) || is_complex_tag(tagtype) || tag_maybe_matchpatt;

       reset_tagtoken(&tagtoken, tagcomplexmodify?htmlmodify_yylval:NULL, tagtype, tagidx);
       tagdata= &tagtoken->tag;
       optionflags=0;
       commentout=0;

       if(!is_endtag) {
	 if(tagidx>=0 || !htokenqueue_isempty(&tokenqueue))
	   disable_output|=DISABLE_TAG;
       }
       else { /* end tag */
	 reset_fchild_sibling_patt_lists(&tagpatterninfo,-1,NULL,0,NULL,&tokenqueue);
	 if(tagidx>=0) {
	   if(stackflags(&tagpatterninfo,tagidx)&TAGPSTACK) {
	     PattOptionsList sl=poptagpstack(&tagpstack, tagidx, tokenqueue.frontserialnum);
	     append_sibling_patt_list(&tagpatterninfo, sl.first, sl.last);
	   }
	 }
	 /* Popping tagpstack or calling reset_fchild_sibling_patt_lists() may have resolved the front token in the queue. */
	 if(!htokenqueue_isempty(&tokenqueue) && (tokenqueue.front->tokentype!=htokenTag || tokenqueue.front->tag.resolved))
	   goto unwind_token_queue;
       continue_end_tag:
	 if(tagidx>=0) {
	   if(htokenqueue_isempty(&tokenqueue) && (stackflags(&tagpatterninfo,tagidx)&TAGDSTACK)) {
	     optionflags=poptagstack(&tagdstack,tagidx);
	     if((optionflags&DISABLEMATCHINGTAG) && !tagdata->disable)
	       tagdata->disable=disabled_matching_tag;
	   }
	 }
       }

       if(htokenqueue_isempty(&tokenqueue)) {
	 disable_script = (disable_all_scripts || (after_body && disable_script_after_body) ||
			    (!tag_maybe_matchpatt && (scriptdisablelevel>0 || disable_all_following_scripts)));
	 maybe_disable_script = (disable_all_scripts || (after_body && disable_script_after_body) ||
				 tag_maybe_matchpatt || scriptdisablelevel>0 || disable_all_following_scripts);
	 switch (tagtype) {
	   disabled_t disable;
	 case tag__a:
	   if((disable= popendtagstack(&tagstack,tagtype)))
	     tagdata->disable= disable;
	   if(anchor_modify) {
	     if(!tagdata->disable) {
	       if(url_cached==1)
		 {if(anchor_modify_end[0]) {yy_rawout(anchor_modify_end[0]);}}
	       else if(url_cached==2)
		 {if(anchor_modify_end[1]) {yy_rawout(anchor_modify_end[1]);}}
	       else if(url_cached==-1)
		 {if(anchor_modify_end[2]) {yy_rawout(anchor_modify_end[2]);}}
	     }
	     url_cached=0;
	   }
	   break;
	 case tag__applet:
	   popendtagstack(&tagstack,tagtype);
	   break;
	 case tag__body: case tag__html:
	   if(cache_info)
	     {yy_rawout(cache_info); free(cache_info); cache_info=NULL;}
	   /* Normally there should be only one /body tag, at the end,
	      but I have seen broken HTML with multiple /body tags
	      with text in between. */
	   if(insertfile && (!insertedfile || tagtype==tag__body))
	     {add_insertfile(insertfile); insertedfile=1;}
	   after_body=1;
	   break;
	 case tag__embed:
	 case tag__iframe:
	 case tag__object:
	   if((disable= popendtagstack(&tagstack,tagtype)))
	     tagdata->disable= disable;
	   break;
	 case tag__head:
	   if(insertheadfile)
	     add_insertfile(insertheadfile);
	   break;
	 case tag_param:
	   if(tagstack && tagstack->val)
	     tagdata->disable= tagstack->val;
	   break;
	 case tag_script:
	   if(!tag_maybe_matchpatt) {
	     /* This tag cannot match a pattern */
	     if(disable_all_scripts)
	       is_disabled_script= disabled_script;
	     else if(after_body && disable_script_after_body)
	       is_disabled_script= disabled_script_after_body;
	     else if(disable_all_following_scripts)
	       is_disabled_script= disabled_all_following_scripts;
	     else if(disable_following_script)
	       is_disabled_script= disabled_following_script;
	     else if(scriptdisablelevel>0)
	       is_disabled_script= disabled_inner_script;
	     else {
	       is_disabled_script= 0;
	       if(disable_dontget_script)
		 disable_output|=DISABLE_SCRIPT;
	     }
	     if(is_disabled_script) tagdata->disable= is_disabled_script;
	   }
	   else
	     disable_output|=DISABLE_SCRIPT;
	   break;
	 case tag__script:
	   if(is_disabled_script) tagdata->disable= is_disabled_script;
	   is_disabled_script=0;
	   break;
	 case tag_style:
	   if(disable_style)
	     tagdata->disable=is_disabled_style= disabled_style;
	   else {
	     is_disabled_style= 0;
	     if(maybe_disable_script) /* could be script pretending to be style */
	       disable_output|=DISABLE_STYLE;
	   }
	   break;
	 case tag__style:
	   if(is_disabled_style) tagdata->disable= is_disabled_style;
	   is_disabled_style=0;
	   break;
	 default: ;
	 }
       }
       else {
	 disable_script = disable_all_scripts;
	 maybe_disable_script = (disable_all_scripts || tag_maybe_matchpatt);
	 switch(tagtype) {
	 case tag_param:
	   disable_output|=DISABLE_PARAM;
	   break;
	 case tag_script:
	   disable_output|=DISABLE_SCRIPT;
	   break;
	 case tag_style:
	   disable_output|=DISABLE_STYLE;
	   break;
	 default:;
	 }
       }

       /* These are "safe" operations that can be done irrespective whether the tag is going to be queued or not. */
       switch(tagtype) {
       case tag_a:
	 if(disable_dontget_anchors || maybe_disable_script)
	   disable_output|=DISABLE_A;
	 break;
       case tag_applet:
       case tag__applet:
	 if(disable_applet)
	   tagdata->disable= disabled_applet;
	 break;
       case tag_blink: case tag__blink:
	 if(disable_blink)
	   tagdata->disable= disabled_blink;
	 break;
       case tag_embed:
       case tag_object:
	 if(replace_html_dontget_images || replace_html_webbug_images ||
	    disable_applet || disable_flash ||
	    disable_dontget_iframes || maybe_disable_script)
	   disable_output|=DISABLE_OBJECT;
	 break;
       case tag_iframe:
	 if(disable_iframes)
	   tagdata->disable= disabled_iframe;
	 else if(disable_dontget_iframes || maybe_disable_script)
	   disable_output|=DISABLE_IFRAME;
	 break;
       case tag_img:
	 if(replace_html_dontget_images || replace_html_webbug_images || maybe_disable_script)
	   disable_output|=DISABLE_IMG;
	 break;
       case tag_link:
	 if(disable_style || maybe_disable_script)
	   disable_output|=DISABLE_LINK;
	 break;
       case tag_marquee: case tag__marquee:
	 if(disable_marquee)
	   tagdata->disable= disabled_marquee;
	 break;
       case tag_meta:
	 if(disable_meta_refresh_self || disable_meta_refresh || disable_meta_set_cookie)
	   disable_output|=DISABLE_META;
	 break;
       case tag_noscript: case tag__noscript:
	 if(disable_all_scripts)
	   tagdata->disable= disabled_script;
	 break;
       default:;
       }

       if(!disable_output) {
	 yy_rawout("<");
	 if(tagdata->disable)
	   yy_rawout(tagprefix[tagdata->disable]);
	 yy_comout(htmlmodify_yylval,tagdata->disable);
       }
       break;

      case LEX_TAG_END_TRUNC:
       tagdata->ending=0;
       goto case_tag_end;

      case LEX_TAG_END_XHTML:
       tagdata->ending=2;
       goto case_tag_end;

      case LEX_TAG_END:
       tagdata->ending=1;
      case_tag_end:
       if(!disable_output) {
	 if(tagdata->disable)
	   yy_rawout(" --");
	 if(yychar==LEX_TAG_END || tagdata->disable)
	   yy_rawout(">");
	 else if(yychar==LEX_TAG_END_XHTML)
	   yy_rawout("/>");
       }

       if(tag_maybe_matchpatt && tagdata->ending) {
	 struct activepattlists pattlists=match_tag_pattern_info(tagtoken,&tagpatterninfo,&tokenqueue);
	 HTokenListNode *parenthtoklist=reset_fchild_sibling_patt_lists(&tagpatterninfo, tagidx,
									pattlists.fchildlist, pattlists.maxfirstlinehasdesc,
									(tagdata->resolved || pattlists.maxtarghasdesc>1)?NULL:tagtoken,
									&tokenqueue);
	 if(stackflags(&tagpatterninfo,tagidx)&TAGPSTACK) {
	   if(tagdata->ending==1)
	     pushtagpstack(&tagpstack, tagidx, pattlists.pattstatelist, pattlists.siblist,
			   tagdata->resolved?NULL:tagtoken, tokenqueue.nextserialnum,
			   parenthtoklist);
	   else /* if(tagdata->ending==2) */ {
	     /* unwind_pattstate_list(pattlists.pattstatelist); */ /* pattlists.desclist and fchildlist should be NULL if tagdata->ending!=1. */
	     PattOptionsListNode **last= unwind_pattoptions_list(&pattlists.siblist);
	     append_sibling_patt_list(&tagpatterninfo, pattlists.siblist, last);
	   }
	 }
	 /* else; */ /* pattlists should be empty if the TAGPSTACK flag is false. */
       }
       else {
	 tagdata->resolved=1;
	 if(!is_endtag)
	   reset_fchild_sibling_patt_lists(&tagpatterninfo,-1,NULL,0,NULL,&tokenqueue);
       }

       /* Calling match_tag_pattern_info() or reset_fchild_sibling_patt_lists()
	  may have resolved the front token in the queue. */
       if(!htokenqueue_isempty(&tokenqueue) && (tokenqueue.front->tokentype!=htokenTag || tokenqueue.front->tag.resolved))
	 goto unwind_token_queue;
     continue_start_tag:

       if(!(tagdata->resolved && htokenqueue_isempty(&tokenqueue))) {
	 htokenqueue_push(&tokenqueue, tagtoken);
	 tagtoken=NULL;
	 disable_output=DISABLE_ALLOUTPUT;  /* Clear all the other flags. */
	 break;
       }
       else {
	 /* tokenqueue remains empty. Nevertheless, simulate passing the token through the queue. */
	 htokenqueue_serialincr(&tokenqueue);
       }

       if(tag_maybe_matchpatt) {
	 matchingpatt= tagtoken->matchingpatt;
	 if(matchingpatt) {
	   optionflags= get_optionflags(matchingpatt);
	   if(optionflags&RESETFOLLOWINGSCRIPT)
	     disable_following_script=0;
	   if(optionflags&RESETALLFOLLOWINGSCRIPTS)
	     disable_all_following_scripts=0;
	 }
	 if(stackflags(&tagpatterninfo,tagidx)&TAGDSTACK)  {
	   if(tagdata->ending==1) {
	     pushtagstack(&tagdstack, tagidx, optionflags);
	   }
	 }
	 disable_script= (disable_all_scripts || (after_body && disable_script_after_body) ||
			  scriptdisablelevel>0 || (optionflags&DISABLEINNERSCRIPT) ||
			  disable_all_following_scripts);
	 if(matchingpatt || disable_script)
	   process_tag_style_replacement(tagdata,matchingpatt,disable_script);
       }

       if(tagtype!=tag_script && tagtype!=tag__script)
	 disable_following_script=0;

       if(disable_output&DISABLE_TAG) {
	 disable_output&=~DISABLE_TAG;
	 if(!disable_output) output_tag(tagdata,NULL);
       }

       switch (tagtype) {
	 disabled_t disable;
       case tag_a:
	 disable=0;
	 if(disable_output&DISABLE_A) {
	   disable_output&=~DISABLE_A;
	   disable=handle_a_tag(tagdata,disable_dontget_anchors,disable_script);
	 }

	 if(tagdata->ending==1) {
	   pushtagstack(&tagstack,tagtype,disable);

	   if(!disable && anchor_modify) {
	     if(url_cached==1)
	       {if(anchor_modify_begin[0]) {yy_rawout(anchor_modify_begin[0]);}}
	     else if(url_cached==2)
	       {if(anchor_modify_begin[1]) {yy_rawout(anchor_modify_begin[1]);}}
	     else if(url_cached==-1)
	       {if(anchor_modify_begin[2]) {yy_rawout(anchor_modify_begin[2]);}}
	   }
	 }
	 break;
       case tag_applet:
	 if(tagdata->ending==1)
	   pushtagstack(&tagstack,tagtype,disable_applet?disabled_applet:0);
	 break;
       case tag_embed:
       case tag_object:
	 disable=0;
	 if(disable_output&DISABLE_OBJECT)
	   {
	     disable_output&=~DISABLE_OBJECT;
	     disable= handle_object_tag(tagdata,replace_html_dontget_images,replacement_html_dontget_image,
					replace_html_webbug_images,replacement_html_webbug_image,
					disable_applet,
					disable_flash,
					disable_dontget_iframes,
					disable_script);
	   }

	 if(tagdata->ending==1)
	   pushtagstack(&tagstack,tagtype,disable);
	 break;
       case tag_iframe:
	 disable=0;
	 if(disable_iframes)
	   disable=disabled_iframe;
	 else if(disable_output&DISABLE_IFRAME)
	   {
	     disable_output&=~DISABLE_IFRAME;
	     disable=handle_iframe_tag(tagdata,disable_dontget_iframes,disable_script);
	   }

	 if(tagdata->ending==1)
	   pushtagstack(&tagstack,tagtype,disable);
	 break;
       case tag_img:
	 if(disable_output&DISABLE_IMG) {
	   disable_output&=~DISABLE_IMG;
	   handle_img_tag(tagdata,replace_html_dontget_images,replacement_html_dontget_image,
			  replace_html_webbug_images,replacement_html_webbug_image,
			  disable_script);
	 }
	 break;
       case tag_link:
	 if(disable_output&DISABLE_LINK) {
	   disable_output&=~DISABLE_LINK;
	   handle_link_tag(tagdata,disable_style,disable_script);
	 }
	 break;
       case tag_meta:
	 if((disable_output&DISABLE_META) ||
	    demoronise_ms_chars || fix_mixed_cyrillic)
	 {
	   disable_output^=DISABLE_META;
	   handle_meta_tag(tagdata,disable_meta_refresh,disable_meta_refresh_self,replacement_meta_refresh_time,disable_meta_set_cookie);
	   disable_output&=~DISABLE_META;
	 }
	 break;
       case tag_param:
	 if(disable_output&DISABLE_PARAM) {
	   disable_output&=~DISABLE_PARAM;
	   if(tagstack && tagstack->val)
	     tagdata->disable= tagstack->val;
	   output_tag(tagdata,NULL);
	 }
	 break;
       case tag_script:
	 if(disable_output&DISABLE_SCRIPT) {
	   disable_output&=~DISABLE_SCRIPT;

	   if(disable_all_scripts)
	     is_disabled_script= disabled_script;
	   else if(after_body && disable_script_after_body)
	     is_disabled_script= disabled_script_after_body;
	   else if(disable_all_following_scripts)
	     is_disabled_script= disabled_all_following_scripts;
	   else if(disable_following_script)
	     is_disabled_script= disabled_following_script;
	   else if(scriptdisablelevel>0)
	     is_disabled_script= disabled_inner_script;
	   else if(optionflags&DISABLEINNERSCRIPT)
	     is_disabled_script=disabled_matching_script;
	   else
	     is_disabled_script=0;

	   if(disable_dontget_script)
	     is_disabled_script=handle_script_tag(tagdata,is_disabled_script);
	   else {
	     if(is_disabled_script) tagdata->disable= is_disabled_script;
	     output_tag(tagdata,NULL);
	   }
	 }
	 if(is_disabled_script && tagdata->ending==1)
	   commentout=1;
	 break;
       case tag_style:
	 if(disable_output&DISABLE_STYLE) {
	   disable_output&=~DISABLE_STYLE;

	   if(disable_style) {
	     tagdata->disable= is_disabled_style= disabled_style;
	     output_tag(tagdata,NULL);
	   }
	   else if(disable_script) {
	     /* could be script pretending to be style */
	     is_disabled_style=handle_style_script_tag(tagdata);
	   }
	   else {
	     is_disabled_style=0;
	     output_tag(tagdata,NULL);
	   }
	 }
	 if(is_disabled_style && tagdata->ending==1)
	   commentout=1;
	 break;
       default: ;
       }

       if(optionflags) {
	 if(is_endtag) {
	   if((optionflags&DISABLEINNERSCRIPT) && scriptdisablelevel>0)
	     --scriptdisablelevel;
	   if(optionflags&DISABLEFOLLOWINGSCRIPT)
	     disable_following_script=1;
	   if(optionflags&DISABLEALLFOLLOWINGSCRIPTS)
	     disable_all_following_scripts=1;
	 }
	 else if(tagdata->ending==1) {
	   if(optionflags&DISABLEINNERSCRIPT)
	     ++scriptdisablelevel;
	 }
	 else if(tagdata->ending==2) {
	   if(optionflags&DISABLEFOLLOWINGSCRIPT)
	     disable_following_script=1;
	   if(optionflags&DISABLEALLFOLLOWINGSCRIPTS)
	     disable_all_following_scripts=1;
	 }
       }

       /* tagtype=-1; */
       /* key=-1; */
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
	   if(htokenqueue_isempty(&tokenqueue)) {
	     if(tagtype==tag_a) {       /* Links */
	       if(anchor_modify)
		 url_cached= get_url_cached_type(htmlmodify_yylval);
	     }
	     else if(tagtype==tag_base)       /* Base tag */
	       setbase_url(htmlmodify_yylval);
	   }
	   break;

	 case att_onblur: case att_onchange: case att_onclick: case att_ondblclick: case att_onfocus:
	 case att_onerror: case att_onkeydown: case att_onkeypress: case att_onload: case att_onmousedown:
	 case att_onmousemove: case att_onmouseout: case att_onmouseover: case att_onmouseup:
	 case att_onreset: case att_onselect: case att_onsubmit: case att_onunload:
	   if(disable_script)       /* Script events */
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
	 add_attribute(tagdata, key,key_string,htmlmodify_yylval,quote,disable_key);

       if(!disable_output)
	 output_attr_key_val(key_string,htmlmodify_yylval,quote,disable_key,tagdata->disable);

       /* key=-1; */
       break;

     case 0:
     unwind_token_queue:
       while(!htokenqueue_isempty(&tokenqueue) && (!yychar || tokenqueue.front->tokentype!=htokenTag || tokenqueue.front->tag.resolved)) {
	 HToken *htok= htokenqueue_pop(&tokenqueue);
	 switch(htok->tokentype) {
	 case htokenTag: {
	   Tag *tag= &htok->tag;
	   HTokenPattNode *matchingpatt=NULL;
	   int optionflags=0;
	   Bool_t disable_script;

	   if(tag->idx>=0) {
	     if(!tag_is_endtag(tag)) {
	       matchingpatt= htok->matchingpatt;
	       if(matchingpatt) {
		 optionflags= get_optionflags(matchingpatt);
		 if(optionflags&RESETFOLLOWINGSCRIPT)
		   disable_following_script=0;
		 if(optionflags&RESETALLFOLLOWINGSCRIPTS)
		   disable_all_following_scripts=0;
	       }
	       if(stackflags(&tagpatterninfo,tag->idx)&TAGDSTACK) {
		 if(tag->ending==1) {
		   pushtagstack(&tagdstack, tag->idx, optionflags);
		 }
	       }
	     }
	     else if(stackflags(&tagpatterninfo,tag->idx)&TAGDSTACK) {
	       optionflags=poptagstack(&tagdstack,tag->idx);
	       if((optionflags&DISABLEMATCHINGTAG) && !tag->disable)
		 tag->disable=disabled_matching_tag;
	     }
	   }

	   if(tag->type!=tag_script && tag->type!=tag__script)
	     disable_following_script=0;

	   disable_script =  (disable_all_scripts || (after_body && disable_script_after_body) ||
			       scriptdisablelevel>0 || (!tag_is_endtag(tag) && (optionflags&DISABLEINNERSCRIPT)) ||
			       disable_all_following_scripts);

	   if(matchingpatt || disable_script)
	     process_tag_style_replacement(tag, matchingpatt, disable_script);

	   switch (tag->type) {
	     disabled_t disable;
	   case tag_a:
	     if(anchor_modify)
	       url_cached= tag_get_url_cached_type(tag);
	     disable= handle_a_tag(tag,disable_dontget_anchors,disable_script);

	     if(tag->ending==1) {
	       pushtagstack(&tagstack,tag->type,disable);

	       if(!tag->disable && anchor_modify) {
		 if(url_cached==1)
		   {if(anchor_modify_begin[0]) {yy_rawout(anchor_modify_begin[0]);}}
		 else if(url_cached==2)
		   {if(anchor_modify_begin[1]) {yy_rawout(anchor_modify_begin[1]);}}
		 else if(url_cached==-1)
		   {if(anchor_modify_begin[2]) {yy_rawout(anchor_modify_begin[2]);}}
	       }
	     }
	     break;
	   case tag__a:
	     if((disable= popendtagstack(&tagstack,tag->type)))
	       tag->disable= disable;
	     if(anchor_modify) {
	       if(!tag->disable) {
		 if(url_cached==1)
		   {if(anchor_modify_end[0]) {yy_rawout(anchor_modify_end[0]);}}
		 else if(url_cached==2)
		   {if(anchor_modify_end[1]) {yy_rawout(anchor_modify_end[1]);}}
		 else if(url_cached==-1)
		   {if(anchor_modify_end[2]) {yy_rawout(anchor_modify_end[2]);}}
	       }
	       url_cached=0;
	     }
	     goto call_output_tag;
	   case tag_applet:
	     if(tag->ending==1)
	       pushtagstack(&tagstack,tag->type,disable_applet?disabled_applet:0);
	     goto call_output_tag;
	   case tag__applet:
	     popendtagstack(&tagstack,tag->type);
	     goto call_output_tag;
	   case tag_base:
	     tag_setbase_url(tag);
	     goto call_output_tag;
#if 0
	   case tag_blink: case tag__blink:
	     if(disable_blink)
	       tag->disable= disabled_blink;
	     goto call_output_tag;
#endif
	   case tag__body: case tag__html:
	     if(cache_info)
	       {yy_rawout(cache_info); free(cache_info); cache_info=NULL;}
	     /* Normally there should be only one /body tag, at the end,
		but I have seen broken HTML with multiple /body tags
		with text in between. */
	     if(insertfile && (!insertedfile || tag->type==tag__body))
	       {add_insertfile(insertfile); insertedfile=1;}
	     after_body=1;
	     goto call_output_tag;
	   case tag_embed:
	   case tag_object:
	     disable= handle_object_tag(tag,replace_html_dontget_images,replacement_html_dontget_image,
					replace_html_webbug_images,replacement_html_webbug_image,
					disable_applet,
					disable_flash,
					disable_dontget_iframes,
					disable_script);
	     if(tag->ending==1)
	       pushtagstack(&tagstack,tag->type,disable);
	     break;
	   case tag__embed:
	   case tag__iframe:
	   case tag__object:
	     if((disable= popendtagstack(&tagstack,tag->type)))
	       tag->disable= disable;
	     goto call_output_tag;
	   case tag__head:
	     if(insertheadfile)
	       add_insertfile(insertheadfile);
	     goto call_output_tag;
	   case tag_iframe:
	     disable=0;
	     if(disable_iframes)
	       tag->disable= disable=disabled_iframe;
	     else
	       disable=handle_iframe_tag(tag,disable_dontget_iframes,disable_script);

	     if(tag->ending==1)
	       pushtagstack(&tagstack,tag->type,disable);
	     if(disable_iframes)
	       goto call_output_tag;
	     break;
	   case tag_img:
	     handle_img_tag(tag,replace_html_dontget_images,replacement_html_dontget_image,
			    replace_html_webbug_images,replacement_html_webbug_image,
			    disable_script);
	     break;
	   case tag_link:
	     handle_link_tag(tag,disable_style,disable_script);
	     break;
#if 0
	   case tag_marquee: case tag__marquee:
	     if(disable_marquee)
	       tag->disable= disabled_marquee;
	     goto call_output_tag;
#endif
	   case tag_meta:
	     handle_meta_tag(tag,disable_meta_refresh,disable_meta_refresh_self,replacement_meta_refresh_time,disable_meta_set_cookie);
	     break;
#if 0
	   case tag_noscript: case tag__noscript:
	     if(disable_all_scripts)
	       tag->disable= disabled_script;
	     goto call_output_tag;
#endif
	   case tag_param:
	     if(tagstack && tagstack->val)
	       tag->disable= tagstack->val;
	     goto call_output_tag;
	   case tag_script:
	     if(disable_all_scripts)
	       is_disabled_script= disabled_script;
	     else if(after_body && disable_script_after_body)
	       is_disabled_script= disabled_script_after_body;
	     else if(disable_all_following_scripts)
	       is_disabled_script= disabled_all_following_scripts;
	     else if(disable_following_script)
	       is_disabled_script= disabled_following_script;
	     else if(scriptdisablelevel>0)
	       is_disabled_script= disabled_inner_script;
	     else if(optionflags&DISABLEINNERSCRIPT)
	       is_disabled_script=disabled_matching_script;
	     else
	       is_disabled_script=0;

	     if(disable_dontget_script)
	       is_disabled_script=handle_script_tag(tag,is_disabled_script);
	     else {
	       if(is_disabled_script) tag->disable= is_disabled_script;
	       goto call_output_tag;
	     }
	     break;
	   case tag__script:
	     if(is_disabled_script) tag->disable= is_disabled_script;
	     is_disabled_script=0;
	     goto call_output_tag;
	   case tag_style:
	     if(disable_style) {
	       tag->disable= is_disabled_style= disabled_style;
	       goto call_output_tag;
	     }
	     else if(disable_script) {
	       /* could be script pretending to be style */
	       is_disabled_style=handle_style_script_tag(tag);
	     }
	     else {
	       is_disabled_style=0;
	       goto call_output_tag;
	     }
	     break;
	   case tag__style:
	     if(is_disabled_style) tag->disable= is_disabled_style;
	     is_disabled_style=0;
	     goto call_output_tag;
	   default:
	   call_output_tag:
	     output_tag(&htok->tag,NULL);
	   }

	   if(optionflags) {
	     if(tag_is_endtag(tag)) {
	       if((optionflags&DISABLEINNERSCRIPT) && scriptdisablelevel>0)
		 --scriptdisablelevel;
	       if(optionflags&DISABLEFOLLOWINGSCRIPT)
		 disable_following_script=1;
	       if(optionflags&DISABLEALLFOLLOWINGSCRIPTS)
		 disable_all_following_scripts=1;
	     }
	     else if(tag->ending==1) {
	       if(optionflags&DISABLEINNERSCRIPT)
		 ++scriptdisablelevel;
	     }
	     else if(tag->ending==2) {
	       if(optionflags&DISABLEFOLLOWINGSCRIPT)
		 disable_following_script=1;
	       if(optionflags&DISABLEALLFOLLOWINGSCRIPTS)
		 disable_all_following_scripts=1;
	     }
	   }
	 }
	   break;

	 case htokenText:
	   handle_text(htok->text.str,htok->text.len);
	   goto handle_optionflags;

	 case htokenScriptText:
	 case htokenStyleText: {
	   const char *str= htok->stext.str;
	   unsigned len= htok->stext.len;
	   unsigned flags= htok->stext.flags;

	   if((is_disabled_script || is_disabled_style) && !(flags&STEXTISEMPTY)) {
	     unsigned begin=htok->stext.beginlen, closelen= htok->stext.closelen, end= len-closelen;
	     if(begin>0) yy_write(str,begin);
	     if(!(flags&STEXTCOMMOPEN)) yy_rawout("<!--\n");
	     if(end>begin)
	       yy_comwrite(str+begin,end-begin,1);
	     if(!(flags&STEXTCOMMCLOSE)) {
	       if(htok->tokentype==htokenScriptText) yy_rawout("//-->\n");
	       else yy_rawout("\n-->\n");
	     }
	     if(closelen>0) yy_write(str+end,closelen);
	   }
	   else if(len>0)
	     yy_write(str,len);
	 }
	   break;

	 case htokenComment:
	   yy_write(htok->text.str, htok->text.len);
	 handle_optionflags: {
	     HTokenPattNode *matchingpatt=htok->matchingpatt;
	     if(matchingpatt) {
	       int optionflags=get_optionflags(matchingpatt);
	       if(optionflags&RESETFOLLOWINGSCRIPT)
		 disable_following_script=0;
	       if(optionflags&RESETALLFOLLOWINGSCRIPTS)
		 disable_all_following_scripts=0;
	       if(optionflags&DISABLEFOLLOWINGSCRIPT)
		 disable_following_script=1;
	       if(optionflags&DISABLEALLFOLLOWINGSCRIPTS)
		 disable_all_following_scripts=1;
	     }
	   }
	   break;

	 case htokenDoctype:
	 case htokenMisc:
	   yy_write(htok->text.str, htok->text.len);
	   break;
	 }

	 free_htoken(htok);
       }

       if(!yychar)
	 break;

       if(htokenqueue_isempty(&tokenqueue)) {
	 int prev_disable_output=disable_output;
	 disable_output &= ~DISABLE_ALLOUTPUT;

	 /* Finish processing the current input token. */
	 switch(yychar) {
	 case LEX_PLAINTEXT:
	   if(prev_disable_output)
	     handle_text(htmlmodify_yylval,htmlmodify_yylval_len);
	   goto handle_optionflags2;

	 case LEX_SCRIPTTEXT:
	 case LEX_STYLETEXT:
	   if((is_disabled_script || is_disabled_style) && stextstart>=0) {
	     if(stextstart>0) yy_write(htmlmodify_yylval,stextstart);
	     if(!stextcommopen) yy_rawout("<!--\n");
	     if(stextend>stextstart)
	       yy_comwrite(htmlmodify_yylval+stextstart,stextend-stextstart,1);
	     if(!stextcommclose) {
	       if(yychar==LEX_SCRIPTTEXT) yy_rawout("//-->\n");
	       else yy_rawout("\n-->\n");
	     }
	     if(stextend>=0 && htmlmodify_yylval_len>stextend)
	       yy_write(htmlmodify_yylval+stextend,htmlmodify_yylval_len-stextend);
	   }
	   else if(htmlmodify_yylval_len>0)
	     yy_write(htmlmodify_yylval,htmlmodify_yylval_len);
	   break;

	 case LEX_COMMENT:
	   if(prev_disable_output)
	     yy_write(htmlmodify_yylval,htmlmodify_yylval_len);
	 handle_optionflags2:
	   if(matchingpatt) {
	     int optionflags=get_optionflags(matchingpatt);
	     if(optionflags&RESETFOLLOWINGSCRIPT)
	       disable_following_script=0;
	     if(optionflags&RESETALLFOLLOWINGSCRIPTS)
	       disable_all_following_scripts=0;
	     if(optionflags&DISABLEFOLLOWINGSCRIPT)
	       disable_following_script=1;
	     if(optionflags&DISABLEALLFOLLOWINGSCRIPTS)
	       disable_all_following_scripts=1;
	   }
	   break;

	 case LEX_TAG_BEGIN:
	   goto continue_end_tag;
	 case LEX_TAG_END:
	 case LEX_TAG_END_XHTML:
	 case LEX_TAG_END_TRUNC:
	   goto continue_start_tag;
	 }
       }
       else
	 switch(yychar) {
	 case LEX_PLAINTEXT:
	 case LEX_COMMENT:
	   htokenqueue_push_texttoken(&tokenqueue,htoktype,htmlmodify_yylval,htmlmodify_yylval_len,matchingpatt);
	   break;
	 case LEX_SCRIPTTEXT:
	 case LEX_STYLETEXT:
	   goto queue_push_stext_token;
	 case LEX_TAG_BEGIN:
	   goto continue_end_tag;
	 case LEX_TAG_END:
	 case LEX_TAG_END_XHTML:
	 case LEX_TAG_END_TRUNC:
	   goto continue_start_tag;
	 }

       break;

      default:
       break;
     }
 } while(yychar);

 if(cache_info)
   {yy_rawout(cache_info); free(cache_info); cache_info=NULL;}

 if(insertfile && !insertedfile)
   {add_insertfile(insertfile); insertedfile=1;}

 if(key_string)
    free(key_string);

 /* Delete the tag data */

 if(tagtoken) free_htoken(tagtoken);
 /* As long as yylex() returned 0, the token queue should have been emptied. */
 free_tagstack(tagstack);
 free_tagstack(tagdstack);
 free_tagpstack(tagpstack);
 free_pattern_info(&tagpatterninfo);

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

  char *p points to the character.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_high_bit(const char *p)
{
 unsigned char ch= *(const unsigned char*)p;

 if(demoronise_ms_chars && ch<=0x9F)
   {
    yy_rawout("<!-- WWWOFFLE (demoronise-ms-chars) - '");
    yy_write(p,1);
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
       yy_write(p,1);
      }
    else
      {
       char html_buffer[9];
       sprintf(html_buffer,"&#x%04X;",unicode_value);

       yy_rawout("<!-- WWWOFFLE (fix-mixed-cyrillic) - '");
       yy_write(p,1);
       yy_rawout("' -->");
       yy_rawout(html_buffer);
      }
   }
 else
    yy_write(p,1);
}

static void handle_text(const char *str, size_t len)
{
  const char *p= str, *end= p + len;
  while(p<end) {
    const char *s= p;
    unsigned char c;
    while((c= *(unsigned char*)p)>=0x80 && c<=0xBF) {
      if(++p>=end) break;
    }
    if(p>s) yy_write(s,p-s);
    if(p>=end) break;
    handle_high_bit(p);
    ++p;
  }
}

#define YY_SKIP_YYWRAP 1 /* Remove error with prototype of ..._yywrap */
#ifndef htmlmodify_yywrap
/*+ Needed in lex but does nothing. +*/
#define htmlmodify_yywrap() 1
#endif

/*+ Reset the current string. +*/
#define RESET_STRING \
{if(!string) string=malloc(stringallocsize=64); *string=0; stringlen=0; \
 stextcommopen=0; stextcommclose=0; stextstart=-1; stextend=-1;}

/*+ append information to the current string. +*/
#define APPEND_STRING(xx,len) \
{size_t _new_len=stringlen+(len); \
 if(_new_len>=stringallocsize) string=(char*)realloc((void*)string,stringallocsize=_new_len+1); \
 *((char *)mempcpy(string+stringlen,xx,(len)))=0; \
 stringlen=_new_len;}

#define APPEND_STRING_YYTEXT APPEND_STRING(htmlmodify_yytext,htmlmodify_yyleng)

/*+ Don't include the yyinput() or input() function in the lexer. +*/
#define YY_NO_INPUT

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
{if((result=wwwoffles_read_data(buf,max_size))==-1) result=0;}

#define WRITE_YYTEXT yy_write(htmlmodify_yytext,htmlmodify_yyleng)
#define WRITE_COMMENT_YYTEXT yy_comwrite(htmlmodify_yytext,htmlmodify_yyleng,commentout)

#define YY_RETURN(type,str,len) {htmlmodify_yylval=str; htmlmodify_yylval_len=len; return(type);}
%}

%%
 /* Must use static variables since the parser returns often. */
 static char *string=NULL;
 static size_t stringallocsize=0,stringlen=0;

 static int after_tag=INITIAL;

 /* Handle comments and other tags */

[^<]+                       { if(!disable_output) handle_text(htmlmodify_yytext,htmlmodify_yyleng);
                              YY_RETURN(LEX_PLAINTEXT,htmlmodify_yytext,htmlmodify_yyleng); }

"<!DOCTYPE"                 { RESET_STRING; if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;}
                              BEGIN(DOCTYPE); }
"<!--"                      { RESET_STRING; if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;}
                              stextstart=stringlen; BEGIN(COMMENT); }
"<!"{W}*"-"*                { RESET_STRING; if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;}
	                      stextstart=stringlen; BEGIN(COMMENT_BAD); }
"<"{W}*                     { RESET_STRING; APPEND_STRING_YYTEXT; BEGIN(TAG_START); }

 /* Doctype (DTD) */

<DOCTYPE>">"                { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;}
                              BEGIN(INITIAL); YY_RETURN(LEX_DOCTYPE,string,stringlen); }
<DOCTYPE>[^>]+              { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;} }

 /* Comments - COMMENT_BAD is not a legal comment format (except <!>) but people use it as one.
               COMMENT is not strictly correct, but works better than the real thing. */

<COMMENT>"--"{W}*">"        |
<COMMENT_BAD>">"            { stextend=stringlen; if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;}
                              BEGIN(INITIAL); YY_RETURN(LEX_COMMENT,string,stringlen); }
<COMMENT>"-"                |
<COMMENT>[^-]+              |
<COMMENT_BAD>[^>]+          { if(!disable_output) WRITE_YYTEXT; if(disable_output || parsecomment) APPEND_STRING_YYTEXT; }

 /* Miscellaneous stuff delimited by < and > that doesn't fit elsewhere. */

<MISC>[^<>]*">"             |
<MISC>[^<>]+                { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;}
                              BEGIN(INITIAL); YY_RETURN(LEX_MISC,string,stringlen); }
<MISC>"<"                   { unput(htmlmodify_yytext[0]); BEGIN(INITIAL); YY_RETURN(LEX_MISC,string,stringlen); }

 /* Tags */

<TAG_START>"script"/{tagdel}  { BEGIN(TAG); after_tag=SCRIPT_START; YY_RETURN(LEX_TAG_BEGIN,htmlmodify_yytext,htmlmodify_yyleng); }
<TAG_START>"style"/{tagdel}   { BEGIN(TAG); after_tag=STYLE_START;  YY_RETURN(LEX_TAG_BEGIN,htmlmodify_yytext,htmlmodify_yyleng); }
<TAG_START>"/"?{tag}/{tagdel} { BEGIN(TAG); after_tag=INITIAL;      YY_RETURN(LEX_TAG_BEGIN,htmlmodify_yytext,htmlmodify_yyleng); }
<TAG_START>(.|\n)             { unput(htmlmodify_yytext[0]); if(!disable_output) yy_write(string,stringlen); BEGIN(MISC); }

<TAG>{W}+                   { if(!disable_output) WRITE_YYTEXT; }
<TAG>"/>"                   { BEGIN(INITIAL); YY_RETURN(LEX_TAG_END_XHTML,"",0); }
<TAG>">"                    { BEGIN(after_tag); if(after_tag!=INITIAL) {RESET_STRING;} YY_RETURN(LEX_TAG_END,"",0); }
<TAG>"<"                    { unput(htmlmodify_yytext[0]); BEGIN(after_tag); if(after_tag!=INITIAL) {RESET_STRING;}
                              YY_RETURN(LEX_TAG_END_TRUNC,"",0); }
<TAG>{key}                  { BEGIN(TAG_ATTR_KEY); YY_RETURN(LEX_ATTR_KEY,htmlmodify_yytext,htmlmodify_yyleng); }
<TAG>(.|\n)                 { if(!disable_output) WRITE_YYTEXT; }

<TAG_ATTR_KEY>{W}*=         { BEGIN(TAG_ATTR_VAL); }
<TAG_ATTR_KEY>(.|\n)        { unput(htmlmodify_yytext[0]); BEGIN(TAG); YY_RETURN(LEX_ATTR_VAL,NULL,0); }

<TAG_ATTR_VAL>\"            { BEGIN(DQUOTED); RESET_STRING; }
<TAG_ATTR_VAL>\'            { BEGIN(SQUOTED); RESET_STRING; }
<TAG_ATTR_VAL>{W}+          { }
<TAG_ATTR_VAL>{val}         { BEGIN(TAG);                              YY_RETURN(LEX_ATTR_VAL,htmlmodify_yytext,htmlmodify_yyleng); }
<TAG_ATTR_VAL>(.|\n)        { unput(htmlmodify_yytext[0]); BEGIN(TAG); YY_RETURN(LEX_ATTR_VAL,"",0); }

 /* Quoted strings */

<DQUOTED>\"                 { BEGIN(TAG); YY_RETURN(LEX_ATTR_VAL_DQ,string,stringlen); }
<DQUOTED>[^\"]+             { APPEND_STRING_YYTEXT; }

<SQUOTED>\'                 { BEGIN(TAG); YY_RETURN(LEX_ATTR_VAL_SQ,string,stringlen); }
<SQUOTED>[^\']+             { APPEND_STRING_YYTEXT; }

 /* Scripts */

<SCRIPT_START>{W}+          { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;} }
<SCRIPT_START>"<"/"/script"{tagdel} {
                              BEGIN(TAG_START); YY_RETURN(LEX_SCRIPTTEXT,string,stringlen); }
<SCRIPT_START>"<!--"        { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;}
                              stextcommopen=1; stextstart=stringlen; BEGIN(SCRIPT); }
<SCRIPT_START>(.|\n)        { unput(htmlmodify_yytext[0]);
                              stextcommopen=0; stextstart=stringlen; if(!disable_output && commentout) yy_rawout("<!--\n");
                              BEGIN(SCRIPT); }

<SCRIPT>"-->"{W}*"<"/"/script"{tagdel} {
                              stextcommclose=1; stextend=stringlen;
		              if(disable_output) {APPEND_STRING(htmlmodify_yytext,htmlmodify_yyleng-1);}
			      else {yy_write(htmlmodify_yytext,htmlmodify_yyleng-1);}
                              BEGIN(TAG_START); YY_RETURN(LEX_SCRIPTTEXT,string,stringlen); }
<SCRIPT>"-"+                { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_COMMENT_YYTEXT;} }
<SCRIPT>"<"/"/script"{tagdel} {
                              stextcommclose=0; stextend=stringlen;
			      if(!disable_output && commentout) yy_rawout("//-->\n");
                              BEGIN(TAG_START); YY_RETURN(LEX_SCRIPTTEXT,string,stringlen); }
<SCRIPT>"<"                 |
<SCRIPT>[^-<]+              { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;} }

 /* Styles */

<STYLE_START>{W}+           { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;} }
<STYLE_START>"<"/"/style"{tagdel} {
                              BEGIN(TAG_START); YY_RETURN(LEX_STYLETEXT,string,stringlen); }
<STYLE_START>"<!--"         { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;}
                              stextcommopen=1; stextstart=stringlen; BEGIN(STYLE); }
<STYLE_START>(.|\n)         { unput(htmlmodify_yytext[0]);
                              stextcommopen=0; stextstart=stringlen; if(!disable_output && commentout) yy_rawout("<!--\n");
                              BEGIN(STYLE); }

<STYLE>"-->"{W}*"<"/"/style"{tagdel} {
                              stextcommclose=1; stextend=stringlen;
		              if(disable_output) {APPEND_STRING(htmlmodify_yytext,htmlmodify_yyleng-1);}
			      else {yy_write(htmlmodify_yytext,htmlmodify_yyleng-1);}
                              BEGIN(TAG_START); YY_RETURN(LEX_STYLETEXT,string,stringlen); }
<STYLE>"-"+                 { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_COMMENT_YYTEXT;} }
<STYLE>"<"/"/style"{tagdel} { stextcommclose=0; stextend=stringlen; if(!disable_output && commentout) yy_rawout("\n-->\n");
                              BEGIN(TAG_START); YY_RETURN(LEX_STYLETEXT,string,stringlen); }
<STYLE>"<"                  |
<STYLE>[^-<]+               { if(disable_output) {APPEND_STRING_YYTEXT;} else {WRITE_YYTEXT;} }

 /* End of file */

<<EOF>>                     { free(string); string=NULL; stringallocsize=stringlen=0;
                              after_tag=INITIAL; BEGIN(INITIAL); htmlmodify_yylval=NULL; htmlmodify_yylval_len=0;
			      return(0); }

%%
