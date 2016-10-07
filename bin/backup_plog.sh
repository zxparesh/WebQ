#!/bin/bash 

if [ $? -ne 0 ]; then
    echo "usage: $0 <folder_containing_file> <file_to_backup> <folder_where_to_back>"
fi

fol=$1
file=$2
backfol=$3

new_file=`date +'%F_%H%M%S'`-$file
cp $fol/$file $backfol/$new_file

if [ $? -ne 0 ]; then
	echo "failed to copy $fol/$file"
fi 
