#!/bin/bash

# copy file to all other machines
# arg1 : file containing list of peer ips
# arg2 : file to copy
# arg3 : copy to this ip only, 1st arg ignored

if [ "$#" -lt 2 ]
then
        echo "usage: $0 <file containing peer ips> <file to copy> <single ip, 1st arg ignored>"
        exit 0
fi

if [ "$#" -eq 3 ]
then
	scp $2 paresh@$3:$2
else
	while read -r ip
	do
		echo -e "\n----- $ip -----"
		scp $2 paresh@$ip:$2
	done < $1
fi
