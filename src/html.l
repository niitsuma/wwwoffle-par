W               [ \t\r\n\f]

nonascii        [\200-\377]
ascii           [ -~]
alpha           [[:alpha:]]
alphanum        [[:alnum:]]
punct           [][!\"#$%&\'()*+,./:;<=>?@\\^_`{|}~-]
safepunct       [][!\#$%&\*+,./:;=?@\\^_`{|}~-]

tag             {alpha}({alphanum}|[-_:.])*
tagdel          [ \t\r\n\f/>]
key             ({alphanum}|[-_:.])+
val             ({alphanum}|{nonascii}|{safepunct})+

ident           ({nonascii}|{alphanum}|-)+
url             ({alphanum}|{nonascii}|{safepunct})+

%x DOCTYPE
%x COMMENT COMMENT_BAD
%x TAG_START TAG TAG_ATTR_KEY TAG_ATTR_VAL
%x DQUOTED SQUOTED
%x SCRIPT
%x CSS
%x CSS_COMMENT
%x CSS_IMPORT CSS_IMPORT_URL
%x CSS_MEDIA
%x CSS_DECL_LIST CSS_DECL_PROP CSS_DECL_VALUE CSS_DECL_VALUE_URL
%x TITLE_START TITLE

%{
/***************************************

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9f.
  Parse the HTML and look for the images, links and other things.
  ******************/ /******************
  Written by Andrew M. Bishop.
  Object and Parameter handling by Walter Pfannenmller.
  Modified by Paul A. Rombouts.

  This file Copyright 1997-2009 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2006,2007,2008,2011 Paul A. Rombouts
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
#define LEX_TITLE_TEXT     4

#define LEX_TAG_BEGIN     11
#define LEX_TAG_END       12
#define LEX_TAG_END_XHTML 13

#define LEX_ATTR_KEY      21
#define LEX_ATTR_VAL      22
#define LEX_ATTR_VAL_SQ   23
#define LEX_ATTR_VAL_DQ   24

#define LEX_CSS_STYLESHEET  101
#define LEX_CSS_PROPERTY    102
#define LEX_CSS_VALUE       103

/*+ Tag types +*/

typedef enum _HTMLTags
{
 tag_a         = 0  /* "a"          */ ,
 tag_area      = 1  /* "area"       */ ,
 tag_base      = 2  /* "base"       */ ,
 tag_blockquote= 3  /* "blockquote" */ ,
 tag_body      = 4  /* "body"       */ ,
 tag_del       = 5  /* "del"        */ ,
 tag_frame     = 6  /* "frame"      */ ,
 tag_head      = 7  /* "head"       */ ,
 tag_iframe    = 8  /* "iframe"     */ ,
 tag_input     = 9  /* "input"      */ ,
 tag_ins       =10  /* "ins"        */ ,
 tag_q         =11  /* "q"          */ ,
 tag_script    =12  /* "script"     */ ,
 tag_td        =13  /* "td"         */ ,

 tag_complex   =14  /* Complex tags, stored and processed as a whole. */,

 tag_applet    =15  /* "applet"     */ ,
 tag_embed     =16  /* "embed"      */ ,
 tag_img       =17  /* "img"        */ ,
 tag_link      =18  /* "link"       */ ,
 tag_meta      =19  /* "meta"       */ ,
 tag_object    =20  /* "object"     */ ,
 tag_param     =21  /* "param"      */ ,
 tag_xml       =22  /* "xml"        */ ,

 tag_ntags     =23
}
HTMLTags;

/*+ Tag strings +*/

static const char* const tags[]=
{
 /* tag_a         = 0  */ "a"           ,
 /* tag_area      = 1  */ "area"        ,
 /* tag_base      = 2  */ "base"        ,
 /* tag_blockquote= 3  */ "blockquote"  ,
 /* tag_body      = 4  */ "body"        ,
 /* tag_del       = 5  */ "del"         ,
 /* tag_frame     = 6  */ "frame"       ,
 /* tag_head      = 7  */ "head"        ,
 /* tag_iframe    = 8  */ "iframe"      ,
 /* tag_input     = 9  */ "input"       ,
 /* tag_ins       =10  */ "ins"         ,
 /* tag_q         =11  */ "q"           ,
 /* tag_script    =12  */ "script"      ,
 /* tag_td        =13  */ "td"          ,

 /* tag_complex   =14  */ ""            ,

 /* tag_applet    =15  */ "applet"      ,
 /* tag_embed     =16  */ "embed"       ,
 /* tag_img       =17  */ "img"         ,
 /* tag_link      =18  */ "link"        ,
 /* tag_meta      =19  */ "meta"        ,
 /* tag_object    =20  */ "object"      ,
 /* tag_param     =21  */ "param"       ,
 /* tag_xml       =22  */ "xml"
};

/*+ Attribute types +*/

typedef enum _HTMLAttributes
{
 att_archive   = 0  /* "archive"    */ ,
 att_background= 1  /* "background" */ ,
 att_cite      = 2  /* "cite"       */ ,
 att_classid   = 3  /* "classid"    */ ,
 att_code      = 4  /* "code"       */ ,
 att_codebase  = 5  /* "codebase"   */ ,
 att_codetype  = 6  /* "codetype"   */ ,
 att_content   = 7  /* "content"    */ ,
 att_data      = 8  /* "data"       */ ,
 att_height    = 9  /* "height"     */ ,
 att_href      =10  /* "href"       */ ,
 att_http_equiv=11  /* "http-equiv" */ ,
 att_longdesc  =12  /* "longdesc"   */ ,
 att_name      =13  /* "name"       */ ,
 att_object    =14  /* "object"     */ ,
 att_profile   =15  /* "profile"    */ ,
 att_rel       =16  /* "rel"        */ ,
 att_src       =17  /* "src"        */ ,
 att_style     =18  /* "style"      */ ,
 att_type      =19  /* "type"       */ ,
 att_usemap    =20  /* "usemap"     */ ,
 att_value     =21  /* "value"      */ ,
 att_valuetype =22  /* "valuetype"  */ ,
 att_width     =23  /* "width"      */ ,

 att_natts     =24
}
HTMLAttributes;

/*+ Attribute strings. +*/

static const char* const attributes[]=
{
 /* att_archive   = 0 */  "archive"     ,
 /* att_background= 1 */  "background"  ,
 /* att_cite      = 2 */  "cite"        ,
 /* att_classid   = 3 */  "classid"     ,
 /* att_code      = 4 */  "code"        ,
 /* att_codebase  = 5 */  "codebase"    ,
 /* att_codetype  = 6 */  "codetype"    ,
 /* att_content   = 7 */  "content"     ,
 /* att_data      = 8 */  "data"        ,
 /* att_height    = 9 */  "height"      ,
 /* att_href      =10 */  "href"        ,
 /* att_http_equiv=11 */  "http-equiv"  ,
 /* att_longdesc  =12 */  "longdesc"    ,
 /* att_name      =13 */  "name"        ,
 /* att_object    =14 */  "object"      ,
 /* att_profile   =15 */  "profile"     ,
 /* att_rel       =16 */  "rel"         ,
 /* att_src       =17 */  "src"         ,
 /* att_style     =18 */  "style"       ,
 /* att_type      =19 */  "type"        ,
 /* att_usemap    =20 */  "usemap"      ,
 /* att_value     =21 */  "value"       ,
 /* att_valuetype =22 */  "valuetype"   ,
 /* att_width     =23 */  "width"
};

/*+ A structure to hold a tag and its attributes. +*/

typedef struct _Tag
{
 HTMLTags type;                 /*+ The type of the tag. +*/

 int nattr;                     /*+ The number of attributes. +*/
 int nattr_malloc;              /*+ The number of attributes that space is malloced for. +*/

 int *attr_type;                /*+ The list of attribute types. +*/
 char **attr_val;               /*+ The list of attribute values. +*/
}
Tag;

/* Local functions */

static void parse_html(const URL *Url);

static /*@null@*/ char *html_yylval=NULL;
extern int html_yylex(void);

static void add_codebase_url(char *obj,char *codebase,RefType refType);
static void add_java_applet_url(char *obj,char *codebase,RefType refType);

static void handle_object_tag(Tag *tag,int fetch_webbug_images);
static void handle_param_tag(Tag *tag);
static void handle_link_tag(Tag *tag,int fetch_icon_images);
static void handle_meta_tag(Tag *tag);
static void handle_img_tag(Tag *tag,int fetch_webbug_images);
static void handle_style_attribute(char *value);


/*+ The file descriptor that we are reading from. +*/
static int html_yyfd=-1;



/* Must use static variables since the parser returns often. */
static char *string=NULL;
static size_t stringallocsize=0,stringlen=0;
static int after_tag;
static int css_after_brace;

static void init_parser();
static void reset_parser();


/*++++++++++++++++++++++++++++++++++++++
  Parse the HTML and look for references to image/links/frames.

  int fd The file descriptor of the file to parse.

  URL *Url The reference URL to use.
  ++++++++++++++++++++++++++++++++++++++*/

void ParseHTML(int fd,const URL *Url)
{
 PrintMessage(Debug,"Parsing document using HTML parser.");

 html_yyfd=fd;
 init_parser();

 parse_html(Url);

 reset_parser();
}


/*++++++++++++++++++++++++++++++++++++++
  Add a reference to a URL using the codebase.

  char *obj The object to add.

  char *codebase The codebase (base URL).

  RefType refType The reference type.
  ++++++++++++++++++++++++++++++++++++++*/

static void add_codebase_url(char *obj,char *codebase,RefType refType)
{
 if(*obj)
   {
    if(codebase)
      {
       char *p;
       char url[strlen(codebase)+strlen(obj)+sizeof("/")];

       p=stpcpy(url,codebase);
       if(p>url && *(p-1)!='/') *p++='/'; 
       stpcpy(p,obj);

       AddReference(url,refType);

      }
    else
       AddReference(obj,refType);
   }
}

/*++++++++++++++++++++++++++++++++++++++
  Add a reference to a Java class.

  char *obj The object to add.

  char *codebase The codebase (base URL).

  RefType refType The reference type.
  ++++++++++++++++++++++++++++++++++++++*/

static void add_java_applet_url(char *obj,char *codebase,RefType refType)
{
 if(*obj)
   {
    char *applet=obj;
    static const char class_suffix[]=".class";
    int strlen_obj=strlen(obj);
    int strlen_applet=strlen_obj;
    int strlen_diff=strlen_obj-strlitlen(class_suffix);

    if(strlen_diff<0 || strcmp(&obj[strlen_diff],class_suffix))
      {
       strlen_applet=strlen_obj+strlitlen(class_suffix);
       applet=(char*)alloca(strlen_applet+1);

       stpcpy(stpcpy(applet,obj),class_suffix);
      }

    {
      char *lastdot=applet+strlen_applet-strlitlen(class_suffix);
      char *p;
      for(p=applet;p<lastdot;++p)
	if(*p=='.') *p = '/';
    }

    add_codebase_url(applet,codebase,refType);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the object/applet/embed/xml tag and parse it.

  Tag *tag The tag information.

  int fetch_webbug_images Set to true if webbug images are to be fetched.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_object_tag(Tag *tag,int fetch_webbug_images)
{
 int i;
 char *codebase=NULL,*codetype=NULL,*uri=NULL;
 RefType refType=RefInlineObject;
 int is_image=0,is_java=0;
 int width=1000,height=1000;

 /* Find the codebase and codetype. */

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_codebase && tag->attr_val[i] && tag->attr_val[i][0])
       codebase=tag->attr_val[i];
    else if(tag->attr_type[i]==att_codetype && tag->attr_val[i] && tag->attr_val[i][0])
       codetype=tag->attr_val[i];

 /* Check for images. */

 for(i=0;i<tag->nattr;i++)
    if((tag->attr_type[i]==att_codetype && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"image")) ||
       (tag->attr_type[i]==att_type && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"image")))
       is_image=1;
    else if(tag->attr_type[i]==att_width && tag->attr_val[i])
       width=atoi(tag->attr_val[i]);
    else if(tag->attr_type[i]==att_height && tag->attr_val[i])
       height=atoi(tag->attr_val[i]);

 /* Check for Java */

 if(codetype && !strcmp(codetype,"application/java"))
    is_java=1;

 /* Check for archives or inline objects. */

 for(i=0;i<tag->nattr;i++)
   {
    if(tag->attr_type[i]==att_src && tag->attr_val[i] && tag->attr_val[i][0])
       AddReference(tag->attr_val[i],RefInlineObject);
    else if(tag->attr_type[i]==att_archive && tag->attr_val[i])
      {
       char *p,*q=tag->attr_val[i];

       while((p=strtok(q," \t\r\n,")))
         {
          add_codebase_url(p,codebase,RefObject);
          refType=RefObject;
          q=NULL;
         }
      }
   }

 /* Find the rest ... */

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_val[i])
      {
       if(tag->attr_type[i]==att_code)          {uri=tag->attr_val[i]; is_java=1;}
       else if(tag->attr_type[i]==att_object)   {uri=tag->attr_val[i]; is_java=1;}
       else if(tag->attr_type[i]==att_usemap)   {uri=tag->attr_val[i];}
       else if(tag->attr_type[i]==att_longdesc) {uri=tag->attr_val[i];}
       else if(tag->attr_type[i]==att_data)     {uri=tag->attr_val[i];}
       else if(tag->attr_type[i]==att_classid)
         {
          if(!strcasecmp_litbeg(tag->attr_val[i],"java:"))       {uri=tag->attr_val[i]+strlitlen("java:"); is_java=1;}
          else if(!strcasecmp_litbeg(tag->attr_val[i],"clsid:")) {uri=tag->attr_val[i]+strlitlen("clsid:");}
          else if(strcasecmp_litbeg(tag->attr_val[i],"data:"))   {uri=tag->attr_val[i];}
         }
      }

 /* ... fetch them. */

 if(uri)
   {
    if(is_image)
      {
       if(!fetch_webbug_images && height==1 && width==1)
         {
          if(codebase)
             PrintMessage(Debug,"The object URL '%s/%s' appears to be a webbug.",codebase,uri);
          else
             PrintMessage(Debug,"The object URL '%s' appears to be a webbug.",uri);
         }
       else
          add_codebase_url(uri,codebase,RefImage);
      }
    else if(is_java)
       add_java_applet_url(uri,codebase,refType);
    else
       add_codebase_url(uri,codebase,RefLink);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the param tag and parse it.

  Tag *tag The tag information.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_param_tag(Tag *tag)
{
 int i;
 int valuetype_is_ref=0;

 for(i=0;i<tag->nattr;i++)
    if(((tag->attr_type[i]==att_valuetype || tag->attr_type[i]==att_name) &&
         tag->attr_val[i] && !strcasecmp(tag->attr_val[i],"ref")) ||
       ((tag->attr_type[i]==att_name && tag->attr_val[i] &&
         (!strcasecmp(tag->attr_val[i],"href") || !strcasecmp(tag->attr_val[i],"file")))))
      {valuetype_is_ref=1;break;}

 if(valuetype_is_ref)
    for(i=0;i<tag->nattr;i++)
       if(tag->attr_type[i]==att_value && tag->attr_val[i] && tag->attr_val[i][0])
          AddReference(tag->attr_val[i],RefObject);
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the link tag and parse it.

  Tag *tag The tag information.

  int fetch_icon_images A flag to indicate if icons are to be fetched.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_link_tag(Tag *tag,int fetch_icon_images)
{
 int i;
 int is_stylesheet=0,is_icon=0;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_rel && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"Stylesheet"))
      {is_stylesheet=1;break;}

 if(fetch_icon_images)
    for(i=0;i<tag->nattr;i++)
       if(tag->attr_type[i]==att_rel && tag->attr_val[i] &&
	  (!strcasecmp_litbeg(tag->attr_val[i],"Shortcut Icon") ||
           !strcasecmp_litbeg(tag->attr_val[i],"Icon")))
	 {is_icon=1;break;}

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_href && tag->attr_val[i] && tag->attr_val[i][0])
      {
       if(is_stylesheet)
          AddReference(tag->attr_val[i],RefStyleSheet);
       else if(is_icon)
          AddReference(tag->attr_val[i],RefImage);
       else
          AddReference(tag->attr_val[i],RefLink);
      }
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the meta tag and parse it.

  Tag *tag The tag information.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_meta_tag(Tag *tag)
{
 int i;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_http_equiv && tag->attr_val[i] && !strcasecmp_litbeg(tag->attr_val[i],"Refresh"))
       goto found_refresh;

 return;

found_refresh:
 for(i=0;i<tag->nattr;i++)
   if(tag->attr_type[i]==att_content && tag->attr_val[i] && tag->attr_val[i][0])
     goto found_content;

 return;

found_content:
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
       if(*p==';' || *p==',')
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
	     AddReference(p,RefMetaRefresh);
	 }
     }
 }
}


