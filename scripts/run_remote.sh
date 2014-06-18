#!/bin/bash

# function error() {
# 	echo $1
# 	exit 1
# }

# function usage() {

#     echo "Usage: $0"
#     exit 1
# }

# [[ -n "$1" ]] || usage

function get_command() {
    next_command=". projects/gm/env.sh;\
	GOMP_CPU_AFFINITY=0-$1 projects/gm/apps/output_cpp/bin/pagerank projects/graphs/big.bin $1 |\
	projects/gm/scripts/extract_result.py"
}

(
    . ~/.bashrc
    echo "x y e"
    for i in {6..0}
    do
	get_command $((2**$i))
	echo "Executing command [${next_command}]" 1>&2
	exec_avg ssh sgs-r820-01 "${next_command}" | awk '/^comp:/ { print $2 }' | skstat.py
    done
)
