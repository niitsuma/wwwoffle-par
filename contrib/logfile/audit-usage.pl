#!/bin/sh
#
# WWWOFFLE - World Wide Web Offline Explorer - Version 2.7b.
#
# A Perl script to get audit information from the standard error output.
#
# Written by Andrew M. Bishop
#
# This file Copyright 1998,99,2000,01,02 Andrew M. Bishop
# It may be distributed under the GNU Public License, version 2, or
# any higher version.  See section COPYING of the GNU Public license
# for conditions under which this file may be redistributed.
#

exec perl -x $0

exit 1

#!perl

%commands=(
           "Online","-online",
           "Fetch","-fetch",
           "Offline","-offline",
           "In Autodial Mode","-autodial",
           "Re-Reading Configuration File","-config",
           "Purge","-purge",
           "Status","-status",
           "Kill","-kill"
           );

%modes=(
        "-spool","S",
        "-fetch","F",
        "-real","R",
        "-autodial","A"
        );

%statuses=(
           "Internal Page","I",
           "Cached Page Used","C",
           "New Page","N",
           "Forced Reload","F",
           "Modified on Server","M",
           "Unmodified on Server","U",
           "Not Cached","X"
           );

$time='';
%host=();
%user=();
%ip=();
%mode=();
$url='';

print "# Mode  : F=Fetch, R=Online (Real), S=Offline (Spool), A=Autodial,\n";
print "#         W=WWWOFFLE Command, T=Timestamp, X=Error Condition\n";
print "#\n";
print "# Status: C=Cached version used, N=New page, F=Forced refresh, I=Internal,\n";
print "#         M=Modified on server, U=Unmodified on server, X=Not cached\n";
print "#         z=Compressed transfer.\n";
print "#\n";
print "# Mode Status\n";
print "# ---- ------\n";
print "# / ,---'\n";
print "#/ /         Hostname              IP Username Details\n";
print "# ## ---------------- --------------- -------- -------\n";

while(<STDIN>)
  {
   if(/^wwwoffled.([0-9]+)/)
       {
        if(/Timestamp: ([a-zA-Z0-9 :]+)/)
            {
             $time=$1;
             printf("T %1s%1s %16s %15s %8s %s\n","-"," ","-","-","-",$time);
            }
        elsif(/HTTP Proxy connection from host ([^ ]+) .([0-9a-f.:]+)/)
            {
             $host=$1;
             $ip=$2;
             $ip=~s/^::ffff:([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)/\1/;
            }
        elsif(/HTTP Proxy connection rejected from host ([^ ]+) .([0-9a-f.:]+)/)
            {
             $host=$1;
             $ip=$2;
             $ip=~s/^::ffff:([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)/\1/;
             printf("X %1s%1s %16s %15s %8s HTTP Proxy Host Connection Rejected\n","-"," ",$host,$ip,"-");
            }
        elsif(/WWWOFFLE Connection from host ([^ ]+) .([0-9a-f.:]+)/)
            {
             $host=$1;
             $ip=$2;
             $ip=~s/^::ffff:([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)/\1/;
            }
        elsif(/WWWOFFLE Connection rejected from host ([^ ]+) .([0-9a-f.:]+)/)
            {
             $host=$1;
             $ip=$2;
             $ip=~s/^::ffff:([0-9]+\.[0-9]+\.[0-9]+\.[0-9]+)/\1/;
             printf("X %1s%1s %16s %15s %8s WWWOFFLE Connection Rejected\n","-"," ",$host,$ip,"-");
            }
        elsif(/Forked wwwoffles (-[a-z]+) .pid=([0-9]+)/)
            {
             $pid=$2;
             $mode=$modes{$1};
             if($mode eq "F")
                 {
                  $host='-';
                  $ip='-';
                 }

             $mode{$pid}=$mode;
             $host{$pid}=$host;
             $ip{$pid}=$ip;
             $user{$pid}='-';
            }
        elsif(/Child wwwoffles (exited|terminated) with status ([0-9]+) .pid=([0-9]+)/)
            {
             $pid=$3;
             $mode    =delete $mode{$pid};
             $status  =delete $status{$pid};
             $compress=delete $compress{$pid};
             $host    =delete $host{$pid};
             $ip      =delete $ip{$pid};
             $user    =delete $user{$pid};
             $url     =delete $url{$pid};
             printf("%1s %1s%1s %16s %15s %8s %s\n",$mode,$status,$compress,$host,$ip,$user,$url) if($2 ne "3");
            }
        elsif(/WWWOFFLE (Online|Fetch|Offline|In Autodial Mode|Re-Reading Configuration File|Purge|Kill)\./)
            {
             $command=$commands{$1};
             printf("W %1s%1s %16s %15s %8s WWWOFFLE %s\n","-"," ",$host,$ip,"-",$command);
            }
        elsif(/WWWOFFLE (Incorrect Password|Not a command|Unknown control command)/)
            {
             printf("X %1s%1s %16s %15s %8s WWWOFFLE Bad Connection (%s)\n","-"," ",$host,$ip,"-",$1);
            }
       }
   elsif(/^wwwoffles.([0-9]+)/)
       {
        $pid=$1;

        if(/: (URL|SSL)=\'([^\']+)/)
            {
             if($1 eq "SSL") {$url="$2 (SSL)";}
             if($1 eq "URL") {$url=$2;}
             $url{$pid}=$url;
             $status{$pid}="I";
            }
        elsif(/: Cache Access Status=\'([^\']+)/)
            {
             $status=$statuses{$1};
             $status{$pid}=$status;
            }
        elsif(/: Server has used .Content-Encoding:/)
            {
             $compress{$pid}="z";
            }
        elsif(/HTTP Proxy connection from user '([^ ]+)'/)
            {
             $user{$pid}=$1;
            }
        elsif(/HTTP Proxy connection rejected from unauthenticated user/)
            {
             $mode=delete $mode{$pid};
             $host=delete $host{$pid};
             $ip  =delete $ip{$pid};
             printf("X %1s%1s %16s %15s %8s HTTP Proxy User Connection Rejected\n","-"," ",$host,$ip,"-");
            }
        elsif(/(Cannot open temporary spool file|Could not parse HTTP request)/)
            {
             $mode=delete $mode{$pid};
             $host=delete $host{$pid};
             $ip  =delete $ip{$pid};
             printf("X %1s%1s %16s %15s %8s Internal Error '$1'\n","-"," ",$host,$ip,"-");
            }
       }
  }
