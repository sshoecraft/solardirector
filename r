killall jk
killall jbd
sleep 1
for i in $(seq 1 14)
do
	n=$(printf "%02d" $i)
#	if test $i -eq 2; then
#		/opt/sd/bin/jk -b -c /opt/sd/etc/pack_${n}.conf -l /opt/sd/log/pack_${n}.log -a
#	else
		/opt/sd/bin/jbd -b -c /opt/sd/etc/pack_${n}.conf -l /opt/sd/log/pack_${n}.log -a
#	fi
done
#exit 0
systemctl daemon-reload
#systemctl restart solard
systemctl restart rdevserver
systemctl restart watchsi
systemctl restart sb1
systemctl restart sb2
systemctl restart sb3
systemctl restart pvc
systemctl restart btc
systemctl restart rheem
exit 0
systemctl restart si
