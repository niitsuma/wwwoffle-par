#!/usr/bin/perl
#-*-perl-*-

#
# Copyright Andrew M. Bishop 1996.97,98,2001.
#
# $Header: /home/amb/wwwoffle/doc/scripts/RCS/README.CONF-html.pl 1.2 2001/09/04 19:09:59 amb Exp $
#
# Usage: README.CONF-html.pl < README.CONF > README.CONF.html
#

$_=<STDIN>;
s/^ *//;
s/ *\n//;
$title=$_;

print "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2//EN\">\n";
print "<HTML>\n";
print "\n";
print "<HEAD>\n";
print "<TITLE>$title</TITLE>\n";
print "</HEAD>\n";
print "\n";
print "<BODY>\n";
print "\n";
print "<h1>$title</h1>\n";
print "\n";

$hr=1;
$blank=0;
$intro=-1;
$appendix=0;
$first=1;
$dl=0;

while(<STDIN>)
  {
   chop;

   s/&/&amp;/g;
   s/</&lt;/g;
   s/>/&gt;/g;

# Separator

   if ($_ eq "--------------------------------------------------------------------------------")
       {
        $hr=1;
        $intro=0;
       }

# Underlines

   elsif(m/^ *[-=]+ *$/)
       {
        next;
       }

# Section heading

   elsif ($hr==1 && m/^([-A-Za-z0-9]+)$/)
       {
        $section = $1;

        if($section eq "WILDCARD")
            {
             $appendix=1;
            }

        $intro=1 if($intro==-1);

        print "<h2><a name=\"$section\"></a>$_</h2>\n";

        $hr=0;
        $blank=0;
        $first=1;
       }

# Item

   elsif (!$intro && !$appendix && m/^(\[?&lt;URL-SPEC&gt;\]? *)?(.?[-()a-z0-9]+)( *= *.+)?$/)
       {
        $item=$section."_".$2;

        s%(URL-SPECIFICATION|URL-SPEC)%<a href="#URL-SPECIFICATION">$1</a>%g;
        s%(WILDCARD)%<a href="#WILDCARD">$1</a>%g;

        print "<h3><a name=\"$item\"></a>$_\n</h3>\n";

        $blank=0;
        $first=1;
       }

# Item

   elsif(!$intro && !$appendix && (m/^(\[!\])?URL-SPECIFICATION/ || m/^\(/))
       {
        $item = "";

        s%(URL-SPECIFICATION|URL-SPEC)%<a href="#URL-SPECIFICATION">$1</a>%g;
        s%(WILDCARD)%<a href="#WILDCARD">$1</a>%g;

        print "<h3>$_</h3>\n";

        $blank=0;
        $first=1;
       }

# Blank

   elsif (m/^$/)
       {
        print "</dl>\n" if($dl);

        $blank=1 if(!$first);
        $dl=0;
       }

# Text list

   elsif($appendix && m%^([-a-zA-Z0-9():?*/.]+)   +(.+)%)
       {
        $thing=$1;
        $descrip=$2;

        print "\n<dl>\n<dt>$thing\n<dd>$descrip\n";

        $blank=0;
        $first=0;
        $dl=1;
       }

# Text

   else
       {
        s/^ *//;

        s%(URL-SPECIFICATION|URL-SPEC)%<a href="#URL-SPECIFICATION">$1</a>%g;
        s%(WILDCARD)%<a href="#WILDCARD">$1</a>%g;

        print "<p>\n" if($blank);
        print "$_\n";

        $blank=0;
        $first=0;
       }
  }

print "\n";
print "</BODY>\n";
print "\n";
print "</HTML>\n";
