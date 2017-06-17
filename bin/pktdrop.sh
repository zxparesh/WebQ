#!/bin/bash

# block packets from list of ips
# arg1 : file containing list of ips to block

if [ "$#" -lt 1 ]
then
        echo "usage: $0 <file containing list of ips to block>"
        exit 0
fi

while read -r ip
do
    echo -e "----- $ip -----"
    sudo iptables -I INPUT -s $ip -j DROP
done < $1
