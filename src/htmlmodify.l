W               [ \t\r\n]
F               [-a-z0-9$_.!*(),%;/?:@&=+~|#]
K               [a-z0-9-]

%x DOCTYPE
%x COMMENT COMMENT_BAD
%x TAG_START TAG TAG_ATTR_KEY TAG_ATTR_VAL
%x DQUOTED SQUOTED
%x SCRIPT_START SCRIPT SCRIPT_COMMENT_C SCRIPT_COMMENT_CPP SCRIPT_DQUOTED SCRIPT_SQUOTED
%x STYLE_START STYLE

%{
/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/htmlmodify.l 1.54 2002/10/28 19:00:35 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7g.
  Parse the HTML and modify the source.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02 Andrew M. Bishop
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
#include "document.h"
#include "config.h"
#include "misc.h"


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
 tag__a        = 0  /* "/a"        */ ,
 tag_base      = 1  /* "base"      */ ,
 tag_blink     = 2  /* "blink"     */ ,
 tag__blink    = 3  /* "/blink"    */ ,
 tag__body     = 4  /* "/body"     */ ,
 tag__html     = 5  /* "/html"     */ ,
 tag_noscript  = 6  /* "noscript"  */ ,
 tag__noscript = 7  /* "/noscript" */ ,
 tag_script    = 8  /* "script"    */ ,
 tag__script   = 9  /* "/script"   */ ,
 tag__style    =10  /* "/style"    */ ,
 tag_applet    =11  /* "applet"    */ ,
 tag__applet   =12  /* "/applet"   */ ,
 tag_param     =13  /* "param"     */ ,
 tag__object   =14  /* "/object"   */ ,
 tag__iframe   =15  /* "/iframe"   */ ,
 tag__embed    =16  /* "/embed"    */ ,

 tag_complex   =17  /* Complex tags, stored and processed as a whole. */,

 tag_style     =17  /* "style"     */ ,
 tag_meta      =18  /* "meta"      */ ,
 tag_link      =19  /* "link"      */ ,
 tag_object    =20  /* "object"    */ ,
 tag_a         =21  /* "a"         */ ,
 tag_iframe    =22  /* "iframe"    */ ,
 tag_img       =23  /* "img"       */ ,
 tag_embed     =24  /* "embed"     */ ,

 tag_ntags     =25
}
HTMLTags;

/*+ Tag strings +*/

static char *tags[]=
{
 /* tag__a        = 0  */  "/a"        ,
 /* tag_base      = 1  */  "base"      ,
 /* tag_blink     = 2  */  "blink"     ,
 /* tag__blink    = 3  */  "/blink"    ,
 /* tag__body     = 4  */  "/body"     ,
 /* tag__html     = 5  */  "/html"     ,
 /* tag_noscript  = 6  */  "noscript"  ,
 /* tag__noscript = 7  */  "/noscript" ,
 /* tag_script    = 8  */  "script"    ,
 /* tag__script   = 9  */  "/script"   ,
 /* tag__style    =10  */  "/style"    ,
 /* tag_applet    =11  */  "applet"    ,
 /* tag__applet   =12  */  "/applet"   ,
 /* tag_param     =13  */  "param"     ,
 /* tag__object   =14  */  "/object"   ,
 /* tag__iframe   =15  */  "/iframe"   ,
 /* tag__embed    =16  */  "/embed"    ,

 /* tag_style     =16  */  "style"     ,
 /* tag_meta      =17  */  "meta"      ,
 /* tag_link      =18  */  "link"      ,
 /* tag_object    =19  */  "object"    ,
 /* tag_a         =20  */  "a"         ,
 /* tag_iframe    =21  */  "iframe"    ,
 /* tag_img       =22  */  "img"       ,
 /* tag_embed     =23  */  "embed"     
};

/*+ Attribute types +*/

typedef enum _HTMLAttributes
{
 att_content     = 0  /* "content"     */ ,
 att_href        = 1  /* "href"        */ ,
 att_http_equiv  = 2  /* "http-equiv"  */ ,
 att_onblur      = 3  /* "onblur"      */ ,
 att_onchange    = 4  /* "onchange"    */ ,
 att_onclick     = 5  /* "onclick"     */ ,
 att_ondblclick  = 6  /* "ondblclick"  */ ,
 att_onfocus     = 7  /* "onfocus"     */ ,
 att_onkeydown   = 8  /* "onkeydown"   */ ,
 att_onkeypress  = 9  /* "onkeypress"  */ ,
 att_onload      =10  /* "onload"      */ ,
 att_onmousedown =11  /* "onmousedown" */ ,
 att_onmousemove =12  /* "onmousemove" */ ,
 att_onmouseout  =13  /* "onmouseout"  */ ,
 att_onmouseover =14  /* "onmouseover" */ ,
 att_onmouseup   =15  /* "onmouseup"   */ ,
 att_onreset     =16  /* "onreset"     */ ,
 att_onselect    =17  /* "onselect"    */ ,
 att_onsubmit    =18  /* "onsubmit"    */ ,
 att_onunload    =19  /* "onunload"    */ ,
 att_rel         =20  /* "rel"         */ ,
 att_style       =21  /* "style"       */ ,
 att_codetype    =22  /* "codetype"    */ ,
 att_classid     =23  /* "classid"     */ ,
 att_src         =24  /* "src"         */ ,
 att_width       =25  /* "width"       */ ,
 att_height      =26  /* "height"      */ ,
 att_data        =27  /* "data"        */ ,
 att_type        =28  /* "type"        */ ,

 att_natts       =29
}
HTMLAttributes;

/*+ Attribute strings. +*/

static char *attributes[]=
{
 /* att_content     = 0 */ "content"     ,
 /* att_href        = 1 */ "href"        ,
 /* att_http_equiv  = 2 */ "http-equiv"  ,
 /* att_onblur      = 3 */ "onblur"      ,
 /* att_onchange    = 4 */ "onchange"    ,
 /* att_onclick     = 5 */ "onclick"     ,
 /* att_ondblclick  = 6 */ "ondblclick"  ,
 /* att_onfocus     = 7 */ "onfocus"     ,
 /* att_onkeydown   = 8 */ "onkeydown"   ,
 /* att_onkeypress  = 9 */ "onkeypress"  ,
 /* att_onload      =10 */ "onload"      ,
 /* att_onmousedown =11 */ "onmousedown" ,
 /* att_onmousemove =12 */ "onmousemove" ,
 /* att_onmouseout  =13 */ "onmouseout"  ,
 /* att_onmouseover =14 */ "onmouseover" ,
 /* att_onmouseup   =15 */ "onmouseup"   ,
 /* att_onreset     =16 */ "onreset"     ,
 /* att_onselect    =17 */ "onselect"    ,
 /* att_onsubmit    =18 */ "onsubmit"    ,
 /* att_onunload    =19 */ "onunload"    ,
 /* att_rel         =20 */ "rel"         ,
 /* att_style       =21 */ "style"       ,
 /* att_codetype    =22 */ "codetype"    ,
 /* att_classid     =23 */ "classid"     ,
 /* att_src         =24 */ "src"         ,
 /* att_width       =25 */ "width"       ,
 /* att_height      =26 */ "height"      ,
 /* att_data        =27 */ "data"        ,
 /* att_type        =28 */ "type"        
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


/* Ideas taken from the public domain Demoroniser perl script */

/*************************************************/
/* De-moron-ise Text from Microsoft Applications */
/*         by John Walker -- January 1998        */
/*            http://www.fourmilab.ch/           */
/*************************************************/

/* Microsoft Character mapping */

static int demoronise_ms_chars;

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

/* Definitions of why the output is disabled. */

#define DISABLE_NONE         0

#define DISABLE_META         1
#define DISABLE_LINK         2
#define DISABLE_OBJECT       4
#define DISABLE_A            8
#define DISABLE_IFRAME      16
#define DISABLE_IMG         32
#define DISABLE_STYLE       64

#define DISABLE_PARSE      256

/* Local functions */

static void modify_html(URL *Url);

static /*@null@*/ char *htmlmodify_yylval=NULL;
extern int htmlmodify_yylex(void);

static int handle_a_tag(Tag *tag,int disable_html_dontget_anchors);
static int handle_iframe_tag(Tag *tag,int disable_html_dontget_iframes);
static void handle_img_tag(Tag *tag,int replace_dontget,char *dontget_replacement,
                                    int replace_webbug,char *webbug_replacement);
static int handle_object_tag(Tag *tag,int replace_dontget,char *dontget_replacement,
                                      int replace_webbug,char *webbug_replacement,
                                      int disable_html_applet,
                                      int disable_html_flash,
                                      int disable_html_dontget_iframes);
static void output_img_or_object_tag(Tag *tag,int src_att,
                                              int replace_dontget,char *dontget_replacement,
                                              int replace_webbug,char *webbug_replacement);
static void handle_link_tag(Tag *tag,int disable_html_style);
static int handle_style_script_tag(Tag *tag,int disable_html_script);
static void handle_meta_tag(Tag *tag,int disable_html_metarefresh,int disable_html_metarefresh_self);
static void output_tag(Tag *tag,char *prefix,char *suffix);


/*+ The file descriptor to output to. +*/
static int output_fd=-1;

/*+ The add-cache-info optional footer. +*/
static /*@null@*/ /*@observer@*/ char *cache_info=NULL;

/*+ The file descriptor that we are reading from. +*/
static int htmlmodify_yyfd=-1;
static int htmlmodify_read_err=0;

/*+ The base URL of this page. +*/
static /*@null@*/ URL *baseUrl=NULL;

/*+ Set this to disable the output. +*/
static int disable_output=DISABLE_NONE;


/*++++++++++++++++++++++++++++++++++++++
  Output the file with the modificatons if it is HTML, else just output.
  Returns -1 if error was encountered while reading spool.

  int client The file to write to.

  int spool The file to read from.

  URL *Url The URL that we are parsing.
  ++++++++++++++++++++++++++++++++++++++*/

int OutputHTMLWithModifications(int client,int spool,URL *Url)
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

    cache_info=HTMLMessageBody(-1,"AddCacheInfo",
                               "url",Url->name,
                               "date",date,
                               "time",timeago,
                               "unit",timeunit,
                               NULL);
   }

 baseUrl=Url;

 output_fd=client;
 htmlmodify_yyfd=spool;
 htmlmodify_read_err=0;
 if(!first)
    htmlmodify_yyrestart(NULL);

 modify_html(Url);

 cache_info=NULL;

 first=0;
 return(htmlmodify_read_err);
}


