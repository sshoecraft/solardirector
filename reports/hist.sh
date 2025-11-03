#!/bin/bash

influx_host=http://localhost
influx_db=power
tz="America/Chicago"

repdata=/tmp/power.dat
#trap '{ rm -f $repdata; }' EXIT

influx_query() {
	q="$(echo $3 | sed 's: :+:g' | sed 's:;$::')"
	curl -s "${1}:8086/query?db=${2}&q=${q}+tz('${tz}');${4}"
}

influx_singleval() {
	data=$(influx_query $influx_host $influx_db "${1}")
	if test -n "$data"; then
		val=$(echo "$data" | jq .results[0].series[0].values[0][1])
	else
		val=0.0
	fi
	test "$val" = "null" && val="NULL"
	echo $val
}

get_first() {
	first_year=""
	first_year=$(cat $repdata 2>/dev/null | head -1 | awk -F, '{ print $1 }' | awk -F- '{ print $1 }')
	if test -z "$first_year"; then
		data=$(influx_query $influx_host $influx_db "select first(input_power) from inverter where input_power < 0")
		test -n "$data" && first_year=$(echo "$data" | jq .results[0].series[0].values[0][0] | xargs | awk '{ print substr($0,1,4) }')
	fi
	test -z "$first_year" && first_year=$(date "+%Y")
}

get_usage() {
	if test -n "${2}"; then
		local total=0
		local count=0
		dt=${1}
		ed=${2}
		test -z "${ed}" && ed=$(date -d "$dt +1 day" "+%Y-%m-%d")
		while test "${dt}" != "${ed}"
		do
			usage=$(cat $repdata | grep "^${dt}" | awk -F, '{ print $2 }')
			if test -n "$usage" -a "$usage" != "NULL"; then
				total=$(echo "3k $usage $total + p" | dc 2>&1)
				test $(echo "$total" | grep -c "stack empty") -gt 0 && exit 1
				((count++))
			fi
			dt=$(date -d "$dt +1 day" "+%Y-%m-%d")
		done
		printf "%f,%d" "$total" "$count"
	else
		usage=$(cat $repdata | grep "^${1}" | awk -F, '{ print $2 }')
		test -z "$usage" && usage="NULL"
		echo $usage
	fi
}

old_collect() {
	rm -f $repdata; touch $repdata
	year=$first_year
	while test $year -le $end_year
	do
		total=0
		dt="${year}-${start_month}-${start_day}"
		if test $end_year -gt $start_year; then
			ey=`expr $year + 1`
		else
			ey=$year
		fi
		ed="${ey}-${end_month}-${end_day}"
		echo "dt: $dt, ed: $ed"
		data=$(influx_query $influx_host $influx_db "select sum(input_power)*-0.0000027778 from inverter where input_power < 0 and time >= '${dt} 00:00:00' and time < '${ed} 00:00:00' group by time(1d) fill(null)")
		if test $(echo "$data" | grep -c series) -gt 0; then
			echo "$data" | jq .results[0].series[].values[][] | xargs printf "%s,%s\n" | sed 's:null:NULL:' >> $repdata
			cycle_days=$(cat $repdata | grep "^${year}" | wc -l)
		fi
		((year++))
	done
}

new_collect() {
	rm -f $repdata; touch $repdata
	data=$(influx_query $influx_host $influx_db "select sum(input_power)*-0.0000027778 from inverter where input_power < 0 and time >= '${start} 00:00:00' and time < '${end} 00:00:00' group by time(1d) fill(null)")
	if test $(echo "$data" | grep -c series) -gt 0; then
		echo "$data" | jq .results[0].series[].values[][] | xargs printf "%s,%s\n" | sed 's:null:NULL:' >> $repdata
		cycle_days=$(cat $repdata | grep "^${year}" | wc -l)
	fi
}

collect() {
	rm -f $repdata; touch $repdata
	data=$(influx_query $influx_host $influx_db "select sum(input_power)*-0.0000027778 from inverter where input_power < 0 and time >= '${1} 00:00:00' and time < '${2} 00:00:00' group by time(1d) fill(null)")
	if test $(echo "$data" | grep -c series) -gt 0; then
		echo "$data" | jq .results[0].series[].values[][] | xargs printf "%s,%s\n" | sed 's:null:NULL:' >> $repdata
	fi
}

