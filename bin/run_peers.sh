#!/bin/bash

# run make_script.sh on peer tokengen machines
# arg1 : file containing list of peer ips

if [ "$#" -ne 1 ]
then
        echo "usage: $0 <file containing peer ips>"
        exit 0
fi

ips=`cat $1`
for ip in $ips
do
	echo -e "\n---------- $ip ----------"
	ssh paresh@$ip 'cd ~/WebQ/TokenGen/src; ./make_script.sh; exit'
done