/*+ A macro to output the data if valid to do so. +*/
#define YY_OUTPUT(text) \
           if(!disable_output) \
              write_string(output_fd,text)


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the anchor tag and parse it.

  int handle_a_tag Returns 1 if the anchor was disabled.

  Tag *tag The tag information.

  int disable_html_dontget_anchors The option to disable links to URLs that are not got.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_a_tag(Tag *tag,int disable_html_dontget_anchors)
{
 int i;
 int is_dontget=0;
 char *href=NULL;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_href && tag->attr_val[i])
       href=tag->attr_val[i];

 if(href)
   {
    char *link=LinkURL(baseUrl,href);
    URL* linkUrl=SplitURL(link);

    is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

    if(link!=href)
       free(link);
    FreeURL(linkUrl);
   }

 /* Output the original or modified tag. */

 if(disable_html_dontget_anchors && is_dontget)
    output_tag(tag,"!-- WWWOFFLE (disable-dontget-links) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return(is_dontget);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the iframe tag and parse it.

  int handle_iframe_tag Returns 1 if the iframe was disabled.

  Tag *tag The tag information.

  int disable_html_dontget_iframes The option to disable iframes to URLs that are not got.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_iframe_tag(Tag *tag,int disable_html_dontget_iframes)
{
 int i;
 int is_dontget=0;
 char *src=NULL;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_src && tag->attr_val[i])
       src=tag->attr_val[i];

 if(src)
   {
    char *link=LinkURL(baseUrl,src);
    URL* linkUrl=SplitURL(link);

    is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

    if(link!=src)
       free(link);
    FreeURL(linkUrl);
   }

 /* Output the original or modified tag. */

 if(disable_html_dontget_iframes && is_dontget)
    output_tag(tag,"!-- WWWOFFLE (disable-dontget-iframes) - "," --");
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

static void handle_img_tag(Tag *tag,int replace_dontget,char *dontget_replacement,
                                    int replace_webbug,char *webbug_replacement)
{
 int i;
 int src_att=-1;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_src && tag->attr_val[i])
       src_att=i;

 output_img_or_object_tag(tag,src_att,replace_dontget,dontget_replacement,replace_webbug,webbug_replacement);
}


