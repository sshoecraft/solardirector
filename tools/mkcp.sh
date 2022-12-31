#!/bin/bash

inc=$1
test -z "$1" && exit 0
if test ! -f ${inc}; then
	echo "error: ${inc} not found"
	exit 1
fi
name=$2
if test -z "$name"; then
	name=$(echo $inc | sed "s:\.h::")
else
	prefix=$3
	flags=$4
fi
PREFIX=$(echo $name | tr '[:lower:]' '[:upper:]')
test -z "$prefix" && prefix="s->"
test -z "$flags" && flags=0

have_start=0
open=0
tmp=/tmp/mkjs_${name}.dat
rm -f $tmp
while read line
do
	line="$(echo "$line" | sed 's:*::g')"
	test $(echo "$line" | grep -c "//") -gt 0 && continue
#	echo "have_start: $have_start, open: $open"
	if test $(echo "$line" | grep -c "{") -gt 0;  then
		((open++))
	elif test $(echo "$line" | grep -c "}") -gt 0;  then
		((open--))
		continue;
	fi
	if test $have_start -eq 1; then
		if test $open -eq 0; then
			break;
		else
			echo $line >> $tmp
		fi
	elif test $(echo "$line" | grep -c "^struct ${name} {") -gt 0; then
		have_start=1
	fi
done < ${inc}
if test $have_start -eq 0; then
	echo "error: unable to find struct"
	exit 1
fi

printf "\tconfig_property_t ${name}_props[] = {\n";
while read n
do
#	echo $n
	t=$(echo $n | awk '{ print $1 }')
	v=$(echo $n | awk '{ print $2 }' | sed -e 's:\.:_:g' -e "s/[:;]//g")
	sz=0
	if test $(echo "$v" | grep -c -e '(') -gt 0; then
		echo "//unhandled top: $t"
		continue
	elif test $(echo "$v" | grep -c -e '\[') -gt 0; then
		if test "$t" = "char"; then
			v=$(echo $v | awk -F'[' '{ print $1 }')
			dt=DATA_TYPE_STRING;
		else
			dt=DATA_TYPE_STRING;
		fi
		sz="sizeof(${v})-1"
	elif test "$t" = "bool"; then
		dt=DATA_TYPE_INT
	elif test "$t" = "uint8_t"; then
		dt=DATA_TYPE_BYTE
	elif test "$t" = "char"; then
		dt=DATA_TYPE_STRING
		sz="sizeof(${v})-1"
	elif test "$t" = "unsigned"; then
#		echo "$n $t $v"
		dt=""
		if test $(echo $n | awk '{ print $2 }' | grep -c ':$') -gt 0; then
			t=$(echo $n | awk '{ print $3 }' | sed 's:;$::')
#			echo "new t: $t"
			if test "$t" = "1"; then
				dt=DATA_TYPE_INT
			fi
		fi
		test -z "$dt" && dt=DATA_TYPE_INT
	elif test "$t" = "uint16_t" -o "$t" = "short"; then
		dt=DATA_TYPE_SHORT
	elif test "$t" = "uint32_t"; then
		dt=DATA_TYPE_INT
	elif test "$t" = "int" -o "$t" = "long" -o "$t" = "time_t"; then
		dt=DATA_TYPE_INT
	elif test "$t" = "float"; then
		dt=DATA_TYPE_FLOAT
	elif test "$t" = "double"; then
		dt=DATA_TYPE_DOUBLE
	elif test "$t" = "struct"; then
		continue
	else
		c=$(echo $t | awk '{ print substr($0,1,1) }')
		test "$c" = "" -o "$c" = "/" && continue
		echo "//unhandled: $t $v"
		continue
	fi
	printf "\t\t{ \"$v\", $dt, &${prefix}${v}, ${sz}, 0, ${flags} },\n"
done < $tmp
printf "\t\t{ 0 }\n"
printf "\t};\n"
printf "\treturn 0;\n"
printf "}\n"
