#!/bin/bash

inc=$1
test -z "$1" && exit 0
if test ! -f ${inc}; then
	echo "error: ${inc} not found"
	exit 1
fi
name=$2
PREFIX=$(echo $name | tr '[:lower:]' '[:upper:]')
prefix=$3
test -z "$prefix" && prefix="s->"
start=$4
test -z "$start" && start=end
if test "$5" = "ro"; then
	ro=1
	ROSPEC="| JSPROP_READONLY"
else
	ro=0
fi
out=${name}_defs.h

if test "$start" = "end"; then
	start=127
	dir=1
else
	dir=0
fi

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

do_enums() {
#out=${name}_propid.h
echo "#define ${PREFIX}_PROPIDS \\" > $out
count=0
while read n
do
#	echo $n
	c=$(echo "$n" | awk '{ print substr($0,1,1); }')
#	echo "c: $c"
	test "$c" = "#" && continue
	t=$(echo $n | awk '{ print $1 }')
	v=$(echo $n | awk '{ print $2 }' | sed -e 's:\.:_:g' -e 's/[:;]//g' | awk -F'[' '{ print $1 }')
#	echo $t $v $p
	if test $(echo "$v" | grep -c -e '(' -e '\[') -gt 0; then
		continue
	elif test "$t" = "bool"; then
		:
	elif test "$t" = "char" -o "$t" = "uint8_t"; then
		:
	elif test "$t" = "unsigned" -o "$t" = "uint16_t" -o "$t" = "uint32_t"; then
		:
	elif test "$t" = "int" -o "$t" = "long" -o "$t" = "time_t"; then
		:
	elif test "$t" = "float"; then
		:
	elif test "$t" = "void"; then
		:
	elif test "$t" = "/"; then
		:
	else
		echo "unknown type: $t"
		continue
	fi
#	if test $count -gt 0; then
#		p="${PREFIX}_PROPERTY_ID_$(echo $v | tr '[:lower:]' '[:upper:]')"
#	else
		p="${PREFIX}_PROPERTY_ID_$(echo $v | tr '[:lower:]' '[:upper:]')=$start"
#	fi
	test $count -gt 0 && printf ",\\\\\n" >> $out
	printf "\t%s" "$p" >> $out
	if test $dir -eq 0; then
		((start++))
	else
		((start--))
	fi
	((count++))
done < $tmp
test $count -gt 0 && printf "\n\n" >> $out
}
do_gsw() {
#out=${name}_getprop.h
echo "#define ${PREFIX}_GETPROP \\" >> $out
count=0
while read n
do
#	echo $n
	t=$(echo $n | awk '{ print $1 }')
	v=$(echo $n | awk '{ print $2 }' | sed -e 's:\.:_:g' -e "s/[:;]//g")
	p="${PREFIX}_PROPERTY_ID_$(echo $v | awk -F'[' '{ print $1 }' | tr '[:lower:]' '[:upper:]'):"
#	echo $t $v $p
	if test $(echo "$v" | grep -c -e '(') -gt 0; then
		continue
	elif test $(echo "$v" | grep -c -e '\[') -gt 0; then
		if test "$t" = "char"; then
			v=$(echo $v | awk -F'[' '{ print $1 }')
			c="*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,${prefix}${v}));"
		else
			continue
		fi
#	elif test "$t" = "void"; then
#		c="*rval = JSVAL_VOID;"
	elif test "$t" = "bool"; then
		c="*rval = BOOLEAN_TO_JSVAL(${prefix}${v});"
	elif test "$t" = "char"; then
		c="*rval = STRING_TO_JSVAL(JS_NewStringCopyZ(cx,${prefix}${v}));"
	elif test "$t" = "unsigned"; then
		c=""
		if test $(echo $n | awk '{ print $2 }' | grep -c ':$') -gt 0; then
			t=$(echo $n | awk '{ print $3 }' | sed 's:;$::')
			if test "$t" = "1"; then
				c="*rval = BOOLEAN_TO_JSVAL(${prefix}${v});"
			fi
		fi
		test -z "$c" && c="*rval = INT_TO_JSVAL(${prefix}${v});"
	elif test "$t" = "uint8_t" -o "$t" = "uint16_t" -o "$t" = "uint32_t"; then
		c="*rval = INT_TO_JSVAL(${prefix}${v});"
	elif test "$t" = "int" -o "$t" = "long" -o "$t" = "time_t"; then
		c="*rval = INT_TO_JSVAL(${prefix}${v});"
	elif test "$t" = "float"; then
		c="JS_NewDoubleValue(cx, ${prefix}${v}, rval);"
	else
		continue
	fi
	test $count -gt 0 && printf "\\\\\n" >> $out
	printf "\t\tcase %s\n" "${p}\\" >> $out
#	printf "\t\t\t%s\n" "dprintf(0,\"${v}\n\");\\" >> $out
	printf "\t\t\t%s\n" "${c}\\" >> $out
	printf "\t\t\t%s" "break;" >> $out
	((count++))
done < $tmp
test $count -gt 0 && printf "\n\n" >> $out
}
do_psw() {
#out=${name}_setprop.h
echo "#define ${PREFIX}_SETPROP \\" >> $out
count=0
while read n
do
#	echo $n
	t=$(echo $n | awk '{ print $1 }')
#	v=$(echo $n | awk '{ print $2 }' | sed -e 's:\.:_:g' -e "s/[:;]//g" | awk -F'[' '{ print $1 }')
	v=$(echo $n | awk '{ print $2 }' | sed -e 's:\.:_:g' -e "s/[:;]//g")
	p="${PREFIX}_PROPERTY_ID_$(echo $v | awk -F'[' '{ print $1 }' | tr '[:lower:]' '[:upper:]'):"
#	echo $t $v $p
	if test $(echo "$v" | grep -c -e '(') -gt 0; then
		continue
	elif test $(echo "$v" | grep -c -e '\[') -gt 0; then
		if test "$t" = "char"; then
			v=$(echo $v | awk -F'[' '{ print $1 }')
#			c="strcpy(${prefix}${v},JS_GetStringBytes(JSVAL_TO_STRING(*vp)));"
			c="jsvaltotype(DATA_TYPE_STRING,&${prefix}${v},sizeof(${prefix}${v})-1,cx,*vp);"
		else
			continue
		fi
	elif test "$t" = "bool"; then
#		c="${prefix}${v} = JSVAL_TO_BOOLEAN(*vp);"
		c="jsvaltotype(DATA_TYPE_BOOLEAN,&${prefix}${v},0,cx,*vp);"
	elif test "$t" = "char"; then
#		c="strcpy(${prefix}${v},JS_GetStringBytes(JSVAL_TO_STRING(*vp)));"
		c="jsvaltotype(DATA_TYPE_STRING,&${prefix}${v},sizeof(${prefix}${v})-1,cx,*vp);"
	elif test "$t" = "unsigned"; then
		c=""
		if test $(echo $n | awk '{ print $2 }' | grep -c ':$') -gt 0; then
			t=$(echo $n | awk '{ print $3 }' | sed 's:;$::')
			if test "$t" = "1"; then
#				c="${prefix}${v} = JSVAL_TO_BOOLEAN(*vp);"
				c="jsvaltotype(DATA_TYPE_BOOLEAN,&${prefix}${v},0,cx,*vp);"
			fi
		fi
#		test -z "$c" && c="${prefix}${v} = JSVAL_TO_INT(*vp);"
		test -z "$c" && c="jsvaltotype(DATA_TYPE_INT,&${prefix}${v},0,cx,*vp);"
	elif test "$t" = "uint8_t"; then
		c="jsvaltotype(DATA_TYPE_U8,&${prefix}${v},0,cx,*vp);"
	elif test "$t" = "uint16_t"; then
		c="jsvaltotype(DATA_TYPE_U16,&${prefix}${v},0,cx,*vp);"
	elif test "$t" = "uint32_t" -o "$t" = "time_t"; then
#		c="${prefix}${v} = JSVAL_TO_INT(*vp);"
		c="jsvaltotype(DATA_TYPE_U32,&${prefix}${v},0,cx,*vp);"
	elif test "$t" = "int" -o "$t" = "long"; then
#		c="${prefix}${v} = JSVAL_TO_INT(*vp);"
		c="jsvaltotype(DATA_TYPE_S32,&${prefix}${v},0,cx,*vp);"
	elif test "$t" = "float"; then
#		c="${prefix}${v} = *JSVAL_TO_DOUBLE(*vp);"
		c="jsvaltotype(DATA_TYPE_FLOAT,&${prefix}${v},0,cx,*vp);"
	else
		continue
	fi
	test $count -gt 0 && printf "\\\\\n" >> $out
	printf "\t\tcase %s\n" "${p}\\" >> $out
	printf "\t\t\t%s\n" "${c}\\" >> $out
#	printf "\t\t\t%s\n" "name = \"${v}\";\\" >> $out
	printf "\t\t\t%s" "break;" >> $out
	((count++))
done < $tmp
test $count -gt 0 && printf "\n\n" >> $out
}
do_prop() {
#out=${name}_propspec.h
echo "#define ${PREFIX}_PROPSPEC \\" >> $out
count=0
while read n
do
#	echo $n
	t=$(echo $n | awk '{ print $1 }')
#	v=$(echo $n | awk '{ print $2 }' | sed -e 's:\.:_:g' -e "s/[:;]//g" | awk -F'[' '{ print $1 }')
	v=$(echo $n | awk '{ print $2 }' | sed -e 's:\.:_:g' -e 's/[:;]//g')
	if test $(echo "$v" | grep -c -e '(') -gt 0; then
		continue
	elif test $(echo "$v" | grep -c -e '\[') -gt 0; then
		if test "$t" = "char"; then
			v=$(echo $v | awk -F'[' '{ print $1 }')
			:
		else
			continue
		fi
	elif test "$t" = "bool"; then
		:
	elif test "$t" = "unsigned" -o "$t" = "uint16_t"; then
		:
	elif test "$t" = "char" -o "$t" = "uint8_t"; then
		:
	elif test "$t" = "int" -o "$t" = "time_t" -o "$t" = "uint32_t" -o "$t" = "long"; then
		:
	elif test "$t" = "float"; then
		:
#	elif test "$t" = "void"; then
#		:
	else
		continue
	fi
	p="${PREFIX}_PROPERTY_ID_$(echo $v | awk -F'[' '{ print $1 }' | tr '[:lower:]' '[:upper:]')"
	test $count -gt 0 && printf ",\\\\\n" >> $out
#	printf "\t\t{ \"${v}\",${p},JSPROP_ENUMERATE${ROSPEC} },\\\\\n" >> $out
	printf "\t\t{ \"${v}\",${p},JSPROP_ENUMERATE${ROSPEC} }" >> $out
	((count++))
done < $tmp
test $count -gt 0 && printf "\n\n" >> $out
}
do_enums
do_gsw
if test "$ro" -eq 0; then do_psw
else echo "#define ${PREFIX}_SETPROP /* noop */" >> $out; echo "" >> $out
fi
do_prop
rm -f $tmp
