/***************************************
  $Header$

  HTML modification test program
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 2000,01 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/

#include <stdio.h>

#include "wwwoffle.h"
#include "config.h"
#include "document.h"
#include "misc.h"
#include "errors.h"


/*+ The file descriptor of the spool directory. +*/
int fSpoolDir=-1;

int main(int argc,char **argv)
{
 URL *Url;

 if(argc==1)
   {fprintf(stderr,"usage: test-modify URL < contents-of-url\n");return(1);}

 InitErrorHandler("test-modify",0,1);

 InitConfigurationFile("./wwwoffle.conf");

 init_buffer(2);
 if(ReadConfigurationFile(2))
    PrintMessage(Fatal,"Error in configuration file 'wwwoffle.conf'.");

 Url=SplitURL(argv[1]);

 init_buffer(0);

 OutputHTMLWithModifications(1,0,Url);

 FreeURL(Url);

 return(0);
}
