#!/bin/sh

spooldir=/var/spool/wwwoffle
urlhashfile=urlhashtable

if [ $# -ge 1 ]; then
    spooldir=$1
fi

cd "$spooldir" || exit

[ -f "$urlhashfile" ] || {
    echo "Cannot find the file '$urlhashfile' in $spooldir; did you run make_urlhash_file first?"
    exit 1
}

find http https ftp finger outgoing lastout prevout* lasttime prevtime* monitor -type f -name 'U*' -print0 |
  xargs -0 -r -e rm
