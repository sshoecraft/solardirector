#!/bin/bash

#~/.sd.conf, /etc/sd.conf, /usr/local/etc/sd.conf
if test -f ~/.sd.conf; then
	conf=~/.sd.conf
elif test -f /etc/sd.conf; then
	conf=/etc/sd.conf
elif test -f /usr/local/etc/sd.conf; then
	conf=/usr/local/etc/sd.conf
else
	echo "notify: error: unable to find conf"
	exit 1
fi
prefix=$(cat /etc/sd.conf | grep ^prefix | awk -F= '{ print $2 }')
if test -n "$prefix"; then
	etcdir=${prefix}/etc
else
	etcdir=$(cat $conf | grep ^etcdir | awk -F= '{ print $2 }')
fi
test -z "$etcdir" && etcdir=+ETCDIR+

cf=${etcdir}/notify.conf
if test -f $cf; then
	addr=$(cat $cf)
else
	echo "notify: error: unable to find conf file"
	exit 1
fi

tmp=/tmp/msg.$$
trap '{ rm -f $tmp; }' EXIT
u=$(echo "$addr" | awk -F'@' '{ print $1 }')
if test $(echo "$u" | grep -c '^[0-9]*$') -gt 0; then
	txt=1
else
	txt=0
fi
if test $txt -eq 0; then
echo "From: solard@$(hostname) <Solar System>" > $tmp
echo "To: ${addr}" >> $tmp
fi
echo "Content-Type: text/html" >> $tmp
echo "MIME-Version: 1.0" >> $tmp
#echo "Subject: " >> $tmp
test $txt -eq 0 &&
echo "Subject: SOLAR SYSTEM NOTIFICATION" >> $tmp
echo "" >> $tmp
echo "<pre><font size=\"2\"><tt><font face=\"Courier New, Courier, mono\">" >> $tmp
for i in $(seq 1 $#)
do
        v="echo \$${i}"
        eval $v >> $tmp
done
echo "</font></tt></pre>" >> $tmp
cat $tmp
cat $tmp | /usr/sbin/sendmail $addr
exit 0

echo "From: solard@$(hostname)" > $tmp
echo "To: ${addr}" >> $tmp
echo "" >> $tmp
for i in $(seq 1 $#)
do
        v="echo \$${i}"
        eval $v >> $tmp
done
cat $tmp | /usr/sbin/sendmail -t
exit 0
