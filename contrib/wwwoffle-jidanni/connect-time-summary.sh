#                               -*- Mode: Sh -*- 
# connect-time-summary.sh -- find ppp connect time for this month so far
# Copyright       : GNU General Public License
# Author          : Dan Jacobson -- http://www.geocities.com/jidanni/
# Created On      : Feb 2001
# Last Modified By: root
# Last Modified On: Sun Oct 28 05:35:37 2001
# Update Count    : 16
# Status          : quick hack

#aside from getting a general idea of how much I've called this month,
#my ISP allows 20 hours of 'free' connect time... I'd like to know how
#close I am to that limit so I might switch to another ISP for the
#rest of the month.

#to use: must be root [to read the logfile], assumes you didn't let
#logfile grow for more than 11 months long as logfile has no year
#info...

#hmmm didn't distinguish between ISPs, ok, at least one can think of phone co. $
#set -x
export LC_TIME=en
set `date`
month=$2 date=$3
awk "
!/^$month/ {next}
\$7 ~ /^\\(ATDT412/  {phone_no=\$7} 
/^$month.*: Connect time / {print; total[phone_no] = total[phone_no]+ \$8}
END{
    limit=20 #the cheap or free hours my ISP allows me per month
    for (i in total) {print i, \"total time\", total[i]/60, \"hours\"
    date=$date
    #to do: different month lengths
    print \"Today is the \", date \"th day of the month.  Assuming 30 days to a month,\"
    printf \"we are %.1f%% thru the month, and I have used %.1f%% of \" limit \" hours.\n\",\
     date / 30*100, total[i]/60/limit*100
}}
" /var/log/ppp