/*++++++++++++++++++++++++++++++++++++++
  Output an image tag or an object tag that contains an image.

  Tag *tag The tag.

  int src_att The attribute number the contains the URI.

  int replace_dontget The option to replace the images in the DontGet section.

  char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  char *webbug_replacement The webbug replacement image.
  ++++++++++++++++++++++++++++++++++++++*/

static void output_img_or_object_tag(Tag *tag,int src_att,
                                              int replace_dontget,char *dontget_replacement,
                                              int replace_webbug,char *webbug_replacement)
{
 int i;
 int is_dontget=0,is_webbug=0;

 if(src_att>=0)
   {
    if(replace_dontget)
      {
       char *link=LinkURL(baseUrl,tag->attr_val[src_att]);
       URL* linkUrl=SplitURL(link);

       is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

       if(link!=tag->attr_val[src_att])
          free(link);
       FreeURL(linkUrl);
      }

    if(replace_webbug)
      {
       int width=1000,height=1000;

       for(i=0;i<tag->nattr;i++)
          if(tag->attr_type[i]==att_width && tag->attr_val[i])
             width=atoi(tag->attr_val[i]);
          else if(tag->attr_type[i]==att_height && tag->attr_val[i])
             height=atoi(tag->attr_val[i]);

       if(width<=1 && height<=1)
          is_webbug=1;
      }
   }

 /* Modify the src attribute (if required). */

 if(is_dontget)
   {
    output_tag(tag,"!-- WWWOFFLE (replace-dontget-images) - "," --");

    if(tag->attr_val[src_att]) free(tag->attr_val[src_att]);
    tag->attr_val[src_att]=strdup(dontget_replacement);
   }
 else if(is_webbug)
   {
    output_tag(tag,"!-- WWWOFFLE (replace-webbug-images) - "," --");

    if(tag->attr_val[src_att]) free(tag->attr_val[src_att]);
    tag->attr_val[src_att]=strdup(webbug_replacement);
   }

 /* Blank the alt attribute or add an empty one (if required). */

 if(is_dontget || is_webbug)
   {
    int seen_alt=0;

    for(i=0;i<tag->nattr;i++)
       if(!strcasecmp(tag->attr_key[i],"alt"))
         {
          if(tag->attr_val[i])
             free(tag->attr_val[i]);

          tag->attr_val  [i]=(char *)calloc(1,1);
          tag->attr_quote[i]="\"";

          seen_alt=1;
         }

    if(!seen_alt)
      {
       if(tag->nattr==tag->nattr_malloc)
         {
          tag->attr_type=(int*)realloc((void*)tag->attr_type,(tag->nattr_malloc+1)*sizeof(int));
          tag->attr_key=(char**)realloc((void*)tag->attr_key,(tag->nattr_malloc+1)*sizeof(char*));
          tag->attr_val=(char**)realloc((void*)tag->attr_val,(tag->nattr_malloc+1)*sizeof(char*));
          tag->attr_quote=(char**)realloc((void*)tag->attr_quote,(tag->nattr_malloc+1)*sizeof(char*));

          tag->attr_key[tag->nattr_malloc]=NULL;
          tag->attr_val[tag->nattr_malloc]=NULL;

          tag->nattr_malloc+=1;
         }

       tag->attr_type [tag->nattr]=att_natts;
       tag->attr_key  [tag->nattr]=(char *)malloc(4); strcpy(tag->attr_key[tag->nattr],"alt");
       tag->attr_val  [tag->nattr]=(char *)calloc(1,1);
       tag->attr_quote[tag->nattr]="\"";

       tag->nattr++;
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

  Tag *tag The tag information.

  int replace_dontget The option to replace the images in the DontGet section.

  char *dontget_replacement The DontGet replacement image.

  int replace_webbug The option to replace the 1x1 pixel webbug images.

  char *webbug_replacement The webbug replacement image.

  int disable_html_applet The option to disable Java applets.

  int disable_html_flash The option to disable Flash animations.

  int disable_html_dontget_iframes The option to disable inline frames that are on the dontget list.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_object_tag(Tag *tag,int replace_dontget,char *dontget_replacement,
                                      int replace_webbug,char *webbug_replacement,
                                      int disable_html_applet,
                                      int disable_html_flash,
                                      int disable_html_dontget_iframes)
{
 int i;
 int is_image=0,is_java=0,is_flash=0,is_inline=0,is_dontget=0;
 int data_att=-1;

 /* Check for images. */

 if(replace_dontget || replace_webbug)
   {
    for(i=0;i<tag->nattr;i++)
       if((tag->attr_type[i]==att_codetype && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"image")) ||
          (tag->attr_type[i]==att_type && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"image")))
          is_image=1;
       else if(tag->attr_type[i]==att_data && tag->attr_val[i])
          data_att=i;
   }

 /* Check for Java */

 if(disable_html_applet)
   {
    for(i=0;i<tag->nattr;i++)
       if(tag->attr_type[i]==att_codetype && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"application/java"))
          is_java=1;
       else if(tag->attr_type[i]==att_classid && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"java:"))
          is_java=1;
   }

 /* Check for Flash */

 if(disable_html_flash)
   {
    for(i=0;i<tag->nattr;i++)
       if((tag->attr_type[i]==att_codetype || tag->attr_type[i]==att_type) &&
          tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"application/x-shockwave-flash"))
          is_flash=1;
       else if(tag->attr_type[i]==att_classid && tag->attr_val[i] &&
               !strcasecmp_litbeg(tag->attr_val[i],"clsid:D27CDB6E-AE6D-11cf-96B8-444553540000"))
          is_flash=1;
       else if(tag->attr_type[i]==att_src && tag->attr_val[i] &&
               !strcasecmp_litend(tag->attr_val[i],".swf"))
          is_flash=1;
   }

 /* Check for inline HTML (text) object */

 if(disable_html_dontget_iframes)
   {
    for(i=0;i<tag->nattr;i++)
       if((tag->attr_type[i]==att_codetype && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"text")) ||
          (tag->attr_type[i]==att_type && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"text")))
          is_inline=1;
       else if(tag->attr_type[i]==att_data && tag->attr_val[i])
         {
          char *link=LinkURL(baseUrl,tag->attr_val[i]);
          URL* linkUrl=SplitURL(link);

          is_dontget=ConfigBooleanMatchURL(DontGet,linkUrl);

          if(link!=tag->attr_val[i])
             free(link);
          FreeURL(linkUrl);
         }
   }

 /* Output the original or modified tag. */

 if(is_image && (replace_dontget || replace_webbug))
    output_img_or_object_tag(tag,data_att,replace_dontget,dontget_replacement,replace_webbug,webbug_replacement);
 else if(disable_html_applet && is_java)
    output_tag(tag,"!-- WWWOFFLE (disable-applet) - "," --");
 else if(disable_html_flash && is_flash)
    output_tag(tag,"!-- WWWOFFLE (disable-flash) - "," --");
 else if(disable_html_dontget_iframes && is_inline && is_dontget)
    output_tag(tag,"!-- WWWOFFLE (disable-dontget-iframes) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return(is_image?0:
        (disable_html_applet && is_java)?1:
        (disable_html_flash && is_flash)?2:
        (disable_html_dontget_iframes && is_inline && is_dontget)?3:
        0);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the link tag and parse it.

  Tag *tag The tag information.

  int disable_html_style Set to true if stylesheets are disabled.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_link_tag(Tag *tag,int disable_html_style)
{
 int i;
 int is_stylesheet=0;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_rel && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"Stylesheet"))
       is_stylesheet=1;

 /* Output the original or modified tag. */

 if(disable_html_style && is_stylesheet)
    output_tag(tag,"!-- WWWOFFLE (disable-style) - "," --");
 else
    output_tag(tag,NULL,NULL);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the style tag and parse it.

  int handle_style_script_tag Returns true if a script was detected.

  Tag *tag The tag information.

  int disable_html_script Set to true if scripts are disabled.

  See "WhiteHat Security Advisory [Number: WH-08152001-1]" for the details of this.

  The HTML <style type="application/x-javascript"> or <style type="text/javascript">
  can introduce Javascript that WWWOFFLE would not otherwise have blocked.
  ++++++++++++++++++++++++++++++++++++++*/

