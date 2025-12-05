#!/bin/sh
S=/shared 
L=$S/.lock 
I=$(hostname) 
C=0
mkdir -p $S; touch $L

exec 300>"$L"

while true; do
	current_file=""
	flock -x 300
	i=1
	while [ $i -le 999 ]; do
		f=$(printf %03d $i) 
		p=$S/$f
		if [ ! -e "$p" ]; then
			C=$((C+1))
			echo "$I $C" > "$p"
			current_file="$f"
			break
		fi
		i=$((i+1))
	done
	
	flock -u 300
	
	if [ -n "$current_file" ]; then
		sleep 1
		rm -f "$S/$current_file"
		sleep 1
	else
		sleep 2
	fi
done
