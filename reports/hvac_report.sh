#!/bin/bash

MAILTO=$HOME/.mailto
if test ! -f $MAILTO; then
	echo "error: $MAILTO does not exist.  Create that file and put your email address into it"
	exit 1
fi
TO=$(cat $MAILTO)

REPORT=$HOME/src/sd/reports/hvac_report.py

day=$(date -d "yesterday" "+%m/%d/%Y")
tmp=/tmp/hvac_report.tmp

# Run the report
python3 $REPORT > $tmp 2>&1

# Build email with HTML to preserve monospace formatting
{
    echo "From: HVAC System <hvac@home.net>"
    echo "Subject: HVAC Report for $day"
    echo "Content-Type: text/html; charset=utf-8"
    echo "MIME-Version: 1.0"
    echo ""
    echo "<html><body><pre><font face=\"Courier New, Courier, mono\">"
    cat $tmp
    echo "</font></pre></body></html>"
} | /usr/sbin/sendmail $TO

rm -f $tmp