disp_hist() {
	# grid layout usage report by year
	# format:
	# date 	year	year	year
	# 01-01	xxx	yyy	zzz
	# 01-02	xxx	yyy	zzz
	# ...

	# use a leap year for report day iteration
	dt_start=2020-${start_month}-${start_day}
	if test $end_year -ne $start_year; then
		ey=2021
	else
		ey=2020
	fi
	dt_end=${ey}-${end_month}-${end_day}

	# Create formats
	hformat="%-7s "
	format="%-7s "

	md=$(echo $end | awk -F- '{ print $2"-"$3 }')
	if test "$md" = "01-01"; then
		hist_end_year=$(expr $end_year - 1)
	else
		hist_end_year=$end_year
	fi
	year=$start_year
	while test $year -le $hist_end_year
	do
		hformat="$hformat  %8.8s"
		format="$format  %8.8s"
		((year++))
	done
	hformat="${hformat}\n"
#	echo "hformat: $hformat"
	format="${format}\n"
#	echo "format: $format"

	echo ""
	echo "Historical Usage:"

	# Display header
	cmd="printf \"\${hformat}\" \"date\""
	year=$start_year
	while test $year -le $hist_end_year
	do
		cmd="$cmd \"${year}\""
		((year++))
	done
	#echo "cmd: $cmd"
	eval $cmd
	cmd="printf \"\${hformat}\" \"=====\""
	year=$start_year
	while test $year -le $hist_end_year
	do
		cmd="$cmd \"=======\""
		((year++))
	done
	#echo "cmd: $cmd"
	eval $cmd

	# Init arrays
	declare -A totals
	declare -A mins
	declare -A maxs
	declare -A counts
	declare -A days
	year=$start_year
	while test $year -le $hist_end_year
	do
		eval totals[${year}]=0.0
		eval mins[${year}]=99999999.99
		eval maxs[${year}]=0.0
		eval counts[${year}]=0
		eval days[${year}]=0
		((year++))
	done

	# collect the data into a "daily" array
	dt=$dt_start
	while test $dt != $dt_end
	do
#		echo "dt: $dt, ed: $dt_end"
		month=$(echo "$dt" | awk -F- '{ print $2 }')
		day=$(echo "$dt" | awk -F- '{ print $3 }')
		arr="a${month}${day}"
		year=$start_year
		while test $year -le $hist_end_year
		do
			uday=${year}-${month}-${day}
#			eval $arr[$year]=$(get_usage "$uday")
			val=$(get_usage "$uday")
			eval $arr[$year]=$val
			if test $(echo "$val" | grep -ci null) -eq 0; then
				counts[$year]=`expr ${counts[${year}]} + 1`
			fi
			days[$year]=`expr ${days[${year}]} + 1`
			((year++))
		done
		# XXX forever leap year
		dy=$(echo "$dt" | awk -F- '{ print $1 }')
		if test "$arr" = "a0228"; then
			dt="${dy}-02-29"
		elif test "$arr" = "a0229"; then
			dt="${dy}-03-01"
		else
			dt=$(date -d "$dt +1 day" "+%Y-%m-%d")
		fi
	done

	dt=$dt_start
	while test $dt != $dt_end
	do
#		echo "dt: $dt, ed: $dt_end"
		month=$(echo "$dt" | awk -F- '{ print $2 }')
		day=$(echo "$dt" | awk -F- '{ print $3 }')
		cmd="printf \"\${format}\" \"${month}-${day}\""
		arr="a${month}${day}"
		year=$start_year
		while test $year -le $hist_end_year
		do
			eval usage="\${$arr[$year]}"
			test -z "$usage" && usage="NULL"
			if test "$usage" != "NULL"; then
				eval totals[${year}]=$(echo "${totals[${year}]} $usage + p" | dc)
				test $(echo "$usage < ${mins[${year}]}" | bc) -eq 1 && eval mins[${year}]=$usage
				test $(echo "$usage > ${maxs[${year}]}" | bc) -eq 1 && eval maxs[${year}]=$usage
				usage=$(printf "%.2f" "$usage")
			fi
			cmd="$cmd \"${usage}\""
			((year++))
		done
		eval $cmd
		# XXX forever leap year
		dy=$(echo "$dt" | awk -F- '{ print $1 }')
		if test "$arr" = "a0228"; then
			dt="${dy}-02-29"
		elif test "$arr" = "a0229"; then
			dt="${dy}-03-01"
		else
			dt=$(date -d "$dt +1 day" "+%Y-%m-%d")
		fi
	done

	cmd="printf \"\${hformat}\" \"=====\""
	year=$start_year
	while test $year -le $hist_end_year
	do
		cmd="$cmd \"=======\""
		((year++))
	done
	#echo "cmd: $cmd"
	eval $cmd

	cmd="printf \"\${hformat}\" \"Total\""
	year=$start_year
	while test $year -le $hist_end_year
	do
		eval val=\${totals[${year}]}
		total=$(printf "%6.2f" "$val")
		cmd="$cmd \"${total}\""
		((year++))
	done
	#echo "cmd: $cmd"
	eval $cmd

	cmd="printf \"\${hformat}\" \"Min\""
	year=$start_year
	while test $year -le $hist_end_year
	do
		eval val=\${mins[${year}]}
		str=$(printf "%6.2f" "$val")
		cmd="$cmd \"${str}\""
		((year++))
	done
	#echo "cmd: $cmd"
	eval $cmd

	cmd="printf \"\${hformat}\" \"Max\""
	year=$start_year
	while test $year -le $hist_end_year
	do
		eval val=\${maxs[${year}]}
		str=$(printf "%6.2f" "$val")
		cmd="$cmd \"${str}\""
		((year++))
	done
	#echo "cmd: $cmd"
	eval $cmd

	cmd="printf \"\${hformat}\" \"Average\""
	year=$start_year
	while test $year -le $hist_end_year
	do
		eval val=\${totals[${year}]}
		val=$(echo "6k $val ${counts[$year]} / p" | dc)
		str=$(printf "%6.2f" "$val")
		cmd="$cmd \"${str}\""
		((year++))
	done
	#echo "cmd: $cmd"
	eval $cmd

	cmd="printf \"\${hformat}\" \"YearPro\""
	year=$start_year
	while test $year -le $hist_end_year
	do
		eval val=\${totals[${year}]}
		avg=$(echo "6k $val ${counts[$year]} / p" | dc)
		val=$(echo "6k $avg 366 * p" | dc)
		str=$(printf "%6.2f" "$val")
		cmd="$cmd \"${str}\""
		((year++))
	done
	#echo "cmd: $cmd"
	eval $cmd
}