static int handle_style_script_tag(Tag *tag,int disable_html_script)
{
 int i;
 int is_really_script=0;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_type && tag->attr_val[i])
      {
       int j=strlen(tag->attr_val[i])-10;

       while(j>=0) /* strcasestr() */
         {
          if(!strcasecmp_litbeg(&tag->attr_val[i][j],"javascript"))
             is_really_script=1;
          j--;
         }
      }

 /* Output the original or modified tag. */

 if(disable_html_script && is_really_script)
    output_tag(tag,"!-- WWWOFFLE (disable-script) - "," --");
 else
    output_tag(tag,NULL,NULL);

 return(is_really_script);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the meta tag and parse it.

  Tag *tag The tag information.

  int disable_html_metarefresh Set to the disable-meta-refresh option.

  int disable_html_metarefresh_self Set to the disable-meta-refresh-self option.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_meta_tag(Tag *tag,int disable_html_metarefresh,int disable_html_metarefresh_self)
{
 int i;
 int is_meta_http_equiv_refresh=0;
 char *meta_refresh=NULL;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_http_equiv && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"Refresh"))
       is_meta_http_equiv_refresh=1;

 if(is_meta_http_equiv_refresh)
    for(i=0;i<tag->nattr;i++)
       if(tag->attr_type[i]==att_content && tag->attr_val[i] && tag->attr_val[i][0])
         {
          char *p;

          /* ' *[0-9].?[0-9]* *[;,] *(URL *= *|)http://...' */

          p=tag->attr_val[i];
          while(isspace(*p)) p++;
          if(!isdigit(*p))
             ; /* unparseable */
          else
            {
             while(isdigit(*p)) p++;
             if(*p=='.')
               {p++; while(isdigit(*p)) p++;}
             while(isspace(*p)) p++;
             if(!*p)
               {
                meta_refresh=strdup(baseUrl->name);
               }
             else if(*p!=';' && *p!=',')
                ; /* unparseable */
             else
               {
                p++;
                while(isspace(*p)) p++;
                if(!strcasecmp_litbeg(p,"URL"))
                  {
                   p+=3;
                   while(isspace(*p)) p++;
                   if(*p!='=')
                      ; /* unparseable */
                   else
                     {
                      p++;
                      while(isspace(*p)) p++;
                     }
                   if(!*p)
                      ; /* unparseable */
                   else
                     {
                      meta_refresh=strdup(p);
                     }
                  }
               }
            }
         }

 /* Check if link is to itself. */

 if(meta_refresh && disable_html_metarefresh_self && !disable_html_metarefresh)
   {
    char *link=LinkURL(baseUrl,meta_refresh);
    URL* linkUrl=SplitURL(link);

    if(link!=meta_refresh)
       free(link);

    if(strcmp(baseUrl->name,linkUrl->name))
      {
       free(meta_refresh);
       meta_refresh=NULL;
      }

    FreeURL(linkUrl);
   }

 /* Output the original or modified tag. */

 if(meta_refresh && disable_html_metarefresh)
    output_tag(tag,"!-- WWWOFFLE (disable-meta-refresh) - "," --");
 else if(meta_refresh && disable_html_metarefresh_self)
    output_tag(tag,"!-- WWWOFFLE (disable-meta-refresh-self) - "," --");
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
    YY_OUTPUT(" ");
    YY_OUTPUT(tag->attr_key[i]);
    if(tag->attr_val[i])
      {
       YY_OUTPUT("=");
       if(tag->attr_quote[i][0]) {YY_OUTPUT(tag->attr_quote[i]);}
       YY_OUTPUT(tag->attr_val[i]);
       if(tag->attr_quote[i][0]) {YY_OUTPUT(tag->attr_quote[i]);}
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
 HTMLTags tag=tag_ntags;
 HTMLAttributes key=att_natts;
 int url_cached=0;
 int yychar,i;
 int disable_key_val;
 char *key_string=NULL,*prefix,*suffix,*quote;
 Tag tagdata;

 char *anchor_modify_begin[3];
 char *anchor_modify_end[3];
 int disable_html_script=ConfigBooleanURL(DisableHTMLScript,Url);
 int disable_html_applet=ConfigBooleanURL(DisableHTMLApplet,Url);
 int object_nesting=0,disable_html_object[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
 int disable_html_style=ConfigBooleanURL(DisableHTMLStyle,Url),disable_html_style_script=0;
 int disable_html_blink=ConfigBooleanURL(DisableHTMLBlink,Url);
 int disable_html_flash=ConfigBooleanURL(DisableHTMLFlash,Url);
 int disable_html_metarefresh=ConfigBooleanURL(DisableHTMLMetaRefresh,Url);
 int disable_html_metarefresh_self=ConfigBooleanURL(DisableHTMLMetaRefreshSelf,Url);
 int disable_html_dontget_anchors=ConfigBooleanURL(DisableHTMLDontGetAnchors,Url),disable_html_anchor=0;
 int disable_html_dontget_iframes=ConfigBooleanURL(DisableHTMLDontGetIFrames,Url),disable_html_iframe=0;
 int replace_html_dontget_images=ConfigBooleanURL(ReplaceHTMLDontGetImages,Url);
 char *replacement_html_dontget_image=ConfigStringURL(ReplacementHTMLDontGetImage,Url);
 int replace_html_webbug_images=ConfigBooleanURL(ReplaceHTMLWebbugImages,Url);
 char *replacement_html_webbug_image=ConfigStringURL(ReplacementHTMLWebbugImage,Url);

 anchor_modify_begin[0]=ConfigStringURL(AnchorModifyBegin[0],Url);
 anchor_modify_begin[1]=ConfigStringURL(AnchorModifyBegin[1],Url);
 anchor_modify_begin[2]=ConfigStringURL(AnchorModifyBegin[2],Url);
 anchor_modify_end[0]=ConfigStringURL(AnchorModifyEnd[0],Url);
 anchor_modify_end[1]=ConfigStringURL(AnchorModifyEnd[1],Url);
 anchor_modify_end[2]=ConfigStringURL(AnchorModifyEnd[2],Url);

 demoronise_ms_chars=ConfigBooleanURL(DemoroniseMSChars,Url);

 if(*replacement_html_dontget_image=='/')
   {
    char *copy=replacement_html_dontget_image;
    char *localhost=GetLocalHost(1);

    replacement_html_dontget_image=(char*)alloca(sizeof("http://")+strlen(localhost)+strlen(copy));

    stpcpy(stpcpy(stpcpy(replacement_html_dontget_image,"http://"),localhost),copy);

    free(localhost);
   }

 if(*replacement_html_webbug_image=='/')
   {
    char *copy=replacement_html_webbug_image;
    char *localhost=GetLocalHost(1);

    replacement_html_webbug_image=(char*)alloca(sizeof("http://")+strlen(localhost)+strlen(copy));

    stpcpy(stpcpy(stpcpy(replacement_html_webbug_image,"http://"),localhost),copy);

    free(localhost);
   }

 /* Initialise the tagdata */

 tagdata.type=tag_ntags;
 tagdata.tag=NULL;
 tagdata.xhtml=0;
 tagdata.nattr=0;
 tagdata.nattr_malloc=16;
 tagdata.attr_type=(int*)calloc(16,sizeof(int));
 tagdata.attr_key=(char**)calloc(16,sizeof(char*));
 tagdata.attr_val=(char**)calloc(16,sizeof(char*));
 tagdata.attr_quote=(char**)calloc(16,sizeof(char*));

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
       for(tag=0;tag<tag_ntags;tag++)
          if(!strcasecmp(htmlmodify_yylval,tags[tag]))
             break;

       if(tag>=tag_complex && tag<tag_ntags)
         {
          tagdata.type=tag;
          if(tagdata.tag) free(tagdata.tag);
          tagdata.tag=strdup(htmlmodify_yylval);
          tagdata.nattr=0;
          tagdata.xhtml=0;
         }

       prefix=NULL;

       if(tag==tag__a)
         {
          if(disable_html_anchor)
             prefix="!-- WWWOFFLE (disable-dontget-links) - ";
          else
            {
             if(url_cached==1)
               {if(anchor_modify_end[0]) {YY_OUTPUT(anchor_modify_end[0]);}}
             else if(url_cached==2)
               {if(anchor_modify_end[1]) {YY_OUTPUT(anchor_modify_end[1]);}}
             else if(url_cached==-1)
               {if(anchor_modify_end[2]) {YY_OUTPUT(anchor_modify_end[2]);}}
             url_cached=0;
            }
         }
       else if(tag==tag__iframe && disable_html_iframe)
          prefix="!-- WWWOFFLE (disable-dontget-iframes) - ";
       else if(tag==tag__body && cache_info)
         {YY_OUTPUT(cache_info); free(cache_info); cache_info=NULL;}
       else if(tag==tag__html && cache_info)
         {YY_OUTPUT(cache_info); free(cache_info); cache_info=NULL;}
       else if((tag==tag_blink || tag==tag__blink) && disable_html_blink)
          prefix="!-- WWWOFFLE (disable-blink) - ";
       else if((tag==tag_script || tag==tag__script) && disable_html_script)
          prefix="!-- WWWOFFLE (disable-script) - ";
       else if((tag==tag_noscript || tag==tag__noscript) && disable_html_script)
          prefix="!-- WWWOFFLE (disable-script) - ";
       else if((tag==tag_applet || tag==tag__applet) && disable_html_applet)
          prefix="!-- WWWOFFLE (disable-applet) - ";
       else if(tag==tag_param && disable_html_object[object_nesting]==1)
          prefix="!-- WWWOFFLE (disable-applet) - ";
       else if(tag==tag_param && disable_html_object[object_nesting]==2)
          prefix="!-- WWWOFFLE (disable-flash) - ";
       else if(tag==tag_param && disable_html_object[object_nesting]==3)
          prefix="!-- WWWOFFLE (disable-dontget-iframes) - ";
       else if(tag==tag__object && disable_html_object[object_nesting]==1)
          prefix="!-- WWWOFFLE (disable-applet) - ";
       else if(tag==tag__object && disable_html_object[object_nesting]==2)
          prefix="!-- WWWOFFLE (disable-flash) - ";
       else if(tag==tag__object && disable_html_object[object_nesting]==3)
          prefix="!-- WWWOFFLE (disable-dontget-iframes) - ";
       else if(tag==tag__embed && disable_html_object[object_nesting])
          prefix="!-- WWWOFFLE (disable-flash) - ";
       else if(tag==tag_style && disable_html_style)
          prefix="!-- WWWOFFLE (disable-style) - ";
       else if(tag==tag_style && disable_html_script)
          disable_output|=DISABLE_STYLE;
       else if(tag==tag__style && disable_html_style)
          prefix="!-- WWWOFFLE (disable-style) - ";
       else if(tag==tag__style && disable_html_style_script)
          prefix="!-- WWWOFFLE (disable-script) - ";
       else if(tag==tag_meta && (disable_html_metarefresh_self || disable_html_metarefresh))
          disable_output|=DISABLE_META;
       else if(tag==tag_link && disable_html_style)
          disable_output|=DISABLE_LINK;
       else if((tag==tag_object || tag==tag_embed) &&
               (disable_html_applet || disable_html_flash ||
                disable_html_dontget_iframes || replace_html_dontget_images || replace_html_webbug_images))
          disable_output|=DISABLE_OBJECT;
       else if(tag==tag_a && disable_html_dontget_anchors)
          disable_output|=DISABLE_A;
       else if(tag==tag_iframe && disable_html_dontget_iframes)
          disable_output|=DISABLE_IFRAME;
       else if(tag==tag_img && (replace_html_dontget_images || replace_html_webbug_images))
          disable_output|=DISABLE_IMG;

       disable_output&=~DISABLE_PARSE;

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

       if(tag==tag_applet)
         {if(object_nesting<sizeof(disable_html_object)/sizeof(disable_html_object[0])) object_nesting++;}

       if(tag==tag__a && disable_html_anchor)
         {suffix=" --";disable_html_anchor=0;}
       else if(tag==tag__iframe && disable_html_iframe)
         {suffix=" --";disable_html_iframe=0;}
       else if((tag==tag_blink || tag==tag__blink) && disable_html_blink)
         suffix=" --";
       else if((tag==tag_script || tag==tag__script) && disable_html_script)
         suffix=" --";
       else if((tag==tag_noscript || tag==tag__noscript) && disable_html_script)
         suffix=" --";
       else if(tag==tag_applet && disable_html_applet)
         {suffix=" --"; disable_html_object[object_nesting]=1;}
       else if(tag==tag__applet && disable_html_applet)
         {suffix=" --"; disable_html_object[object_nesting]=0;}
       else if(tag==tag_param && disable_html_object[object_nesting])
         suffix=" --";
       else if(tag==tag__object && disable_html_object[object_nesting])
         {suffix=" --"; disable_html_object[object_nesting]=0;}
       else if(tag==tag__embed && disable_html_object[object_nesting])
         {suffix=" --"; disable_html_object[object_nesting]=0;}
       else if((tag==tag_style || tag==tag__style) && disable_html_style)
         suffix=" --";
       else if((tag==tag_style || tag==tag__style) && disable_html_style)
         suffix=" --";
       else if(tag==tag__style && disable_html_style_script)
         {suffix="--"; disable_html_style_script=0;}

       if(tag==tag__object || tag==tag__applet || tag==tag__embed)
         {if(object_nesting>0) object_nesting--;}

       if(suffix)
         {YY_OUTPUT(suffix);}
       if(yychar==LEX_TAG_END_XHTML && !suffix)
         {YY_OUTPUT(" />");}
       else
         {YY_OUTPUT(">");}

       if(tag==tag_meta && (disable_html_metarefresh_self || disable_html_metarefresh))
         {
          disable_output&=~DISABLE_META;
          handle_meta_tag(&tagdata,disable_html_metarefresh,disable_html_metarefresh_self);
         }
       else if(tag==tag_link && disable_html_style)
         {
          disable_output&=~DISABLE_LINK;
          handle_link_tag(&tagdata,disable_html_style);
         }
       else if((tag==tag_object || tag==tag_embed) &&
               (disable_html_applet || disable_html_flash ||
                disable_html_dontget_iframes || replace_html_dontget_images || replace_html_webbug_images))
         {
          disable_output&=~DISABLE_OBJECT;
          if(object_nesting<sizeof(disable_html_object)/sizeof(disable_html_object[0])) object_nesting++;
          disable_html_object[object_nesting]=handle_object_tag(&tagdata,replace_html_dontget_images,replacement_html_dontget_image,
                                                                replace_html_webbug_images,replacement_html_webbug_image,
                                                                disable_html_applet,
                                                                disable_html_flash,
                                                                disable_html_dontget_iframes);
         }
       else if(tag==tag_a)
         {
          if(disable_html_dontget_anchors)
            {
             disable_output&=~DISABLE_A;
             disable_html_anchor=handle_a_tag(&tagdata,disable_html_dontget_anchors);
            }

          if(!disable_html_anchor)
            {
             if(url_cached==1)
               {if(anchor_modify_begin[0]) {YY_OUTPUT(anchor_modify_begin[0]);}}
             else if(url_cached==2)
               {if(anchor_modify_begin[1]) {YY_OUTPUT(anchor_modify_begin[1]);}}
             else if(url_cached==-1)
               {if(anchor_modify_begin[2]) {YY_OUTPUT(anchor_modify_begin[2]);}}
            }
         }
       else if(tag==tag_iframe)
         {
          if(disable_html_dontget_iframes)
            {
             disable_output&=~DISABLE_IFRAME;
             disable_html_iframe=handle_iframe_tag(&tagdata,disable_html_dontget_iframes);
            }
         }
       else if(tag==tag_img && (replace_html_dontget_images || replace_html_webbug_images))
         {
          disable_output&=~DISABLE_IMG;
          handle_img_tag(&tagdata,replace_html_dontget_images,replacement_html_dontget_image,
                                  replace_html_webbug_images,replacement_html_webbug_image);
         }
       else if(tag==tag_script && disable_html_script)
          disable_output|=DISABLE_PARSE;
       else if(tag==tag_style && disable_html_style)
          disable_output|=DISABLE_PARSE;
       else if(tag==tag_style && disable_html_script)
         {
          disable_output&=~DISABLE_STYLE;

          disable_html_style_script=handle_style_script_tag(&tagdata,disable_html_script);

          if(disable_html_style_script)
             disable_output|=DISABLE_PARSE;
         }

       tag=tag_ntags;
       key=att_natts;
       break;

      case LEX_ATTR_KEY:
       if(key_string) free(key_string);
       key_string=strdup(htmlmodify_yylval);

       for(key=0;key<att_natts;key++)
          if(!strcasecmp(htmlmodify_yylval,attributes[key]))
             break;
      break;

      case LEX_ATTR_VAL_DQ:
       /*@fallthrough@*/
      case LEX_ATTR_VAL_SQ:
       /*@fallthrough@*/
      case LEX_ATTR_VAL:
       disable_key_val=0;

       if(yychar==LEX_ATTR_VAL_DQ)
          quote="\"";
       else if(yychar==LEX_ATTR_VAL_SQ)
          quote="\'";
       else
          quote="";

       /* Links */

       if(key==att_href && tag==tag_a && htmlmodify_yylval)
         {
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
             else if(ExistsWebpageSpoolFile(Url) || IsLocalNetHost(Url->host))
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

       /* Base tag */

       if(key==att_href && htmlmodify_yylval && tag==tag_base)
          baseUrl=SplitURL(htmlmodify_yylval);

       /* Script events */

       else if(disable_html_script &&
               (key==att_onblur || key==att_onchange || key==att_onclick || key==att_ondblclick || key==att_onfocus ||
                key==att_onkeydown || key==att_onkeypress || key==att_onload || key==att_onmousedown ||
                key==att_onmousemove || key==att_onmouseout || key==att_onmouseover || key==att_onmouseup ||
                key==att_onreset || key==att_onselect || key==att_onsubmit || key==att_onunload))
          disable_key_val=1;

       /* Style references */

       else if(disable_html_style && key==att_style)
          disable_key_val=1;

       /* More complicated tags that depend on other attributes are stored and done later. */

       else if(tag>=tag_complex && tag<tag_ntags)
         {
          if(tagdata.nattr==tagdata.nattr_malloc)
            {
             tagdata.attr_type=(int*)realloc((void*)tagdata.attr_type,(tagdata.nattr_malloc+1)*sizeof(int));
             tagdata.attr_key=(char**)realloc((void*)tagdata.attr_key,(tagdata.nattr_malloc+1)*sizeof(char*));
             tagdata.attr_val=(char**)realloc((void*)tagdata.attr_val,(tagdata.nattr_malloc+1)*sizeof(char*));
             tagdata.attr_quote=(char**)realloc((void*)tagdata.attr_quote,(tagdata.nattr_malloc+1)*sizeof(char*));

             tagdata.attr_key[tagdata.nattr_malloc]=NULL;
             tagdata.attr_val[tagdata.nattr_malloc]=NULL;

             tagdata.nattr_malloc+=1;
            }

          tagdata.attr_type[tagdata.nattr]=key;
          if(tagdata.attr_key[tagdata.nattr]) free(tagdata.attr_key[tagdata.nattr]);
          tagdata.attr_key[tagdata.nattr]=strdup(key_string);
	  if(tagdata.attr_val[tagdata.nattr]) free(tagdata.attr_val[tagdata.nattr]);
          if(htmlmodify_yylval)
            {
             tagdata.attr_val[tagdata.nattr]=strdup(htmlmodify_yylval);
            }
          else
            {
             tagdata.attr_val[tagdata.nattr]=NULL;
            }
          tagdata.attr_quote[tagdata.nattr]=quote;

          tagdata.nattr++;
         }

       /* Output the attribute and key or not. */

       if(!disable_key_val)
         {
          YY_OUTPUT(key_string);
          if(htmlmodify_yylval)
            {
             YY_OUTPUT("=");
             if(*quote)
               {YY_OUTPUT(quote);}
             YY_OUTPUT(htmlmodify_yylval);
             if(*quote)
               {YY_OUTPUT(quote);}
            }
         }

       key=att_natts;
       break;

      default:
       break;
      }

 if(cache_info)
   {YY_OUTPUT(cache_info); free(cache_info); cache_info=NULL;}

 if(key_string)
    free(key_string);

 /* Delete the tagdata */

 for(i=0;i<tagdata.nattr_malloc;i++)
   {
    if(tagdata.attr_key[i]) free(tagdata.attr_key[i]);
    if(tagdata.attr_val[i]) free(tagdata.attr_val[i]);
   }

 if(tagdata.tag) free(tagdata.tag);

 free(tagdata.attr_type);
 free(tagdata.attr_key);
 free(tagdata.attr_val);
 free(tagdata.attr_quote);
}


#ifndef htmlmodify_yywrap
/*+ Needed in lex but does nothing. +*/
#define htmlmodify_yywrap() 1
#endif

/*+ Reset the current string. +*/
#define reset_string \
{if(string) *string=0; stringused=0;}

/*+ append information to the current string. +*/
#define append_string(xx) \
{size_t _str_len=strlen(xx),_new_len=stringused+_str_len; \
 if(_new_len>=stringallocsize) string=(char*)realloc((void*)string,stringallocsize=_new_len+1); \
 memcpy(string+stringused,xx,_str_len+1); \
 stringused=_new_len;}

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
{if(htmlmodify_read_err==-1 || (result=htmlmodify_read_err=read_data(htmlmodify_yyfd,buf,max_size))==-1) result=0;}

%}

