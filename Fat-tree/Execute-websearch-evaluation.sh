#!/bin/bash
randSeed=5217

allStartTime=$(date)

load=0.1
MAX_LOAD=0.9
LOAD_STEP=0.1
CON_LOAD_NUM=3
while [ $(bc <<< "$load <= $MAX_LOAD") -eq 1 ]; do
	echo "********* started current_load=$load MAX_LOAD=$MAX_LOAD ***********"
	date

	for ((  j = 1 ;  j <= $CON_LOAD_NUM;  j++  )); do
		if [[ $(bc <<< "$load + $LOAD_STEP <= $MAX_LOAD") -eq 1  && $j < $CON_LOAD_NUM ]]; then
			echo "Complicated-AlltoAll-websearch-RMT-FatTree-long-connection.pl $load $randSeed &"
			perl Complicated-AlltoAll-websearch-RMT-FatTree-long-connection.pl $load $randSeed &
			load=$(echo "$load + $LOAD_STEP" | bc)
		else
			echo "Complicated-AlltoAll-websearch-RMT-FatTree-long-connection.pl $load $randSeed"
			perl Complicated-AlltoAll-websearch-RMT-FatTree-long-connection.pl $load $randSeed
			load=$(echo "$load + $LOAD_STEP" | bc)
			break
		fi
	done

	echo "********* finished ***********"
	date
done

echo "********* All start from ***********"
echo $allStartTime
echo "********* All finished ***********"
date