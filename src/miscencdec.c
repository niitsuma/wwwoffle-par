/*
   This file was generated using make_bitvector.pl.
   Do not edit this file, because your changes will be lost the next time make_bitvector.pl is run.
   Edit miscencdec.c.pre instead.
*/
#line 1 "miscencdec.c.pre"
/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/miscencdec.c 1.15 2006/01/15 10:13:18 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.9.
  Miscellaneous HTTP / HTML Encoding & Decoding functions.
  ******************/ /******************
  Written by Andrew M. Bishop
  Modified by Paul A. Rombouts

  This file Copyright 1997,98,99,2000,01,02,03,04,05,06 Andrew M. Bishop
  Parts of this file Copyright (C) 2002,2003,2004,2005,2006,2007 Paul A. Rombouts
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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
static const unsigned char unhexstring[256]={
#line 48 "miscencdec.c.pre"
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
    0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
#line 50 "miscencdec.c.pre"
};

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
 size_t length=0;
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

 static const unsigned char allowed[256/8]= {
#line 151 "miscencdec.c.pre"
   0x00,0x00,0x00,0x00  /*                                */
  ,0x12,0xf7,0xff,0x07  /* !  $   ()* ,-./0123456789:     */
  ,0xfe,0xff,0xff,0x87  /* ABCDEFGHIJKLMNOPQRSTUVWXYZ    _*/
  ,0xfe,0xff,0xff,0x57  /* abcdefghijklmnopqrstuvwxyz | ~ */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