/*++++++++++++++++++++++++++++++++++++++
  Take the information for the img tag and parse it.

  Tag *tag The tag information.

  int fetch_webbug_images Set to true if webbug images are to be fetched.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_img_tag(Tag *tag,int fetch_webbug_images)
{
 int i;
 char *src=NULL;
 int width=1000,height=1000;

 for(i=0;i<tag->nattr;i++)
    if(tag->attr_type[i]==att_src && tag->attr_val[i])
       src=tag->attr_val[i];
    else if(tag->attr_type[i]==att_width && tag->attr_val[i])
       width=atoi(tag->attr_val[i]);
    else if(tag->attr_type[i]==att_height && tag->attr_val[i])
       height=atoi(tag->attr_val[i]);

 if(src)
   {
    if(!fetch_webbug_images && width==1 && height==1)
       PrintMessage(Debug,"The image URL '%s' appears to be a webbug.",src);
    else
       AddReference(src,RefImage);
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Take the contents of a style attribute and parse it.

  char *value The value of the style tag.
  ++++++++++++++++++++++++++++++++++++++*/

static void handle_style_attribute(char *value)
{
 char *p=value;

 while(p)
   {
    int property_is_image=0;

    if(!strncasecmp("background-image",p,16))
       property_is_image=1;
    else if(!strncasecmp("background",p,10))
       property_is_image=1;
    else if(!strncasecmp("list-style-image",p,16))
       property_is_image=1;

    if(property_is_image)
      {
       char *e,*copy;

       while(*p && *p!=':')     /* find ':' */
          p++;
       if(!*p)
          break;

       p++;
       while(*p && *p==' ')     /* skip ' ' */
          p++;

       if(!*p || strncasecmp("url",p,3)) /* find 'url' */
          break;

       p+=3;
       while(*p && *p!='(')     /* find '(' */
          p++;
       if(!*p)
          break;

       p++;
       if(!*p)
          break;

       e=p;
       while(*e && *e!=')')     /* find ')' */
          e++;
       if(!*e)
          break;

       if(*(p+1)=='\'' && *(e-1)=='\'')
          p++,e--;
       if(*(p+1)=='"' && *(e-1)=='"')
          p++,e--;

       copy=(char*)malloc(e-p+1);
       strncpy(copy,p,e-p);
       copy[e-p]=0;

       AddReference(copy,RefImage);

       free(copy);
      }

    if((p=strchr(p,';')))
       p++;
   }
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the HTML and look for references to image/links/frames/objects etc.

  URL *Url The URL being parsed.
  ++++++++++++++++++++++++++++++++++++++*/

static void parse_html(const URL *Url)
{
 HTMLTags tag=tag_ntags;
 HTMLAttributes key=att_natts;
 int property_is_image=0;
 RefType ref;
 int yychar,i;
 Tag tagdata;

 int fetch_webbug_images=ConfigBooleanURL(FetchWebbugImages,Url);
 int fetch_icon_images=ConfigBooleanURL(FetchIconImages,Url);

 /* Initialise the tagdata */

 tagdata.type=tag_ntags;
 tagdata.nattr=0;
 tagdata.nattr_malloc=16;
 tagdata.attr_type=(int*)malloc(tagdata.nattr_malloc*sizeof(int));
 tagdata.attr_val=(char**)malloc(tagdata.nattr_malloc*sizeof(char*));

 /* The actual parser. */

 while((yychar=html_yylex()))
    switch(yychar)
      {
      case LEX_PLAINTEXT:
      case LEX_TITLE_TEXT:
      case LEX_COMMENT:
      case LEX_DOCTYPE:
       break;

      case LEX_TAG_BEGIN:
       for(tag=0;tag<tag_ntags;tag++)
          if(!strcasecmp(html_yylval,tags[tag]))
             break;

       tagdata.type=tag;
       for(i=0;i<tagdata.nattr;i++)
	 if(tagdata.attr_val[i]) free(tagdata.attr_val[i]);
       tagdata.nattr=0;
       break;

      case LEX_TAG_END_XHTML:
       /*@fallthrough@*/
      case LEX_TAG_END:
       if(tag==tag_object || tag==tag_applet || tag==tag_embed || tag==tag_xml)
          handle_object_tag(&tagdata,fetch_webbug_images);
       else if(tag==tag_param)
          handle_param_tag(&tagdata);
       else if(tag==tag_meta)
          handle_meta_tag(&tagdata);
       else if(tag==tag_link)
          handle_link_tag(&tagdata,fetch_icon_images);
       else if(tag==tag_img)
          handle_img_tag(&tagdata,fetch_webbug_images);

       tag=tag_ntags;
       key=att_natts;

       break;

      case LEX_ATTR_KEY:
       if(tag==tag_ntags)
          break;

       for(key=0;key<att_natts;key++)
          if(!strcasecmp(html_yylval,attributes[key]))
             break;
      break;

      case LEX_ATTR_VAL_DQ:
       /*@fallthrough@*/
      case LEX_ATTR_VAL_SQ:
       /*@fallthrough@*/
      case LEX_ATTR_VAL:
       if(key==att_natts)
          break;

       /* Simple links and stuff that can be done using a single attribute. */

       ref=NRefTypes;

       if(key==att_href && (tag==tag_a || tag==tag_area))
          ref=RefLink;
       else if(key==att_src && tag==tag_input)
          ref=RefImage;
       else if(key==att_src && tag==tag_img && fetch_webbug_images)
          ref=RefImage;
       else if(key==att_src && tag==tag_script)
          ref=RefScript;
       else if(key==att_src && tag==tag_frame)
          ref=RefFrame;
       else if(key==att_src && tag==tag_iframe)
          ref=RefIFrame;
       else if(key==att_cite && (tag==tag_q || tag==tag_blockquote || tag==tag_ins || tag==tag_del))
          ref=RefLink;
       else if(key==att_background && (tag==tag_body || tag==tag_td))
          ref=RefImage;
       else if(key==att_longdesc && (tag==tag_frame || tag==tag_iframe || tag==tag_img))
          ref=RefLink;
       else if(key==att_usemap && (tag==tag_input || tag==tag_img))
          ref=RefImage;
       else if(key==att_profile && tag==tag_head)
          ref=RefLink;

       if(ref!=NRefTypes && html_yylval!=NULL)
          AddReference(html_yylval,ref);

       /* Other simple ones using a single attribute. */

       else if(key==att_href && tag==tag_base && html_yylval!=NULL)
          AddReference(html_yylval,RefBaseUrl);

       else if(key==att_style && html_yylval!=NULL)
          handle_style_attribute(html_yylval);

       /* More complicated ones that depend on other attributes are stored and done later. */

       if(tag>tag_complex)
         {
          if(tagdata.nattr==tagdata.nattr_malloc)
            {
             ++tagdata.nattr_malloc;
             tagdata.attr_type=(int*)realloc((void*)tagdata.attr_type,(tagdata.nattr_malloc)*sizeof(int));
             tagdata.attr_val=(char**)realloc((void*)tagdata.attr_val,(tagdata.nattr_malloc)*sizeof(char*));
            }

          tagdata.attr_type[tagdata.nattr]=key;
	  tagdata.attr_val[tagdata.nattr]=(html_yylval?strdup(html_yylval):NULL);

          ++tagdata.nattr;
         }

       key=att_natts;
       break;

      case LEX_CSS_STYLESHEET:
       AddReference(html_yylval,RefStyleSheet);
       break;

      case LEX_CSS_PROPERTY:
       if(!strcasecmp("background-image",html_yylval))
          property_is_image=1;
       else if(!strcasecmp("background",html_yylval))
          property_is_image=1;
       else if(!strcasecmp("list-style-image",html_yylval))
          property_is_image=1;
       break;

      case LEX_CSS_VALUE:
       if(property_is_image && strcasecmp(html_yylval,"none"))
          AddReference(html_yylval,RefImage);
       property_is_image=0;
       break;

      default:
       break;
      }

 /* Delete the tagdata */

 for(i=0;i<tagdata.nattr;i++)
    if(tagdata.attr_val[i]) free(tagdata.attr_val[i]);

 free(tagdata.attr_type);
 free(tagdata.attr_val);
}


/*++++++++++++++++++++++++++++++++++++++
  Parse the HTML and look for the title.

  int fd The file descriptor of the file to parse.

  char *HTML_title returns a pointer to a newly allocated string or NULL.
  ++++++++++++++++++++++++++++++++++++++*/

char *HTML_title(int fd)
{
 int yychar;
 char *result=NULL;

 html_yyfd=fd;
 init_parser();

 /* Parse HTML until title text is found. */

 while((yychar=html_yylex()))
   if(yychar==LEX_TITLE_TEXT)
     goto found_title;
   else if(yychar==LEX_TAG_BEGIN) {
     if(!strcasecmp(html_yylval,"/title") || !strcasecmp(html_yylval,"/head"))
       break;
   }

 goto cleanup_return;

found_title:
 if(html_yylval) {
   char *t=html_yylval,*end;

   /* trim the string */
   for(;;++t) {
     if(!*t) goto cleanup_return;
     if(!isspace(*t)) break;
   }
   end=strchrnul(t,0);
   do {if(--end<t) goto cleanup_return;} while(isspace(*end));
   ++end;

   result= STRDUP2(t,end);
 }

cleanup_return:
 reset_parser();
 return result;
}


#define YY_SKIP_YYWRAP 1 /* Remove error with prototype of ..._yywrap */
#ifndef html_yywrap
/*+ Needed in lex but does nothing. +*/
#define html_yywrap() 1
#endif

/*+ Reset the current string. +*/
#define reset_string \
{if(!string) string=malloc(stringallocsize=64); *string=0; stringlen=0;}

/*+ append information to the current string. +*/
#define append_string(xx) \
{size_t _str_len=strlen(xx),_new_len=stringlen+_str_len; \
 if(_new_len>=stringallocsize) string=(char*)realloc((void*)string,stringallocsize=_new_len+1); \
 memcpy(string+stringlen,xx,_str_len+1); \
 stringlen=_new_len;}

/*+ Don't include the yyinput() or input() function in the lexer. +*/
#define YY_NO_INPUT

/*+ A macro to read data that can be used by the lexer. +*/
#define YY_INPUT(buf,result,max_size) \
{if((result=read_data(html_yyfd,buf,max_size))==-1) result=0;}

%}

