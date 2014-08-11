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
    echo "Usage: $0 {pagerank,hop_dist,triangle_counting} <num_threads> {ours,theirs} {huge,soc-LiveJournal1,twitter_rv,big}"
    exit 1
}

#WORKLOAD_BASE=/run/shm/
BASE=/home/skaestle/projects/gm
WORKLOAD_BASE=$BASE/../graphs/

#WORKLOAD=$BASE/../graphs/huge.bin
#WORKLOAD=$BASE/../graphs/soc-LiveJournal1.bin
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

WORKLOAD=$WORKLOAD_BASE/$4.bin

[[ -f "${WORKLOAD}" ]] || error "Cannot find workload [$WORKLOAD]"

DEBUG=0
if [[ "$5" == "-d" ]]; then
	DEBUG=1
	shift
fi

shift
shift
shift
shift

echo "Executing program [${INPUT}] with [$NUM] threads"
echo "Loading workload from [${WORKLOAD}]"

if [[ $NUM -gt 32 ]]
then
    COREMAX=$(($NUM-1))
    AFF="0-64"
else
	if [[ $(hostname) == "sgs-r815-03" ]]; then
		COREMAX=$(($NUM*2-1))
		AFF="0-${COREMAX}:2"
	else
		COREMAX=$(($NUM-1))
		AFF="0-${COREMAX}"
	fi
fi

# --------------------------------------------------
# CONFIGURATION
# --------------------------------------------------
export SHL_HUGEPAGE=1
export SHL_REPLICATION=1
export SHL_DISTRIBUTION=1
# --------------------------------------------------

res=0
if [[ $DEBUG -eq 0 ]]; then
	. $BASE/env.sh
	set -x
	GOMP_CPU_AFFINITY="$AFF" SHL_CPU_AFFINITY="$AFF" \
		stdbuf -o0 -e0 -i0 ${INPUT} ${WORKLOAD} ${NUM} $@ | $BASE/scripts/extract_result.py -workload ${WORKLOAD}

	if [[ $? -ne 0 ]]; then
		error "Execution was unsuccessful"
	fi
else
	. $BASE/env.sh
	set -x
	GOMP_CPU_AFFINITY="$AFF" SHL_CPU_AFFINITY="$AFF" \
		gdb --args ${INPUT} ${WORKLOAD} ${NUM} $@
fi
