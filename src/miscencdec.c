/*
   This file was generated using make_bitvector.pl.
   Do not edit this file, because your changes will be lost the next time make_bitvector.pl is run.
   Edit miscencdec.c.pre instead.
*/
/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/miscencdec.c 1.5 2002/04/13 14:45:24 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7b.
  Miscellaneous HTTP / HTML Encoding & Decoding functions.
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1997,98,99,2000,01,02 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

#include "misc.h"
#include "md5.h"


/* To understand why the URLDecode*() and URLEncode*() functions are coded this way see README.URL */

/*+ For conversion from integer to hex string. +*/
static const char hexstring[16]={'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};

/*+ For conversion from hex string to integer. +*/
static const unsigned char unhexstring[256]={ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x00-0x0f "                " */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x10-0x1f "                " */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x20-0x2f " !"#$%&'()*+,-./" */
					      0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0, 0, 0,  /* 0x30-0x3f "0123456789:;<=>?" */
					      0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x40-0x4f "@ABCDEFGHIJKLMNO" */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x50-0x5f "PQRSTUVWXYZ[\]^_" */
					      0,10,11,12,13,14,15, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x60-0x6f "`abcdefghijklmno" */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x70-0x7f "pqrstuvwxyz{|}~ " */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x80-0x8f "                " */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0x90-0x9f "                " */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xa0-0xaf "                " */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xb0-0xbf "                " */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xc0-0xcf "                " */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xd0-0xdf "                " */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  /* 0xe0-0xef "                " */
					      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}; /* 0xf0-0xff "                " */


/* Added by Paul Rombouts */
inline static unsigned char test_bit(const unsigned char *bvec, unsigned char c)
{
  return (bvec[c>>3]>>(c&7))&1;
}


/*++++++++++++++++++++++++++++++++++++++
  Decode a string that has been UrlEncoded.

  char *URLDecodeGeneric Returns a malloced copy of the decoded string.

  const char *str The generic string to be decoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLDecodeGeneric(const char *str)
{
 char *copy=(char*)malloc(strlen(str)+1);
 char *p; const char *q;

 for(p=copy,q=str; *q; ++q)
    if(*q=='%' && *(q+1) && *(q+2))
      {
       unsigned char val;
       val=unhexstring[(unsigned char)(*++q)]<<4;
       val+=unhexstring[(unsigned char)(*++q)];
       *p++=val;
      }
    else
       *p++=*q;

 *p=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Decode a POSTed form or URL arguments that has been UrlEncoded.

  char *URLDecodeFormArgs Returns a malloced copy of the decoded form data string.

  const char *str The form data string to be decoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLDecodeFormArgs(const char *str)
{
 char *copy=(char*)malloc(strlen(str)+1);
 char *p; const char *q;

 /* This is the same as the generic function except that '+' is used for ' ' instead of '%20'. */

 for(p=copy,q=str; *q; ++q)
    if(*q=='%' && *(q+1) && *(q+2))
      {
       unsigned char val;
       val=unhexstring[(unsigned char)(*++q)]<<4;
       val+=unhexstring[(unsigned char)(*++q)];
       *p++=val;
      }
    else if(*q=='+')
       *p++=' ';
    else
       *p++=*q;

 *p=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Take a POSTed form data or a URL argument string and URLDecode and URLEncode it again.

  char *URLRecodeFormArgs Returns a malloced copy of the decoded and re-encoded form/argument string.

  const char *str The form/argument string to be decoded and re-encoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLRecodeFormArgs(const char *str)
{
 int length=0;
 char *copy;
 char *p; const char *q;

 /*
   The characters in the range 0x00-0x1f and 0x7f-0xff are always disallowed.
   The '%' character is always disallowed because it is the quote character.
   RFC 1738 section 2.2 calls " <>"#%{}|\^~[]`" unsafe characters, I make an exception for "|~".
   RFC 1738 section 2.2 calls ";/?:@=&" reserved characters, I make an exception for "/:".
   RFC 1866 section 8.2.1 says that ' ' is replaced by '+'.
   I disallow "'" because it may lead to confusion.
   The unencoded characters "&=;" on the input are left unencoded on the output
   The encoded character "+" on the input is left encoded on the output
   The unencoded character "?" on the input is left unencoded on the output to handle broken servers.
 */

 static const unsigned char allowed[]= {
   0x00,0x00,0x00,0x00  /*                                */
  ,0x12,0xf7,0xff,0x07  /* !  $   ()* ,-./0123456789:     */
  ,0xfe,0xff,0xff,0x87  /* ABCDEFGHIJKLMNOPQRSTUVWXYZ    _*/
  ,0xfe,0xff,0xff,0x57  /* abcdefghijklmnopqrstuvwxyz | ~ */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
 };

 for(q=str; *q; ++q)
   if(test_bit(allowed, (unsigned char)(*q)) || *q=='&' || *q=='=' || *q==';' || *q=='+' || *q=='?' || *q==' ')
     length+=1;
   else if(*q=='%' && *(q+1) && *(q+2))
     {
       unsigned char val;
       val=unhexstring[(unsigned char)(*++q)]<<4;
       val+=unhexstring[(unsigned char)(*++q)];

       if(test_bit(allowed, val) || val==' ')
	 length+=1;
       else
	 length+=3;
     }
   else
     length+=3;

 copy=(char*)malloc(length+1);

 for(p=copy,q=str; *q; ++q)
   if(test_bit(allowed, (unsigned char)(*q)) || *q=='&' || *q=='=' || *q==';' || *q=='+' || *q=='?')
     *p++=*q;
   else if(*q==' ')
     *p++='+';
   else if(*q=='%' && *(q+1) && *(q+2))
     {
       unsigned char val;
       val=unhexstring[(unsigned char)(*++q)]<<4;
       val+=unhexstring[(unsigned char)(*++q)];

       if(test_bit(allowed, val))
	 *p++=val;
       else if(val==' ')
	 *p++='+';
       else
         {
	   *p++='%';
	   *p++=toupper(*(q-1));
	   *p++=toupper(*q);
         }
     }
   else
     {
       unsigned char val=*q;
       *p++='%';
       *p++=hexstring[(val>>4)&0x0f];
       *p++=hexstring[val&0x0f];
     }

 *p=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Encode a string suitable for use as a pathname.

  char *URLEncodePath Returns a malloced copy of the encoded pathname string.

  const char *str The pathname string to be encoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLEncodePath(const char *str)
{
 int length=0;
 char *copy;
 char *p; const char *q;

 /*
   The characters in the range 0x00-0x1f and 0x7f-0xff are always disallowed.
   The '%' character is always disallowed because it is the quote character.
   RFC 1738 section 2.2 calls " <>"#%{}|\^~[]`" unsafe characters, I make an exception for '~'.
   RFC 1738 section 2.2 calls ";/?:@=&" reserved characters, I make an exception for "/:=".
   I disallow "'" because it may lead to confusion.
 */

 static const unsigned char allowed[]= {
   0x00,0x00,0x00,0x00  /*                                */
  ,0x12,0xff,0xff,0x27  /* !  $   ()*+,-./0123456789:  =  */
  ,0xfe,0xff,0xff,0x87  /* ABCDEFGHIJKLMNOPQRSTUVWXYZ    _*/
  ,0xfe,0xff,0xff,0x47  /* abcdefghijklmnopqrstuvwxyz   ~ */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
 };

 for(q=str; *q; ++q)
   if(test_bit(allowed, (unsigned char)(*q)))
     length+=1;
   else
     length+=3;

 copy=(char*)malloc(length+1);

 for(p=copy,q=str; *q; ++q)
   if(test_bit(allowed, (unsigned char)(*q)))
     *p++=*q;
   else
     {
       unsigned char val= *q;
       *p++='%';
       *p++=hexstring[(val>>4)&0x0f];
       *p++=hexstring[val&0x0f];
     }

 *p=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Encode a string suitable for use with the POST method or as URL arguments.

  char *URLEncodeFormArgs Returns a malloced copy of the encoded form data string.

  const char *str The form data string to be encoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLEncodeFormArgs(const char *str)
{
 int length=0;
 char *copy;
 char *p; const char *q;

 /*
   The characters in the range 0x00-0x1f and 0x7f-0xff are always disallowed.
   The '%' character is always disallowed because it is the quote character.
   RFC 1738 section 2.2 calls " <>"#%{}|\^~[]`" unsafe characters, I make an exception for "|~".
   RFC 1738 section 2.2 calls ";/?:@=&" reserved characters, I make an exception for "/:".
   RFC 1866 section 8.2.1 says that ' ' is replaced by '+'.
   I disallow ""'\`" because they may lead to confusion.
 */

 static const unsigned char allowed[]= {
   0x00,0x00,0x00,0x00  /*                                */
  ,0x12,0xf7,0xff,0x07  /* !  $   ()* ,-./0123456789:     */
  ,0xfe,0xff,0xff,0x87  /* ABCDEFGHIJKLMNOPQRSTUVWXYZ    _*/
  ,0xfe,0xff,0xff,0x57  /* abcdefghijklmnopqrstuvwxyz | ~ */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
 };

 for(q=str; *q; ++q)
   if(test_bit(allowed, (unsigned char)(*q)) || *q==' ')
     length+=1;
   else
     length+=3;

 copy=(char*)malloc(length+1);

 for(p=copy,q=str; *q; ++q)
   if(test_bit(allowed, (unsigned char)(*q)))
     *p++=*q;
   else if(*q==' ')
     *p++='+';
   else
     {
       unsigned char val=*q;
       *p++='%';
       *p++=hexstring[(val>>4)&0x0f];
       *p++=hexstring[val&0x0f];
     }

 *p=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Encode a string suitable for use as a username / password in a URL.

  char *URLEncodePassword Returns a malloced copy of the encoded username / password string.

  const char *str The password / username string to be encoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *URLEncodePassword(const char *str)
{
 int length=0;
 char *copy;
 char *p; const char *q;

 /*
   The characters in the range 0x00-0x1f and 0x7f-0xff are always disallowed.
   The '%' character is always disallowed because it is the quote character.
   RFC 1738 section 2.2 calls " <>"#%{}|\^~[]`" unsafe characters, I make an exception for '~'.
   RFC 1738 section 2.2 calls ";/?:@=&" reserved characters, I make an exception for "=".
   RFC 1738 section 3.1 says that "@:/" are disallowed.
   I disallow "'()" because they may lead to confusion.
 */

 static const unsigned char allowed[]= {
   0x00,0x00,0x00,0x00  /*                                */
  ,0x12,0x7c,0xff,0x23  /* !  $     *+,-. 0123456789   =  */
  ,0xfe,0xff,0xff,0x87  /* ABCDEFGHIJKLMNOPQRSTUVWXYZ    _*/
  ,0xfe,0xff,0xff,0x47  /* abcdefghijklmnopqrstuvwxyz   ~ */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
 };

 for(q=str; *q; ++q)
   if(test_bit(allowed, (unsigned char)(*q)))
     length+=1;
   else
     length+=3;

 copy=(char*)malloc(length+1);

 for(p=copy,q=str; *q; ++q)
   if(test_bit(allowed, (unsigned char)(*q)))
     *p++=*q;
   else
     {
       unsigned char val=*q;
       *p++='%';
       *p++=hexstring[(val>>4)&0x0f];
       *p++=hexstring[val&0x0f];
     }

 *p=0;

 return(copy);
}


/*++++++++++++++++++++++++++++++++++++++
  Split a form body or URL arguments up into the component parts.

  char **SplitFormArgs Returns an array of pointers into a copy of the string.

  char *str The form data or arguments to split.
  ++++++++++++++++++++++++++++++++++++++*/

char **SplitFormArgs(char *str)
{
 char **args;
 char *copy,*p;
 int i,n=1;

 copy=strdup(str);

 for(p=copy;*p;++p) if(*p=='&' || *p==';') {*p=0; ++n;}

 args=(char**)malloc(sizeof(char*)*(n+1));
 args[0]=copy;
 for(p=copy,i=1; i<n; ++i) {p=strchrnul(p,0)+1; args[i]=p; }
 args[n]=NULL;

 return(args);
}


/*++++++++++++++++++++++++++++++++++++++
  Generate a hash value for a string.

  char *MakeHash Returns a string that can be used as the hashed string.

  const char *args The arguments.
  ++++++++++++++++++++++++++++++++++++++*/

char *MakeHash(const char *args)
{
 char md5[17];
 char *hash,*p;
 struct MD5Context ctx;

 /* Initialize the computation context.  */
 MD5Init (&ctx);

 /* Process whole buffer but last len % 64 bytes.  */
 MD5Update (&ctx, (const unsigned char*)args, strlen(args));

 /* Put result in desired memory area.  */
 MD5Final ((unsigned char *)md5, &ctx);

 md5[16]=0;

 hash=Base64Encode(md5,16);

 for(p=hash;*p;p++)
    if(*p=='/')
       *p='-';
    else if(*p=='=')
       *p=0;

 return(hash);
}


/*+ Conversion from time_t to date string and back +*/
static const char *weekdays[7]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char *months[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};


/*++++++++++++++++++++++++++++++++++++++
  Convert the time into an RFC 822 compliant date.

  RFC822Date_r Writes the date into a buffer pointed to by its last argument.

  time_t t The time.

  int utc Set to true to get Universal Time, else localtime.
  char *buf  A pointer to the buffer to hold the result.
  ++++++++++++++++++++++++++++++++++++++*/

void RFC822Date_r(time_t t,int utc,char *buf)
{
 char weekday[4];
 char month[4];
 struct tm tim;

 if(utc) /* Get UTC using English language. */
   {
    gmtime_r(&t,&tim);

    strcpy(weekday,weekdays[tim.tm_wday]);
    strcpy(month,months[tim.tm_mon]);
   }
 else /* Get the local time using current language. */
   {
    localtime_r(&t,&tim);

    if(tim.tm_isdst<0)
      {gmtime_r(&t,&tim);utc=1;}

    strftime(weekday,4,"%a",&tim);
    strftime(month,4,"%b",&tim);
   }

 /* Sun, 06 Nov 1994 08:49:37 GMT    ; RFC 822, updated by RFC 1123 */

 sprintf(buf,"%3s, %02d %3s %4d %02d:%02d:%02d %s",
         weekday,
         tim.tm_mday,
         month,
         tim.tm_year+1900,
         tim.tm_hour,
         tim.tm_min,
         tim.tm_sec,
#if defined(HAVE_TM_ZONE)
         utc?"GMT":tim.tm_zone
#elif defined(HAVE_TZNAME)
         utc?"GMT":tzname[tim.tm_isdst>0]
#elif defined(__CYGWIN__)
         utc?"GMT":_tzname[tim.tm_isdst>0]
#else
         utc?"GMT":"???"
#endif
         );

}


/*++++++++++++++++++++++++++++++++++++++
  Convert the time into an RFC 822 compliant date.

  char *RFC822Date Returns a pointer to a fixed string containing the date.

  time_t t The time.

  int utc Set to true to get Universal Time, else localtime.
  ++++++++++++++++++++++++++++++++++++++*/

char *RFC822Date(time_t t,int utc)
{
 static char value[MAXDATESIZE];

 RFC822Date_r(t,utc,value);
 return value;
}


/*++++++++++++++++++++++++++++++++++++++
  Convert a string representing a date into a time.

  time_t DateToTimeT Returns the time.

  const char *date The date string.
  ++++++++++++++++++++++++++++++++++++++*/

time_t DateToTimeT(const char *date)
{
 int  year,day,hour,min,sec;
 char monthstr[4];
 long retval=0;

 if(sscanf(date,"%*s %d %3s %d %d:%d:%d",&day,monthstr,&year,&hour,&min,&sec)==6 ||
    sscanf(date,"%*s %d-%3s-%d %d:%d:%d",&day,monthstr,&year,&hour,&min,&sec)==6 ||
    sscanf(date,"%*s %3s %d %d:%d:%d %d",monthstr,&day,&hour,&min,&sec,&year)==6)
   {
    struct tm tim;
    int mon;

    for(mon=0;mon<12;mon++)
       if(!strcasecmp(monthstr,months[mon]))
          goto found;
    mon=0;
   found:

    tim.tm_sec=sec;
    tim.tm_min=min;
    tim.tm_hour=hour;
    tim.tm_mday=day;
    tim.tm_mon=mon;
    if(year<38)
       tim.tm_year=year+100;
    else if(year<100)
       tim.tm_year=year;
    else
       tim.tm_year=year-1900;
    tim.tm_isdst=0;
    tim.tm_wday=0;              /* unused */
    tim.tm_yday=0;              /* unused */

    retval=timegm(&tim);

    if(retval==-1)
       retval=0;
   }
 else
    sscanf(date,"%ld %1s",&retval,monthstr);

 return(retval);
}


/*++++++++++++++++++++++++++++++++++++++
  Make up a string that contains a duration (in seconds) in human readable format.

  DurationToString_r writes the string into a buffer pointed to by its last argument.

  const long duration The duration in seconds.
  char *buf  A pointer to the buffer to hold the result.
  ++++++++++++++++++++++++++++++++++++++*/

void DurationToString_r(const long duration,char *buf)
{
 char *p=buf;
 long weeks,days,hours,minutes,seconds=duration;

 weeks=seconds/(3600*24*7);
 seconds-=(3600*24*7)*weeks;

 days=seconds/(3600*24);
 seconds-=(3600*24)*days;

 hours=seconds/(3600);
 seconds-=(3600)*hours;

 minutes=seconds/(60);
 seconds-=(60)*minutes;

 if(weeks>4)
    p+=sprintf(p,"%ldw",weeks);
 else
   {days+=7*weeks;weeks=0;}

 if(days)
    p+=sprintf(p,"%s%ldd",weeks?" ":"",days);

 if(hours || minutes || seconds)
   {
    if(days || weeks)
       *p++=' ';
    if(hours && !minutes && !seconds)
       p+=sprintf(p,"%ldh",hours);
    else if(!seconds)
       p+=sprintf(p,"%02ld:%02ld",hours,minutes);
    else
       p+=sprintf(p,"%02ld:%02ld:%02ld",hours,minutes,seconds);
   }

 sprintf(p," (%lds)",duration);

}


/*+ The conversion from a 6 bit value to an ASCII character. +*/
static const char base64[64]={'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
			      'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
			      'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
			      'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'};

/*++++++++++++++++++++++++++++++++++++++
  Decode a base 64 string.

  char *Base64Decode Return a malloced string containing the decoded version.

  const char *str The string to be decoded.

  int *l Returns the length of the decoded string.
  ++++++++++++++++++++++++++++++++++++++*/

char *Base64Decode(const char *str,int *l)
{
 int le=strlen(str);
 char *decoded=(char*)malloc(le+1);
 int i,j,k;

 while(str[le-1]=='=')
    le--;

 *l=3*(le/4)+(le%4)-1+!(le%4);

 for(j=0;j<le;j++)
    for(k=0;k<64;k++)
       if(base64[k]==str[j])
         {decoded[j]=k;break;}

 for(i=j=0;j<(le+4);i+=3,j+=4)
   {
    unsigned long s=0;

    for(k=0;k<4;k++)
       if((j+k)<le)
          s|=((unsigned long)decoded[j+k]&0xff)<<(18-6*k);

    for(k=0;k<3;k++)
       if((i+k)<*l)
          decoded[i+k]=(char)((s>>(16-8*k))&0xff);
   }
 decoded[*l]=0;

 return(decoded);
}


/*++++++++++++++++++++++++++++++++++++++
  Encode a string into base 64.

  char *Base64Encode Return a malloced string containing the encoded version.

  const char *str The string to be encoded.

  int l The length of the string to be encoded.
  ++++++++++++++++++++++++++++++++++++++*/

char *Base64Encode(const char *str,int l)
{
 int le=4*(l/3)+(l%3)+!!(l%3);
 char *encoded=(char*)malloc(4*(le/4)+4*!!(le%4)+1);
 int i,j,k;

 for(i=j=0;i<(l+3);i+=3,j+=4)
   {
    unsigned long s=0;

    for(k=0;k<3;k++)
       if((i+k)<l)
          s|=((unsigned long)str[i+k]&0xff)<<(16-8*k);

    for(k=0;k<4;k++)
       if((j+k)<le)
          encoded[j+k]=(char)((s>>(18-6*k))&0x3f);
   }

 for(j=0;j<le;j++)
    encoded[j]=base64[(int)encoded[j]];
 for(;j%4;j++)
    encoded[j]='=';
 encoded[j]=0;

 return(encoded);
}


/*++++++++++++++++++++++++++++++++++++++
  Make the input string safe to output as HTML ( not < > & " ).

  char* HTMLString Returns a safe HTML string.

  const char* c A non-safe HTML string.

  int nbsp Use a non-breaking space in place of normal ones.
           In addition, if nbsp=2, an itemized list is produced
           (each line is preceded by <li>).
  ++++++++++++++++++++++++++++++++++++++*/

char* HTMLString(const char* c,int nbsp)
{
 int length= 0;
 char *result;
 char *p; const char *q;

 if(nbsp==2 && *c) length+=strlitlen("<li>");

 for(q=c; *q; ++q)
   switch(*q) {
   case '<':
   case '>':
     length+=strlitlen("&lt;");
     break;
   case '&':
     length+=strlitlen("&amp;");
     break;
   case '\n':
     length+=1;
     if(nbsp==2 && *(q+1)) length+=strlitlen("<li>");
     break;
   case ' ':
     if(nbsp)
       {
	 length+=strlitlen("&nbsp;");
	 break;
       }
     /*@ fallthrough @*/
   default:
     length+=1;
   }

 result=p=(char*)malloc(length+1);

 if(nbsp==2 && *c) {
   *p++='<';
   *p++='l';
   *p++='i';
   *p++='>';
 }

 for(q=c; *q; ++q)
   switch(*q) {
   case '<':
     *p++='&';
     *p++='l';
     *p++='t';
     *p++=';';
     break;
   case '>':
     *p++='&';
     *p++='g';
     *p++='t';
     *p++=';';
     break;
   case '&':
     *p++='&';
     *p++='a';
     *p++='m';
     *p++='p';
     *p++=';';
     break;
   case '\n':
     *p++=*q;
     if(nbsp==2 && *(q+1)) {
       *p++='<';
       *p++='l';
       *p++='i';
       *p++='>';
     }
     break;
   case ' ':
     if(nbsp)
       {
	 *p++='&';
	 *p++='n';
	 *p++='b';
	 *p++='s';
	 *p++='p';
	 *p++=';';
	 break;
       }
     /*@ fallthrough @*/
   default:
     *p++=*q;
   }

 *p=0;

 return(result);
}