%%
 /* Must use static variables since the parser returns often. */
 static char *string=NULL;
 static size_t stringallocsize=0,stringused=0;
 static int after_tag=INITIAL;


 /* Handle comments and other tags */

[\200-\237]                 { if(demoronise_ms_chars)
                                {
                                 YY_OUTPUT("<!-- WWWOFFLE (demoronise-ms-chars) - '");
                                 YY_OUTPUT(htmlmodify_yytext);
                                 YY_OUTPUT("' -->");
                                 YY_OUTPUT(demoronise_ms_chars_list[*(unsigned char*)htmlmodify_yytext-0x80]);
                                }
                              else
                                 YY_OUTPUT(htmlmodify_yytext);
                            }
[^<\200-\237]+              { YY_OUTPUT(htmlmodify_yytext); /* htmlmodify_yylval=htmlmodify_yytext; return(LEX_PLAINTEXT); */ }
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
<COMMENT>">"                { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }
<COMMENT>"-"                { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }
<COMMENT>[^->]+             { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }

<COMMENT_BAD>">"            { YY_OUTPUT(htmlmodify_yytext); BEGIN(INITIAL); /* htmlmodify_yylval=string; return(LEX_COMMENT); */ }
<COMMENT_BAD>[^>]+          { YY_OUTPUT(htmlmodify_yytext); /* append_string(htmlmodify_yytext); */ }

 /* Tags */