#line 160 "miscencdec.c.pre"
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
 size_t length=0;
 char *copy;
 char *p; const char *q;

 /*
   The characters in the range 0x00-0x1f and 0x7f-0xff are always disallowed.
   The '%' character is always disallowed because it is the quote character.
   RFC 1738 section 2.2 calls " <>"#%{}|\^~[]`" unsafe characters, I make an exception for '~'.
   RFC 1738 section 2.2 calls ";/?:@=&" reserved characters, I make an exception for ";/:=".
   I disallow "'" because it may lead to confusion.
 */

 static const unsigned char allowed[256/8]= {
#line 241 "miscencdec.c.pre"
   0x00,0x00,0x00,0x00  /*                                */
  ,0x12,0xff,0xff,0x2f  /* !  $   ()*+,-./0123456789:; =  */
  ,0xfe,0xff,0xff,0x87  /* ABCDEFGHIJKLMNOPQRSTUVWXYZ    _*/
  ,0xfe,0xff,0xff,0x47  /* abcdefghijklmnopqrstuvwxyz   ~ */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
#line 250 "miscencdec.c.pre"
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
 size_t length=0;
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

 static const unsigned char allowed[256/8]= {
#line 302 "miscencdec.c.pre"
   0x00,0x00,0x00,0x00  /*                                */
  ,0x12,0xf7,0xff,0x07  /* !  $   ()* ,-./0123456789:     */
  ,0xfe,0xff,0xff,0x87  /* ABCDEFGHIJKLMNOPQRSTUVWXYZ    _*/
  ,0xfe,0xff,0xff,0x57  /* abcdefghijklmnopqrstuvwxyz | ~ */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
#line 311 "miscencdec.c.pre"
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
 size_t length=0;
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

 static const unsigned char allowed[256/8]= {
#line 365 "miscencdec.c.pre"
   0x00,0x00,0x00,0x00  /*                                */
  ,0x12,0x7c,0xff,0x23  /* !  $     *+,-. 0123456789   =  */
  ,0xfe,0xff,0xff,0x87  /* ABCDEFGHIJKLMNOPQRSTUVWXYZ    _*/
  ,0xfe,0xff,0xff,0x47  /* abcdefghijklmnopqrstuvwxyz   ~ */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
  ,0x00,0x00,0x00,0x00  /*                                */
#line 374 "miscencdec.c.pre"
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

  const char *str The form data or arguments to split.
  ++++++++++++++++++++++++++++++++++++++*/

char **SplitFormArgs(const char *str)
{
 char **args;
 char *copy,*p;
 unsigned int i,n=1;

 copy=strdup(str);

 for(p=copy;*p;++p) if(*p=='&' || *p==';') {*p=0; ++n;}

 args=(char**)malloc(sizeof(char*)*(n+1));
 args[0]=copy;
 for(p=copy,i=1; i<n; ++i) {p=strchrnul(p,0)+1; args[i]=p; }
 args[n]=NULL;

 return(args);
}


/*++++++++++++++++++++++++++++++++++++++
  Trim any leading or trailing whitespace from an argument string

  char *TrimArgs The string to modify

  char *str The modified string (modifications made in-place).
  ++++++++++++++++++++++++++++++++++++++*/

char *TrimArgs(char *str)
{
 char *l=str,*r;

 if(isspace(*l))
   {
    l++;

    while(isspace(*l))
       l++;

    r=str;
    while(*l)
       *r++=*l++;

    *r=0;
   }
 else
    r=strchrnul(str,0);

 while(--r>=str && isspace(*r))
   *r=0;

 return(str);
}


/*++++++++++++++++++++++++++++++++++++++
  Generate a hash value for a string.

  MakeHash Returns a binary hash that can be converted to a string with hashbase64encode().

  const char *args The sequence of chars to hash.
  unsigned len The length of the sequence of chars.
  md5hash_t *h  The computed hash value.
  ++++++++++++++++++++++++++++++++++++++*/

void MakeHash(const char *args, unsigned len, md5hash_t *h)
{
 struct MD5Context ctx;

 /* Initialize the computation context.  */
 MD5Init (&ctx);

 /* Process whole buffer but last len % 64 bytes.  */
 MD5Update (&ctx, (const unsigned char *)args, len);

 /* Put result in desired memory area.  */
 MD5Final ((unsigned char *)h, &ctx);
}


char *hashbase64encode(md5hash_t *h, unsigned char *buf, unsigned buflen)
{
 char *hash,*p;

 hash=(char *)Base64Encode((unsigned char *)h,sizeof(md5hash_t),buf,buflen);

 if(hash) {
   for(p=hash;*p;p++)
     if(*p=='/')
       *p='-';
     else if(*p=='=')
       *p=0;
 }
 return(hash);
}


/* Added by Paul Rombouts:
   Get the base64 encoded string version of the URL hash. */
char *GetHash(URL *Url,char *buf, unsigned buflen)
{
  if(!(Url->hashvalid)) {
    MakeStrHash(Url->file,&Url->hash);
    Url->hashvalid=1;
  }
  return hashbase64encode(&Url->hash,(unsigned char *)buf,buflen);
}


/*+ Conversion from time_t to date string and back (day of week). +*/
static const char* const weekdays[7]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};

/*+ Conversion from time_t to date string and back (month of year). +*/
static const char* const months[12]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};


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

  const time_t duration The duration in seconds.
  char *buf  A pointer to the buffer to hold the result.
  ++++++++++++++++++++++++++++++++++++++*/

void DurationToString_r(const time_t duration,char *buf)
{
 char *p=buf;
 time_t weeks,days,hours,minutes,seconds=duration;

 if(seconds>=0) {
   weeks=seconds/(3600*24*7);
   seconds-=(3600*24*7)*weeks;

   days=seconds/(3600*24);
   seconds-=(3600*24)*days;

   hours=seconds/(3600);
   seconds-=(3600)*hours;

   minutes=seconds/(60);
   seconds-=(60)*minutes;

   if(weeks>4)
     p+=sprintf(p,"%uw ",(unsigned)weeks);
   else
     {days+=7*weeks;weeks=0;}

   if(days)
     p+=sprintf(p,"%ud ",(unsigned)days);

   if(seconds)
     p+=sprintf(p,"%02u:%02u:%02u ",(unsigned)hours,(unsigned)minutes,(unsigned)seconds);
   else if(minutes)
     p+=sprintf(p,"%02u:%02u ",(unsigned)hours,(unsigned)minutes);
   else if(hours)
     p+=sprintf(p,"%uh ",(unsigned)hours);
 }

 sprintf(p,"(%lds)",(long)duration);
}


/*+ The conversion from a 6 bit value to an ASCII character. +*/
static const unsigned char base64[64]={
  'A','B','C','D','E','F','G','H','I','J','K','L','M','N','O','P',
  'Q','R','S','T','U','V','W','X','Y','Z','a','b','c','d','e','f',
  'g','h','i','j','k','l','m','n','o','p','q','r','s','t','u','v',
  'w','x','y','z','0','1','2','3','4','5','6','7','8','9','+','/'
};

/*+ The conversion from an ASCII character to a 6 bit value. +*/
static const unsigned char invbase64[256]={
#line 714 "miscencdec.c.pre"
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, 62,  0, 63,  0, 63,
   52, 53, 54, 55, 56, 57, 58, 59, 60, 61,  0,  0,  0,  0,  0,  0,
    0,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
   15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,  0,  0,  0,  0,  0,
    0, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
   41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0
#line 719 "miscencdec.c.pre"
};

/*++++++++++++++++++++++++++++++++++++++
  Decode a base 64 string.

  unsigned char *Base64Decode Return a string containing the decoded version.

  const unsigned char *str The string to be decoded.

  size_t *lp Returns the length of the decoded string.

  unsigned char *buf  If buf is not null, the result is stored in buf,
                      otherwise an malloced buffer is used.

  size_t buflen  The size of buf.
  ++++++++++++++++++++++++++++++++++++++*/

unsigned char *Base64Decode(const unsigned char *str,size_t *lp, unsigned char *buf, size_t buflen)
{
 size_t i,j,k,l,le=strlen((const char *)str);
 unsigned char *decoded;

 while(--le>=0 && str[le]=='=');

 ++le;

 l= (3*le)/4;  /* floor((3./4.)*le) */
 if(lp) *lp=l;

 if(buf) {
   if(l>=buflen) return NULL;
   decoded=buf;
 }
 else {
   decoded=(unsigned char *)malloc(l+1);
   if(!decoded) return NULL;
 }

 for(i=j=0; j<le; i+=3,j+=4)
   {
    unsigned long s=0;
    size_t klim;
    int off;

    klim=j+4;
    if(klim>le) klim=le;
    for(k=j,off=24; k<klim; k++)
      s|=((unsigned long)invbase64[str[k]&0xff])<<(off-=6);

    klim=i+3;
    if(klim>l) klim=l;
    for(k=i,off=24; k<klim; k++)
      decoded[k]= (s>>(off-=8))&0xff;
   }
 decoded[l]=0;

 return(decoded);
}


/*++++++++++++++++++++++++++++++++++++++
  Encode a string into base 64.

  unsigned char *Base64Encode Return a string containing the encoded version.

  const char *str The string to be encoded.

  size_t l The length of the string to be encoded.

  unsigned char *buf  If buf is not null, the result is stored in buf,
                      otherwise an malloced buffer is used.

  size_t buflen  The size of buf.
  ++++++++++++++++++++++++++++++++++++++*/

unsigned char *Base64Encode(const unsigned char *str,size_t l, unsigned char *buf, size_t buflen)
{
 size_t i,j,k,le=(4*l+2)/3;  /* ceil((4./3.)*l) */
 unsigned char *encoded;

 {
   size_t reslen=4*((le+3)/4);
   if(buf) {
     if(reslen>=buflen) return NULL;
     encoded=buf;
   }
   else {
     encoded=(unsigned char *)malloc(reslen+1);
     if(!encoded) return NULL;
   }
 }

 for(i=j=0; i<l; i+=3,j+=4)
   {
    unsigned long s=0;
    size_t klim;
    int off;

    klim=i+3;
    if(klim>l) klim=l;
    for(k=i,off=24; k<klim; k++)
      s|=((unsigned long)(str[k]&0xff))<<(off-=8);

    klim=j+4;
    if(klim>le) klim=le;
    for(k=j,off=24; k<klim; k++)
      encoded[k]=base64[(s>>(off-=6))&0x3f];
   }

 for(j=le; j%4; j++)
    encoded[j]='=';

 encoded[j]=0;

 return(encoded);
}


/*++++++++++++++++++++++++++++++++++++++
  Replace all occurences of '&amp;' with '&' by modifying the string in place.

  char *string The string to be modified.
  ++++++++++++++++++++++++++++++++++++++*/

void URLReplaceAmp(char *string)
{
 char *q,*p=string;

 while(*p)
   {
    if(*p=='&' &&
       (*(p+1)=='a' || *(p+1)=='A') &&
       (*(p+2)=='m' || *(p+2)=='M') &&
       (*(p+3)=='p' || *(p+3)=='P') &&
       *(p+4)==';')
      {
       q=++p;
       p+=4;
       goto shift;
      }

    ++p;
   }

 return;

shift:
 while(*p)
   {
    if(*p=='&' &&
       (*(p+1)=='a' || *(p+1)=='A') &&
       (*(p+2)=='m' || *(p+2)=='M') &&
       (*(p+3)=='p' || *(p+3)=='P') &&
       *(p+4)==';')
      {
       *q++=*p++;
       p+=4;
      }
    else
      {
       *q++=*p++;
      }
   }

 *q=0;
}


/*++++++++++++++++++++++++++++++++++++++
  Make the input string safe to output as HTML ( not < > & " ).

  char* HTMLString Returns a safe HTML string.

  const char* c A non-safe HTML string.

  int nbsp Use a non-breaking space in place of normal ones.
           In addition, if nbsp=2, an itemized list is produced
           (each line is preceded by <li>).

  size_t *lenp  If lenp is not NULL, *lenp should initially contain the length of the input string
                and will be assigned the length of the output string.
  ++++++++++++++++++++++++++++++++++++++*/

char* HTMLString(const char* c, int nbsp, size_t *lenp)
{
 size_t length= 0;
 char *result;
 char *p; const char *q, *end;

 if(lenp) {
   end = c + *lenp;
   if(nbsp==2 && c<end) length+=strlitlen("<li>");

   for(q=c; q<end; ++q)
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
       if(nbsp==2 && q+1<end) length+=strlitlen("<li>");
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

   *lenp= length;
 }
 else {
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

   end= q;
 }

 result=p=(char*)malloc(length+1);

 if(nbsp==2 && c<end) {
   *p++='<';
   *p++='l';
   *p++='i';
   *p++='>';
 }

 for(q=c; q<end; ++q)
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
     if(nbsp==2 && q+1<end) {
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



char *HTML_url(char *url)
{
  char *ques=strchr(url,'?');
  char *dec,*result;

  if(ques)
    {
      char *dec_path,*dec_args;

      dec_path=STRDUP3(url,ques,URLDecodeGeneric);
      dec_args=URLDecodeFormArgs(ques+1);

      dec=(char*)malloc(strlen(dec_path)+strlen(dec_args)+2);
      {
	char *p=stpcpy(dec,dec_path);
	*p++='?';
	stpcpy(p,dec_args);
      }
      free(dec_path);
      free(dec_args);
    }
  else
    dec=URLDecodeGeneric(url);

  result=HTMLString(dec,1,NULL);

  free(dec);

  return result;
}


/* Make a string safe to use inside an HTML comment (break up '--').
   char *HTMLcommentstring returns its original argument or a pointer
   to a newly allocated string.
   If lenp is not NULL, *lenp should initially contain the length of the input string
   or -1 and will be assigned the length of the output string.
 */
char *HTMLcommentstring(char *str, size_t *lenp)
{
 size_t length=0,dlength=0, totlength;
 char *result;
 char *p; const char *q=str, *end;
 char c;

 if(lenp && *lenp!= ~(size_t)0) {
   length= *lenp;
   end= str+length;
   if(q<end)
     while(c= *q++, q<end)
       if(c=='-' && *q=='-')
	 ++dlength;
 }
 else {
   while((c= *q++)) {
     ++length;
     if(c=='-' && *q=='-')
       ++dlength;
   }
   end= q-1;
 }

 totlength= length+dlength;
 if(lenp) *lenp= totlength;
 if(!dlength)
   return str;

 result=p=(char*)malloc(totlength+1);

 q=str;
 if(q<end)
   while(*p++= c= *q++, q<end)
     if(c=='-' && *q=='-')
       *p++=' ';

 *p=0;

 return(result);

}

/* This is like strchrnul(), but only returns a match if the char
   is not escaped by a back-slash. */
char *strunescapechr(const char *str, char c)
{
  const char *p;

  for(p=str; *p; ++p) {
    if(*p=='\\') {
      if(!*++p) break;
    }
    else if(*p==c)
      break;
  }

  return (char *)p;
}

/* This is like strpbrk(), but only returns a match if the char
   is not escaped by a back-slash. */
char *strunescapepbrk(const char *str, const char *stopset)
{
  const char *p, *q;
  char c,d;

  for(p=str; (c= *p); ++p) {
    if(c=='\\') {
      if(!*++p) break;
    }
    else {
      q=stopset;
      while((d= *q++)) if(c==d) goto ret_p;
    }
  }

 ret_p:
  return (char *)p;
}

#if 0
size_t strunescapelen(const char *str)
{
  size_t len=0;
  const char *p;

  for(p=str; *p; ++p) {
    if(*p=='\\' && !*++p)
      break;

    ++len;
  }

  return len;
}

/* Like stpcpy(), but writes a decoded version of src to dst,
   i.e. it removes back-slashes from escape sequences. */
char *strunescapecpy(char *dst, const char *src)
{
  char *p=dst; const char *q=src;

  while(*q) {
    if(*q=='\\' && !*++q)
      break;

    *p++ = *q++;
  }

  *p=0;
  return p;
}

char *strunescapedup(const char *str)
{
  char *copy = malloc(strunescapelen(str)+1);
  strunescapecpy(copy,str);
  return copy;
}
#endif

char *strunescapechr2(const char *str, const char *end, char c)
{
  const char *p;

  for(p=str; p<end; ++p) {
    if(*p=='\\') {
      if(++p>=end) break;
    }
    else if(*p==c)
      break;
  }

  return (char *)p;
}

size_t strunescapelen2(const char *str, const char *end)
{
  size_t len=0;
  const char *p;

  for(p=str; p<end; ++p) {
    if(*p=='\\' && ++p>=end)
      break;

    ++len;
  }

  return len;
}

char *strunescapecpy2(char *dst, const char *src, const char *end)
{
  char *p=dst; const char *q=src;

  while(q<end) {
    if(*q=='\\' && ++q>=end)
      break;

    *p++ = *q++;
  }

  *p=0;
  return p;
}

#if 0
char *strunescapedup2(const char *str, const char *end)
{
  char *copy = malloc(strunescapelen2(str,end)+1);
  strunescapecpy2(copy,str,end);
  return copy;
}
#endif
