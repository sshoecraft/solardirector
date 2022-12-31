#!/bin/bash

# Test sub?
now=`/usr/bin/date`
/usr/bin/mosquitto_sub -h localhost -t '#' -C 1 >/dev/null 2>&1
if test $? -ne 0; then
	echo "[${now}] mosquitto_sub not working correctly"
	exit  1
fi

while true
do
	data=$(/usr/bin/mosquitto_sub -h localhost -t 'SolarD/Agents/si/Data' -C 1 -W 22)
	if test -z "$data"; then
		echo "restarting SI"
		echo "[${now}] restarting SI" >> /var/log/watchsi.log
		p=$(pidof si)
		test -n "$p" && kill $p
		systemctl restart si
		sleep 5
	fi
done

exit 1