%%
 /* Handle comments and other tags */

[^<]+                       { html_yylval=yytext; return(LEX_PLAINTEXT); }
"<!DOCTYPE"                 { BEGIN(DOCTYPE); reset_string; }
"<!--"                      { BEGIN(COMMENT); reset_string; }
"<!"{W}*"-"*                { BEGIN(COMMENT_BAD); reset_string; }
"<"{W}*                     { BEGIN(TAG_START); reset_string; /* append_string(yytext); */ }

 /* Doctype (DTD) */

<DOCTYPE>">"                { BEGIN(INITIAL); /* html_yylval=string; return(LEX_DOCTYPE); */ }
<DOCTYPE>[^>]+              { /* append_string(yytext); */ }

 /* Comments - COMMENT_BAD is not a legal comment format (except <!>) but people use it as one.
               COMMENT is not strictly correct, but works better than the real thing. */

<COMMENT>"--"{W}*">"        { BEGIN(INITIAL); /* html_yylval=string; return(LEX_COMMENT); */ }
<COMMENT>"-"                |
<COMMENT>[^-]+              { /* append_string(yytext); */ }

<COMMENT_BAD>">"            { BEGIN(INITIAL); /* html_yylval=string; return(LEX_COMMENT); */ }
<COMMENT_BAD>[^>]+          { /* append_string(yytext); */ }

 /* Tags */

