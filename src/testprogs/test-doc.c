/***************************************
  $Header$

  Document parser test program
  ******************/ /******************
  Written by Andrew M. Bishop

  This file Copyright 1998,99,2000,01 Andrew M. Bishop
  It may be distributed under the GNU Public License, version 2, or
  any higher version.  See section COPYING of the GNU Public license
  for conditions under which this file may be redistributed.
  ***************************************/

#include <stdio.h>

#include "wwwoffle.h"
#include "document.h"
#include "misc.h"
#include "config.h"
#include "errors.h"


/*+ The file descriptor of the spool directory. +*/
int fSpoolDir=-1;

int main(int argc,char **argv)
{
 URL *Url;
 char **list,*refresh;
 int j;

 if(argc==1)
   {fprintf(stderr,"usage: test-doc URL < contents-of-url\n");return(1);}

 InitErrorHandler("test-doc",0,1);

 InitConfigurationFile("./wwwoffle.conf");

 init_buffer(2);
 if(ReadConfigurationFile(2))
    PrintMessage(Fatal,"Error in configuration file 'wwwoffle.conf'.");

 Url=SplitURL(argv[1]);

 init_buffer(0);

 ParseDocument(0,Url);

 if((refresh=MetaRefresh()))
    printf("Refresh = %s\n",refresh);

 if((list=GetReferences(RefStyleSheet)))
    for(j=0;list[j];j++)
       printf("StyleSheet = %s\n",list[j]);

 if((list=GetReferences(RefImage)))
    for(j=0;list[j];j++)
       printf("Image = %s\n",list[j]);

 if((list=GetReferences(RefFrame)))
    for(j=0;list[j];j++)
       printf("Frame = %s\n",list[j]);

 if((list=GetReferences(RefScript)))
    for(j=0;list[j];j++)
       printf("Script = %s\n",list[j]);

 if((list=GetReferences(RefObject)))
    for(j=0;list[j];j++)
       printf("Object = %s\n",list[j]);

 if((list=GetReferences(RefInlineObject)))
    for(j=0;list[j];j++)
       printf("InlineObject = %s\n",list[j]);

 if((list=GetReferences(RefLink)))
    for(j=0;list[j];j++)
       printf("Link  = %s\n",list[j]);

 FreeURL(Url);

 return(0);
}
