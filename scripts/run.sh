#!/bin/bash

# if [ yes != "$STDBUF" ]; then
# 	echo "Disabling buffer .. "
#     STDBUF=yes /usr/bin/stdbuf -i0 -o0 -e0 "$0" $@
#     exit $?
# fi

function error() {
	echo $1
	exit 1
}

function usage() {
    echo "Usage: $0 {pagerank,hop_dist,triangle_counting} <num_threads> {ours,theirs}"
    exit 1
}

BASE=/home/skaestle/projects/gm
#WORKLOAD=$BASE/../graphs/huge.bin
#WORKLOAD=$BASE/../graphs/soc-LiveJournal1.bin
WORKLOAD=$BASE/../graphs/twitter_rv.bin
#WORKLOAD=$BASE/../graphs/big.bin
#WORKLOAD=/mnt/scratch/skaestle/graphs/twitter_rv-in-order-rename.bin
#WORKLOAD=/mnt/scratch/skaestle/graphs/soc-Live

[[ -n "$3" ]] || usage

NUM=$2

[[ "$3" == "ours" ]] || [[ "$3" == "theirs" ]] || error "Cannot parse argument #3"

if [[ "$3" == "ours" ]]; then
	INPUT=$BASE/apps/output_cpp/bin/$1
fi

if [[ "$3" == "theirs" ]]; then
	INPUT=$BASE/../org_gm/apps/output_cpp/bin/$1
fi

[[ -f ${INPUT} ]] || error "Cannot find program [$INPUT]"

shift
shift
shift

echo "Executing program [${INPUT}]"

. $BASE/env.sh
GOMP_CPU_AFFINITY="0-${NUM}" stdbuf -o0 -e0 -i0 ${INPUT} ${WORKLOAD} ${NUM} $@ | $BASE/scripts/extract_result.py
