#!/bin/bash

## this scripts allow to run a solution locally assigning sockets automatically ##

if [ "$#" -lt 2 ]; then
    echo "Illegal number of parameters"
    echo "usege: $0 <executable> #workers"
    exit -1
fi

echo "Running $1 with $2 workers locally"

worker_sck=""
k=0
for (( i=0; i<$2; i++ ))
do  
   worker_sck+="worker${i}_sck "
    $1 false worker${i}_sck master_sck &#>/dev/null &  #redirecting the output to dev/null
   pid[((k++))]=$!
done

$1 true master_sck $worker_sck


for((i=0;i<k;++i)); do
    wait ${pid[i]}
    if [ $? -ne 0 ]; then
	echo "error waiting pid ${pid[i]}"
    fi
done

exit 0
