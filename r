packs() {
	killall jk
	killall jbd
	sleep 1
	for i in $(seq 1 14)
	do
		n=$(printf "%02d" $i)
		agent=$(awk -F'[][]' '/^\[(JBD|JK)\]/ {print tolower($2); exit}' /opt/sd/etc/pack_${n}.conf 2> /dev/null)
		test -z "$agent" && continue
		/opt/sd/bin/${agent} -b -r -c /opt/sd/etc/pack_${n}.conf -l /opt/sd/log/pack_${n}.log -a
	done
}
#packs
#exit 0
systemctl daemon-reload
#systemctl restart solard
systemctl restart sb1
systemctl restart sb2
systemctl restart sb3
systemctl restart sb4
systemctl restart pvc
systemctl restart btc
systemctl restart pa
systemctl restart rheem
systemctl restart rdevserver
systemctl restart watchsi
exit 0
systemctl restart si

#sb1 sb2 sb3 sb4 pvc btc pa
