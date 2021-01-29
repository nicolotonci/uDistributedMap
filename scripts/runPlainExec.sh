#!/bin/bash

## script to run a comman on the cluster without assigning addresses and ports - Useful for executing simple commands ##   

if [ "$#" -lt 2 ]; then
    echo "Illegal number of parameters"
    echo "usege: $0 <executable> #workers #startingnode(Optional)"
    exit -1
fi

start_node=1

if [ -z ${3+x} ]; then start_node=1; else start_node=$3; fi


echo "Running $1 with $2 workers starting from node $3"


k=0
for (( i=${start_node}; i<${start_node}+$2 && i <= 15; i++ ))
do  
   ssh compute${i} $1 &#>/dev/null  #redirecting the output to dev/null
   pid[((k++))]=$!
done


for((i=0;i<k;++i)); do
    wait ${pid[i]}
    if [ $? -ne 0 ]; then
	echo "error waiting pid ${pid[i]}"
    fi
done

exit 0