<TAG_START>"script"/{W}     { BEGIN(TAG); after_tag=SCRIPT_START; htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"script"/">"     { BEGIN(TAG); after_tag=SCRIPT_START; htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"style"/{W}      { BEGIN(TAG); after_tag=STYLE_START;  htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"style"/">"      { BEGIN(TAG); after_tag=STYLE_START;  htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{K}+/" "     { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{K}+/\t      { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{K}+/\n      { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{K}+/\r      { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{K}+/">"     { BEGIN(TAG); after_tag=INITIAL;      htmlmodify_yylval=htmlmodify_yytext; return(LEX_TAG_BEGIN); }
<TAG_START>(.|\n)           { BEGIN(INITIAL); YY_OUTPUT(string); YY_OUTPUT(htmlmodify_yytext); }

<TAG>{W}*"/>"               { BEGIN(after_tag);                              htmlmodify_yylval=""; return(LEX_TAG_END_XHTML); }
<TAG>">"                    { BEGIN(after_tag);                              htmlmodify_yylval=""; return(LEX_TAG_END); }
<TAG>"<"                    { BEGIN(after_tag); unput(htmlmodify_yytext[0]); htmlmodify_yylval=""; return(LEX_TAG_END); }
<TAG>{K}+                   { BEGIN(TAG_ATTR_KEY); htmlmodify_yylval=htmlmodify_yytext; return(LEX_ATTR_KEY); }
<TAG>(.|\n)                 { YY_OUTPUT(htmlmodify_yytext); }

<TAG_ATTR_KEY>{W}*=         { BEGIN(TAG_ATTR_VAL); }
<TAG_ATTR_KEY>(.|\n)        { BEGIN(TAG); unput(htmlmodify_yytext[0]); htmlmodify_yylval=NULL; return(LEX_ATTR_VAL); }

<TAG_ATTR_VAL>\"{W}*        { BEGIN(DQUOTED); reset_string; }
<TAG_ATTR_VAL>\'{W}*        { BEGIN(SQUOTED); reset_string; }
<TAG_ATTR_VAL>{W}+          { }
<TAG_ATTR_VAL>{F}+          { BEGIN(TAG);                              htmlmodify_yylval=htmlmodify_yytext; return(LEX_ATTR_VAL); }
<TAG_ATTR_VAL>(.|\n)        { BEGIN(TAG); unput(htmlmodify_yytext[0]); htmlmodify_yylval="";                return(LEX_ATTR_VAL); }

 /* Quoted strings */

<DQUOTED>\\\"               { append_string(htmlmodify_yytext); }
<DQUOTED>\\                 { append_string(htmlmodify_yytext); }
<DQUOTED>{W}*\"             { BEGIN(TAG); htmlmodify_yylval=string; return(LEX_ATTR_VAL_DQ); }
<DQUOTED>[\r\n]+            { }
<DQUOTED>[^\\\"\r\n]+       { append_string(htmlmodify_yytext); }

<SQUOTED>\\\'               { append_string(htmlmodify_yytext); }
<SQUOTED>\\                 { append_string(htmlmodify_yytext); }
<SQUOTED>{W}*\'             { BEGIN(TAG); htmlmodify_yylval=string; return(LEX_ATTR_VAL_SQ); }
<SQUOTED>[\r\n]+            { }
<SQUOTED>[^\\\'\r\n]+       { append_string(htmlmodify_yytext); }

 /* Scripts */

<SCRIPT_START>{W}*/"</script" { disable_output^=DISABLE_PARSE; YY_OUTPUT(htmlmodify_yytext); disable_output^=DISABLE_PARSE;
                              BEGIN(INITIAL); }
<SCRIPT_START>(.|\n)        { disable_output^=DISABLE_PARSE; YY_OUTPUT("\n<!-- WWWOFFLE (disable-script) - ... -->\n"); disable_output^=DISABLE_PARSE;
                              unput(htmlmodify_yytext[0]); BEGIN(SCRIPT); }

<SCRIPT>\"                  { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_DQUOTED); }
<SCRIPT>\'                  { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_SQUOTED); }
<SCRIPT>"/*"                { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_COMMENT_C); }
<SCRIPT>"//"                { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT_COMMENT_CPP); }
<SCRIPT>"*"                 { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT>"/"                 { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT>"<"/"/script"       { BEGIN(TAG_START); }
<SCRIPT>"<"                 { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT>[^\'\"</]+          { YY_OUTPUT(htmlmodify_yytext); }

 /* Comments in scripts, C or C++ style */

<SCRIPT_COMMENT_C>"*/"      { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_COMMENT_C>"*"       { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_COMMENT_C>[^*]+     { YY_OUTPUT(htmlmodify_yytext); }

<SCRIPT_COMMENT_CPP>[\r\n]  { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_COMMENT_CPP>"<"/"/script" { BEGIN(TAG_START); }
<SCRIPT_COMMENT_CPP>"<"     { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_COMMENT_CPP>[^\r\n<]+ { YY_OUTPUT(htmlmodify_yytext); }

 /* Quoted strings in scripts - assume they follow usual rules */

<SCRIPT_DQUOTED>\\\"        { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_DQUOTED>\\          { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_DQUOTED>\"          { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_DQUOTED>[^\\\"]+    { YY_OUTPUT(htmlmodify_yytext); }

<SCRIPT_SQUOTED>\\\'        { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_SQUOTED>\\          { YY_OUTPUT(htmlmodify_yytext); }
<SCRIPT_SQUOTED>\'          { YY_OUTPUT(htmlmodify_yytext); BEGIN(SCRIPT); }
<SCRIPT_SQUOTED>[^\\\']+    { YY_OUTPUT(htmlmodify_yytext); }

 /* Styles */

<STYLE_START>{W}*/"</style" { disable_output^=DISABLE_PARSE; YY_OUTPUT(htmlmodify_yytext); disable_output^=DISABLE_PARSE;
                              BEGIN(INITIAL); }
<STYLE_START>(.|\n)         { disable_output^=DISABLE_PARSE; YY_OUTPUT("\n<!-- WWWOFFLE (disable-style) - ... -->\n"); disable_output^=DISABLE_PARSE;
                              unput(htmlmodify_yytext[0]); BEGIN(STYLE); }

<STYLE>"<"/"/style"         { BEGIN(TAG_START); }
<STYLE>"<"                  { YY_OUTPUT(htmlmodify_yytext); }
<STYLE>[^<]+                { YY_OUTPUT(htmlmodify_yytext); }

 /* End of file */

<<EOF>>                     { free(string); stringallocsize=stringused=0; BEGIN(INITIAL); return(0); }
%%
