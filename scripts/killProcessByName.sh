#!/bin/bash

## kill a process by name on all machines of the cluster ##

if [ "$#" -ne 1 ]; then
    echo "Illegal number of parameters"
    echo "usege: $0 <executable> "
    exit -1
fi

echo "Killing all process in the cluster with name $1"

pkill $1

for ((i = 1 ; i <= 15 ; i++)); do
  ssh compute${i} "pkill $1"
done
