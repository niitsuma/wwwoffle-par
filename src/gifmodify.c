/***************************************
  $Header: /home/amb/wwwoffle/src/RCS/gifmodify.c 1.5 2002/06/23 15:05:22 amb Exp $

  WWWOFFLE - World Wide Web Offline Explorer - Version 2.7c.
  A function for filtering GIF data streams in order to "disable" the mostly
  annoying animations.
  ******************/ /******************
  Written by Ingo Kloecker
  Modified by Andrew M. Bishop

  Copyright 1999 Ingo Kloecker

  This file may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/


#include "autoconfig.h"

#include <string.h>

#include "wwwoffle.h"
#include "document.h"
#include "errors.h"

/*
  OutputGIFWithModifications - function to disable the animation of GIF89a files

  Short description:
  In order to "disable" the animation the function searches for Graphic
  Control Extension and sets the delay time for the following image to
  655.35 seconds (which is the maximal possible setting as for example
  the Netscape Navigator misinterprets a value of 0 as no delay instead of 
  infinite delay).
  The GIF89a Specification of CompuServe Inc. says the following about the
  delay time:
    "Delay Time - If not 0, this field specifies the number of
                  hundredths (1/100) of a second to wait before
	  	  continuing with the processing of the Data Stream."
  and
    "In the absence of a specified Delay Time, the decoder should wait
     for user input indefinitely."
  
  Here you can find more information about the structure of GIF files:
  http://members.aol.com/royalef/gifabout.htm
*/

void OutputGIFWithModifications(int client,int spool,/*@unused@*/ URL *Url)
{
 int bytes_to_skip=0;
 unsigned char blocktype='t';
 int pHeader=0;
 unsigned char GIFHeader[7];
 int colors;
 int colorbits;
 int filterGIF = 1;

 unsigned char gifstream[READ_BUFFER_SIZE];
 int n;

 while((n=read_data(spool,(char*)gifstream,READ_BUFFER_SIZE))>0)
   {
    int offset=0;

    while ((filterGIF == 1) && (bytes_to_skip+offset < n)) {
     offset += bytes_to_skip;
     bytes_to_skip = 0;
     if (blocktype==0) {
      /* Identify next block */
      blocktype = gifstream[offset];
      switch (blocktype) {
      case ',': /* Image Descriptor */
       /* PrintMessage(Debug,"Image Descriptor Block"); */
	bytes_to_skip=9; /* skip unnecessary information */
	break;
      case '!': /* Extension Block */
       /* PrintMessage(Debug,"Extension Block"); */
	bytes_to_skip=1; /* skip to Extension Block Identifier */
	break;
      case ';': /* GIF Terminator */
       /* PrintMessage(Debug,"Terminator Block"); */
	filterGIF = 0; /* end filtering of GIF file */
	break;
      default : /* Unknown Block */
       /* PrintMessage(Debug,"Unknown Block Type %d in GIF file.", blocktype); */
	filterGIF = -1;
      }
    }
    else {
      switch (blocktype) {
      case 't': /* Checking GIF-Header */
	if (pHeader<5) { /* If Header hasn't been completely read */
	  GIFHeader[pHeader]=gifstream[offset];
	  pHeader++;
	  bytes_to_skip=1;
	}
	else {
	  GIFHeader[pHeader]=gifstream[offset];		    
	  GIFHeader[6]='\0';
	  if (!strncmp(GIFHeader,"GIF89a",6)) {
           /* PrintMessage(Debug,"File is a GIF89a."); */
	    filterGIF = 1;
	    bytes_to_skip=1+4; /* skip unnecessary information */
	    blocktype='G';
	  }
	  else
	    filterGIF = 0;
	}
	break;
      case 'G': /* Inside Logical Screen Descriptor */
	/* If Global Color Map exists */
	if (gifstream[offset] & 0x80) {
	  colorbits = gifstream[offset] & 0x07;
	  colors=2;
	  for (; colorbits>0; colorbits--)
	    colors*=2;
	}
	else
	  colors=0;
        /* PrintMessage(Debug,"Global Color Map has %d colors.",colors); */
	/* skip last 3 bytes of Logical Screen Descr. and optional GCM */
	bytes_to_skip=3+3*colors;
	blocktype=0;
	break;
      case ',': /* Image Descriptor */
	/* If Local Color Map exists */
	if (gifstream[offset] & 0x80) {
	  colorbits = gifstream[offset] & 0x07;
	  colors=2;
	  for (; colorbits>0; colorbits--)
	    colors*=2;
	}
	else {
	  colors=0;
	}
        /* PrintMessage(Debug,"Image has %d colors.",colors); */
	/* skip last byte of Image Descr., optional LCM and first byte of Raster Data Stream (LZW Minimum Code Size) */
	bytes_to_skip=1+3*colors+1;
	blocktype='D'; /* set blocktype to Data Block */
	break;
      case 'D': /* Data Block */
	bytes_to_skip=1+gifstream[offset];
	if (bytes_to_skip < 1)
	  bytes_to_skip += 256; 
        /* PrintMessage(Debug,"Inside Data Block (blocksize=%d).",bytes_to_skip-1); */
	if (bytes_to_skip == 1) /* If end of Block reached */
	  blocktype=0; /* reset blocktype */
	break;
      case '!': /* Extension Block */
	/* Falls Graphics Control Extension Block */
        if (gifstream[offset]==249) {
         /* PrintMessage(Debug,"Graphics Control Extension Block."); */
	  bytes_to_skip=3; /* goto delay time entry */
	  blocktype='1';
	}
	else { /* skip this block */
         /* PrintMessage(Debug,"Other Extension Block."); */
	  bytes_to_skip=1;
	  blocktype='D';
	}
	break;
      case '1': /* first byte of delay time */
	/* set delay time to maximum (0xFFFF = 655.35s) */
       /* PrintMessage(Debug,"1st byte of delay time was %d",gifstream[offset]); */
	gifstream[offset]=255;
	bytes_to_skip=1; /* goto second byte of delay time entry */
	blocktype='2';
	break;
      case '2': /* second byte of delay time */
       /* PrintMessage(Debug,"2nd byte of delay time was %d",gifstream[offset]); */
	gifstream[offset]=255;
	bytes_to_skip=3; /* skip rest of GCE block */
	blocktype=0;
	break;
      }
    }
  }
  if (filterGIF == 1) {
    bytes_to_skip -= n-offset;
  }

  write_data(client,(char*)gifstream,n);
 }
}
