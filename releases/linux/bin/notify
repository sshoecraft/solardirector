#!/bin/bash

if test -f /usr/local/etc/notify.conf; then
	addr=$(cat /usr/local/etc/notify.conf)
else
	echo "/usr/local/etc/notify.conf missing"
	exit 1
fi

tmp=/tmp/msg.$$
trap '{ rm -f $tmp; }' EXIT
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