if test -z "$1"; then
	get_first
	start="${first_year}-01-01"
else
	start="${1}"
	first_year="$(echo ${1} | awk -F- '{ print $1 }')"
fi
if test -z "$2"; then
	end_year=`expr $(date "+%Y") + 1`
	end="${end_year}-01-01"
else
	end="${2}"
fi
start_year=$(echo $start | awk -F- '{ print $1 }')
start_month=$(echo $start | awk -F- '{ print $2 }')
test -z "$start_month" && start_month=01
start_day=$(echo $start | awk -F- '{ print $3 }')
test -z "$start_day" && start_day=01
start="${start_year}-${start_month}-${start_day}"

end_year=$(echo $end | awk -F- '{ print $1 }')
end_month=$(echo $end | awk -F- '{ print $2 }')
test -z "$end_month" && end_month=01
end_day=$(echo $end | awk -F- '{ print $3 }')
test -z "$end_day" && end_day=01
end="${end_year}-${end_month}-${end_day}"

#echo "start: $start, end: $end"
sds=$(date -d "$start" "+%s" 2>/dev/null)
sds=$(printf "%d" "$sds" 2>/dev/null)
eds=$(date -d "$end" "+%s" 2>/dev/null)
eds=$(printf "%d" "$eds" 2>/dev/null)
if test $eds -lt $sds; then
	echo "error: end cannot be before than start"
	exit 1
fi

#start_year=$(date -d @${sds} "+%Y")
#start_month=$(date -d @${sds} "+%m")
#start_day=$(date -d @${sds} "+%d")
#end_year=$(date -d @${eds} "+%Y")
#end_month=$(date -d @${eds} "+%m")
#end_day=$(date -d @${eds} "+%d")

collect "${start_year}-${start_month}-${start_day}" "${end_year}-${end_month}-${end_day}"
disp_hist
exit 0

exit 0