<TAG_START>"script"/{tagdel}  { BEGIN(TAG); after_tag=SCRIPT; html_yylval=yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"style"/{tagdel}   { BEGIN(TAG); after_tag=CSS;    html_yylval=yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"title"/{tagdel}   { BEGIN(TAG); after_tag=TITLE_START;  html_yylval=yytext; return(LEX_TAG_BEGIN); }
<TAG_START>"/"?{tag}/{tagdel} { BEGIN(TAG); after_tag=INITIAL;      html_yylval=yytext; return(LEX_TAG_BEGIN); }
<TAG_START>(.|\n)             { BEGIN(INITIAL); }

<TAG>{W}+                   { }
<TAG>"/>"                   { BEGIN(INITIAL);                     html_yylval=""; return(LEX_TAG_END_XHTML); }
<TAG>">"                    { BEGIN(after_tag);                   html_yylval=""; return(LEX_TAG_END); }
<TAG>"<"                    { BEGIN(after_tag); unput(yytext[0]); html_yylval=""; return(LEX_TAG_END); }
<TAG>{key}                  { BEGIN(TAG_ATTR_KEY); html_yylval=yytext; return(LEX_ATTR_KEY); }
<TAG>(.|\n)                 { }

<TAG_ATTR_KEY>{W}*=         { BEGIN(TAG_ATTR_VAL); }
<TAG_ATTR_KEY>(.|\n)        { BEGIN(TAG); unput(yytext[0]); html_yylval=NULL; return(LEX_ATTR_VAL); }

<TAG_ATTR_VAL>\"            { BEGIN(DQUOTED); reset_string; }
<TAG_ATTR_VAL>\'            { BEGIN(SQUOTED); reset_string; }
<TAG_ATTR_VAL>{W}+          { }
<TAG_ATTR_VAL>{val}         { BEGIN(TAG);                   html_yylval=yytext; return(LEX_ATTR_VAL); }
<TAG_ATTR_VAL>(.|\n)        { BEGIN(TAG); unput(yytext[0]); html_yylval="";     return(LEX_ATTR_VAL); }

 /* Quoted strings */

<DQUOTED>\"                 { BEGIN(TAG); html_yylval=string; return(LEX_ATTR_VAL_DQ); }
<DQUOTED>[\r\n]+            { }
<DQUOTED>[^\"\r\n]+         { append_string(yytext); }

<SQUOTED>\'                 { BEGIN(TAG); html_yylval=string; return(LEX_ATTR_VAL_SQ); }
<SQUOTED>[\r\n]+            { }
<SQUOTED>[^\'\r\n]+         { append_string(yytext); }

 /* Scripts */

<SCRIPT>"<"/"/script"{tagdel} { BEGIN(TAG_START); }
<SCRIPT>"<"                   |
<SCRIPT>[^<]+                 { }

 /* Styles */

<CSS>"<"/"/style"{tagdel}       { BEGIN(TAG_START); }

 /* CSS (Cascading Style Sheets */

 /* CSS - Handle comments and other tags */

<CSS>"/*"                       { BEGIN(CSS_COMMENT); }
<CSS>"@import"                  { BEGIN(CSS_IMPORT); }
<CSS>"@media"                   { BEGIN(CSS_MEDIA); }
<CSS>\{                         { BEGIN(CSS_DECL_LIST); }
<CSS>[^<{@/]+                   { /* Different from css.l by addition of '<' */ }
<CSS>.|\n                       { }

 /* CSS - Comments in C style */

<CSS_COMMENT>"*"+"/"            { BEGIN(CSS); }
<CSS_COMMENT>"*"+               { }
<CSS_COMMENT>[^*]+              { }

 /* CSS - @import */

<CSS_IMPORT>{W}+                { }
<CSS_IMPORT>url\(\"             { BEGIN(CSS_IMPORT_URL); }
<CSS_IMPORT>url\(\'             { BEGIN(CSS_IMPORT_URL); }
<CSS_IMPORT>url\(               { BEGIN(CSS_IMPORT_URL); }
<CSS_IMPORT>\"                  { BEGIN(CSS_IMPORT_URL); }
<CSS_IMPORT>\'                  { BEGIN(CSS_IMPORT_URL); }
<CSS_IMPORT>.|\n                { BEGIN(CSS); }

<CSS_IMPORT_URL>{W}+            { }
<CSS_IMPORT_URL>{url}           { html_yylval=yytext; return(LEX_CSS_STYLESHEET); }
<CSS_IMPORT_URL>.               { BEGIN(CSS); }

 /* CSS - @media */

<CSS_MEDIA>{W}+                 { }
<CSS_MEDIA>[^{}]+               { }
<CSS_MEDIA>\{                   { css_after_brace=CSS_MEDIA; BEGIN(CSS); }
<CSS_MEDIA>\}                   { css_after_brace=CSS;       BEGIN(CSS); }

 /* CSS - Declaration list */

<CSS_DECL_LIST>{W}+             { }
<CSS_DECL_LIST>{ident}          { BEGIN(CSS_DECL_PROP);  html_yylval=yytext; return(LEX_CSS_PROPERTY); }
<CSS_DECL_LIST>\}               { BEGIN(css_after_brace); }
<CSS_DECL_LIST>.|\n             { }

<CSS_DECL_PROP>{W}*:            { BEGIN(CSS_DECL_VALUE); }
<CSS_DECL_PROP>.|\n             { }

<CSS_DECL_VALUE>{W}+            { }
<CSS_DECL_VALUE>url\(\"         { BEGIN(CSS_DECL_VALUE_URL); }
<CSS_DECL_VALUE>url\(\'         { BEGIN(CSS_DECL_VALUE_URL); }
<CSS_DECL_VALUE>url\(           { BEGIN(CSS_DECL_VALUE_URL); }
<CSS_DECL_VALUE>;               { BEGIN(CSS_DECL_LIST); }
<CSS_DECL_VALUE>\}              { unput('}'); BEGIN(CSS_DECL_LIST); }
<CSS_DECL_VALUE>[^;}u]+         { }
<CSS_DECL_VALUE>.|\n            { }

<CSS_DECL_VALUE_URL>{W}+        { }
<CSS_DECL_VALUE_URL>{url}       { BEGIN(CSS_DECL_VALUE);  html_yylval=yytext; return(LEX_CSS_VALUE); }
<CSS_DECL_VALUE_URL>.           { }

 /* Title, added by Paul Rombouts */

<TITLE_START>{W}*/"</title" { BEGIN(INITIAL); }
<TITLE_START>(.|\n)         { unput(yytext[0]); reset_string; BEGIN(TITLE); }

<TITLE>"<"/"/title"         { BEGIN(TAG_START); html_yylval=string; return(LEX_TITLE_TEXT); }
<TITLE>"<"                  |
<TITLE>[^<]+                { append_string(yytext); }

 /* End of file */

 /* <<EOF>>                 { if(string) free(string); string=NULL; stringallocsize=stringlen=0;
                              css_after_brace=CSS; after_tag=INITIAL; YY_FLUSH_BUFFER; BEGIN(INITIAL); html_yylval=NULL; return(0); } */
%%

static void init_parser()
{
  string=NULL;
  stringallocsize=0; stringlen=0;
  after_tag=INITIAL;
  css_after_brace=CSS;
}

static void reset_parser()
{
  if(string) free(string);
  string=NULL;
  stringallocsize=0; stringlen=0;
  after_tag=INITIAL;
  css_after_brace=CSS;
  YY_FLUSH_BUFFER;
  BEGIN(INITIAL);
  html_yylval=NULL;
}

